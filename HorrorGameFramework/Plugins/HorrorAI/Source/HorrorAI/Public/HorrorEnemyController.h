#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "HorrorEnemyDefinition.h"
#include "HorrorEnemyController.generated.h"

class AHorrorEnemyCharacter;
class UHorrorAITargetComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UHorrorDoorNavigationComponent;
class UPathFollowingComponent;

UCLASS()
class HORRORAI_API AHorrorEnemyController : public AAIController
{
    GENERATED_BODY()
public:
    AHorrorEnemyController();
    void ResumeAfterRestore();
    bool CanUseDoor(const UHorrorDoorNavigationComponent* Link) const;
    bool BeginDoorTraversal(UHorrorDoorNavigationComponent* Link, UPathFollowingComponent* Path, FVector ExitPoint);
    void NotifyDoorTraversalFinished(UHorrorDoorNavigationComponent* Link);
    void CancelDoorInteraction();
    UFUNCTION(BlueprintPure, Category="AI|Door") bool IsWaitingForDoor() const;
    UFUNCTION(BlueprintPure, Category="AI|Door") AActor* GetCurrentDoor() const;
protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FHorrorEnemyBehaviorTest;
    friend class FHorrorEnemyDoorTest;
    enum class EDoorUseState : uint8 { None, Waiting, Crossing };
    bool UpdateDoorTraversal(float Elapsed);
    void FinishDoorTraversal(bool bAbortMove);
    void UpdateBrain();
    void ChangeEnemyState(EHorrorEnemyState State);
    void SetTarget(AActor* Actor);
    void RememberConcealment(FVector EntryLocation);
    void RequestDestination(FVector Goal);
    void BeginSearch();
    void ChooseSearchGoal();
    void AdvancePatrol();
    bool HasSightAt(AActor* Actor, FVector Location) const;
    bool CaptureIfClose(UHorrorAITargetComponent* Target, FVector CaptureLocation, AActor* Anchor);
    void Shutdown();
    UFUNCTION() void HandlePerception(AActor* Actor, FAIStimulus Stimulus);
    UFUNCTION() void HandleConcealmentStarted(AActor* Actor, FVector EntryLocation);
    UFUNCTION() void HandleConcealment(AActor* Actor, bool bConcealed, FVector EntryLocation);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAIPerceptionComponent> Senses;
    UPROPERTY() TObjectPtr<UAISenseConfig_Sight> Sight;
    UPROPERTY() TObjectPtr<UAISenseConfig_Hearing> Hearing;
    TWeakObjectPtr<AHorrorEnemyCharacter> Enemy;
    TWeakObjectPtr<UHorrorAITargetComponent> TrackedTarget;
    TWeakObjectPtr<AActor> WitnessedAnchor;
    FVector WitnessedEntry = FVector::ZeroVector;
    FVector MoveGoal = FVector::ZeroVector;
    FVector SearchOrigin = FVector::ZeroVector;
    TWeakObjectPtr<UHorrorDoorNavigationComponent> DoorLink;
    TWeakObjectPtr<UPathFollowingComponent> DoorPath;
    FVector DoorExit = FVector::ZeroVector;
    EDoorUseState DoorUseState = EDoorUseState::None;
    float DoorRemaining = 0.f;
    double LastUpdateTime = 0.0;
    bool bMoveRequested = false;
    bool bReady = false;
    bool bReportedNavigationFailure = false;
    FTimerHandle BrainTimer;
};
