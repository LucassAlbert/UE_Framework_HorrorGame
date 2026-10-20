#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundAttenuation.h"
#include "HorrorAudioTypes.h"
#include "HorrorSoundDefinition.generated.h"

class USoundBase;
class USoundAttenuation;

UCLASS(BlueprintType)
class HORRORAUDIO_API UHorrorSoundDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio") TSoftObjectPtr<USoundBase> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio") EHorrorAudioChannel Channel = EHorrorAudioChannel::Effects;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio") EHorrorAudioPausePolicy PausePolicy = EHorrorAudioPausePolicy::PauseWithWorld;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "4")) float Volume = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.125", ClampMax = "4")) float Pitch = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "10")) float FadeInSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "10")) float FadeOutSeconds = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Spatial") bool bSpatialized = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Spatial", meta = (EditCondition = "bSpatialized")) bool bUseAttenuationAsset = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Spatial", meta = (EditCondition = "bSpatialized && bUseAttenuationAsset")) TSoftObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Spatial", meta = (EditCondition = "bSpatialized && !bUseAttenuationAsset")) FSoundAttenuationSettings SpatialSettings;
    // Looping is authored in the SoundWave/SoundCue; the manager never mutates shared sound assets.
    bool Validate(FText& Error) const;
};
