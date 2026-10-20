#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

class AActor;

UCLASS( ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent) )
class HORRORINTERACTION_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

public:	
	UInteractableComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    int32 InteractionPriority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionDistance = 200.f;
		
};
