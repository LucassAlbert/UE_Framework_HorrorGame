#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InspectionPreviewActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UDirectionalLightComponent;

// Exists only in a private preview world, never in the gameplay map or actor registry.
UCLASS(Transient, NotBlueprintable)
class AInspectionPreviewActor : public AActor
{
    GENERATED_BODY()
public:
    AInspectionPreviewActor();
    UPROPERTY() TObjectPtr<USceneComponent> Pivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> KeyLight;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> FillLight;
};
