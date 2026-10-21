#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldResidentSpawnPoint.generated.h"
class USaveComponent;

UCLASS()
class HORRORWORLD_API AWorldResidentSpawnPoint : public AActor
{
    GENERATED_BODY()
public:
    AWorldResidentSpawnPoint();
    bool EnsureSpawned(FString& Error);
    UPROPERTY(EditAnywhere, Category="World") TSubclassOf<AActor> ResidentClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World") TObjectPtr<USaveComponent> Save;
private:
    UPROPERTY(SaveGame) bool bSpawned = false;
};
