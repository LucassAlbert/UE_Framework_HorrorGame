#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "HidingParticipantComponent.generated.h"

class UHideSpotComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHidingChanged, bool, bHidden, UHideSpotComponent*, Spot, FVector, EntryLocation);

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORRORINTERACTION_API UHidingParticipantComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHidingParticipantComponent();
    UFUNCTION(BlueprintCallable, Category = "Hiding") bool Enter(UHideSpotComponent* Spot);
    UFUNCTION(BlueprintCallable, Category = "Hiding") bool Exit();
    UFUNCTION(BlueprintPure, Category = "Hiding") bool IsHiding() const { return CurrentSpot.IsValid(); }
    UHideSpotComponent* GetSpot() const { return CurrentSpot.Get(); }
    const FTransform& GetReturnTransform() const { return ReturnTransform; }
    void SetReturnTransform(const FTransform& Transform) { ReturnTransform = Transform; }
    bool Restore(UHideSpotComponent* Spot, const FTransform& SafeReturn);
    void ResetHiding();
    void Look(FVector2D Input);
    UPROPERTY(BlueprintAssignable, Category = "Hiding") FHidingChanged OnHidingChanged;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool EnterInternal(UHideSpotComponent* Spot, bool bRestoring);
    bool Release(bool bReturnToEntry);
    UFUNCTION() void HandleSpotEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    TWeakObjectPtr<UHideSpotComponent> CurrentSpot;
    FTransform ReturnTransform;
    FRotator ReturnLook;
    ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::QueryAndPhysics;
    uint8 PreviousMovement = 0;
    uint8 PreviousCustomMovement = 0;
    bool bHidingApplied = false;
};
