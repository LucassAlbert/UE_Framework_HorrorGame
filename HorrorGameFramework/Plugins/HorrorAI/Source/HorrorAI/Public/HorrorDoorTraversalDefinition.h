#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HorrorDoorTraversalDefinition.generated.h"

UCLASS(BlueprintType)
class HORRORAI_API UHorrorDoorTraversalDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta=(ClampMin="0")) float OpenDelaySeconds = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta=(ClampMin="1")) float InteractionDistance = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta=(ClampMin="0.1")) float CrossingTimeoutSeconds = 5.f;
    bool Validate() const;
};
