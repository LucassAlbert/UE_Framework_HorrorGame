#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Components/SaveComponent.h"
#include "Interfaces/SaveInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorSave, Log, All);

namespace HorrorSave
{
    bool WriteObject(UObject* Object, TArray<uint8>& Bytes)
    {
        FMemoryWriter Writer(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Archive.ArNoDelta = true;
        Object->Serialize(Archive);
        return !Archive.IsError();
    }

    bool ReadObject(UObject* Object, const TArray<uint8>& Bytes)
    {
        // Legacy snapshots may omit defaults. Reset only persistent properties before applying their tags.
        const UObject* Defaults = Object->GetClass()->GetDefaultObject();
        for (TFieldIterator<FProperty> Property(Object->GetClass()); Property; ++Property)
            if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Property->CopyCompleteValue_InContainer(Object, Defaults);
        FMemoryReader Reader(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        Archive.ArIsSaveGame = true;
        Archive.ArNoDelta = true;
        Object->Serialize(Archive);
        return !Archive.IsError();
    }
}

FString USaveSubsystem::GetSlotName(int32 SlotIndex)
{
    return IsValidSlot(SlotIndex) ? FString::Printf(TEXT("SaveSlot_%d"), SlotIndex) : FString();
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ActorRegistry = NewObject<UActorRegistry>(this);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USaveSubsystem::HandleWorldCleanup);
}

void USaveSubsystem::Deinitialize()
{
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    if (RestoreLoadHandle) RestoreLoadHandle->CancelHandle();
    RestoreLoadHandle.Reset();
    if (ActorRegistry) ActorRegistry->Reset();
    PendingSaveGame = nullptr;
    CurrentSaveGame = nullptr;
    LatestCandidate = nullptr;
    bScanningLatest = false;
    bBusy = false;
    Super::Deinitialize();
}

void USaveSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    if (World && World->GetGameInstance() == GetGameInstance())
    {
        ActorRegistry->Reset();
        DestroyedActors.Reset();
    }
}

void USaveSubsystem::RecordDestroyedActor(const FGuid& ActorID)
{
    if (!bRestoring && ActorID.IsValid()) DestroyedActors.Add(ActorID);
}

void USaveSubsystem::SaveGame() { SaveToSlot(0); }
void USaveSubsystem::LoadGame() { LoadFromSlot(0); }

TArray<FSaveSlotInfo> USaveSubsystem::GetSlots() const
{
    TArray<FSaveSlotInfo> Slots;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        FSaveSlotInfo Info;
        Info.SlotIndex = Index;
        Info.bExists = UGameplayStatics::DoesSaveGameExist(GetSlotName(Index), 0);
        Slots.Add(Info);
    }
    return Slots;
}

bool USaveSubsystem::SaveToSlot(int32 SlotIndex)
{
    if (bBusy || !IsValidSlot(SlotIndex) || !GetWorld()) return false;
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>(this);
    FString Error;
    if (!CaptureWorld(Snapshot, Error))
    {
        UE_LOG(LogHorrorSave, Error, TEXT("Cannot capture slot %d: %s"), SlotIndex, *Error);
        OnSaveCompleted.Broadcast(SlotIndex, false, FText::FromString(Error));
        return false;
    }
    PendingSaveGame = Snapshot;
    ActiveSlot = SlotIndex;
    OperationWorld = GetWorld();
    bBusy = true;
    UGameplayStatics::AsyncSaveGameToSlot(Snapshot, GetSlotName(SlotIndex), 0,
        FAsyncSaveGameToSlotDelegate::CreateUObject(this, &USaveSubsystem::OnSlotSaved));
    return true;
}

