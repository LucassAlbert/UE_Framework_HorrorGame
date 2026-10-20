#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObjectiveJournalComponent.generated.h"

class UObjectiveDefinition;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EObjectiveState : uint8 { Inactive, Active, Completed };

USTRUCT(BlueprintType)
struct HORROROBJECTIVES_API FObjectiveRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, SaveGame) TSoftObjectPtr<UObjectiveDefinition> Definition;
    UPROPERTY(BlueprintReadOnly, SaveGame) EObjectiveState State = EObjectiveState::Inactive;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveChanged, UObjectiveDefinition*, Definition, EObjectiveState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectiveJournalChanged);

UCLASS(ClassGroup = (Objectives), meta = (BlueprintSpawnableComponent))
class HORROROBJECTIVES_API UObjectiveJournalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UObjectiveJournalComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Objectives")
    TArray<TSoftObjectPtr<UObjectiveDefinition>> Definitions;
    UPROPERTY(BlueprintAssignable, Category = "Objectives") FOnObjectiveChanged OnObjectiveChanged;
    UPROPERTY(BlueprintAssignable, Category = "Objectives") FOnObjectiveJournalChanged OnJournalChanged;
    UFUNCTION(BlueprintCallable, Category = "Objectives") bool RecordFact(FName Fact);
    UFUNCTION(BlueprintPure, Category = "Objectives") bool HasFact(FName Fact) const { return Facts.Contains(Fact); }
    UFUNCTION(BlueprintPure, Category = "Objectives") TArray<FObjectiveRecord> GetRecords() const { return Records; }
    UFUNCTION(BlueprintPure, Category = "Objectives") bool IsReady() const { return bReady; }
    bool ValidateState(FText& Error) const;
    bool NotifyStateRestored(FText& Error);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void FinishLoading();
    void Refresh(bool bNotify);
    EObjectiveState ComputeState(const UObjectiveDefinition* Definition) const;
    UPROPERTY(SaveGame) TArray<FName> Facts;
    UPROPERTY(SaveGame) TArray<FObjectiveRecord> Records;
    UPROPERTY(Transient) TArray<TObjectPtr<UObjectiveDefinition>> LoadedDefinitions;
    bool bReady = false;
    TSharedPtr<FStreamableHandle> LoadHandle;
};
