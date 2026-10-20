#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "InventoryWorldItem.generated.h"

class UInventoryItemDefinition;
class UStaticMeshComponent;
class USaveComponent;
class UInteractableComponent;
struct FStreamableHandle;

UCLASS()
class HORRORGAMEFRAMEWORK_API AInventoryWorldItem : public AActor, public IInteractionInterface, public ISaveStateListener
{
    GENERATED_BODY()
public:
    AInventoryWorldItem();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item")
    TSoftObjectPtr<UInventoryItemDefinition> Item;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Item", meta = (ClampMin = "1"))
    int32 Quantity = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TObjectPtr<USaveComponent> SaveComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
    void RefreshPresentation();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPresentationReady() {}
private:
    UPROPERTY(SaveGame)
    bool bCollected = false;
    UPROPERTY(Transient)
    TObjectPtr<UInventoryItemDefinition> LoadedItem;
    TSharedPtr<FStreamableHandle> LoadHandle;
};
