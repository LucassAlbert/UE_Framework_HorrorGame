#include "HorrorWorldSubsystem.h"
#include "HorrorWorldDefinition.h"
#include "AreaResidencyComponent.h"
#include "WorldResidentSpawnPoint.h"
#include "Subsystems/SaveSubsystem.h"
#include "Components/SaveComponent.h"
#include "Save/SystemSaveGame.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorWorld, Log, All);

bool UHorrorWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld();
}
USaveSubsystem* UHorrorWorldSubsystem::GetSaves() const
{ return GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr; }
bool UHorrorWorldSubsystem::Configure(UHorrorWorldDefinition* InDefinition, FString& Error)
{
    if (Definition || !InDefinition || !InDefinition->Validate(Error) || !GetSaves())
    { if (Error.IsEmpty()) Error = TEXT("A streaming world can only be configured once."); return false; }
    Definition = InDefinition;
    for (const FHorrorWorldArea& Area : Definition->Areas)
    {
        const FString Package = Area.Level.ToSoftObjectPath().GetLongPackageName();
        ULevelStreamingDynamic* Streaming = NewObject<ULevelStreamingDynamic>(GetWorld());
        Streaming->SetWorldAssetByPackageName(FName(*Package));
        Streaming->SetShouldBeLoaded(false);
        Streaming->SetShouldBeVisible(false);
        Streaming->bShouldBlockOnLoad = false;
        GetWorld()->AddStreamingLevel(Streaming);
        FHorrorAreaRuntime Runtime;
        Runtime.Streaming = Streaming;
        Areas.Add(Area.Id, Runtime);
        Streaming->OnLevelShown.AddUniqueDynamic(this, &UHorrorWorldSubsystem::HandleShown);
        Streaming->OnLevelUnloaded.AddUniqueDynamic(this, &UHorrorWorldSubsystem::HandleUnloaded);
    }
    if (!GetSaves()->RegisterWorldProvider(this)) { Error = TEXT("The save world already has a different provider."); return false; }
    GetSaves()->OnLoadCompleted.AddUniqueDynamic(this, &UHorrorWorldSubsystem::HandleFileLoaded);
    LastUpdate = FPlatformTime::Seconds();
    UpdateTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float Delta)
    { UpdateStreaming(Delta); return !bEnding; }), Definition->UpdateInterval);
    UpdateStreaming();
    return true;
}
void UHorrorWorldSubsystem::Deinitialize()
{
    bEnding = true;
    if (UpdateTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(UpdateTicker);
    UpdateTicker.Reset();
    RestoreCompletion.Unbind();
    if (USaveSubsystem* Saves = GetSaves())
    { Saves->OnLoadCompleted.RemoveDynamic(this, &UHorrorWorldSubsystem::HandleFileLoaded); Saves->UnregisterWorldProvider(this); }
    for (auto& Entry : Areas)
        if (Entry.Value.Streaming)
        {
            Entry.Value.Streaming->OnLevelShown.RemoveDynamic(this, &UHorrorWorldSubsystem::HandleShown);
            Entry.Value.Streaming->OnLevelUnloaded.RemoveDynamic(this, &UHorrorWorldSubsystem::HandleUnloaded);
        }
    Super::Deinitialize();
}
bool UHorrorWorldSubsystem::IsAreaReady(FName AreaId) const
{ const FHorrorAreaRuntime* Area = Areas.Find(AreaId); return Area && Area->State == EHorrorAreaState::Ready; }
bool UHorrorWorldSubsystem::IsSaveAreaResident(FName AreaId) const
{
    const FHorrorAreaRuntime* Area = Areas.Find(AreaId);
    return Area && (Area->State == EHorrorAreaState::Ready || ((bPreparingRestore || bApplyingFile) && Area->State == EHorrorAreaState::Restoring));
}
bool UHorrorWorldSubsystem::IsSaveWorldReady() const
{ return bInitialReady && !bPreparingRestore && !bApplyingFile && OperatingAreaId.IsNone(); }
bool UHorrorWorldSubsystem::CaptureWorldMetadata(USystemSaveGame& Snapshot, FString& Error) const
{
    if (!IsSaveWorldReady()) { Error = TEXT("Wait until the area's state is ready before saving."); return false; }
    Snapshot.StreamingWorldId = Definition->WorldId;
    Snapshot.ActiveAreaId = ActiveAreaId;
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    Snapshot.StreamingAnchor = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
    for (const auto& Entry : Areas) if (IsAreaReady(Entry.Key)) Snapshot.ResidentAreas.Add(Entry.Key);
    return true;
}
void UHorrorWorldSubsystem::SetAreaState(FName AreaId, EHorrorAreaState State)
{
    FHorrorAreaRuntime* Area = Areas.Find(AreaId);
    if (!Area || Area->State == State) return;
    Area->State = State;
    OnAreaChanged.Broadcast(AreaId, State);
    UE_LOG(LogHorrorWorld, Log, TEXT("Area %s: %s"), *AreaId.ToString(), *StaticEnum<EHorrorAreaState>()->GetNameStringByValue(static_cast<int64>(State)));
}
FName UHorrorWorldSubsystem::FindActorArea(const AActor* Actor) const
{
    if (!Actor) return NAME_None;
    for (const auto& Entry : Areas)
        if (Entry.Value.Streaming && Entry.Value.Streaming->GetLoadedLevel() == Actor->GetLevel()) return Entry.Key;
    return Definition ? Definition->FindContainingArea(Actor->GetActorLocation()) : NAME_None;
}
void UHorrorWorldSubsystem::RefreshActorBindings()
{
    if (!Definition || !GetSaves()) return;
    for (AActor* Actor : GetSaves()->ActorRegistry->GetRegisteredActors())
        if (Actor->GetWorld() == GetWorld())
            if (USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>())
                if (Marker->bRecreateOnLoad || Actor->GetLevel() != GetWorld()->PersistentLevel) Marker->StreamingAreaId = FindActorArea(Actor);
}
void UHorrorWorldSubsystem::UpdateStreaming(float DeltaTime)
{
    if (!Definition || bEnding || !GetSaves()) return;
    const double Now = FPlatformTime::Seconds();
    LastUpdate = Now;
    RefreshActorBindings();
    if (!OperatingAreaId.IsNone() && Now > OperationDeadline)
    { FinishArea(OperatingAreaId, false, NSLOCTEXT("HorrorWorld", "Timeout", "Não foi possível preparar esta área a tempo.")); return; }
    if (bResettingAreas) DesiredAreas.Reset();
    else if (bPreparingRestore)
    {
        DesiredAreas = SnapshotResidentAreas;
        DesiredAreas.Append(Definition->SelectAreas(RestoreAnchor, {}));
    }
    else if (!bApplyingFile)
    {
        const APlayerController* PC = GetWorld()->GetFirstPlayerController();
        const APawn* Player = PC ? PC->GetPawn() : nullptr;
        if (!Player) return;
        TSet<FName> Resident;
        for (const auto& Entry : Areas) if (IsAreaReady(Entry.Key)) Resident.Add(Entry.Key);
        DesiredAreas = Definition->SelectAreas(Player->GetActorLocation(), Resident);
        const FName At = Definition->FindContainingArea(Player->GetActorLocation());
        if (!At.IsNone()) ActiveAreaId = At;
        for (AActor* Actor : GetSaves()->ActorRegistry->GetRegisteredActors())
        {
            const UAreaResidencyComponent* Residency = Actor->FindComponentByClass<UAreaResidencyComponent>();
            FName Destination;
            if (!Residency || !Residency->NeedsResidentRoute(Destination)) continue;
            FName Source = FindActorArea(Actor);
            if (Source.IsNone()) Source = Residency->HomeAreaId;
            if (!Source.IsNone()) DesiredAreas.Add(Source);
            if (!Destination.IsNone()) Definition->AddRoute(Source, Destination, DesiredAreas);
            if (!ActiveAreaId.IsNone()) Definition->AddRoute(Source, ActiveAreaId, DesiredAreas);
        }
    }
    PumpOperations();
    CheckReady();
}
void UHorrorWorldSubsystem::PumpOperations()
{
    if (!OperatingAreaId.IsNone())
    {
        const FHorrorAreaRuntime* Active = Areas.Find(OperatingAreaId);
        if (Active && Active->State == EHorrorAreaState::Restoring && !bPreparingRestore && !GetSaves()->IsBusy()) RestoreShownArea();
        return;
    }
    for (FName Id : DesiredAreas)
        if (FHorrorAreaRuntime* Area = Areas.Find(Id); Area && Area->State == EHorrorAreaState::Unloaded) { BeginLoad(Id); return; }
    for (const auto& Entry : Areas)
        if (!DesiredAreas.Contains(Entry.Key) && (Entry.Value.State == EHorrorAreaState::Ready || (bPreparingRestore && Entry.Value.State == EHorrorAreaState::Restoring)))
        { BeginUnload(Entry.Key); return; }
}
void UHorrorWorldSubsystem::BeginLoad(FName AreaId)
{
    FHorrorAreaRuntime& Area = Areas[AreaId];
    OperatingAreaId = AreaId;
    OperationDeadline = FPlatformTime::Seconds() + Definition->OperationTimeout;
    SetAreaState(AreaId, EHorrorAreaState::Loading);
    Area.Streaming->SetShouldBeLoaded(true);
    Area.Streaming->SetShouldBeVisible(true);
}
void UHorrorWorldSubsystem::HandleShown()
{
    FHorrorAreaRuntime* Area = Areas.Find(OperatingAreaId);
    if (!Area || Area->State != EHorrorAreaState::Loading || !Area->Streaming->IsLevelVisible()) return;
    RefreshActorBindings();
    SetAreaState(OperatingAreaId, EHorrorAreaState::Restoring);
    if (bPreparingRestore) { OperatingAreaId = NAME_None; CheckReady(); }
    else RestoreShownArea();
}
void UHorrorWorldSubsystem::RestoreShownArea()
{
    if (GetSaves()->IsBusy()) return;
    const FName Id = OperatingAreaId;
    GetSaves()->RestoreStreamingArea(Id, FSaveAreaRestored::CreateWeakLambda(this,
        [this, Id](bool bSuccess, FText Message) { FinishArea(Id, bSuccess, Message); }));
}
void UHorrorWorldSubsystem::BeginUnload(FName AreaId)
{
    if (GetSaves()->IsBusy() && !bPreparingRestore) return;
    FString Error;
    RefreshActorBindings();
    if (!bPreparingRestore && !GetSaves()->CacheStreamingArea(AreaId, Error))
    { UE_LOG(LogHorrorWorld, Error, TEXT("Area %s remains resident: %s"), *AreaId.ToString(), *Error); SetAreaState(AreaId, EHorrorAreaState::Failed); return; }
    GetSaves()->EvictStreamingRuntimeActors(AreaId);
    OperatingAreaId = AreaId;
    OperationDeadline = FPlatformTime::Seconds() + Definition->OperationTimeout;
    SetAreaState(AreaId, EHorrorAreaState::Unloading);
    Areas[AreaId].Streaming->SetShouldBeVisible(false);
    Areas[AreaId].Streaming->SetShouldBeLoaded(false);
}
void UHorrorWorldSubsystem::HandleUnloaded()
{
    const FHorrorAreaRuntime* Area = Areas.Find(OperatingAreaId);
    if (!Area || Area->State != EHorrorAreaState::Unloading || Area->Streaming->GetLoadedLevel()) return;
    SetAreaState(OperatingAreaId, EHorrorAreaState::Unloaded);
    OperatingAreaId = NAME_None;
    PumpOperations();
    CheckReady();
}
void UHorrorWorldSubsystem::FinishArea(FName AreaId, bool bSuccess, FText Message)
{
    if (bEnding || AreaId != OperatingAreaId) return;
    if (bSuccess)
    {
        ULevel* Level = Areas[AreaId].Streaming->GetLoadedLevel();
        if (Level)
            for (AActor* Actor : Level->Actors)
                if (AWorldResidentSpawnPoint* Spawn = Cast<AWorldResidentSpawnPoint>(Actor))
                {
                    FString Error;
                    if (!Spawn->EnsureSpawned(Error)) { bSuccess = false; Message = FText::FromString(Error); break; }
                }
    }
    SetAreaState(AreaId, bSuccess ? EHorrorAreaState::Ready : EHorrorAreaState::Failed);
    OperatingAreaId = NAME_None;
    if (!bSuccess)
    {
        UE_LOG(LogHorrorWorld, Error, TEXT("Area %s failed: %s"), *AreaId.ToString(), *Message.ToString());
        if (bPreparingRestore) { bPreparingRestore = false; RestoreCompletion.ExecuteIfBound(false, Message); RestoreCompletion.Unbind(); }
    }
    CheckReady();
}
void UHorrorWorldSubsystem::CheckReady()
{
    if (!OperatingAreaId.IsNone() || bEnding) return;
    for (FName Id : DesiredAreas)
    {
        const FHorrorAreaRuntime* Area = Areas.Find(Id);
        if (!Area || (Area->State != EHorrorAreaState::Ready && !(bPreparingRestore && Area->State == EHorrorAreaState::Restoring))) return;
    }
    if (bResettingAreas)
    {
        // Rewind starts from authored defaults, including areas never visited in this snapshot.
        for (const auto& Entry : Areas) if (Entry.Value.State != EHorrorAreaState::Unloaded) return;
        bResettingAreas = false;
        UpdateStreaming();
        return;
    }
    if (bPreparingRestore)
    {
        for (const auto& Entry : Areas)
            if (!DesiredAreas.Contains(Entry.Key) && Entry.Value.State != EHorrorAreaState::Unloaded) return;
        bPreparingRestore = false;
        bApplyingFile = true;
        FSaveWorldPrepared Completion = MoveTemp(RestoreCompletion);
        Completion.ExecuteIfBound(true, FText::GetEmpty());
    }
    else if (!bInitialReady && !bApplyingFile && !DesiredAreas.IsEmpty())
    {
        bInitialReady = true;
        GetSaves()->NotifyWorldReady();
    }
}
void UHorrorWorldSubsystem::PrepareSaveWorld(const USystemSaveGame& Snapshot, FSaveWorldPrepared Completion)
{
    if (!Definition || Snapshot.StreamingWorldId != Definition->WorldId || Snapshot.StreamingAnchor.ContainsNaN())
    { Completion.ExecuteIfBound(false, NSLOCTEXT("HorrorWorld", "WrongWorld", "A definição deste mundo não é compatível com o save.")); return; }
    for (FName Id : Snapshot.VisitedAreas)
        if (!Areas.Contains(Id)) { Completion.ExecuteIfBound(false, NSLOCTEXT("HorrorWorld", "MissingArea", "Uma área deste save não existe mais.")); return; }
    for (const FActorSaveData& Record : Snapshot.SavedActors)
        if (!Record.StreamingAreaId.IsNone() && !Areas.Contains(Record.StreamingAreaId))
        { Completion.ExecuteIfBound(false, NSLOCTEXT("HorrorWorld", "UnknownActorArea", "Um participante do save aponta para uma área inexistente.")); return; }
    SnapshotResidentAreas.Reset();
    for (FName Id : Snapshot.ResidentAreas)
        if (Areas.Contains(Id)) SnapshotResidentAreas.Add(Id);
        else { Completion.ExecuteIfBound(false, NSLOCTEXT("HorrorWorld", "BadResidentArea", "O save exige uma área inexistente.")); return; }
    ActiveAreaId = Snapshot.ActiveAreaId;
    RestoreAnchor = Snapshot.StreamingAnchor;
    RestoreCompletion = MoveTemp(Completion);
    bResettingAreas = true;
    bPreparingRestore = true;
    bApplyingFile = false;
    UpdateStreaming();
}
void UHorrorWorldSubsystem::HandleFileLoaded(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (!bApplyingFile && !bPreparingRestore) return;
    bResettingAreas = bPreparingRestore = bApplyingFile = false;
    RestoreCompletion.Unbind();
    if (!bSuccess) return;
    for (auto& Entry : Areas)
        if (Entry.Value.State == EHorrorAreaState::Restoring)
        { OperatingAreaId = Entry.Key; FinishArea(Entry.Key, true, FText::GetEmpty()); }
    bInitialReady = true;
    GetSaves()->NotifyWorldReady();
}
