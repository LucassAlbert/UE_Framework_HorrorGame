#pragma once

#include "CoreMinimal.h"
#include "HorrorEventDefinition.h"
#include "ObjectiveJournalComponent.h"
#include "FrameworkEventActions.generated.h"

class UHorrorSoundDefinition;
class UInventoryItemDefinition;
class UObjectiveDefinition;

UCLASS(DisplayName = "Require progression facts")
class HORRORGAMEFRAMEWORK_API UFrameworkEventFactCondition : public UHorrorEventCondition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TArray<FName> RequiredFacts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TArray<FName> ForbiddenFacts;
    virtual bool ValidateData(FText& Error) const override;
    virtual bool Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const override;
};

UCLASS(DisplayName = "Require inventory item")
class HORRORGAMEFRAMEWORK_API UFrameworkEventItemCondition : public UHorrorEventCondition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TSoftObjectPtr<UInventoryItemDefinition> Item;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "1")) int32 Quantity = 1;
    virtual bool ValidateData(FText& Error) const override;
    virtual void GatherAssets(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const override;
};

UCLASS(DisplayName = "Require objective state")
class HORRORGAMEFRAMEWORK_API UFrameworkEventObjectiveCondition : public UHorrorEventCondition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TSoftObjectPtr<UObjectiveDefinition> Objective;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") EObjectiveState ExpectedState = EObjectiveState::Completed;
    virtual bool ValidateData(FText& Error) const override;
    virtual void GatherAssets(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool Evaluate_Implementation(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const override;
};

UCLASS(DisplayName = "Play managed sound")
class HORRORGAMEFRAMEWORK_API UFrameworkEventSoundAction : public UHorrorEventAction
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UHorrorSoundDefinition> Sound;
    // None plays at the event actor. Named sources are resolved through its level bindings.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName SourceId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") bool bEmitHearingNoise = false;
    virtual bool ValidateData(FText& Error) const override;
    virtual void GatherAssets(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
};

UCLASS(DisplayName = "Record progression fact")
class HORRORGAMEFRAMEWORK_API UFrameworkEventFactAction : public UHorrorEventAction
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName Fact;
    virtual bool ValidateData(FText& Error) const override;
    virtual bool CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
};

UCLASS(DisplayName = "Enable or disable enemy")
class HORRORGAMEFRAMEWORK_API UFrameworkEventEnemyAction : public UHorrorEventAction
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName TargetId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") bool bEnemyEnabled = true;
    virtual bool ValidateData(FText& Error) const override;
    virtual bool CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
};
