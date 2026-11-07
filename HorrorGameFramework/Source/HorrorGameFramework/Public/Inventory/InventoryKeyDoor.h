#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryItemTarget.h"
#include "HorrorAIDoor.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveInterface.h"
#include "InventoryKeyDoor.generated.h"

class UStaticMeshComponent;
class USaveComponent;
class UInteractableComponent;
class UHorrorDoorNavigationComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API AInventoryKeyDoor : public AActor, public IInventoryItemTarget,
    public IInteractionInterface, public ISaveInterface, public IHorrorAIDoor
{
    GENERATED_BODY()
public:
    AInventoryKeyDoor();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door") FName RequiredKey;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Door") bool bLocked = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Door") bool bOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> SaveComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction") TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User) const override;
    virtual bool ApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User) override;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void OnActorLoaded_Implementation() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Door|AI") TObjectPtr<UHorrorDoorNavigationComponent> AINavigation;
    virtual bool IsAIDoorOpen_Implementation() const override { return bOpen; }
    virtual bool IsAIDoorLocked_Implementation() const override { return bLocked; }
    virtual bool CanAIOpenDoor_Implementation(APawn* Agent) const override;
    virtual bool OpenDoorForAI_Implementation(APawn* Agent) override;
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void BeginPlay() override;
    void ApplyPresentation();
private:
    FRotator ClosedRotation;
};
