#include "StreamingPassageGate.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
AStreamingPassageGate::AStreamingPassageGate()
{
    PrimaryActorTick.bCanEverTick = false;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateBody"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAll"));
}
void AStreamingPassageGate::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->GetSubsystem<UHorrorWorldSubsystem>()->OnAreaChanged.AddUniqueDynamic(this, &AStreamingPassageGate::HandleAreaChanged);
    Refresh();
}
void AStreamingPassageGate::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UHorrorWorldSubsystem* World = GetWorld()->GetSubsystem<UHorrorWorldSubsystem>()) World->OnAreaChanged.RemoveDynamic(this, &AStreamingPassageGate::HandleAreaChanged);
    Super::EndPlay(Reason);
}
void AStreamingPassageGate::HandleAreaChanged(FName AreaId, EHorrorAreaState State) { if (AreaId == RequiredArea) Refresh(); }
void AStreamingPassageGate::Refresh()
{
    const bool bReady = GetWorld()->GetSubsystem<UHorrorWorldSubsystem>()->IsAreaReady(RequiredArea);
    Body->SetVisibility(!bReady);
    Body->SetCollisionEnabled(bReady ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}
