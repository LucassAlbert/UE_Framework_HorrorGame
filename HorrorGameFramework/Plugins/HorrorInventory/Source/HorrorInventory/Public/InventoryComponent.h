#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "InventoryComponent.generated.h"

class UInventoryItemDefinition;
class UInventoryRecipe;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HORRORINVENTORY_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInventoryComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
    int32 SlotCapacity = 5;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    TArray<FInventorySlot> GetSlots() const { return Slots; }
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetEquippedSlot() const;
    UFUNCTION(BlueprintPure, Category = "Inventory")
    UInventoryItemDefinition* GetItemAt(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult AddItem(UInventoryItemDefinition* Item, int32 Quantity = 1);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult RemoveFromSlot(int32 SlotIndex, int32 Quantity = 1);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult EquipSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult Unequip();
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult CombineSlots(int32 FirstSlot, int32 SecondSlot, UInventoryRecipe* Recipe);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult DiscardSlot(int32 SlotIndex);

    // The integration layer loads soft definitions, then calls this after save restoration.
    FInventoryResult NotifyStateRestored();
    FInventoryResult ValidateState() const;
    FInventoryResult CanAddItem(UInventoryItemDefinition* Item, int32 Quantity) const;
    const FInventorySlot* FindSlot(int32 SlotIndex) const;

protected:
    virtual void OnRegister() override;

private:
    static FInventoryResult PlanAdd(TArray<FInventorySlot>& Candidate, UInventoryItemDefinition* Item, int32 Quantity);
    static void RemoveUnits(TArray<FInventorySlot>& Candidate, int32 SlotIndex, int32 Quantity);
    void Commit(TArray<FInventorySlot>&& Candidate);
    void KeepDefinitionsAlive();

    UPROPERTY(SaveGame)
    TArray<FInventorySlot> Slots;
    UPROPERTY(SaveGame)
    FGuid EquippedStackId;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UInventoryItemDefinition>> LoadedDefinitions;
};
