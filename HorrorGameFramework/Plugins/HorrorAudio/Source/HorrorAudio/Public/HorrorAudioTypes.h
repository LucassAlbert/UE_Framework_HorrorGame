#pragma once

#include "CoreMinimal.h"
#include "HorrorAudioTypes.generated.h"

UENUM(BlueprintType)
enum class EHorrorAudioChannel : uint8 { Master, Ambience, Effects, Interface, Music };

UENUM(BlueprintType)
enum class EHorrorAudioPausePolicy : uint8 { PauseWithWorld, ContinueWhilePaused };

USTRUCT(BlueprintType)
struct HORRORAUDIO_API FHorrorAudioVolumes
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Master = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Ambience = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Effects = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Interface = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Music = 1.f;
    bool Validate() const;
    float Get(EHorrorAudioChannel Channel) const;
    bool Set(EHorrorAudioChannel Channel, float Value);
    bool Equals(const FHorrorAudioVolumes& Other) const;
    float GetPlaybackGain(EHorrorAudioChannel Channel) const;
};
