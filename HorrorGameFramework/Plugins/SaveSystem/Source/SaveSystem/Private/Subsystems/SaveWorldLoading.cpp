#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Components/SaveComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogSaveWorldLoading, Log, All);

bool USaveSubsystem::RegisterWorldProvider(UObject* Provider)
{
    if (!IsValid(Provider) || Provider->GetWorld() != GetWorld() || !Cast<ISaveWorldProvider>(Provider)) return false;
    if (WorldProvider.IsValid() && WorldProvider.Get() != Provider) return false;
    WorldProvider = Provider;
    return true;
}
void USaveSubsystem::UnregisterWorldProvider(UObject* Provider)
{
    if (WorldProvider.Get() == Provider) WorldProvider.Reset();
}
bool USaveSubsystem::IsWorldReady() const
{
    const ISaveWorldProvider* Provider = Cast<ISaveWorldProvider>(WorldProvider.Get());
    return !Provider || Provider->IsSaveWorldReady();
}
void USaveSubsystem::NotifyWorldReady() { if (IsWorldReady()) OnWorldReady.Broadcast(); }
bool USaveSubsystem::IsRecordResident(const FActorSaveData& Data) const
{
    if (Data.StreamingAreaId.IsNone()) return true;
    const ISaveWorldProvider* Provider = Cast<ISaveWorldProvider>(WorldProvider.Get());
    return Provider && Provider->IsSaveAreaResident(Data.StreamingAreaId);
}
void USaveSubsystem::BeginLoadDeadline()
{
    LastLoadError = FText::GetEmpty();
    LoadDeadline = FPlatformTime::Seconds() + 90.0;
    LoadDeadlineHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
    {
        if (!bBusy || (!bLoadingSlot && !bRestoringArea)) return false;
        if (FPlatformTime::Seconds() < LoadDeadline) return true;
        CompleteLoad(false, NSLOCTEXT("HorrorSave", "LoadTimeout", "O carregamento não terminou no tempo esperado. Tente outro save."));
        return false;
    }), .25f);
    if (bLoadingSlot) OnLoadStarted.Broadcast();
}
bool USaveSubsystem::LoadFromSlot(int32 SlotIndex)
{
    if (bBusy || !IsValidSlot(SlotIndex) || !GetWorld()) return false;
    if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(SlotIndex), 0))
    {
        OnLoadCompleted.Broadcast(SlotIndex, false, NSLOCTEXT("HorrorSave", "Missing", "Este slot não possui um save."));
        return false;
    }
    ActiveSlot = SlotIndex;
    OperationWorld = GetWorld();
    bBusy = bLoadingSlot = true;
    ++LoadGeneration;
    BeginLoadDeadline();
    UGameplayStatics::AsyncLoadGameFromSlot(GetSlotName(SlotIndex), 0,
        FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &USaveSubsystem::OnSlotLoaded, LoadGeneration));
    return true;
}
bool USaveSubsystem::LoadLatestForCurrentLevel() { return StartLatestScan(true); }
bool USaveSubsystem::LoadLatest() { return StartLatestScan(false); }
bool USaveSubsystem::StartLatestScan(bool bCurrentOnly)
{
    if (bBusy || !GetWorld() || GetWorld()->bIsTearingDown) return false;
    bBusy = bLoadingSlot = true;
    bCurrentMapOnly = bCurrentOnly;
    OperationWorld = GetWorld();
    ActiveSlot = INDEX_NONE;
    LatestScanIndex = 0;
    LatestCandidateSlot = INDEX_NONE;
    LatestCandidate = nullptr;
    bScanningLatest = true;
    ++LoadGeneration;
    BeginLoadDeadline();
    ScanNextLatestSlot();
    return true;
}
void USaveSubsystem::ScanNextLatestSlot()
{
    if (!bScanningLatest) return;
    while (LatestScanIndex < SlotCount && !UGameplayStatics::DoesSaveGameExist(GetSlotName(LatestScanIndex), 0)) ++LatestScanIndex;
    if (!OperationWorld.IsValid() || OperationWorld.Get() != GetWorld())
    { CompleteLoad(false, NSLOCTEXT("HorrorSave", "ChangedWorld", "O mundo mudou durante a leitura dos slots.")); return; }
    if (LatestScanIndex < SlotCount)
    {
        UGameplayStatics::AsyncLoadGameFromSlot(GetSlotName(LatestScanIndex), 0,
            FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &USaveSubsystem::OnLatestSlotRead, LoadGeneration));
        return;
    }
    bScanningLatest = false;
    if (LatestCandidate)
    {
        ActiveSlot = LatestCandidateSlot;
        USystemSaveGame* Candidate = LatestCandidate;
        LatestCandidate = nullptr;
        OnSlotLoaded(GetSlotName(ActiveSlot), 0, Candidate, LoadGeneration);
    }
    else CompleteLoad(false, bCurrentMapOnly
        ? NSLOCTEXT("HorrorSave", "NoLevelSave", "Nenhum save válido foi encontrado para o mapa atual.")
        : NSLOCTEXT("HorrorSave", "NoSave", "Nenhum save válido foi encontrado."));
}
void USaveSubsystem::OnLatestSlotRead(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame, uint32 Request)
{
    if (!bScanningLatest || Request != LoadGeneration) return;
    USystemSaveGame* Candidate = Cast<USystemSaveGame>(LoadedGame);
    if (!Candidate || Candidate->FormatVersion < 1 || Candidate->FormatVersion > 3 || Candidate->CurrentLevel.IsEmpty())
    { UE_LOG(LogSaveWorldLoading, Warning, TEXT("Ignoring slot %d: invalid header or unsupported version."), LatestScanIndex); }
    else if ((!bCurrentMapOnly || Candidate->CurrentLevel == UGameplayStatics::GetCurrentLevelName(this, true))
        && (!LatestCandidate || Candidate->SavedAt > LatestCandidate->SavedAt))
    { LatestCandidate = Candidate; LatestCandidateSlot = LatestScanIndex; }
    ++LatestScanIndex;
    ScanNextLatestSlot();
}
void USaveSubsystem::OnSlotLoaded(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame, uint32 Request)
{
    if (!bLoadingSlot || !bBusy || Request != LoadGeneration) return;
    PendingSaveGame = Cast<USystemSaveGame>(LoadedGame);
    if (!PendingSaveGame || PendingSaveGame->FormatVersion < 1 || PendingSaveGame->FormatVersion > 3 || PendingSaveGame->CurrentLevel.IsEmpty())
    { CompleteLoad(false, NSLOCTEXT("HorrorSave", "BadHeader", "Save inválido ou de uma versão não compatível.")); return; }
    FString TargetPackage = PendingSaveGame->PersistentMapPackage;
    const bool bSameMap = PendingSaveGame->CurrentLevel == UGameplayStatics::GetCurrentLevelName(this, true)
        && (TargetPackage.IsEmpty() || FPackageName::GetLongPackagePath(GetWorld()->PersistentLevel->GetOutermost()->GetName()) / PendingSaveGame->CurrentLevel == TargetPackage);
    if (!bSameMap)
    {
        if (TargetPackage.IsEmpty() && !FPackageName::SearchForPackageOnDisk(PendingSaveGame->CurrentLevel, &TargetPackage))
        { CompleteLoad(false, NSLOCTEXT("HorrorSave", "MissingLegacyMap", "O mapa deste save antigo não foi encontrado.")); return; }
        if (!FPackageName::IsValidLongPackageName(TargetPackage) || !FPackageName::DoesPackageExist(TargetPackage))
        { CompleteLoad(false, NSLOCTEXT("HorrorSave", "MissingMap", "O mapa deste save não está disponível.")); return; }
        bAwaitingMap = true;
        UGameplayStatics::OpenLevel(this, FName(*TargetPackage));
        return;
    }
    PreparePendingWorld(Request);
}
void USaveSubsystem::HandleMapLoaded(UWorld* World)
{
    if (!bAwaitingMap || !bLoadingSlot || !World || World->GetGameInstance() != GetGameInstance()) return;
    bAwaitingMap = false;
    OperationWorld = World;
    PreparePendingWorld(LoadGeneration);
}
void USaveSubsystem::PreparePendingWorld(uint32 Request)
{
    if (!bLoadingSlot || Request != LoadGeneration || !PendingSaveGame) return;
    if (ISaveWorldProvider* Provider = Cast<ISaveWorldProvider>(WorldProvider.Get()))
    {
        bPreparingWorld = true;
        Provider->PrepareSaveWorld(*PendingSaveGame, FSaveWorldPrepared::CreateWeakLambda(this,
            [this, Request](bool bSuccess, FText Message)
            {
                if (Request != LoadGeneration || !bLoadingSlot) return;
                bPreparingWorld = false;
                if (!bSuccess) CompleteLoad(false, Message);
                else LoadResidentDependencies(Request);
            }));
    }
    else if (!PendingSaveGame->StreamingWorldId.IsNone())
        CompleteLoad(false, NSLOCTEXT("HorrorSave", "NoWorldProvider", "O mapa não possui o coordenador de áreas exigido pelo save."));
    else LoadResidentDependencies(Request);
}
void USaveSubsystem::LoadResidentDependencies(uint32 Request)
{
    if (!PendingSaveGame || Request != LoadGeneration || !bBusy) return;
    TArray<FSoftObjectPath> Paths;
    for (const FActorSaveData& Data : PendingSaveGame->SavedActors)
    {
        if (bRestoringArea ? Data.StreamingAreaId != RestoringAreaId : !IsRecordResident(Data)) continue;
        if (!Data.RuntimeClass.IsNull()) Paths.AddUnique(Data.RuntimeClass.ToSoftObjectPath());
        for (const FSoftObjectPath& Path : Data.Dependencies) if (Path.IsValid()) Paths.AddUnique(Path);
    }
    Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.ResolveObject() != nullptr; });
    if (Paths.IsEmpty()) { FinishSlotRestore(); return; }
    RestoreLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateWeakLambda(this, [this, Request]
        { if (Request == LoadGeneration && bBusy) FinishSlotRestore(); }));
    if (!RestoreLoadHandle) CompleteLoad(false, NSLOCTEXT("HorrorSave", "DependenciesFailed", "Não foi possível preparar os recursos do save."));
}
void USaveSubsystem::FinishSlotRestore()
{
    FString Error;
    bool bSuccess = false;
    if (!OperationWorld.IsValid() || OperationWorld.Get() != GetWorld()) Error = TEXT("World changed while loading.");
    else bSuccess = RestoreWorld(PendingSaveGame, Error, RestoringAreaId, bRestoringArea);
    if (bSuccess && !bRestoringArea)
    {
        CurrentSaveGame = PendingSaveGame;
        CachedAreaActors.Reset();
        VisitedAreaIds.Reset();
        for (FName Area : PendingSaveGame->VisitedAreas) VisitedAreaIds.Add(Area);
        for (const FActorSaveData& Record : PendingSaveGame->SavedActors)
            if (!Record.StreamingAreaId.IsNone()) CachedAreaActors.Add(Record.ActorID, Record);
    }
    CompleteLoad(bSuccess, bSuccess ? NSLOCTEXT("HorrorSave", "Loaded", "Jogo carregado.") : FText::FromString(Error));
}
void USaveSubsystem::CompleteLoad(bool bSuccess, const FText& Message)
{
    const int32 Slot = ActiveSlot;
    const bool bArea = bRestoringArea;
    FSaveAreaRestored AreaCompletion = MoveTemp(AreaRestoreCompletion);
    ++LoadGeneration;
    if (LoadDeadlineHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(LoadDeadlineHandle);
    LoadDeadlineHandle.Reset();
    if (RestoreLoadHandle) RestoreLoadHandle->CancelHandle();
    RestoreLoadHandle.Reset();
    PendingSaveGame = nullptr;
    LatestCandidate = nullptr;
    ActiveSlot = INDEX_NONE;
    RestoringAreaId = NAME_None;
    bBusy = bLoadingSlot = bAwaitingMap = bPreparingWorld = bRestoringArea = bScanningLatest = false;
    if (!bSuccess)
    {
        LastLoadError = Message;
        UE_LOG(LogSaveWorldLoading, Warning, TEXT("Restore failed (slot %d, area %d): %s"), Slot, bArea, *Message.ToString());
    }
    if (bArea) AreaCompletion.ExecuteIfBound(bSuccess, Message);
    else OnLoadCompleted.Broadcast(Slot, bSuccess, Message);
}
bool USaveSubsystem::CacheStreamingArea(FName AreaId, FString& Error)
{
    if (bBusy || AreaId.IsNone()) { Error = TEXT("An area snapshot cannot be captured during another save operation."); return false; }
    USystemSaveGame* Segment = NewObject<USystemSaveGame>(this);
    if (!CaptureWorld(Segment, Error, AreaId, true)) return false;
    for (auto It = CachedAreaActors.CreateIterator(); It; ++It)
        if (It.Value().StreamingAreaId == AreaId) It.RemoveCurrent();
    for (const FActorSaveData& Record : Segment->SavedActors) CachedAreaActors.Add(Record.ActorID, Record);
    VisitedAreaIds.Add(AreaId);
    return true;
}
void USaveSubsystem::EvictStreamingRuntimeActors(FName AreaId)
{
    if (AreaId.IsNone() || (bBusy && !HasPendingLoad())) return;
    TGuardValue<bool> StreamingRemoval(bRestoring, true);
    for (AActor* Actor : ActorRegistry->GetRegisteredActors())
    {
        const USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>();
        if (Marker && Marker->StreamingAreaId == AreaId && Marker->bRecreateOnLoad) Actor->Destroy();
    }
}
bool USaveSubsystem::RestoreStreamingArea(FName AreaId, FSaveAreaRestored Completion)
{
    if (bBusy || AreaId.IsNone() || !GetWorld()) return false;
    if (!VisitedAreaIds.Contains(AreaId)) { Completion.ExecuteIfBound(true, FText::GetEmpty()); return true; }
    PendingSaveGame = NewObject<USystemSaveGame>(this);
    PendingSaveGame->CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
    PendingSaveGame->DestroyedActors = DestroyedActors;
    for (const TPair<FGuid, FActorSaveData>& Entry : CachedAreaActors)
        if (Entry.Value.StreamingAreaId == AreaId && !DestroyedActors.Contains(Entry.Key)) PendingSaveGame->SavedActors.Add(Entry.Value);
    OperationWorld = GetWorld();
    RestoringAreaId = AreaId;
    AreaRestoreCompletion = MoveTemp(Completion);
    bBusy = bRestoringArea = true;
    ++LoadGeneration;
    BeginLoadDeadline();
    LoadResidentDependencies(LoadGeneration);
    return true;
}
