#include "HorrorEventDefinition.h"
#include "HorrorEventComponent.h"
#include "GameFramework/Actor.h"

bool UHorrorEventCondition::Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const
{
    Error = NSLOCTEXT("Events", "ConditionMissing", "A condição precisa implementar Evaluate.");
    return false;
}
bool UHorrorEventAction::CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const { return true; }
bool UHorrorEventAction::Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    Error = NSLOCTEXT("Events", "ActionMissing", "A ação precisa implementar Execute.");
    return false;
}
bool UHorrorEventAction::RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const { return true; }
bool UHorrorEventStateAction::ValidateData(FText& Error) const
{
    if (!TargetId.IsNone() && !State.IsNone()) return true;
    Error = NSLOCTEXT("Events", "StateConfig", "Configure o alvo e o estado da ação.");
    return false;
}
bool UHorrorEventStateAction::CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    AActor* Target = Event ? Event->GetTarget(TargetId) : nullptr;
    if (IsValid(Target) && Target->Implements<UHorrorEventTarget>())
        return IHorrorEventTarget::Execute_CanApplyEventState(Target, State, Error);
    Error = FText::Format(NSLOCTEXT("Events", "TargetMissing", "O alvo {0} não está disponível ou não implementa HorrorEventTarget."), FText::FromName(TargetId));
    return false;
}
bool UHorrorEventStateAction::Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    if (!CanExecute(Event, Error)) return false;
    return IHorrorEventTarget::Execute_ApplyEventState(Event->GetTarget(TargetId), State, false);
}
bool UHorrorEventStateAction::RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const
{
    if (!CanExecute(Event, Error)) return false;
    return IHorrorEventTarget::Execute_ApplyEventState(Event->GetTarget(TargetId), State, true);
}
bool UHorrorEventDefinition::Validate(FText& Error) const
{
    if (EventId.IsNone() || Revision < 1 || Steps.IsEmpty() || Steps.Num() > 256 || !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0)
    {
        Error = NSLOCTEXT("Events", "DefinitionInvalid", "Configure um ID, até 256 ações e um intervalo válido.");
        return false;
    }
    for (const auto& Condition : Conditions)
    {
        if (!Condition)
        {
            Error = NSLOCTEXT("Events", "MissingCondition", "A lista de requisitos contém uma condição vazia.");
            return false;
        }
        if (!Condition->ValidateData(Error)) return false;
    }
    for (const auto& Step : Steps)
        if (!FMath::IsFinite(Step.DelayBefore) || Step.DelayBefore < 0 || !Step.Action || !Step.Action->ValidateData(Error))
        {
            if (Error.IsEmpty()) Error = NSLOCTEXT("Events", "StepInvalid", "A sequência contém uma ação ou duração inválida.");
            return false;
        }
    return true;
}
void UHorrorEventDefinition::GatherAssets(TArray<FSoftObjectPath>& Paths) const
{
    for (const auto& Condition : Conditions) if (Condition) Condition->GatherAssets(Paths);
    for (const auto& Step : Steps) if (Step.Action) Step.Action->GatherAssets(Paths);
}