bool USaveSubsystem::LoadFromSlot(int32 SlotIndex)
{
    if (bBusy || !IsValidSlot(SlotIndex) || !GetWorld()) return false;
    if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(SlotIndex), 0))
    {
        OnLoadCompleted.Broadcast(SlotIndex, false, NSLOCTEXT("HorrorSave", "Missing", "Este slot nao possui um save."));
        return false;
    }
    ActiveSlot = SlotIndex;
    OperationWorld = GetWorld();
    bBusy = true;
    UGameplayStatics::AsyncLoadGameFromSlot(GetSlotName(SlotIndex), 0,
        FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &USaveSubsystem::OnSlotLoaded));
    return true;
}

bool USaveSubsystem::LoadLatestForCurrentLevel()
{
    if (bBusy || !GetWorld() || GetWorld()->bIsTearingDown) return false;
    bBusy = true;
    OperationWorld = GetWorld();
    LatestScanIndex = 0;
    LatestCandidateSlot = INDEX_NONE;
    LatestCandidate = nullptr;
    bScanningLatest = true;
    ScanNextLatestSlot();
    return true;
}

void USaveSubsystem::ScanNextLatestSlot()
{
    if (!bScanningLatest) return;
    while (LatestScanIndex < SlotCount && !UGameplayStatics::DoesSaveGameExist(GetSlotName(LatestScanIndex), 0)) ++LatestScanIndex;
    if (OperationWorld.IsValid() && OperationWorld.Get() == GetWorld() && LatestScanIndex < SlotCount)
    {
        UGameplayStatics::AsyncLoadGameFromSlot(GetSlotName(LatestScanIndex), 0,
            FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &USaveSubsystem::OnLatestSlotRead));
        return;
    }
    if (LatestCandidate && OperationWorld.IsValid() && OperationWorld.Get() == GetWorld())
    {
        ActiveSlot = LatestCandidateSlot;
        USystemSaveGame* Candidate = LatestCandidate;
        LatestCandidate = nullptr;
        bScanningLatest = false;
        OnSlotLoaded(GetSlotName(ActiveSlot), 0, Candidate);
        return;
    }
    LatestCandidate = nullptr;
    bScanningLatest = false;
    bBusy = false;
    OnLoadCompleted.Broadcast(INDEX_NONE, false, NSLOCTEXT("HorrorSave", "NoLevelSave", "Nenhum save válido foi encontrado para o mapa atual."));
}

void USaveSubsystem::OnLatestSlotRead(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame)
{
    if (!bScanningLatest) return;
    USystemSaveGame* Candidate = Cast<USystemSaveGame>(LoadedGame);
    if (!Candidate || Candidate->FormatVersion < 1 || Candidate->FormatVersion > 2)
    {
        UE_LOG(LogHorrorSave, Warning, TEXT("Cannot use slot %d while selecting the latest save: invalid data or unsupported version."), LatestScanIndex);
    }
    else if (OperationWorld.IsValid() && OperationWorld.Get() == GetWorld()
        && Candidate->CurrentLevel == UGameplayStatics::GetCurrentLevelName(this, true)
        && (!LatestCandidate || Candidate->SavedAt > LatestCandidate->SavedAt))
    {
        LatestCandidate = Candidate;
        LatestCandidateSlot = LatestScanIndex;
    }
    ++LatestScanIndex;
    ScanNextLatestSlot();
}

