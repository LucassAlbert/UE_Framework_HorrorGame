#include "HorrorNoiseEmitterComponent.h"
#include "HorrorNoiseDefinition.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Perception/AISense_Hearing.h"

bool UHorrorNoiseDefinition::Validate() const
{
    return !NoiseTag.IsNone() && FMath::IsFinite(Loudness) && Loudness >= 0.f && Loudness <= 1.f
        && FMath::IsFinite(Range) && Range > 0.f && FMath::IsFinite(CooldownSeconds) && CooldownSeconds >= 0.f
        && FMath::IsFinite(MinimumImpactImpulse) && MinimumImpactImpulse >= 0.f;
}
UHorrorNoiseEmitterComponent::UHorrorNoiseEmitterComponent() { PrimaryComponentTick.bCanEverTick = false; bAutoActivate = true; }
void UHorrorNoiseEmitterComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bReportPhysicsImpacts)
        if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            ImpactComponent = Root;
            Root->SetNotifyRigidBodyCollision(true);
            Root->OnComponentHit.AddUniqueDynamic(this, &UHorrorNoiseEmitterComponent::HandleImpact);
        }
}
bool UHorrorNoiseEmitterComponent::ReportNoise(FVector Location, AActor* InstigatorActor)
{
    UWorld* World = GetWorld();
    if (!IsActive() || !World || World->IsPaused() || World->bIsTearingDown || !IsValid(Definition) || !Definition->Validate()
        || Definition->Loudness <= 0.f || Location.ContainsNaN() || World->GetTimeSeconds() - LastNoiseTime < Definition->CooldownSeconds) return false;
    AActor* Source = IsValid(InstigatorActor) ? InstigatorActor : GetOwner();
    if (!IsValid(Source) || Source->GetWorld() != World) return false;
    LastNoiseTime = World->GetTimeSeconds();
    UAISense_Hearing::ReportNoiseEvent(World, Location, Definition->Loudness, Source, Definition->Range, Definition->NoiseTag);
    return true;
}
void UHorrorNoiseEmitterComponent::HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (HitComponent && HitComponent->IsSimulatingPhysics() && IsValid(Definition)
        && NormalImpulse.Size() >= Definition->MinimumImpactImpulse) ReportNoise(Hit.ImpactPoint);
}
void UHorrorNoiseEmitterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ImpactComponent.IsValid()) ImpactComponent->OnComponentHit.RemoveDynamic(this, &UHorrorNoiseEmitterComponent::HandleImpact);
    ImpactComponent.Reset();
    Super::EndPlay(Reason);
}
