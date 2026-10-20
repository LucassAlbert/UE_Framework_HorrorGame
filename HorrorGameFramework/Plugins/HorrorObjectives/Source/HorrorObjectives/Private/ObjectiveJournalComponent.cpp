#include "ObjectiveJournalComponent.h"
#include "ObjectiveDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorObjectives, Log, All);
UObjectiveJournalComponent::UObjectiveJournalComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UObjectiveJournalComponent::BeginPlay()
{
    Super::BeginPlay();
    TArray<FSoftObjectPath> Paths;
    for (const auto& Definition : Definitions)
        if (!Definition.IsNull() && !Definition.Get()) Paths.AddUnique(Definition.ToSoftObjectPath());
    if (Paths.IsEmpty()) FinishLoading();
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateUObject(this, &UObjectiveJournalComponent::FinishLoading));
}

EObjectiveState UObjectiveJournalComponent::ComputeState(const UObjectiveDefinition* Definition) const
{
    for (FName Fact : Definition->ActivationFacts) if (!Facts.Contains(Fact)) return EObjectiveState::Inactive;
    for (FName Fact : Definition->CompletionFacts) if (!Facts.Contains(Fact)) return EObjectiveState::Active;
    return EObjectiveState::Completed;
}

void UObjectiveJournalComponent::FinishLoading()
{
    LoadedDefinitions.Reset();
    TSet<FName> IDs;
    for (const auto& Reference : Definitions)
    {
        UObjectiveDefinition* Definition = Reference.Get();
        FText Error;
        if (!Definition || !Definition->Validate(Error) || IDs.Contains(Definition->ObjectiveId))
        {
            UE_LOG(LogHorrorObjectives, Error, TEXT("Invalid or duplicate objective definition on %s: %s"),
                *GetNameSafe(GetOwner()), *Reference.ToString());
            return;
        }
        IDs.Add(Definition->ObjectiveId);
        LoadedDefinitions.Add(Definition);
    }
    if (Records.IsEmpty())
        for (const auto& Definition : Definitions) { FObjectiveRecord Record; Record.Definition = Definition; Records.Add(Record); }
    bReady = true;
    Refresh(true);
}

void UObjectiveJournalComponent::Refresh(bool bNotify)
{
    TArray<TPair<UObjectiveDefinition*, EObjectiveState>> Changes;
    for (FObjectiveRecord& Record : Records)
    {
        UObjectiveDefinition* Definition = Record.Definition.Get();
        if (!Definition) continue;
        const EObjectiveState State = ComputeState(Definition);
        if (Record.State == State) continue;
        Record.State = State;
        Changes.Emplace(Definition, State);
    }
    // Every observer sees the complete journal, including objectives activated by the same fact.
    if (bNotify)
        for (const auto& Change : Changes) OnObjectiveChanged.Broadcast(Change.Key, Change.Value);
    OnJournalChanged.Broadcast();
}

bool UObjectiveJournalComponent::RecordFact(FName Fact)
{
    FText Error;
    if (Fact.IsNone() || Facts.Contains(Fact) || (bReady && !ValidateState(Error))) return false;
    Facts.Add(Fact);
    // Gameplay can produce facts while definitions are still loading; none are lost.
    if (bReady) Refresh(true);
    return true;
}

bool UObjectiveJournalComponent::ValidateState(FText& Error) const
{
    if (Records.Num() != Definitions.Num())
    {
        Error = NSLOCTEXT("Objectives", "NotReady", "As definições de objetivos ainda não estão prontas.");
        return false;
    }
    TSet<FName> FactIDs;
    for (FName Fact : Facts)
    {
        if (Fact.IsNone() || FactIDs.Contains(Fact))
        {
            Error = NSLOCTEXT("Objectives", "InvalidFacts", "O registro de progresso contém dados inválidos.");
            return false;
        }
        FactIDs.Add(Fact);
    }
    TSet<FName> ObjectiveIDs;
    for (const FObjectiveRecord& Record : Records)
    {
        UObjectiveDefinition* Definition = Record.Definition.Get();
        if (!Definition || !Definitions.Contains(Record.Definition) || !Definition->Validate(Error)
            || ObjectiveIDs.Contains(Definition->ObjectiveId) || Record.State != ComputeState(Definition))
        {
            if (Error.IsEmpty()) Error = NSLOCTEXT("Objectives", "InvalidState", "O estado dos objetivos não corresponde às suas definições.");
            return false;
        }
        ObjectiveIDs.Add(Definition->ObjectiveId);
    }
    return true;
}

bool UObjectiveJournalComponent::NotifyStateRestored(FText& Error)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    if (!ValidateState(Error)) return false;
    LoadedDefinitions.Reset();
    for (const auto& Reference : Definitions) LoadedDefinitions.Add(Reference.Get());
    bReady = true;
    // Restoring a snapshot refreshes UI, but does not replay completion notices/rewards.
    OnJournalChanged.Broadcast();
    return true;
}

void UObjectiveJournalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    OnObjectiveChanged.Clear();
    OnJournalChanged.Clear();
    LoadedDefinitions.Reset();
    bReady = false;
    Super::EndPlay(Reason);
}
