#include "Inventory/InventoryDroppedItem.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"

AInventoryDroppedItem::AInventoryDroppedItem()
{
    SaveComponent->bRecreateOnLoad = true;
    SaveComponent->bSavePhysics = true;
    // Small pickups can cross thin floors between physics steps at low frame rates.
    Mesh->BodyInstance.bUseCCD = true;
}

void AInventoryDroppedItem::OnPresentationReady()
{
    Mesh->SetSimulatePhysics(true);
    Mesh->SetEnableGravity(true);
}
