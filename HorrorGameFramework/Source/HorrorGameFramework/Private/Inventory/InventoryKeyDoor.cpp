#include "Inventory/InventoryKeyDoor.h"
#include "InventoryItemDefinition.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AInventoryKeyDoor::AInventoryKeyDoor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    Mesh->SetupAttachment(GetRootComponent());
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}
void AInventoryKeyDoor::BeginPlay()
{
    Super::BeginPlay();
    ClosedRotation = Mesh->GetRelativeRotation();
    ApplyPresentation();
}
bool AInventoryKeyDoor::CanApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User) const
{
    return bLocked && IsValid(User) && IsValid(Item) && !RequiredKey.IsNone() && Item->ItemId == RequiredKey;
}
bool AInventoryKeyDoor::ApplyInventoryItem_Implementation(UInventoryItemDefinition* Item, AActor* User)
{
    if (!CanApplyInventoryItem_Implementation(Item, User)) return false;
    bLocked = false;
    return true;
}
bool AInventoryKeyDoor::CanInteract_Implementation() const { return !bLocked; }
void AInventoryKeyDoor::Interact_Implementation(AActor* Interactor)
{
    if (!IsValid(Interactor) || bLocked) return;
    bOpen = !bOpen;
    ApplyPresentation();
}
FText AInventoryKeyDoor::GetInteractionText_Implementation() const
{
    return bOpen ? NSLOCTEXT("Inventory", "CloseDoor", "Fechar porta") : NSLOCTEXT("Inventory", "OpenDoor", "Abrir porta");
}
void AInventoryKeyDoor::ApplyPresentation()
{
    Mesh->SetRelativeRotation(ClosedRotation + FRotator(0.f, bOpen ? 90.f : 0.f, 0.f));
}
void AInventoryKeyDoor::OnActorLoaded_Implementation() { ApplyPresentation(); }
