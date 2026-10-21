#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "FrameworkSavePoint.generated.h"
class UStaticMeshComponent;
class UInteractableComponent;
UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkSavePoint : public AActor, public IInteractionInterface
{
    GENERATED_BODY()
public:
    AFrameworkSavePoint();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
};
