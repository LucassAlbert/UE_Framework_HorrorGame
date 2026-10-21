#include "Streaming/FrameworkSavePoint.h"
#include "Streaming/SaveMenuScreens.h"
#include "UIManagerSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "InteractableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
AFrameworkSavePoint::AFrameworkSavePoint()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}
bool AFrameworkSavePoint::CanInteract_Implementation() const
{
    const USaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    return Saves && !Saves->IsBusy() && Saves->IsWorldReady();
}
void AFrameworkSavePoint::Interact_Implementation(AActor* Interactor)
{
    if (!IsValid(Interactor) || Interactor->GetWorld() != GetWorld() || !CanInteract_Implementation()) return;
    const AController* Controller = Cast<AController>(Interactor);
    const AActor* User = Controller ? Controller->GetPawn() : Interactor;
    if (!IsValid(User) || FVector::DistSquared(User->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance)) return;
    UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (UFrameworkSaveSlotsScreen* Screen = Cast<UFrameworkSaveSlotsScreen>(UI->OpenScreen(EHorrorScreen::SaveSlots))) Screen->SetSaveMode();
}
FText AFrameworkSavePoint::GetInteractionText_Implementation() const { return NSLOCTEXT("SaveMenu", "SavePoint", "Salvar jogo"); }
