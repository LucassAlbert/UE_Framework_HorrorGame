#include "HidingParticipantComponent.h"
#include "HideSpotComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UHidingParticipantComponent::UHidingParticipantComponent() { PrimaryComponentTick.bCanEverTick = false; }
bool UHidingParticipantComponent::Enter(UHideSpotComponent* Spot)
{
    return EnterInternal(Spot, false);
}
bool UHidingParticipantComponent::Restore(UHideSpotComponent* Spot, const FTransform& SafeReturn)
{
    if (!SafeReturn.IsValid() || !EnterInternal(Spot, true)) return false;
    ReturnTransform = SafeReturn;
    return true;
}
bool UHidingParticipantComponent::EnterInternal(UHideSpotComponent* Spot, bool bRestoring)
{
    ACharacter* User = Cast<ACharacter>(GetOwner());
    APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr;
    if (bHidingApplied || !User || !PC || !IsValid(Spot) || Spot->GetWorld() != GetWorld() || Spot->IsOccupied()
        || !GetWorld() || (!bRestoring && GetWorld()->IsPaused()) || !IsValid(Spot->GetOwner())
        || !FMath::IsFinite(Spot->EntryDistance) || Spot->EntryDistance <= 0.f
        || !FMath::IsFinite(Spot->LookYawLimit) || Spot->LookYawLimit < 0.f || Spot->LookYawLimit > 89.f
        || !FMath::IsFinite(Spot->LookPitchLimit) || Spot->LookPitchLimit < 0.f || Spot->LookPitchLimit > 89.f
        || (!bRestoring && FVector::Dist(User->GetActorLocation(), Spot->GetOwner()->GetActorLocation()) > Spot->EntryDistance)) return false;
    const FTransform Destination = Spot->GetHiddenTransform();
    if (!Destination.IsValid()) return false;
    ReturnTransform = User->GetActorTransform();
    ReturnLook = PC->GetControlRotation();
    PreviousCollision = User->GetCapsuleComponent()->GetCollisionEnabled();
    PreviousMovement = User->GetCharacterMovement()->MovementMode;
    PreviousCustomMovement = User->GetCharacterMovement()->CustomMovementMode;
    CurrentSpot = Spot;
    bHidingApplied = true;
    Spot->Occupant = this;
    Spot->GetOwner()->OnEndPlay.AddUniqueDynamic(this, &UHidingParticipantComponent::HandleSpotEndPlay);
    // Notify before relocation so witnesses validate the entry, never an unseen position inside the prop.
    OnHidingChanged.Broadcast(true, Spot, ReturnTransform.GetLocation());
    if (!IsValid(User) || !IsValid(Spot) || !bHidingApplied || CurrentSpot.Get() != Spot) { ResetHiding(); return false; }
    User->ConsumeMovementInputVector();
    User->GetCharacterMovement()->StopMovementImmediately();
    User->GetCharacterMovement()->DisableMovement();
    User->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    User->SetActorTransform(Destination, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation(Destination.Rotator());
    return true;
}
bool UHidingParticipantComponent::Exit() { return Release(true); }
void UHidingParticipantComponent::ResetHiding() { Release(false); }
bool UHidingParticipantComponent::Release(bool bReturnToEntry)
{
    UHideSpotComponent* Spot = CurrentSpot.Get();
    ACharacter* User = Cast<ACharacter>(GetOwner());
    if (!bHidingApplied || !User) return false;
    if (bReturnToEntry)
    {
        UCapsuleComponent* Capsule = User->GetCapsuleComponent();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HidingExit), false, User);
        if (GetWorld()->OverlapBlockingTestByChannel(ReturnTransform.GetLocation(), ReturnTransform.GetRotation(),
            Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params)) return false;
        User->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        if (APlayerController* PC = Cast<APlayerController>(User->GetController())) PC->SetControlRotation(ReturnLook);
    }
    if (Spot)
    {
        if (AActor* Owner = Spot->GetOwner()) Owner->OnEndPlay.RemoveDynamic(this, &UHidingParticipantComponent::HandleSpotEndPlay);
        Spot->Occupant.Reset();
    }
    CurrentSpot.Reset();
    bHidingApplied = false;
    User->GetCapsuleComponent()->SetCollisionEnabled(PreviousCollision);
    User->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(PreviousMovement), PreviousCustomMovement);
    OnHidingChanged.Broadcast(false, Spot, ReturnTransform.GetLocation());
    return true;
}
void UHidingParticipantComponent::Look(FVector2D Input)
{
    const UHideSpotComponent* Spot = GetSpot();
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Spot || !PC || Input.ContainsNaN()) return;
    const FRotator Center = Spot->GetHiddenTransform().Rotator();
    FRotator Look = PC->GetControlRotation();
    Look.Yaw = Center.Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(Center.Yaw, Look.Yaw) + Input.X, -Spot->LookYawLimit, Spot->LookYawLimit);
    Look.Pitch = Center.Pitch + FMath::Clamp(FMath::FindDeltaAngleDegrees(Center.Pitch, Look.Pitch) + Input.Y, -Spot->LookPitchLimit, Spot->LookPitchLimit);
    PC->SetControlRotation(Look);
}
void UHidingParticipantComponent::HandleSpotEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
    ACharacter* User = Cast<ACharacter>(GetOwner());
    if (User && Reason == EEndPlayReason::Destroyed) User->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    ResetHiding();
}
void UHidingParticipantComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetHiding(); Super::EndPlay(Reason); }
