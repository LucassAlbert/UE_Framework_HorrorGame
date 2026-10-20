#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "GameFramework/Actor.h"

UHideSpotComponent::UHideSpotComponent() { PrimaryComponentTick.bCanEverTick = false; }
bool UHideSpotComponent::IsOccupied() const { return Occupant.IsValid(); }
FTransform UHideSpotComponent::GetHiddenTransform() const { return HiddenOffset * GetOwner()->GetActorTransform(); }
