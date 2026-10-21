#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AreaResidencyComponent.generated.h"

// Moving actors may retain the route needed for their activity without coupling streaming to AI.
UCLASS(ClassGroup=(Horror), meta=(BlueprintSpawnableComponent))
class HORRORWORLD_API UAreaResidencyComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAreaResidencyComponent();
    virtual bool NeedsResidentRoute(FName& DestinationArea) const { DestinationArea = HomeAreaId; return bKeepResident; }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World") bool bKeepResident = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World") FName HomeAreaId;
};
