#include "Inventory/InventoryFlashlight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"

AInventoryFlashlight::AInventoryFlashlight()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightLight"));
    Light->SetupAttachment(Mesh);
    Light->SetIntensity(1500.f);
    Light->SetAttenuationRadius(1800.f);
    Light->SetInnerConeAngle(12.f);
    Light->SetOuterConeAngle(28.f);
}
