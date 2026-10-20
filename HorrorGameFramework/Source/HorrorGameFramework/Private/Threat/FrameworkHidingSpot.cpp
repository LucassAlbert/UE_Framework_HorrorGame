#include "Threat/FrameworkHidingSpot.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AFrameworkHidingSpot::AFrameworkHidingSpot()
{
    PrimaryActorTick.bCanEverTick = false;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HideBody"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    HideSpot = CreateDefaultSubobject<UHideSpotComponent>(TEXT("HideSpot"));
    Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}
bool AFrameworkHidingSpot::CanInteract_Implementation() const { return !HideSpot->IsOccupied() && GetWorld() && !GetWorld()->IsPaused(); }
void AFrameworkHidingSpot::Interact_Implementation(AActor* Interactor)
{
    const AController* Controller = Cast<AController>(Interactor);
    AActor* User = Controller ? Controller->GetPawn() : Interactor;
    if (!IsValid(User) || !CanInteract_Implementation()) return;
    if (UHidingParticipantComponent* Hiding = User->FindComponentByClass<UHidingParticipantComponent>()) Hiding->Enter(HideSpot);
}
FText AFrameworkHidingSpot::GetInteractionText_Implementation() const { return NSLOCTEXT("Threat", "EnterHide", "Esconder-se"); }
