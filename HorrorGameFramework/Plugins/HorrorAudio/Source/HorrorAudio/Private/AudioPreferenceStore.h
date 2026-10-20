#pragma once
#include "CoreMinimal.h"
#include "HorrorAudioTypes.h"

// Small, versioned user preferences; independent from world snapshots and progress slots.
namespace HorrorAudioPreferences
{
    bool Load(const FString& Path, FHorrorAudioVolumes& OutVolumes, FString& Error);
    bool Save(const FString& Path, const FHorrorAudioVolumes& Volumes, FString& Error);
}