bool USaveSubsystem::CaptureWorld(USystemSaveGame* Snapshot, FString& Error)
{
    Snapshot->CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
    Snapshot->SavedAt = FDateTime::UtcNow();
    Snapshot->DestroyedActors = DestroyedActors;
    TSet<FGuid> IDs;
    for (AActor* Actor : ActorRegistry->GetRegisteredActors())
    {
        if (Actor->GetWorld() != GetWorld()) continue;
        const USaveComponent* Save = Actor->FindComponentByClass<USaveComponent>();
        if (!Save || !Save->PersistentID.IsValid() || IDs.Contains(Save->PersistentID))
        {
            Error = FString::Printf(TEXT("Invalid or duplicate save ID: %s"), *GetNameSafe(Actor));
            return false;
        }
        IDs.Add(Save->PersistentID);
        FActorSaveData Data;
        Data.ActorID = Save->PersistentID;
        if (Save->bRecreateOnLoad) Data.RuntimeClass = Actor->GetClass();
        Data.Transform = Actor->GetActorTransform();
        UPrimitiveComponent* PhysicsRoot = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
        if (Save->bSavePhysics && PhysicsRoot && PhysicsRoot->IsSimulatingPhysics())
        {
            Data.bHasPhysicsState = true;
            Data.LinearVelocity = PhysicsRoot->GetPhysicsLinearVelocity();
            Data.AngularVelocity = PhysicsRoot->GetPhysicsAngularVelocityInRadians();
            Data.bPhysicsSleeping = !PhysicsRoot->IsAnyRigidBodyAwake();
        }
        if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Actor))
        {
            if (!Listener->ValidateSaveState(Error)) return false;
            Listener->GatherSaveDependencies(Data.Dependencies);
        }
        if (!HorrorSave::WriteObject(Actor, Data.ByteData)) { Error = TEXT("Actor serialization failed."); return false; }
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
        {
            if (Component == Save) continue;
            if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Component))
            {
                if (!Listener->ValidateSaveState(Error)) return false;
                Listener->GatherSaveDependencies(Data.Dependencies);
            }
            FComponentSaveData ComponentData;
            ComponentData.ComponentName = Component->GetFName();
            if (!HorrorSave::WriteObject(Component, ComponentData.ByteData)) { Error = TEXT("Component serialization failed."); return false; }
            Data.Components.Add(MoveTemp(ComponentData));
        }
        Snapshot->DestroyedActors.Remove(Data.ActorID);
        Snapshot->SavedActors.Add(MoveTemp(Data));
    }
    return true;
}

