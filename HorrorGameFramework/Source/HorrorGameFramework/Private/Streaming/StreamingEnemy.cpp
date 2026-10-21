#include "Streaming/StreamingEnemy.h"
#include "Components/SaveComponent.h"
AStreamingEnemy::AStreamingEnemy()
{
    Save->bRecreateOnLoad = true;
    Residency = CreateDefaultSubobject<UEnemyAreaResidencyComponent>(TEXT("Residency"));
}
bool UEnemyAreaResidencyComponent::NeedsResidentRoute(FName& DestinationArea) const
{
    DestinationArea = HomeAreaId;
    const AFrameworkEnemy* Enemy = Cast<AFrameworkEnemy>(GetOwner());
    const USaveComponent* Marker = Enemy ? Enemy->FindComponentByClass<USaveComponent>() : nullptr;
    if (!Enemy || !Marker || HomeAreaId.IsNone()) return false;
    const bool bActive = Enemy->Memory.State == EHorrorEnemyState::Chase || Enemy->Memory.State == EHorrorEnemyState::Investigate || Enemy->Memory.State == EHorrorEnemyState::Search;
    return bActive || Marker->StreamingAreaId != HomeAreaId;
}
