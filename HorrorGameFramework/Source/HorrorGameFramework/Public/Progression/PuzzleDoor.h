#pragma once

#include "Inventory/InventoryKeyDoor.h"
#include "Interfaces/SaveStateListener.h"
#include "PuzzleDoor.generated.h"

class UItemPuzzleComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API APuzzleDoor : public AInventoryKeyDoor, public ISaveStateListener
{
    GENERATED_BODY()
public:
    APuzzleDoor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle") TObjectPtr<UItemPuzzleComponent> Puzzle;
    virtual bool CanApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User) const override { return false; }
    virtual bool ApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User) override { return false; }
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandlePuzzleChanged(bool bSolved, bool bRestored);
};
