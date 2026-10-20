#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InventoryItemDefinition.generated.h"

class UStaticMesh;
class UTexture2D;
class UInventoryRecipe;
class AActor;

// Shared configuration; mutable quantities and equipment state belong to the component.
UCLASS(BlueprintType)
class HORRORINVENTORY_API UInventoryItemDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
    FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
    int32 MaxStackSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.01"))
    float WorldScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FRotator InspectionRotation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bCanUse = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bCanEquip = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bCanInspect = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bCanDrop = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bCanDiscard = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
    bool bConsumeOnUse = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
    TSoftClassPtr<AActor> EquipmentActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
    FTransform EquipmentTransform = FTransform(FRotator::ZeroRotator, FVector(40.f, -18.f, -15.f));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
    TArray<TSoftObjectPtr<UInventoryRecipe>> Recipes;
};
