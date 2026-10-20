#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "InventoryTypes.h"
#include "InventoryGameplayComponent.generated.h"

class UInventoryComponent;
class UInventoryItemDefinition;
class UInventoryRecipe;
class UUIScreenBase;
class AInventoryWorldItem;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryFeedback, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryInspectRequested, UInventoryItemDefinition*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryObjectivesRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryDocumentsRequested);

// Project adapter: the inventory plugin itself knows nothing about this project's UI, saves or world.
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UInventoryGameplayComponent : public UActorComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UInventoryGameplayComponent();
    UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = "1"))
    float UseDistance = 250.f;
    UPROPERTY(EditAnywhere, Category = "Inventory")
    TEnumAsByte<ECollisionChannel> TargetChannel = ECC_GameTraceChannel1;
    UPROPERTY(EditAnywhere, Category = "Inventory")
    TSubclassOf<AInventoryWorldItem> DroppedItemClass;
    UPROPERTY(EditAnywhere, Category = "Inventory")
    TSubclassOf<UUIScreenBase> InventoryScreenClass;
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryFeedback OnFeedback;
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryInspectRequested OnInspectRequested;
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryObjectivesRequested OnObjectivesRequested;
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryDocumentsRequested OnDocumentsRequested;
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RequestDocuments();
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RequestObjectives();
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void OpenInventory();
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult UseSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult EquipSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult DropSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult CombineSlots(int32 FirstSlot, int32 SecondSlot);
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FInventoryResult InspectSlot(int32 SlotIndex);
    UInventoryComponent* GetInventory() const;
    void ReportResult(const FInventoryResult& Result);
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION()
    void HandleInventoryChanged();
    void UpdateEquipment();
    AActor* SpawnEquipment(UInventoryItemDefinition* Item);
    FGuid EquippedPresentationId;
    void CloseInventoryScreen();
    UPROPERTY(Transient)
    TObjectPtr<AActor> EquipmentActor;
    TWeakObjectPtr<UUIScreenBase> InventoryScreen;
    TSharedPtr<FStreamableHandle> EquipmentLoadHandle;
    TSharedPtr<FStreamableHandle> RecipeLoadHandle;
    TSharedPtr<FStreamableHandle> DropLoadHandle;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> LoadedRecipeAssets;
};
