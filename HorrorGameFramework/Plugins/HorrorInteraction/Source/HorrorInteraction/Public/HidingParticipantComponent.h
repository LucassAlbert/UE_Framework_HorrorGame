#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "HidingParticipantComponent.generated.h"

class UHideSpotComponent;
class APlayerController;
class UHidingTransitionDefinition;

UENUM(BlueprintType)
enum class EHidingState : uint8 { Outside, Entering, Hidden, Exiting };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHidingChanged, bool, bHidden, UHideSpotComponent*, Spot, FVector, EntryLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHidingStateChanged, EHidingState, State, UHideSpotComponent*, Spot);

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORRORINTERACTION_API UHidingParticipantComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHidingParticipantComponent();
    UFUNCTION(BlueprintCallable, Category = "Hiding") bool Enter(UHideSpotComponent* Spot);
    UFUNCTION(BlueprintCallable, Category = "Hiding") bool Exit();
    // Occupancy includes entry/exit. Concealment starts only at Hidden.
    UFUNCTION(BlueprintPure, Category = "Hiding") bool IsHiding() const { return CurrentSpot.IsValid(); }
    UFUNCTION(BlueprintPure, Category = "Hiding") bool IsFullyHidden() const { return State == EHidingState::Hidden; }
    UFUNCTION(BlueprintPure, Category = "Hiding") bool IsTransitioning() const { return State == EHidingState::Entering || State == EHidingState::Exiting; }
    UFUNCTION(BlueprintPure, Category = "Hiding") EHidingState GetState() const { return State; }
    UHideSpotComponent* GetSpot() const { return CurrentSpot.Get(); }
    const FTransform& GetReturnTransform() const { return ReturnTransform; }
    FRotator GetReturnLook() const { return ReturnLook; }
    void SetReturnTransform(const FTransform& Transform) { ReturnTransform = Transform; }
    void SetReturnLook(FRotator Look) { ReturnLook = Look; }
    bool IsReturnLocationClear() const;
    bool Restore(UHideSpotComponent* Spot, const FTransform& SafeReturn);
    void ResetHiding();
    void Look(FVector2D Input);
    UPROPERTY(BlueprintAssignable, Category = "Hiding") FHidingChanged OnHidingChanged;
    UPROPERTY(BlueprintAssignable, Category = "Hiding") FHidingStateChanged OnStateChanged;
protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool EnterInternal(UHideSpotComponent* Spot, bool bRestoring);
    bool Release(bool bReturnToEntry);
    void ChangeState(EHidingState NewState, UHideSpotComponent* Spot);
    void BeginTransition(EHidingState NewState, float Duration);
    bool MoveTransition(float Alpha);
    bool IsPathClear(FVector From, FVector To) const;
    void FinishTransition();
    UFUNCTION() void HandleSpotEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    TWeakObjectPtr<UHideSpotComponent> CurrentSpot;
    TWeakObjectPtr<APlayerController> TransitionController;
    UPROPERTY(Transient) TObjectPtr<UHidingTransitionDefinition> ActiveDefinition;
    FTransform ReturnTransform;
    FRotator ReturnLook;
    FTransform TransitionFrom;
    FTransform TransitionTo;
    FQuat LookFrom;
    FQuat LookTo;
    float Elapsed = 0;
    float TransitionSeconds = 0;
    ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::QueryAndPhysics;
    uint8 PreviousMovement = 0;
    uint8 PreviousCustomMovement = 0;
    EHidingState State = EHidingState::Outside;
    bool bHidingApplied = false;
};
