#include "Inventory/InventoryKeyDoor.h"
#include "InventoryItemDefinition.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HorrorDoorNavigationComponent.h"
#include "HorrorDoorTraversalDefinition.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

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
    AINavigation = CreateDefaultSubobject<UHorrorDoorNavigationComponent>(TEXT("AINavigation"));
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
    AINavigation->RefreshDoorNavigation();
    return true;
}
bool AInventoryKeyDoor::CanInteract_Implementation() const { return !bLocked && !(bOpen && AINavigation->HasMovingAgents()); }
void AInventoryKeyDoor::Interact_Implementation(AActor* Interactor)
{
    if (!IsValid(Interactor) || !CanInteract_Implementation()) return;
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
    AINavigation->RefreshDoorNavigation();
}
void AInventoryKeyDoor::OnActorLoaded_Implementation() { ApplyPresentation(); }

void AInventoryKeyDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    AINavigation->RefreshDoorNavigation();
}
bool AInventoryKeyDoor::CanAIOpenDoor_Implementation(APawn* Agent) const
{
    return !bLocked && IsValid(Agent) && Agent->GetWorld() == GetWorld() && Agent->GetController() && !Agent->IsPlayerControlled()
        && AINavigation->HasValidConfiguration();
}
bool AInventoryKeyDoor::OpenDoorForAI_Implementation(APawn* Agent)
{
    if (bLocked || !IsValid(Agent) || !GetWorld() || !AINavigation->HasValidConfiguration()
        || !IHorrorAIDoor::Execute_CanAIOpenDoor(this, Agent) || GetWorld()->IsPaused()
        || FVector::Dist(Agent->GetActorLocation(), GetActorLocation()) > AINavigation->Definition->InteractionDistance) return false;
    if (!bOpen) { bOpen = true; ApplyPresentation(); }
    return true;
}