bool USaveSubsystem::RestoreWorld(const USystemSaveGame* Snapshot, FString& Error)
{
    if (!Snapshot || (Snapshot->FormatVersion < 1 || Snapshot->FormatVersion > 2))
    {
        Error = TEXT("Unsupported or corrupt save format.");
        return false;
    }
    if (Snapshot->CurrentLevel != UGameplayStatics::GetCurrentLevelName(this, true))
    {
        Error = TEXT("Load the saved map before restoring this slot. Cross-map restoration is not implemented yet.");
        return false;
    }

    TMap<FGuid, AActor*> Actors;
    for (AActor* Actor : ActorRegistry->GetRegisteredActors())
    {
        if (Actor->GetWorld() != GetWorld()) continue;
        const USaveComponent* Save = Actor->FindComponentByClass<USaveComponent>();
        if (!Save || !Save->PersistentID.IsValid() || Actors.Contains(Save->PersistentID))
        {
            Error = TEXT("World contains invalid or duplicate save IDs.");
            return false;
        }
        Actors.Add(Save->PersistentID, Actor);
    }
    // Validate the whole snapshot before spawning or mutating existing participants.
    TSet<FGuid> SavedIDs;
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        AActor* const* Existing = Actors.Find(Data.ActorID);
        if (!Data.ActorID.IsValid() || SavedIDs.Contains(Data.ActorID)
            || Snapshot->DestroyedActors.Contains(Data.ActorID) || Data.Transform.ContainsNaN()
            || Data.LinearVelocity.ContainsNaN() || Data.AngularVelocity.ContainsNaN())
        {
            Error = TEXT("Save contains invalid or duplicate actor records.");
            return false;
        }
        SavedIDs.Add(Data.ActorID);
        UClass* RuntimeClass = Data.RuntimeClass.Get();
        if (!Existing && (!RuntimeClass || RuntimeClass->HasAnyClassFlags(CLASS_Abstract)
            || !RuntimeClass->IsChildOf(AActor::StaticClass())))
        {
            Error = TEXT("Saved actor is missing or its runtime class could not be loaded.");
            return false;
        }
        if (Existing && !Data.RuntimeClass.IsNull() && (*Existing)->GetClass() != RuntimeClass)
        {
            Error = TEXT("Runtime actor class does not match the save.");
            return false;
        }
    }

    TGuardValue<bool> RestoringGuard(bRestoring, true);
    TArray<AActor*> Spawned;
    auto FailSpawn = [&Spawned, this, &Error](const TCHAR* Message)
    {
        Error = Message;
        for (AActor* Actor : Spawned)
        {
            ActorRegistry->UnregisterActor(Actor);
            Actor->Destroy();
        }
        return false;
    };
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        if (Actors.Contains(Data.ActorID)) continue;
        AActor* Actor = GetWorld()->SpawnActorDeferred<AActor>(Data.RuntimeClass.Get(), Data.Transform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Actor) return FailSpawn(TEXT("Could not recreate a saved runtime actor."));
        Spawned.Add(Actor);
        USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>();
        if (!Marker || !Marker->bRecreateOnLoad || !Marker->PersistentKey.IsNone())
            return FailSpawn(TEXT("Saved class does not support runtime reconstruction."));
        Marker->PersistentID = Data.ActorID;
        Actor->FinishSpawning(Data.Transform);
        ActorRegistry->RegisterActor(Actor);
        Actors.Add(Data.ActorID, Actor);
    }
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        TInlineComponentArray<UActorComponent*> Components(Actors[Data.ActorID]);
        TSet<FName> ComponentNames;
        for (const FComponentSaveData& ComponentData : Data.Components)
        {
            if (ComponentNames.Contains(ComponentData.ComponentName)
                || !Components.ContainsByPredicate([&ComponentData](const UActorComponent* Component)
                    { return Component->GetFName() == ComponentData.ComponentName; }))
                return FailSpawn(TEXT("Saved component is missing or duplicated. Migration is required."));
            ComponentNames.Add(ComponentData.ComponentName);
        }
    }

    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        AActor* Actor = Actors[Data.ActorID];
        if (!HorrorSave::ReadObject(Actor, Data.ByteData)) return FailSpawn(TEXT("Actor restore failed."));
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (const FComponentSaveData& ComponentData : Data.Components)
        {
            UActorComponent* Component = *Components.FindByPredicate([&ComponentData](const UActorComponent* Candidate)
                { return Candidate->GetFName() == ComponentData.ComponentName; });
            if (!HorrorSave::ReadObject(Component, ComponentData.ByteData)) return FailSpawn(TEXT("Component restore failed."));
        }
        if (Actor->FindComponentByClass<USaveComponent>()->bSaveTransform)
            Actor->SetActorTransform(Data.Transform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    // Domain validation follows deserialization and precedes presentation and actor removal.
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        AActor* Actor = Actors[Data.ActorID];
        if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Actor))
            if (!Listener->ValidateSaveState(Error)) { FailSpawn(TEXT("Saved actor state is invalid.")); return false; }
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
            if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Component))
                if (!Listener->ValidateSaveState(Error)) { FailSpawn(TEXT("Saved component state is invalid.")); return false; }
    }
    // Drops created after this snapshot must disappear when rewinding.
    for (const TPair<FGuid, AActor*>& Entry : Actors)
    {
        const USaveComponent* Marker = Entry.Value->FindComponentByClass<USaveComponent>();
        if (Marker && Marker->bRecreateOnLoad && !SavedIDs.Contains(Entry.Key))
            Entry.Value->Destroy();
    }
    for (const FGuid& ID : Snapshot->DestroyedActors)
    {
        if (AActor* const* Actor = Actors.Find(ID)) (*Actor)->Destroy();
    }
    DestroyedActors = Snapshot->DestroyedActors;
    // Notify after every participant has restored, so dependencies observe the final state.
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        AActor* Actor = Actors[Data.ActorID];
        if (!IsValid(Actor)) continue;
        if (ISaveStateListener* Listener = Cast<ISaveStateListener>(Actor)) Listener->OnSaveStateRestored();
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
            if (ISaveStateListener* Listener = Cast<ISaveStateListener>(Component)) Listener->OnSaveStateRestored();
        if (Actor->Implements<USaveInterface>()) ISaveInterface::Execute_OnActorLoaded(Actor);
        if (Data.bHasPhysicsState)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
            if (!Root || !Root->IsSimulatingPhysics())
            {
                Error = FString::Printf(TEXT("Restored actor %s no longer supports its saved physics state."), *GetNameSafe(Actor));
                return false;
            }
            Root->SetPhysicsLinearVelocity(Data.LinearVelocity);
            Root->SetPhysicsAngularVelocityInRadians(Data.AngularVelocity);
            if (Data.bPhysicsSleeping) Root->PutAllRigidBodiesToSleep();
        }
    }
    return true;
}

