#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.generated.h"

class UInventoryItemDefinition;

USTRUCT(BlueprintType)
struct HORRORINVENTORY_API FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
    TSoftObjectPtr<UInventoryItemDefinition> Item;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
    int32 Quantity = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
    FGuid StackId;

    bool IsEmpty() const { return Item.IsNull() && Quantity == 0; }
};

USTRUCT(BlueprintType)
struct HORRORINVENTORY_API FInventoryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Inventory")
    bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly, Category = "Inventory")
    FText Message;

    static FInventoryResult Success()
    {
        FInventoryResult Result;
        Result.bSuccess = true;
        return Result;
    }
    static FInventoryResult Failure(const FText& Reason)
    {
        FInventoryResult Result;
        Result.Message = Reason;
        return Result;
    }
};
