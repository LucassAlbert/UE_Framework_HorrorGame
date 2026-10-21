#pragma once
#include "CoreMinimal.h"
#include "Threat/FrameworkEnemy.h"
#include "AreaResidencyComponent.h"
#include "StreamingEnemy.generated.h"
UCLASS()
class HORRORGAMEFRAMEWORK_API UEnemyAreaResidencyComponent : public UAreaResidencyComponent
{
    GENERATED_BODY()
public:
    virtual bool NeedsResidentRoute(FName& DestinationArea) const override;
};
UCLASS()
class HORRORGAMEFRAMEWORK_API AStreamingEnemy : public AFrameworkEnemy
{
    GENERATED_BODY()
public:
    AStreamingEnemy();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UEnemyAreaResidencyComponent> Residency;
};
