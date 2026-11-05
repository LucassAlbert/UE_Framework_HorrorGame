#include "HidingParticipantComponent.h"
#include "HideSpotComponent.h"
#include "HidingTransitionDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UHidingParticipantComponent::UHidingParticipantComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}
void UHidingParticipantComponent::ChangeState(EHidingState NewState, UHideSpotComponent* Spot)
{
    State = NewState;
    OnStateChanged.Broadcast(State, Spot);
}
bool UHidingParticipantComponent::Enter(UHideSpotComponent* Spot) { return EnterInternal(Spot, false); }
bool UHidingParticipantComponent::Restore(UHideSpotComponent* Spot, const FTransform& SafeReturn)
{
    if (!SafeReturn.IsValid() || !EnterInternal(Spot, true)) return false;
    ReturnTransform = SafeReturn;
    GetOwner()->SetActorScale3D(SafeReturn.GetScale3D());
    return true;
}
bool UHidingParticipantComponent::EnterInternal(UHideSpotComponent* Spot, bool bRestoring)
{
    ACharacter* User = Cast<ACharacter>(GetOwner());
    APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr;
    if (State != EHidingState::Outside || bHidingApplied || !User || !PC || !IsValid(Spot) || Spot->GetWorld() != GetWorld() || Spot->IsOccupied()
        || !GetWorld() || (!bRestoring && GetWorld()->IsPaused()) || !IsValid(Spot->GetOwner())
        || !FMath::IsFinite(Spot->EntryDistance) || Spot->EntryDistance <= 0.f
        || !FMath::IsFinite(Spot->LookYawLimit) || Spot->LookYawLimit < 0.f || Spot->LookYawLimit > 89.f
        || !FMath::IsFinite(Spot->LookPitchLimit) || Spot->LookPitchLimit < 0.f || Spot->LookPitchLimit > 89.f
        || (Spot->TransitionDefinition && !Spot->TransitionDefinition->Validate())
        || (!bRestoring && FVector::Dist(User->GetActorLocation(), Spot->GetOwner()->GetActorLocation()) > Spot->EntryDistance)) return false;
    FTransform Destination = Spot->GetHiddenTransform();
    if (!Destination.IsValid()) return false;
    ReturnTransform = User->GetActorTransform();
    ReturnLook = PC->GetControlRotation();
    Destination.SetScale3D(ReturnTransform.GetScale3D());
    PreviousCollision = User->GetCapsuleComponent()->GetCollisionEnabled();
    PreviousMovement = User->GetCharacterMovement()->MovementMode;
    PreviousCustomMovement = User->GetCharacterMovement()->CustomMovementMode;
    CurrentSpot = Spot;
    ActiveDefinition = Spot->TransitionDefinition;
    TransitionController = PC;
    bHidingApplied = true;
    Spot->Occupant = this;
    Spot->GetOwner()->OnEndPlay.AddUniqueDynamic(this, &UHidingParticipantComponent::HandleSpotEndPlay);
    User->ConsumeMovementInputVector();
    User->GetCharacterMovement()->StopMovementImmediately();
    User->GetCharacterMovement()->DisableMovement();
    User->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const float Duration = !bRestoring && ActiveDefinition ? ActiveDefinition->EnterSeconds : 0;
    ChangeState(Duration > 0 ? EHidingState::Entering : EHidingState::Hidden, Spot);
    if (!bHidingApplied || CurrentSpot.Get() != Spot) return false;
    // Witnesses are notified while the pawn is still at its visible entry position.
    OnHidingChanged.Broadcast(true, Spot, ReturnTransform.GetLocation());
    if (!IsValid(User) || !IsValid(Spot) || !bHidingApplied || CurrentSpot.Get() != Spot || (!bRestoring && GetWorld()->IsPaused()))
    { ResetHiding(); return false; }
    TransitionFrom = ReturnTransform;
    TransitionTo = Destination;
    LookFrom = ReturnLook.Quaternion();
    LookTo = Destination.GetRotation();
    if (Duration > 0) BeginTransition(EHidingState::Entering, Duration);
    else
    {
        User->SetActorTransform(Destination, false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation(Destination.Rotator());
        Spot->SetDoorAmount(0);
    }
    return true;
}
void UHidingParticipantComponent::BeginTransition(EHidingState NewState, float Duration)
{
    Elapsed = 0;
    TransitionSeconds = Duration;
    if (UHideSpotComponent* Spot = GetSpot()) Spot->SetDoorAmount(0);
    if (!bHidingApplied || !CurrentSpot.IsValid()) return;
    if (State != NewState) ChangeState(NewState, GetSpot());
    if (bHidingApplied && State == NewState) SetComponentTickEnabled(true);
}
bool UHidingParticipantComponent::IsReturnLocationClear() const
{
    const ACharacter* User = Cast<ACharacter>(GetOwner());
    if (!User || !GetWorld() || !ReturnTransform.IsValid()) return false;
    const UCapsuleComponent* Capsule = User->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HidingExit), false, User);
    return !GetWorld()->OverlapBlockingTestByChannel(ReturnTransform.GetLocation(), ReturnTransform.GetRotation(),
        Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
}
bool UHidingParticipantComponent::IsPathClear(FVector From, FVector To) const
{
    const ACharacter* User = Cast<ACharacter>(GetOwner());
    const UHideSpotComponent* Spot = GetSpot();
    if (!User || !Spot || From.Equals(To, .01f)) return true;
    const UCapsuleComponent* Capsule = User->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HidingMotion), false, User);
    Params.AddIgnoredActor(Spot->GetOwner());
    FHitResult Hit;
    const float Radius = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 1.f);
    const float Height = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - 1.f);
    return !GetWorld()->SweepSingleByChannel(Hit, From, To, User->GetActorQuat(), Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Radius, Height), Params);
}
bool UHidingParticipantComponent::Exit()
{
    ACharacter* User = Cast<ACharacter>(GetOwner());
    APlayerController* PC = User ? Cast<APlayerController>(User->GetController()) : nullptr;
    if (State != EHidingState::Hidden || !PC || GetWorld()->IsPaused() || !IsReturnLocationClear()
        || !IsPathClear(User->GetActorLocation(), ReturnTransform.GetLocation())) return false;
    const float Duration = ActiveDefinition ? ActiveDefinition->ExitSeconds : 0;
    if (Duration <= 0) return Release(true);
    TransitionFrom = User->GetActorTransform();
    TransitionTo = ReturnTransform;
    LookFrom = PC->GetControlRotation().Quaternion();
    LookTo = ReturnLook.Quaternion();
    BeginTransition(EHidingState::Exiting, Duration);
    return true;
}
bool UHidingParticipantComponent::MoveTransition(float Alpha)
{
    ACharacter* User = Cast<ACharacter>(GetOwner());
    APlayerController* PC = TransitionController.Get();
    if (!User || !PC || User->GetController() != PC) return false;
    FTransform Pose;
    Pose.Blend(TransitionFrom, TransitionTo, Alpha);
    if (!IsPathClear(User->GetActorLocation(), Pose.GetLocation())) return false;
    User->SetActorTransform(Pose, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation(FQuat::Slerp(LookFrom, LookTo, Alpha).Rotator());
    return true;
}
void UHidingParticipantComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsTransitioning() || GetWorld()->IsPaused()) return;
    UHideSpotComponent* Spot = GetSpot();
    if (!Spot || !ActiveDefinition || !TransitionController.IsValid()) { ResetHiding(); return; }
    Elapsed += DeltaTime;
    const float Progress = FMath::Clamp(Elapsed / TransitionSeconds, 0.f, 1.f);
    const float Motion = ActiveDefinition->GetMotionAlpha(Progress);
    if (!FMath::IsFinite(Motion) || !MoveTransition(Motion))
    {
        if (State == EHidingState::Entering && IsReturnLocationClear()) Release(true);
        else
        {
            FTransform Hidden = Spot->GetHiddenTransform();
            Hidden.SetScale3D(ReturnTransform.GetScale3D());
            GetOwner()->SetActorTransform(Hidden, false, nullptr, ETeleportType::TeleportPhysics);
            TransitionController->SetControlRotation(Hidden.Rotator());
            SetComponentTickEnabled(false);
            Spot->SetDoorAmount(0);
            ChangeState(EHidingState::Hidden, Spot);
        }
        return;
    }
    if (!IsTransitioning() || CurrentSpot.Get() != Spot || !ActiveDefinition) return;
    Spot->SetDoorAmount(ActiveDefinition->GetDoorAlpha(Progress));
    if (Progress >= 1 && IsTransitioning() && CurrentSpot.Get() == Spot) FinishTransition();
}
void UHidingParticipantComponent::FinishTransition()
{
    SetComponentTickEnabled(false);
    UHideSpotComponent* Spot = GetSpot();
    if (!Spot) { ResetHiding(); return; }
    Spot->SetDoorAmount(0);
    if (!IsTransitioning() || CurrentSpot.Get() != Spot) return;
    if (State == EHidingState::Entering) ChangeState(EHidingState::Hidden, Spot);
    else if (!Release(true))
    {
        FTransform Hidden = Spot->GetHiddenTransform();
        Hidden.SetScale3D(ReturnTransform.GetScale3D());
        GetOwner()->SetActorTransform(Hidden, false, nullptr, ETeleportType::TeleportPhysics);
        TransitionController->SetControlRotation(Hidden.Rotator());
        ChangeState(EHidingState::Hidden, Spot);
    }
}
void UHidingParticipantComponent::ResetHiding() { Release(false); }
bool UHidingParticipantComponent::Release(bool bReturnToEntry)
{
    UHideSpotComponent* Spot = CurrentSpot.Get();
    ACharacter* User = Cast<ACharacter>(GetOwner());
    if (!bHidingApplied || !User) return false;
    if (bReturnToEntry)
    {
        if (!IsReturnLocationClear()) return false;
        User->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        if (auto* PC = Cast<APlayerController>(User->GetController())) PC->SetControlRotation(ReturnLook);
    }
    SetComponentTickEnabled(false);
    bHidingApplied = false;
    if (Spot)
    {
        if (AActor* Owner = Spot->GetOwner()) Owner->OnEndPlay.RemoveDynamic(this, &UHidingParticipantComponent::HandleSpotEndPlay);
        Spot->SetDoorAmount(0);
        Spot->Occupant.Reset();
    }
    CurrentSpot.Reset();
    bHidingApplied = false;
    ActiveDefinition = nullptr;
    TransitionController.Reset();
    User->GetCapsuleComponent()->SetCollisionEnabled(PreviousCollision);
    User->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(PreviousMovement), PreviousCustomMovement);
    ChangeState(EHidingState::Outside, Spot);
    OnHidingChanged.Broadcast(false, Spot, ReturnTransform.GetLocation());
    return true;
}
void UHidingParticipantComponent::Look(FVector2D Input)
{
    const UHideSpotComponent* Spot = GetSpot();
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Spot || !PC || State != EHidingState::Hidden || Input.ContainsNaN()) return;
    const FRotator Center = Spot->GetHiddenTransform().Rotator();
    FRotator Look = PC->GetControlRotation();
    Look.Yaw = Center.Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(Center.Yaw, Look.Yaw) + Input.X, -Spot->LookYawLimit, Spot->LookYawLimit);
    Look.Pitch = Center.Pitch + FMath::Clamp(FMath::FindDeltaAngleDegrees(Center.Pitch, Look.Pitch) + Input.Y, -Spot->LookPitchLimit, Spot->LookPitchLimit);
    PC->SetControlRotation(Look);
}
void UHidingParticipantComponent::HandleSpotEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
    if (Reason == EEndPlayReason::Destroyed && IsReturnLocationClear())
        GetOwner()->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    ResetHiding();
}
void UHidingParticipantComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetHiding(); Super::EndPlay(Reason); }
