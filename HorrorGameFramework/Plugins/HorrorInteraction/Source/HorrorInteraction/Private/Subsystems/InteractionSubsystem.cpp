#include "Subsystems/InteractionSubsystem.h"
#include "Interfaces/InteractionInterface.h"
#include "InteractableComponent.h"
#include "InteractionScannerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UInteractionSubsystem::Deinitialize()
{
    SetFocusedActor(nullptr);
    Super::Deinitialize();
}

void UInteractionSubsystem::Interact(AActor* InstigatorActor)
{
    TryInteract(InstigatorActor);
}

bool UInteractionSubsystem::TryInteract(AActor* InstigatorActor)
{
    AActor* Target = FocusedActor.Get();
    const APlayerController* PC = Cast<APlayerController>(InstigatorActor);
    AActor* Viewer = PC ? PC->GetPawn() : InstigatorActor;
    UWorld* World = GetWorld();
    if (!IsValid(Target) || !IsValid(Viewer) || !World || World->IsPaused()
        || Target->GetWorld() != World || !Target->Implements<UInteractionInterface>())
    {
        SetFocusedActor(nullptr);
        return false;
    }

    const UInteractableComponent* Interactable = Target->FindComponentByClass<UInteractableComponent>();
    const UInteractionScannerComponent* Scanner = Viewer->FindComponentByClass<UInteractionScannerComponent>();
    if (!Scanner && PC) Scanner = PC->FindComponentByClass<UInteractionScannerComponent>();
    if (!Interactable || !Scanner || !IInteractionInterface::Execute_CanInteract(Target))
    {
        SetFocusedActor(nullptr);
        return false;
    }

    FVector Location;
    FRotator Rotation;
    Viewer->GetActorEyesViewPoint(Location, Rotation);
    if (PC) PC->GetPlayerViewPoint(Location, Rotation);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Viewer);
    if (PC) Params.AddIgnoredActor(PC);
    FHitResult Hit;
    const float Distance = FMath::Min(Scanner->ScanDistance, Interactable->InteractionDistance);
    const bool bHit = World->LineTraceSingleByChannel(Hit, Location,
        Location + Rotation.Vector() * Distance, Scanner->TraceChannel, Params);
    if (!bHit || Hit.GetActor() != Target)
    {
        SetFocusedActor(nullptr);
        return false;
    }

    // Visibility is checked separately so an interaction-only channel cannot see through walls.
    FHitResult Obstruction;
    if (World->LineTraceSingleByChannel(Obstruction, Location, Hit.ImpactPoint, ECC_Visibility, Params)
        && Obstruction.GetActor() != Target)
    {
        SetFocusedActor(nullptr);
        return false;
    }

    IInteractionInterface::Execute_Interact(Target, InstigatorActor);
    RefreshFocusedActor();
    return true;
}

void UInteractionSubsystem::SetFocusedActor(AActor* NewFocusedActor)
{
    if (IsValid(NewFocusedActor) && (!NewFocusedActor->Implements<UInteractionInterface>()
        || !IInteractionInterface::Execute_CanInteract(NewFocusedActor)))
    {
        NewFocusedActor = nullptr;
    }

    AActor* Previous = FocusedActor.Get();
    if (Previous == NewFocusedActor)
    {
        RefreshFocusedActor();
        return;
    }
    if (IsValid(Previous) && Previous->Implements<UInteractionInterface>())
    {
        IInteractionInterface::Execute_OnFocusEnd(Previous);
    }
    if (Previous || !FocusedText.IsEmpty()) OnFocusEnd.Broadcast(FocusedText);
    FocusedActor = NewFocusedActor;
    FocusedText.Reset();
    if (IsValid(NewFocusedActor))
    {
        IInteractionInterface::Execute_OnFocusBegin(NewFocusedActor);
        if (IsValid(NewFocusedActor))
        {
            FocusedText = IInteractionInterface::Execute_GetInteractionText(NewFocusedActor).ToString();
            OnFocusOn.Broadcast(FocusedText);
        }
    }
}

void UInteractionSubsystem::RefreshFocusedActor()
{
    AActor* Target = FocusedActor.Get();
    if (!IsValid(Target))
    {
        if (!FocusedText.IsEmpty()) OnFocusEnd.Broadcast(FocusedText);
        FocusedActor.Reset();
        FocusedText.Reset();
        return;
    }
    if (!IInteractionInterface::Execute_CanInteract(Target))
    {
        SetFocusedActor(nullptr);
        return;
    }
    const FString NewText = IInteractionInterface::Execute_GetInteractionText(Target).ToString();
    if (NewText != FocusedText)
    {
        FocusedText = NewText;
        OnFocusOn.Broadcast(FocusedText);
    }
}

AActor* UInteractionSubsystem::GetFocusedActor() const
{
    return FocusedActor.Get();
}
