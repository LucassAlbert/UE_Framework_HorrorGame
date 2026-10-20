#pragma once

#include "Inventory/InventoryWorldItem.h"
#include "InventoryDroppedItem.generated.h"

UCLASS()
class HORRORGAMEFRAMEWORK_API AInventoryDroppedItem : public AInventoryWorldItem
{
    GENERATED_BODY()
public:
    AInventoryDroppedItem();
protected:
    virtual void OnPresentationReady() override;
};
