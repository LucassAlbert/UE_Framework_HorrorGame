#include "HorrorDoorNavigationComponent.h"
#include "HorrorDoorTraversalDefinition.h"
#include "HorrorAIDoor.h"
#include "HorrorEnemyController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorDoorNavigation, Log, All);
UHorrorDoorNavigationComponent::UHorrorDoorNavigationComponent(const FObjectInitializer& Initializer) : Super(Initializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    bAttachToOwnersRoot = false;
}
bool UHorrorDoorNavigationComponent::HasValidConfiguration() const
{
    const AActor* Owner = GetOwner();
    return Owner && Owner->Implements<UHorrorAIDoor>() && Definition && Definition->Validate()
        && !StartOffset.ContainsNaN() && !EndOffset.ContainsNaN() && !StartOffset.Equals(EndOffset, 1.f)
        && !Owner->GetActorScale3D().ContainsNaN() && Owner->GetActorScale3D().GetAbsMin() > UE_SMALL_NUMBER;
}
void UHorrorDoorNavigationComponent::RefreshDoorNavigation()
{
    const bool bValid = HasValidConfiguration();
    if (bValid)
    {
        const FVector Scale = GetOwner()->GetActorScale3D();
        SetLinkData(StartOffset / Scale, EndOffset / Scale, ENavLinkDirection::BothWays);
    }
    SetNavigationRelevancy(bValid);
    SetEnabled(bValid && !IHorrorAIDoor::Execute_IsAIDoorLocked(GetOwner()));
}
void UHorrorDoorNavigationComponent::OnRegister()
{
    RefreshDoorNavigation();
    Super::OnRegister();
}
void UHorrorDoorNavigationComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Definition && !HasValidConfiguration())
        UE_LOG(LogHorrorDoorNavigation, Warning, TEXT("Configure a valid door profile, interface, scale and distinct floor-level endpoints on %s."), *GetNameSafe(GetOwner()));
    RefreshDoorNavigation();
}
bool UHorrorDoorNavigationComponent::IsLinkPathfindingAllowed(const UObject* Querier) const
{
    if (!HasValidConfiguration() || IHorrorAIDoor::Execute_IsAIDoorLocked(GetOwner())) return false;
    if (IHorrorAIDoor::Execute_IsAIDoorOpen(GetOwner())) return true;
    const AHorrorEnemyController* AI = Cast<AHorrorEnemyController>(Querier);
    if (const APawn* Pawn = Cast<APawn>(Querier)) AI = Cast<AHorrorEnemyController>(Pawn->GetController());
    return AI && AI->CanUseDoor(this);
}
bool UHorrorDoorNavigationComponent::OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint)
{
    UPathFollowingComponent* Path = Cast<UPathFollowingComponent>(PathComp);
    AHorrorEnemyController* AI = Path ? Cast<AHorrorEnemyController>(Path->GetOwner()) : nullptr;
    if (!AI || !AI->BeginDoorTraversal(this, Path, DestPoint)) return false;
    Super::OnLinkMoveStarted(PathComp, DestPoint);
    return true;
}
void UHorrorDoorNavigationComponent::OnLinkMoveFinished(UObject* PathComp)
{
    Super::OnLinkMoveFinished(PathComp);
    const UPathFollowingComponent* Path = Cast<UPathFollowingComponent>(PathComp);
    if (AHorrorEnemyController* AI = Path ? Cast<AHorrorEnemyController>(Path->GetOwner()) : nullptr)
        AI->NotifyDoorTraversalFinished(this);
}
void UHorrorDoorNavigationComponent::OnUnregister()
{
    const auto Agents = MovingAgents;
    for (const auto& Agent : Agents)
        if (const UPathFollowingComponent* Path = Cast<UPathFollowingComponent>(Agent.Get()))
            if (AHorrorEnemyController* AI = Cast<AHorrorEnemyController>(Path->GetOwner())) AI->CancelDoorInteraction();
    Super::OnUnregister();
}
#if WITH_EDITOR
void UHorrorDoorNavigationComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    RefreshDoorNavigation();
}
#endif
