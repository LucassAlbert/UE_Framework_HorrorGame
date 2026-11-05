#include "Events/FrameworkEventActions.h"
#include "Events/FrameworkEventActor.h"
#include "HorrorEventComponent.h"
#include "HorrorSoundDefinition.h"
#include "HorrorAudioSubsystem.h"
#include "HorrorNoiseEmitterComponent.h"
#include "HorrorNoiseDefinition.h"
#include "HorrorEnemyCharacter.h"
#include "HorrorEnemyController.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "ObjectiveDefinition.h"
#include "Engine/GameInstance.h"

namespace
{
    UObjectiveJournalComponent* FindJournal(AActor* Actor)
    {
        return IsValid(Actor) ? Actor->FindComponentByClass<UObjectiveJournalComponent>() : nullptr;
    }
}

bool UFrameworkEventFactCondition::ValidateData(FText& Error) const
{
    if (RequiredFacts.Contains(NAME_None) || ForbiddenFacts.Contains(NAME_None))
    {
        Error = NSLOCTEXT("Events", "EmptyFact", "Os requisitos contêm um fato sem ID.");
        return false;
    }
    for (FName Fact : RequiredFacts)
        if (ForbiddenFacts.Contains(Fact))
        {
            Error = NSLOCTEXT("Events", "ConflictingFacts", "Um fato não pode ser exigido e proibido ao mesmo tempo.");
            return false;
        }
    return true;
}
bool UFrameworkEventFactCondition::Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const
{
    UObjectiveJournalComponent* Journal = FindJournal(Candidate);
    if (!Journal || !Journal->IsReady()) return false;
    for (FName Fact : RequiredFacts) if (!Journal->HasFact(Fact)) return false;
    for (FName Fact : ForbiddenFacts) if (Journal->HasFact(Fact)) return false;
    return true;
}
bool UFrameworkEventItemCondition::ValidateData(FText& Error) const
{
    if (!Item.IsNull() && Quantity > 0) return true;
    Error = NSLOCTEXT("Events", "ItemRequired", "Configure o item e a quantidade exigidos.");
    return false;
}
void UFrameworkEventItemCondition::GatherAssets(TArray<FSoftObjectPath>& Paths) const
{
    if (!Item.IsNull()) Paths.AddUnique(Item.ToSoftObjectPath());
}
bool UFrameworkEventItemCondition::Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const
{
    const UInventoryComponent* Inventory = IsValid(Candidate) ? Candidate->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) return false;
    int64 Count = 0;
    for (const FInventorySlot& Slot : Inventory->GetSlots()) if (Slot.Item == Item) Count += Slot.Quantity;
    return Count >= Quantity;
}
bool UFrameworkEventObjectiveCondition::ValidateData(FText& Error) const
{
    if (!Objective.IsNull() && ExpectedState <= EObjectiveState::Completed) return true;
    Error = NSLOCTEXT("Events", "ObjectiveRequired", "Configure o objetivo e um estado válido.");
    return false;
}
void UFrameworkEventObjectiveCondition::GatherAssets(TArray<FSoftObjectPath>& Paths) const
{
    if (!Objective.IsNull()) Paths.AddUnique(Objective.ToSoftObjectPath());
}
bool UFrameworkEventObjectiveCondition::Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const
{
    UObjectiveJournalComponent* Journal = FindJournal(Candidate);
    if (!Journal || !Journal->IsReady()) return false;
    for (const auto& Record : Journal->GetRecords()) if (Record.Definition == Objective) return Record.State == ExpectedState;
    return false;
}
bool UFrameworkEventSoundAction::ValidateData(FText& Error) const { return Sound && Sound->Validate(Error); }
void UFrameworkEventSoundAction::GatherAssets(TArray<FSoftObjectPath>& Paths) const
{
    if (!Sound) return;
    if (!Sound->Sound.IsNull()) Paths.AddUnique(Sound->Sound.ToSoftObjectPath());
    if (!Sound->Attenuation.IsNull()) Paths.AddUnique(Sound->Attenuation.ToSoftObjectPath());
}
bool UFrameworkEventSoundAction::CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    AActor* Source = Event ? (SourceId.IsNone() ? Event->GetOwner() : Event->GetTarget(SourceId)) : nullptr;
    auto* Host = Event ? Cast<AFrameworkEventActor>(Event->GetOwner()) : nullptr;
    if (Sound && IsValid(Source) && Host && Host->GetGameInstance()
        && (!bEmitHearingNoise || (Host->Noise->Definition && Host->Noise->Definition->Validate()))) return true;
    Error = NSLOCTEXT("Events", "AudioConfig", "A ação de áudio exige um emissor válido e, para audição da IA, uma definição de ruído.");
    return false;
}
bool UFrameworkEventSoundAction::Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    auto* Host = CastChecked<AFrameworkEventActor>(Event->GetOwner());
    AActor* Source = SourceId.IsNone() ? Host : Event->GetTarget(SourceId);
    auto* Audio = Host->GetGameInstance()->GetSubsystem<UHorrorAudioSubsystem>();
    const FGuid Handle = Audio->PlaySound(Host, Sound, Source->GetActorLocation(), Source->GetRootComponent());
    if (!Handle.IsValid())
    {
        Error = NSLOCTEXT("Events", "AudioRejected", "O gerenciador de áudio recusou o som do evento.");
        return false;
    }
    Host->TrackPlayback(Handle);
    if (bEmitHearingNoise) Host->Noise->ReportNoise(Source->GetActorLocation(), Host);
    return true;
}
bool UFrameworkEventFactAction::ValidateData(FText& Error) const
{
    if (!Fact.IsNone()) return true;
    Error = NSLOCTEXT("Events", "FactRequired", "Configure o fato registrado pela ação.");
    return false;
}
bool UFrameworkEventFactAction::CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    if (Event && FindJournal(Event->GetEventInstigator())) return true;
    Error = NSLOCTEXT("Events", "JournalMissing", "O jogador não possui um registro de objetivos.");
    return false;
}
bool UFrameworkEventFactAction::Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    UObjectiveJournalComponent* Journal = FindJournal(Event->GetEventInstigator());
    return Journal && (Journal->HasFact(Fact) || Journal->RecordFact(Fact));
}
bool UFrameworkEventFactAction::RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    // Restore dependencies are loaded before callbacks, but the journal may still be refreshing its ready flag.
    return Execute_Implementation(Event, Error);
}
bool UFrameworkEventEnemyAction::ValidateData(FText& Error) const
{
    if (!TargetId.IsNone()) return true;
    Error = NSLOCTEXT("Events", "EnemyRequired", "Configure o alvo do inimigo.");
    return false;
}
bool UFrameworkEventEnemyAction::CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    auto* Enemy = Event ? Cast<AHorrorEnemyCharacter>(Event->GetTarget(TargetId)) : nullptr;
    FString Detail;
    if (Enemy && Enemy->Definition && Enemy->Definition->Validate(Detail) && Enemy->Memory.Validate()
        && Cast<AHorrorEnemyController>(Enemy->GetController()) && Enemy->Memory.State != EHorrorEnemyState::Captured) return true;
    Error = NSLOCTEXT("Events", "EnemyMissing", "O inimigo configurado não está disponível para ativação.");
    return false;
}
bool UFrameworkEventEnemyAction::Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    auto* Enemy = CastChecked<AHorrorEnemyCharacter>(Event->GetTarget(TargetId));
    // Enabling an already active enemy preserves its ongoing investigation or pursuit.
    if (bEnemyEnabled && Enemy->Memory.State != EHorrorEnemyState::Disabled) return true;
    Enemy->Memory.State = bEnemyEnabled ? EHorrorEnemyState::Patrol : EHorrorEnemyState::Disabled;
    if (auto* AI = Cast<AHorrorEnemyController>(Enemy->GetController())) AI->StopMovement();
    Enemy->ResumeAfterRestore();
    return true;
}
bool UFrameworkEventEnemyAction::RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    return CanExecute(Event, Error) && Execute_Implementation(Event, Error);
}
