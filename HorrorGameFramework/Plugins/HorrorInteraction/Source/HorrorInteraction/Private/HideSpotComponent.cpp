#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "GameFramework/Actor.h"

UHideSpotComponent::UHideSpotComponent() { PrimaryComponentTick.bCanEverTick = false; }
bool UHideSpotComponent::IsOccupied() const { return Occupant.IsValid(); }
FTransform UHideSpotComponent::GetHiddenTransform() const { return HiddenOffset * GetOwner()->GetActorTransform(); }

void UHideSpotComponent::SetDoorAmount(float Amount)
{
    DoorAmount = FMath::Clamp(Amount, 0.f, 1.f);
    OnDoorChanged.Broadcast(DoorAmount);
}
