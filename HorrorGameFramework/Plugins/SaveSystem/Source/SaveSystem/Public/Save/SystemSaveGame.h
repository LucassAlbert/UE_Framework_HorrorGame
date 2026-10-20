#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/ActorSaveData.h"
#include "SystemSaveGame.generated.h"

UCLASS()
class SAVESYSTEM_API USystemSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY()
    FString CurrentLevel;
    UPROPERTY()
    TArray<FActorSaveData> SavedActors;
    UPROPERTY()
    TSet<FGuid> DestroyedActors;
    UPROPERTY()
    int32 FormatVersion = 2;
    UPROPERTY()
    FDateTime SavedAt;
};
