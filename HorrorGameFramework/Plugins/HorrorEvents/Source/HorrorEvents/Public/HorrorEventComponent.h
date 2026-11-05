#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HorrorEventComponent.generated.h"

class AActor;
class UHorrorEventDefinition;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EHorrorEventState : uint8 { Idle, Running, Completed, Failed };

USTRUCT(BlueprintType)
struct HORROREVENTS_API FHorrorEventProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FName EventId;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Revision = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) EHorrorEventState State = EHorrorEventState::Idle;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 NextStep = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 CompletedCount = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) float CooldownRemaining = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHorrorEventSignal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHorrorEventFailure, FText, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHorrorEventFinished, bool, bRestored);

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORROREVENTS_API UHorrorEventComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHorrorEventComponent();
    virtual void Serialize(FArchive& Ar) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events") TObjectPtr<UHorrorEventDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events") TMap<FName, TSoftObjectPtr<AActor>> Targets;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events") bool bEnabled = true;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Events") FHorrorEventProgress Progress;
    UPROPERTY(BlueprintAssignable, Category = "Events") FHorrorEventSignal OnReady;
    UPROPERTY(BlueprintAssignable, Category = "Events") FHorrorEventSignal OnStarted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FHorrorEventSignal OnInterrupted;
    UPROPERTY(BlueprintAssignable, Category = "Events") FHorrorEventFinished OnFinished;
    UPROPERTY(BlueprintAssignable, Category = "Events") FHorrorEventFailure OnFailed;
    UFUNCTION(BlueprintPure, Category = "Events") bool IsReady() const { return bReady; }
    UFUNCTION(BlueprintPure, Category = "Events") AActor* GetEventInstigator() const { return EventInstigator.Get(); }
    UFUNCTION(BlueprintPure, Category = "Events") AActor* GetTarget(FName Id) const;
    UFUNCTION(BlueprintCallable, Category = "Events") bool TryStart(AActor* InstigatorActor, FText& Error);
    UFUNCTION(BlueprintCallable, Category = "Events") void SetEventsEnabled(bool bNewEnabled);
    bool CanStart(AActor* InstigatorActor, FText& Error);
    bool ValidateState(FText& Error) const;
    bool ValidateActions(AActor* InstigatorActor, FText& Error);
    bool NotifyStateRestored(AActor* InstigatorActor, FText& Error);
    void GatherAssets(TArray<FSoftObjectPath>& Paths) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void FinishLoading();
    void ScheduleStep();
    void ExecuteStep();
    void Finish(bool bRestored);
    void Fail(const FText& Error);
    void StopWork();
    void BeginCooldown(float Seconds);
    void CooldownEnded();
    UPROPERTY(Transient) TWeakObjectPtr<AActor> EventInstigator;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> LoadedAssets;
    TSharedPtr<FStreamableHandle> LoadHandle;
    FTimerHandle StepTimer;
    FTimerHandle CooldownTimer;
    bool bReady = false;
    bool bDispatching = false;
};
