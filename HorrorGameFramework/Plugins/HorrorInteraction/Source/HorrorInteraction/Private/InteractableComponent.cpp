
#include "InteractableComponent.h"
#include <Subsystems/InteractionSubsystem.h>


UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();

    if (Owner && !Owner->ActorHasTag("Interactable"))
    {
        Owner->Tags.Add("Interactable");
    }
}

	


