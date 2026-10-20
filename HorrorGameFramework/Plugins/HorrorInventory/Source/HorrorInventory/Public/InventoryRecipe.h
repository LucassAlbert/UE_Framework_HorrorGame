#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InventoryRecipe.generated.h"

class UInventoryItemDefinition;

// A combination consumes one unit of each ingredient, regardless of selection order.
UCLASS(BlueprintType)
class HORRORINVENTORY_API UInventoryRecipe : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
    TSoftObjectPtr<UInventoryItemDefinition> FirstIngredient;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
    TSoftObjectPtr<UInventoryItemDefinition> SecondIngredient;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
    TSoftObjectPtr<UInventoryItemDefinition> ResultItem;
};
