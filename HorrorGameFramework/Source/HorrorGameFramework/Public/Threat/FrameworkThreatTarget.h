#pragma once
#include "CoreMinimal.h"
#include "HorrorAITargetComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "HidingParticipantComponent.h"
#include "FrameworkThreatTarget.generated.h"

class UHideSpotComponent;
class UHorrorNoiseDefinition;
class UHorrorNoiseEmitterComponent;

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UFrameworkThreatTarget : public UHorrorAITargetComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UFrameworkThreatTarget();
    virtual void Serialize(FArchive& Ar) override;
    virtual bool IsAvailable() const override;
    virtual bool CanBeCaptured() const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise") TObjectPtr<UHorrorNoiseDefinition> FootstepNoise;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "1")) float StepDistance = 160.f;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleHidingChanged(bool bHidden, UHideSpotComponent* Spot, FVector EntryLocation);
    UFUNCTION() void HandleHidingStateChanged(EHidingState State, UHideSpotComponent* Spot);
    void CacheHidingSnapshot();
    void UpdateFootsteps();
    UPROPERTY(SaveGame) uint8 HidingSnapshotVersion = 0;
    UPROPERTY(SaveGame) bool bExitInProgress = false;
    UPROPERTY(SaveGame) FRotator SavedReturnLook = FRotator::ZeroRotator;
    UPROPERTY(SaveGame) FGuid HiddenSpotId;
    UPROPERTY(SaveGame) FTransform HiddenReturnTransform;
    UPROPERTY(Transient) TObjectPtr<UHorrorNoiseEmitterComponent> Noise;
    FVector PreviousLocation = FVector::ZeroVector;
    float DistanceSinceStep = 0.f;
    bool bRestoring = false;
    FTimerHandle StepTimer;
};
