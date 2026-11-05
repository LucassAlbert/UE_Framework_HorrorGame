#include "HorrorEventComponent.h"
#include "HorrorEventDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Serialization/Archive.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorEvents, Log, All);

UHorrorEventComponent::UHorrorEventComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UHorrorEventComponent::BeginPlay()
{
    Super::BeginPlay();
    FText Error;
    if (!Definition || !Definition->Validate(Error))
    {
        UE_LOG(LogHorrorEvents, Error, TEXT("Invalid event on %s: %s"), *GetNameSafe(GetOwner()), *Error.ToString());
        return;
    }
    if (Progress.EventId.IsNone())
    {
        Progress.EventId = Definition->EventId;
        Progress.Revision = Definition->Revision;
    }
    TArray<FSoftObjectPath> Paths;
    GatherAssets(Paths);
    Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.ResolveObject() != nullptr; });
    if (Paths.IsEmpty()) FinishLoading();
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateUObject(this, &UHorrorEventComponent::FinishLoading));
}
void UHorrorEventComponent::GatherAssets(TArray<FSoftObjectPath>& Paths) const
{
    if (Definition) Definition->GatherAssets(Paths);
}
void UHorrorEventComponent::FinishLoading()
{
    TArray<FSoftObjectPath> Paths;
    GatherAssets(Paths);
    LoadedAssets.Reset();
    for (const auto& Path : Paths)
    {
        UObject* Asset = Path.ResolveObject();
        if (!Asset)
        {
            Fail(FText::Format(NSLOCTEXT("Events", "AssetMissing", "Não foi possível carregar {0}."), FText::FromString(Path.ToString())));
            return;
        }
        LoadedAssets.Add(Asset);
    }
    bReady = true;
    OnReady.Broadcast();
}
AActor* UHorrorEventComponent::GetTarget(FName Id) const
{
    const auto* Reference = Targets.Find(Id);
    return Reference ? Reference->Get() : nullptr;
}
bool UHorrorEventComponent::CanStart(AActor* InstigatorActor, FText& Error)
{
    if (!bEnabled || !bReady || !IsValid(InstigatorActor) || bDispatching || !Definition || !GetWorld() || GetWorld()->IsPaused()
        || Progress.State == EHorrorEventState::Running || Progress.State == EHorrorEventState::Failed
        || (Definition->bOnceOnly && Progress.CompletedCount > 0) || GetWorld()->GetTimerManager().IsTimerActive(CooldownTimer))
    {
        Error = NSLOCTEXT("Events", "Unavailable", "O evento está indisponível, em execução ou já foi consumido.");
        return false;
    }
    if (!ValidateState(Error)) return false;
    TGuardValue<bool> Guard(bDispatching, true);
    TGuardValue<TWeakObjectPtr<AActor>> ContextGuard(EventInstigator, TWeakObjectPtr<AActor>(InstigatorActor));
    for (const auto& Condition : Definition->Conditions)
        if (!Condition->Evaluate(this, InstigatorActor, Error)) return false;
    return ValidateActions(InstigatorActor, Error);
}
bool UHorrorEventComponent::ValidateActions(AActor* InstigatorActor, FText& Error)
{
    if (!Definition || !Definition->Validate(Error)) return false;
    TGuardValue<bool> Guard(bDispatching, true);
    TGuardValue<TWeakObjectPtr<AActor>> ContextGuard(EventInstigator, TWeakObjectPtr<AActor>(InstigatorActor));
    for (const auto& Step : Definition->Steps)
        if (!Step.Action->CanExecute(this, Error)) return false;
    return true;
}
bool UHorrorEventComponent::TryStart(AActor* InstigatorActor, FText& Error)
{
    if (!CanStart(InstigatorActor, Error)) return false;
    TGuardValue<bool> Guard(bDispatching, true);
    EventInstigator = InstigatorActor;
    Progress.State = EHorrorEventState::Running;
    Progress.NextStep = 0;
    Progress.CooldownRemaining = 0;
    OnStarted.Broadcast();
    if (Progress.State == EHorrorEventState::Running && bEnabled) ScheduleStep();
    return true;
}
void UHorrorEventComponent::ScheduleStep()
{
    const float Delay = Definition->Steps[Progress.NextStep].DelayBefore;
    auto& Timers = GetWorld()->GetTimerManager();
    if (Delay == 0) StepTimer = Timers.SetTimerForNextTick(this, &UHorrorEventComponent::ExecuteStep);
    else Timers.SetTimer(StepTimer, this, &UHorrorEventComponent::ExecuteStep, Delay, false);
}
void UHorrorEventComponent::ExecuteStep()
{
    if (Progress.State != EHorrorEventState::Running || !bEnabled) return;
    TGuardValue<bool> Guard(bDispatching, true);
    FText Error;
    UHorrorEventAction* Action = Definition->Steps[Progress.NextStep].Action;
    if (!IsValid(EventInstigator.Get()) || !Action->CanExecute(this, Error) || !Action->Execute(this, Error))
    {
        if (Error.IsEmpty()) Error = NSLOCTEXT("Events", "ExecutionFailed", "A execução do evento perdeu um alvo ou falhou.");
        Fail(Error);
        return;
    }
    // Actions can close their owning area through Blueprint callbacks.
    if (!IsValid(this) || !bReady || Progress.State != EHorrorEventState::Running || !bEnabled) return;
    ++Progress.NextStep;
    if (Progress.NextStep == Definition->Steps.Num()) Finish(false);
    else ScheduleStep();
}
void UHorrorEventComponent::BeginCooldown(float Seconds)
{
    Progress.CooldownRemaining = Seconds;
    if (Seconds > 0) GetWorld()->GetTimerManager().SetTimer(CooldownTimer, this, &UHorrorEventComponent::CooldownEnded, Seconds, false);
}
void UHorrorEventComponent::CooldownEnded() { Progress.CooldownRemaining = 0; }
void UHorrorEventComponent::Finish(bool bRestored)
{
    Progress.State = EHorrorEventState::Completed;
    Progress.NextStep = Definition->Steps.Num();
    ++Progress.CompletedCount;
    BeginCooldown(Definition->CooldownSeconds);
    OnFinished.Broadcast(bRestored);
}
void UHorrorEventComponent::StopWork()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StepTimer);
        GetWorld()->GetTimerManager().ClearTimer(CooldownTimer);
    }
    OnInterrupted.Broadcast();
}
void UHorrorEventComponent::Fail(const FText& Error)
{
    StopWork();
    Progress.State = EHorrorEventState::Failed;
    Progress.CooldownRemaining = 0;
    UE_LOG(LogHorrorEvents, Error, TEXT("Event %s on %s failed: %s"), *GetNameSafe(Definition), *GetNameSafe(GetOwner()), *Error.ToString());
    OnFailed.Broadcast(Error);
}
void UHorrorEventComponent::SetEventsEnabled(bool bNewEnabled)
{
    bEnabled = bNewEnabled;
    if (!bEnabled)
    {
        StopWork();
        if (Progress.State == EHorrorEventState::Running)
        {
            Progress.State = EHorrorEventState::Idle;
            Progress.NextStep = 0;
        }
        Progress.CooldownRemaining = 0;
    }
}
bool UHorrorEventComponent::ValidateState(FText& Error) const
{
    if (!Definition || !Definition->Validate(Error)) return false;
    const bool bCompleted = Progress.State == EHorrorEventState::Completed;
    if (Progress.EventId != Definition->EventId || Progress.Revision != Definition->Revision || Progress.CompletedCount < 0
        || (Definition->bOnceOnly && Progress.CompletedCount > 1)
        || !FMath::IsFinite(Progress.CooldownRemaining) || Progress.CooldownRemaining < 0
        || Progress.State > EHorrorEventState::Failed || Progress.NextStep < 0 || Progress.NextStep > Definition->Steps.Num()
        || (bCompleted && (Progress.CompletedCount == 0 || Progress.NextStep != Definition->Steps.Num()))
        || (!bCompleted && Progress.NextStep == Definition->Steps.Num())
        || (Progress.State == EHorrorEventState::Idle && Progress.NextStep != 0))
    {
        Error = NSLOCTEXT("Events", "SnapshotInvalid", "O estado salvo do evento não corresponde à sua definição.");
        return false;
    }
    return true;
}
bool UHorrorEventComponent::NotifyStateRestored(AActor* InstigatorActor, FText& Error)
{
    StopWork();
    if (!ValidateState(Error)) return false;
    EventInstigator = InstigatorActor;
    if (Progress.State == EHorrorEventState::Running)
    {
        TGuardValue<bool> Guard(bDispatching, true);
        for (const auto& Step : Definition->Steps)
            if (!Step.Action->RestoreFinalState(this, Error)) { Fail(Error); return false; }
        Finish(true);
    }
    else if (Progress.State == EHorrorEventState::Completed) BeginCooldown(Progress.CooldownRemaining);
    return true;
}
void UHorrorEventComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsSaveGame() && Ar.IsSaving() && GetWorld())
        Progress.CooldownRemaining = FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(CooldownTimer));
    Super::Serialize(Ar);
}
void UHorrorEventComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopWork();
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedAssets.Reset();
    bReady = false;
    Super::EndPlay(Reason);
}
