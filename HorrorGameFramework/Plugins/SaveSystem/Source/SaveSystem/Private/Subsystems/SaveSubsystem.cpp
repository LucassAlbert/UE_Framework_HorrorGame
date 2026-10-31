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
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "Containers/Ticker.h"

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

    bool HasPersistentProperties(const UClass* Class)
    {
        for (TFieldIterator<FProperty> Property(Class); Property; ++Property)
            if (Property->HasAnyPropertyFlags(CPF_SaveGame)) return true;
        return false;
    }

    bool IsEmptyLegacyComponent(const FComponentSaveData& Data)
    {
        // Old snapshots also recorded components with no persistent payload. Their generated
        // names may differ after travel. Only an exact empty serialization is dispensable.
        static const TArray<uint8> EmptyPayload = []()
        {
            TArray<uint8> Bytes;
            WriteObject(UActorComponent::StaticClass()->GetDefaultObject(), Bytes);
            return Bytes;
        }();
        return !EmptyPayload.IsEmpty() && Data.ByteData == EmptyPayload;
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
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USaveSubsystem::HandleMapLoaded);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USaveSubsystem::HandleWorldCleanup);
}

void USaveSubsystem::Deinitialize()
{
    ++LoadGeneration;
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    if (LoadDeadlineHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(LoadDeadlineHandle);
    LoadDeadlineHandle.Reset();
    AreaRestoreCompletion.Unbind();
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
    if (World && World == GetWorld())
    {
        ActorRegistry->Reset();
        DestroyedActors.Reset();
        CachedAreaActors.Reset();
        VisitedAreaIds.Reset();
        WorldProvider.Reset();
        if (bLoadingSlot && !bAwaitingMap) CompleteLoad(false, NSLOCTEXT("HorrorSave", "WorldInterrupted", "O mundo mudou durante o carregamento."));
    }
}

void USaveSubsystem::RecordDestroyedActor(const FGuid& ActorID)
{
    if (!bRestoring && ActorID.IsValid())
    {
        DestroyedActors.Add(ActorID);
        CachedAreaActors.Remove(ActorID);
    }
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
    if (bBusy || !IsValidSlot(SlotIndex) || !GetWorld() || !IsWorldReady()) return false;
    if (ISaveWorldProvider* Provider = Cast<ISaveWorldProvider>(WorldProvider.Get())) Provider->RefreshSaveAreaBindings();
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

bool USaveSubsystem::CaptureWorld(USystemSaveGame* Snapshot, FString& Error, FName AreaId, bool bOnlyArea)
{
    Snapshot->CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
    Snapshot->SavedAt = FDateTime::UtcNow();
    const FString Package = GetWorld()->PersistentLevel->GetOutermost()->GetName();
    Snapshot->PersistentMapPackage = FPackageName::GetLongPackagePath(Package) / Snapshot->CurrentLevel;
    if (!bOnlyArea)
    {
        if (const ISaveWorldProvider* Provider = Cast<ISaveWorldProvider>(WorldProvider.Get()))
            if (!Provider->CaptureWorldMetadata(*Snapshot, Error)) return false;
        Snapshot->VisitedAreas = VisitedAreaIds.Array();
    }
    Snapshot->DestroyedActors = DestroyedActors;
    TSet<FGuid> IDs;
    for (AActor* Actor : ActorRegistry->GetRegisteredActors())
    {
        if (Actor->GetWorld() != GetWorld()) continue;
        const USaveComponent* Save = Actor->FindComponentByClass<USaveComponent>();
        if (bOnlyArea && (!Save || Save->StreamingAreaId != AreaId)) continue;
        if (!Save || !Save->PersistentID.IsValid() || IDs.Contains(Save->PersistentID))
        {
            Error = FString::Printf(TEXT("Invalid or duplicate save ID: %s"), *GetNameSafe(Actor));
            return false;
        }
        IDs.Add(Save->PersistentID);
        FActorSaveData Data;
        Data.ActorID = Save->PersistentID;
        Data.StreamingAreaId = Save->StreamingAreaId;
        if (!Data.StreamingAreaId.IsNone()) Snapshot->VisitedAreas.AddUnique(Data.StreamingAreaId);
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
            if (!HorrorSave::HasPersistentProperties(Component->GetClass()) && !Cast<ISaveStateListener>(Component)) continue;
            FComponentSaveData ComponentData;
            ComponentData.ComponentName = Component->GetFName();
            if (!HorrorSave::WriteObject(Component, ComponentData.ByteData)) { Error = TEXT("Component serialization failed."); return false; }
            Data.Components.Add(MoveTemp(ComponentData));
        }
        Snapshot->DestroyedActors.Remove(Data.ActorID);
        Snapshot->SavedActors.Add(MoveTemp(Data));
    }
    if (!bOnlyArea)
        for (const TPair<FGuid, FActorSaveData>& Entry : CachedAreaActors)
            if (!IDs.Contains(Entry.Key) && !DestroyedActors.Contains(Entry.Key)) Snapshot->SavedActors.Add(Entry.Value);
    return true;
}

bool USaveSubsystem::RestoreWorld(const USystemSaveGame* Snapshot, FString& Error, FName AreaId, bool bOnlyArea)
{
    if (!Snapshot || (Snapshot->FormatVersion < 1 || Snapshot->FormatVersion > 3))
    {
        Error = TEXT("Unsupported or corrupt save format.");
        return false;
    }
    if (Snapshot->CurrentLevel != UGameplayStatics::GetCurrentLevelName(this, true))
    {
        Error = TEXT("The loaded world does not match the saved map.");
        return false;
    }

    TMap<FGuid, AActor*> Actors;
    for (AActor* Actor : ActorRegistry->GetRegisteredActors())
    {
        if (Actor->GetWorld() != GetWorld()) continue;
        const USaveComponent* Save = Actor->FindComponentByClass<USaveComponent>();
        if (bOnlyArea && (!Save || Save->StreamingAreaId != AreaId)) continue;
        if (!Save || !Save->PersistentID.IsValid() || Actors.Contains(Save->PersistentID))
        {
            Error = TEXT("World contains invalid or duplicate save IDs.");
            return false;
        }
        Actors.Add(Save->PersistentID, Actor);
    }
    TArray<FActorSaveData> Records;
    TSet<FGuid> AllIDs;
    for (const FActorSaveData& Data : Snapshot->SavedActors)
    {
        if (!Data.ActorID.IsValid() || AllIDs.Contains(Data.ActorID) || Snapshot->DestroyedActors.Contains(Data.ActorID)
            || !Data.Transform.IsValid() || Data.LinearVelocity.ContainsNaN() || Data.AngularVelocity.ContainsNaN())
        { Error = TEXT("Save contains invalid actor records."); return false; }
        AllIDs.Add(Data.ActorID);
        if (bOnlyArea ? Data.StreamingAreaId == AreaId : IsRecordResident(Data)) Records.Add(Data);
    }
    // Validate before spawning or mutating the selected participants.
    TSet<FGuid> SavedIDs;
    for (const FActorSaveData& Data : Records)
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
    for (const FActorSaveData& Data : Records)
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
        Marker->StreamingAreaId = Data.StreamingAreaId;
        Actor->FinishSpawning(Data.Transform);
        ActorRegistry->RegisterActor(Actor);
        Actors.Add(Data.ActorID, Actor);
    }
    for (const FActorSaveData& Data : Records)
    {
        TInlineComponentArray<UActorComponent*> Components(Actors[Data.ActorID]);
        TSet<FName> ComponentNames;
        for (const FComponentSaveData& ComponentData : Data.Components)
        {
            if (ComponentNames.Contains(ComponentData.ComponentName))
                return FailSpawn(*FString::Printf(TEXT("Duplicate saved component %s on %s."),
                    *ComponentData.ComponentName.ToString(), *GetNameSafe(Actors[Data.ActorID])));
            ComponentNames.Add(ComponentData.ComponentName);
            if (!Components.ContainsByPredicate([&ComponentData](const UActorComponent* Component)
                { return Component->GetFName() == ComponentData.ComponentName; }) && !HorrorSave::IsEmptyLegacyComponent(ComponentData))
                return FailSpawn(*FString::Printf(TEXT("Saved component %s is missing on %s. Migration is required."),
                    *ComponentData.ComponentName.ToString(), *GetNameSafe(Actors[Data.ActorID])));
        }
    }

    for (const FActorSaveData& Data : Records)
    {
        AActor* Actor = Actors[Data.ActorID];
        Actor->FindComponentByClass<USaveComponent>()->StreamingAreaId = Data.StreamingAreaId;
        if (!HorrorSave::ReadObject(Actor, Data.ByteData)) return FailSpawn(TEXT("Actor restore failed."));
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (const FComponentSaveData& ComponentData : Data.Components)
        {
            UActorComponent* const* Component = Components.FindByPredicate([&ComponentData](const UActorComponent* Candidate)
                { return Candidate->GetFName() == ComponentData.ComponentName; });
            if (!Component)
            {
                if (!HorrorSave::IsEmptyLegacyComponent(ComponentData)) return FailSpawn(TEXT("A persistent component disappeared during restore."));
                continue;
            }
            if (!HorrorSave::ReadObject(*Component, ComponentData.ByteData)) return FailSpawn(TEXT("Component restore failed."));
        }
        if (Actor->FindComponentByClass<USaveComponent>()->bSaveTransform)
            Actor->SetActorTransform(Data.Transform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    // Domain validation follows deserialization and precedes presentation and actor removal.
    for (const FActorSaveData& Data : Records)
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
    for (const FActorSaveData& Data : Records)
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
