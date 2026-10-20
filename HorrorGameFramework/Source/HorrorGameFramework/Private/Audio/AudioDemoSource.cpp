#include "Audio/AudioDemoSource.h"
#include "HorrorAudioEmitter.h"
#include "HorrorSoundDefinition.h"
#include "InteractableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

AAudioDemoSource::AAudioDemoSource()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AudioSourceMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    Emitter = CreateDefaultSubobject<UHorrorAudioEmitterComponent>(TEXT("AudioEmitter"));
    Emitter->SetupAttachment(Mesh);
    Emitter->bAutoPlay = false;
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}
bool AAudioDemoSource::CanInteract_Implementation() const
{
    FText Error;
    return GetWorld() && !GetWorld()->IsPaused() && GetWorld()->GetTimeSeconds() >= NextUseTime
        && Emitter->Definition && Emitter->Definition->Validate(Error);
}
void AAudioDemoSource::Interact_Implementation(AActor* Interactor)
{
    if (!IsValid(Interactor) || Interactor->GetWorld() != GetWorld() || !CanInteract_Implementation()) return;
    AActor* User = Interactor;
    if (const AController* Controller = Cast<AController>(Interactor)) User = Controller->GetPawn();
    if (!User || FVector::DistSquared(User->GetActorLocation(), GetActorLocation()) > FMath::Square(300.f)) return;
    // The shared interaction scanner revalidates focus, trace and visibility immediately before this call.
    NextUseTime = GetWorld()->GetTimeSeconds() + .5;
    Emitter->Play();
}
FText AAudioDemoSource::GetInteractionText_Implementation() const
{
    return NSLOCTEXT("AudioDemo", "Play", "Reproduzir som 3D");
}
