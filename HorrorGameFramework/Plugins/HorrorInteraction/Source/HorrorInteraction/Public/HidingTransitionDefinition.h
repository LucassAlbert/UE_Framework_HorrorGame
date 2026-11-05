#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HidingTransitionDefinition.generated.h"

class UCurveFloat;

UCLASS(BlueprintType)
class HORRORINTERACTION_API UHidingTransitionDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0")) float EnterSeconds = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0")) float ExitSeconds = .9f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.01", ClampMax = "0.45")) float DoorPhaseFraction = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition") FRotator DoorOpenRotation = FRotator(0, 95, 0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition") TObjectPtr<UCurveFloat> MovementCurve;
    bool Validate() const;
    float GetMotionAlpha(float Progress) const;
    float GetDoorAlpha(float Progress) const;
};
