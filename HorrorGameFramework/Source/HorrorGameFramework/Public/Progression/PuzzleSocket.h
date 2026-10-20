#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "PuzzleSocket.generated.h"

class APuzzleDoor;
class UInventoryItemDefinition;
class UStaticMeshComponent;
class USaveComponent;
class UInteractableComponent;
struct FStreamableHandle;

UCLASS()
class HORRORGAMEFRAMEWORK_API APuzzleSocket : public AActor, public IInteractionInterface, public ISaveStateListener
{
    GENERATED_BODY()
public:
    APuzzleSocket();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle") TSoftObjectPtr<APuzzleDoor> Door;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FName SlotId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle") TObjectPtr<UStaticMeshComponent> Frame;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle") TObjectPtr<UStaticMeshComponent> InsertedMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> SaveComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction") TObjectPtr<UInteractableComponent> Interactable;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Puzzle") TSoftObjectPtr<UInventoryItemDefinition> InsertedItem;
    void SetInsertedItem(UInventoryItemDefinition* Item);
    FText GetSlotName() const;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void RefreshPresentation();
    UFUNCTION() void HandlePuzzleChanged(bool bSolved, bool bRestored);
    TWeakObjectPtr<APuzzleDoor> BoundDoor;
    TSharedPtr<FStreamableHandle> LoadHandle;
    uint64 PresentationGeneration = 0;
    UPROPERTY(Transient) TObjectPtr<UInventoryItemDefinition> LoadedItem;
};
