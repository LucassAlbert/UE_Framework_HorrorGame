#include "InteractionScannerComponent.h"
#include "Interfaces/InteractionInterface.h"
#include "Subsystems/InteractionSubsystem.h"
#include "InteractableComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UInteractionScannerComponent::UInteractionScannerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = true;
}

void UInteractionScannerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        InteractionSubsystem = GI->GetSubsystem<UInteractionSubsystem>();
    }
    GetWorld()->GetTimerManager().SetTimer(ScanTimer, this,
        &UInteractionScannerComponent::ScanSingleByChannel, FMath::Max(0.02f, ScanInterval), true);
}

void UInteractionScannerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ScanTimer);
    if (InteractionSubsystem && InteractionSubsystem->GetFocusedActor() == LastBestActor.Get())
    {
        InteractionSubsystem->SetFocusedActor(nullptr);
    }
    LastBestActor.Reset();
    InteractionSubsystem = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UInteractionScannerComponent::Scan()
{
    ScanSingleByChannel();
}

void UInteractionScannerComponent::HandleFocusStability(AActor* NewActor)
{
    if (!InteractionSubsystem) return;
    // Losing focus must be immediate; a stale prompt must not permit another interaction.
    if (!NewActor || NewActor == LastBestActor.Get()
        || GetWorld()->GetTimeSeconds() - LastFocusChangeTime >= FocusStabilityTime)
    {
        if (NewActor != LastBestActor.Get()) LastFocusChangeTime = GetWorld()->GetTimeSeconds();
        LastBestActor = NewActor;
        InteractionSubsystem->SetFocusedActor(NewActor);
    }
}

void UInteractionScannerComponent::ScanSingleByChannel()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !InteractionSubsystem || !IsActive()) return;
    UWorld* World = GetWorld();
    if (World->IsPaused())
    {
        HandleFocusStability(nullptr);
        return;
    }

    const APlayerController* PC = Cast<APlayerController>(Owner);
    const APawn* Pawn = Cast<APawn>(Owner);
    if (!PC && Pawn) PC = Cast<APlayerController>(Pawn->GetController());
    if (PC && !PC->IsLocalController()) return;

    FVector Location;
    FRotator Rotation;
    Owner->GetActorEyesViewPoint(Location, Rotation);
    if (PC) PC->GetPlayerViewPoint(Location, Rotation);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    if (PC && PC->GetPawn()) Params.AddIgnoredActor(PC->GetPawn());

    FHitResult Hit;
    AActor* Candidate = nullptr;
    if (World->LineTraceSingleByChannel(Hit, Location,
        Location + Rotation.Vector() * ScanDistance, TraceChannel, Params))
    {
        AActor* Target = Hit.GetActor();
        if (IsValid(Target) && Target->Implements<UInteractionInterface>()
            && CheckDistance(Target, Hit.Distance) && IInteractionInterface::Execute_CanInteract(Target))
        {
            FHitResult Obstruction;
            if (!World->LineTraceSingleByChannel(Obstruction, Location, Hit.ImpactPoint, ECC_Visibility, Params)
                || Obstruction.GetActor() == Target)
            {
                Candidate = Target;
            }
        }
    }
    HandleFocusStability(Candidate);
}

bool UInteractionScannerComponent::CheckDistance(AActor* NewActor, float Distance)
{
    const UInteractableComponent* Interactable = IsValid(NewActor)
        ? NewActor->FindComponentByClass<UInteractableComponent>() : nullptr;
    return Interactable && Distance >= 0.f && Distance <= Interactable->InteractionDistance;
}
