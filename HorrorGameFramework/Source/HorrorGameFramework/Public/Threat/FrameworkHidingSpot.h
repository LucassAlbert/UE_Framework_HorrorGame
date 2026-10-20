#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "FrameworkHidingSpot.generated.h"

class UHideSpotComponent;
class UStaticMeshComponent;
class USaveComponent;
class UInteractableComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkHidingSpot : public AActor, public IInteractionInterface
{
    GENERATED_BODY()
public:
    AFrameworkHidingSpot();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding") TObjectPtr<UHideSpotComponent> HideSpot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding") TObjectPtr<USaveComponent> Save;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding") TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
};
