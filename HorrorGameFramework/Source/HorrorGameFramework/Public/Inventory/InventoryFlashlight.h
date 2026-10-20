#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryFlashlight.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API AInventoryFlashlight : public AActor
{
    GENERATED_BODY()
public:
    AInventoryFlashlight();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight") TObjectPtr<USpotLightComponent> Light;
};
