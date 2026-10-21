#include "StreamingWorldCoordinator.h"
#include "HorrorWorldSubsystem.h"
#include "HorrorWorldDefinition.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
DEFINE_LOG_CATEGORY_STATIC(LogWorldCoordinator, Log, All);
AStreamingWorldCoordinator::AStreamingWorldCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AStreamingWorldCoordinator::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!GetWorld()->GetSubsystem<UHorrorWorldSubsystem>()->Configure(Definition, Error))
        UE_LOG(LogWorldCoordinator, Error, TEXT("Configure streaming world %s: %s"), *GetNameSafe(this), *Error);
}
