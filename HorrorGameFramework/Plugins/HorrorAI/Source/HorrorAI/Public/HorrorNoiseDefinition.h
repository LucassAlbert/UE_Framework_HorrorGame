#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HorrorNoiseDefinition.generated.h"

// Gameplay hearing is independent of audio output and user volume preferences.
UCLASS(BlueprintType)
class HORRORAI_API UHorrorNoiseDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise") FName NoiseTag = TEXT("Noise");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0", ClampMax = "1")) float Loudness = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "1")) float Range = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0")) float CooldownSeconds = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0")) float MinimumImpactImpulse = 100.f;
    bool Validate() const;
};