void USaveSubsystem::OnSlotSaved(const FString& SlotName, int32 UserIndex, bool bSuccess)
{
    const int32 Slot = ActiveSlot;
    if (bSuccess)
    {
        CurrentSaveGame = PendingSaveGame;
        if (OperationWorld.IsValid() && OperationWorld.Get() == GetWorld())
        {
            for (AActor* Actor : ActorRegistry->GetRegisteredActors())
            {
                const USaveComponent* Save = Actor->FindComponentByClass<USaveComponent>();
                if (Actor->GetWorld() == GetWorld() && Save && Actor->Implements<USaveInterface>()
                    && PendingSaveGame->SavedActors.ContainsByPredicate([Save](const FActorSaveData& Data)
                        { return Data.ActorID == Save->PersistentID; }))
                {
                    ISaveInterface::Execute_OnActorSaved(Actor);
                }
            }
        }
    }
    else UE_LOG(LogHorrorSave, Error, TEXT("Disk write failed: %s"), *SlotName);
    PendingSaveGame = nullptr;
    ActiveSlot = INDEX_NONE;
    bBusy = false;
    OnSaveCompleted.Broadcast(Slot, bSuccess, bSuccess
        ? NSLOCTEXT("HorrorSave", "Saved", "Jogo salvo.")
        : NSLOCTEXT("HorrorSave", "WriteFailed", "Nao foi possivel gravar o save."));
}

void USaveSubsystem::OnSlotLoaded(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame)
{
    PendingSaveGame = Cast<USystemSaveGame>(LoadedGame);
    TArray<FSoftObjectPath> Paths;
    if (PendingSaveGame)
    {
        for (const FActorSaveData& Data : PendingSaveGame->SavedActors)
        {
            if (!Data.RuntimeClass.IsNull()) Paths.AddUnique(Data.RuntimeClass.ToSoftObjectPath());
            for (const FSoftObjectPath& Path : Data.Dependencies) if (Path.IsValid()) Paths.AddUnique(Path);
        }
    }
    if (Paths.IsEmpty()) { FinishSlotRestore(); return; }
    RestoreLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateUObject(this, &USaveSubsystem::FinishSlotRestore));
    if (!RestoreLoadHandle) FinishSlotRestore();
}

void USaveSubsystem::FinishSlotRestore()
{
    const int32 Slot = ActiveSlot;
    FString Error;
    bool bSuccess = false;
    if (!OperationWorld.IsValid() || OperationWorld.Get() != GetWorld()) Error = TEXT("World changed while loading.");
    else bSuccess = RestoreWorld(PendingSaveGame, Error);
    if (bSuccess) CurrentSaveGame = PendingSaveGame;
    else UE_LOG(LogHorrorSave, Error, TEXT("Restore failed for slot %d: %s"), Slot, *Error);
    PendingSaveGame = nullptr;
    RestoreLoadHandle.Reset();
    ActiveSlot = INDEX_NONE;
    bBusy = false;
    OnLoadCompleted.Broadcast(Slot, bSuccess, bSuccess
        ? NSLOCTEXT("HorrorSave", "Loaded", "Jogo carregado.") : FText::FromString(Error));
}
