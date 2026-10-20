#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InventoryItemTarget.generated.h"

class UInventoryItemDefinition;
class AActor;

UINTERFACE(BlueprintType)
class HORRORINVENTORY_API UInventoryItemTarget : public UInterface
{
    GENERATED_BODY()
};

class HORRORINVENTORY_API IInventoryItemTarget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
    bool CanApplyInventoryItem(UInventoryItemDefinition* Item, AActor* User) const;

    // True only after the target has applied the effect; inventory consumption follows success.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
    bool ApplyInventoryItem(UInventoryItemDefinition* Item, AActor* User);
};
