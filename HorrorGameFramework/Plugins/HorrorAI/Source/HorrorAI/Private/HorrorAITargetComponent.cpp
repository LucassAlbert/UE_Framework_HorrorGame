#include "HorrorAITargetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

UHorrorAITargetComponent::UHorrorAITargetComponent() { PrimaryComponentTick.bCanEverTick = false; bAutoActivate = true; }
void UHorrorAITargetComponent::BeginPlay()
{
    Super::BeginPlay();
    Stimuli = NewObject<UAIPerceptionStimuliSourceComponent>(GetOwner());
    Stimuli->RegisterForSense(UAISense_Sight::StaticClass());
    Stimuli->RegisterComponent();
    Stimuli->RegisterWithPerceptionSystem();
}
void UHorrorAITargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Stimuli) { Stimuli->UnregisterFromPerceptionSystem(); Stimuli->DestroyComponent(); }
    Stimuli = nullptr;
    Super::EndPlay(Reason);
}
bool UHorrorAITargetComponent::IsAvailable() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return IsActive() && IsValid(Pawn) && Pawn->IsPlayerControlled() && GetWorld() && !GetWorld()->bIsTearingDown;
}
void UHorrorAITargetComponent::SetConcealed(bool bHidden, AActor* Anchor, FVector EntryLocation, FName PersistentKey)
{
    if (bHidden && (!IsValid(Anchor) || Anchor->GetWorld() != GetWorld() || EntryLocation.ContainsNaN())) return;
    if (bConcealed == bHidden && ConcealmentAnchor.Get() == Anchor) return;
    bConcealed = bHidden;
    ConcealmentAnchor = bHidden ? Anchor : nullptr;
    ConcealmentKey = bHidden ? PersistentKey : NAME_None;
    OnConcealmentChanged.Broadcast(GetOwner(), bConcealed, EntryLocation);
}
