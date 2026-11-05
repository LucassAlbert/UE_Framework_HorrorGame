#include "Threat/FrameworkHidingSpot.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "HidingTransitionDefinition.h"
#include "Components/SceneComponent.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AFrameworkHidingSpot::AFrameworkHidingSpot()
{
    PrimaryActorTick.bCanEverTick = false;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HideBody"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
    DoorPivot->SetupAttachment(Body);
    DoorPivot->SetRelativeLocation(FVector(52, -50, 0));
    Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
    Door->SetupAttachment(DoorPivot);
    Door->SetRelativeLocation(FVector(0, 50, 0));
    Door->SetRelativeScale3D(FVector(.035f, 1, 1));
    Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Door->SetCanEverAffectNavigation(false);
    HideSpot = CreateDefaultSubobject<UHideSpotComponent>(TEXT("HideSpot"));
    Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}
void AFrameworkHidingSpot::BeginPlay()
{
    Super::BeginPlay();
    ClosedDoorRotation = DoorPivot->GetRelativeRotation().Quaternion();
    HideSpot->OnDoorChanged.AddUniqueDynamic(this, &AFrameworkHidingSpot::HandleDoorChanged);
    if (bUsePrototypeCabinet && Body->GetStaticMesh())
    {
        Body->SetVisibility(false, false);
        Door->SetStaticMesh(Body->GetStaticMesh());
        // Leave a viewing slit at eye height; the prototype must support limited hidden look.
        Door->SetRelativeLocation(FVector(0, 50, -13));
        Door->SetRelativeScale3D(FVector(.035f, 1, .74f));
        UStaticMeshComponent* UpperDoor = NewObject<UStaticMeshComponent>(this);
        UpperDoor->SetupAttachment(DoorPivot);
        UpperDoor->SetStaticMesh(Body->GetStaticMesh());
        UpperDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UpperDoor->SetCanEverAffectNavigation(false);
        UpperDoor->SetRelativeLocation(FVector(0, 50, 43));
        UpperDoor->SetRelativeScale3D(FVector(.035f, 1, .14f));
        UpperDoor->RegisterComponent();
        PrototypePanels.Add(UpperDoor);
        const FVector Locations[] = { FVector(-48, 0, 0), FVector(0, -48, 0), FVector(0, 48, 0), FVector(0, 0, 48), FVector(0, 0, -48) };
        const FVector Scales[] = { FVector(.04f, 1, 1), FVector(1, .04f, 1), FVector(1, .04f, 1), FVector(1, 1, .04f), FVector(1, 1, .04f) };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Locations); ++Index)
        {
            UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this);
            Panel->SetupAttachment(Body);
            Panel->SetStaticMesh(Body->GetStaticMesh());
            Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Panel->SetCanEverAffectNavigation(false);
            Panel->SetRelativeLocation(Locations[Index]);
            Panel->SetRelativeScale3D(Scales[Index]);
            Panel->RegisterComponent();
            PrototypePanels.Add(Panel);
        }
    }
    HandleDoorChanged(0);
}
void AFrameworkHidingSpot::HandleDoorChanged(float OpenAmount)
{
    const FQuat Open = HideSpot->TransitionDefinition ? HideSpot->TransitionDefinition->DoorOpenRotation.Quaternion() : FQuat::Identity;
    DoorPivot->SetRelativeRotation(ClosedDoorRotation * FQuat::Slerp(FQuat::Identity, Open, OpenAmount));
}
void AFrameworkHidingSpot::EndPlay(const EEndPlayReason::Type Reason)
{
    HideSpot->OnDoorChanged.RemoveDynamic(this, &AFrameworkHidingSpot::HandleDoorChanged);
    DoorPivot->SetRelativeRotation(ClosedDoorRotation);
    Super::EndPlay(Reason);
}
bool AFrameworkHidingSpot::CanInteract_Implementation() const { return !HideSpot->IsOccupied() && GetWorld() && !GetWorld()->IsPaused(); }
void AFrameworkHidingSpot::Interact_Implementation(AActor* Interactor)
{
    const AController* Controller = Cast<AController>(Interactor);
    AActor* User = Controller ? Controller->GetPawn() : Interactor;
    if (!IsValid(User) || !CanInteract_Implementation()) return;
    if (UHidingParticipantComponent* Hiding = User->FindComponentByClass<UHidingParticipantComponent>()) Hiding->Enter(HideSpot);
}
FText AFrameworkHidingSpot::GetInteractionText_Implementation() const { return NSLOCTEXT("Threat", "EnterHide", "Esconder-se"); }
