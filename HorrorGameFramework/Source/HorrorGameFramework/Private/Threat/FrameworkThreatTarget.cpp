#include "Threat/FrameworkThreatTarget.h"
#include "Threat/FrameworkHidingSpot.h"
#include "Core/HorrorCharacter.h"
#include "HidingParticipantComponent.h"
#include "HideSpotComponent.h"
#include "HorrorNoiseEmitterComponent.h"
#include "HorrorNoiseDefinition.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "Registry/ActorRegistry.h"
#include "Components/SaveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorThreat, Log, All);

UFrameworkThreatTarget::UFrameworkThreatTarget() { bAutoActivate = true; }
bool UFrameworkThreatTarget::IsAvailable() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UGameFlowSubsystem* Flow = GI ? GI->GetSubsystem<UGameFlowSubsystem>() : nullptr;
    const USaveSubsystem* Saves = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
    return Super::IsAvailable() && Flow && Flow->CurrentState == EGameFlowState::Playing && (!Saves || !Saves->IsRestoring());
}
bool UFrameworkThreatTarget::CanBeCaptured() const
{
    const USaveSubsystem* Saves = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    return IsAvailable() && (!Saves || !Saves->IsBusy());
}
void UFrameworkThreatTarget::BeginPlay()
{
    Super::BeginPlay();
    PreviousLocation = GetOwner()->GetActorLocation();
    if (UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>())
        Hiding->OnHidingChanged.AddUniqueDynamic(this, &UFrameworkThreatTarget::HandleHidingChanged);
    Noise = NewObject<UHorrorNoiseEmitterComponent>(GetOwner());
    Noise->Definition = FootstepNoise;
    Noise->RegisterComponent();
    GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &UFrameworkThreatTarget::UpdateFootsteps, .1f, true);
}
void UFrameworkThreatTarget::UpdateFootsteps()
{
    const AHorrorCharacter* User = Cast<AHorrorCharacter>(GetOwner());
    const FVector CurrentLocation = GetOwner()->GetActorLocation();
    const float Distance = FVector::Dist2D(PreviousLocation, CurrentLocation);
    PreviousLocation = CurrentLocation;
    if (!User || !IsAvailable() || IsConcealed() || User->IsHorrorCrouched() || !User->GetCharacterMovement()->IsMovingOnGround()
        || User->GetVelocity().SizeSquared2D() < 100.f || Distance > 400.f || !FMath::IsFinite(StepDistance) || StepDistance <= 0.f)
    { DistanceSinceStep = 0.f; return; }
    DistanceSinceStep += Distance;
    if (DistanceSinceStep < StepDistance) return;
    DistanceSinceStep = FMath::Fmod(DistanceSinceStep, StepDistance);
    Noise->Definition = FootstepNoise;
    Noise->ReportNoise(CurrentLocation);
}
void UFrameworkThreatTarget::HandleHidingChanged(bool bHidden, UHideSpotComponent* Spot, FVector EntryLocation)
{
    const USaveSubsystem* Saves = GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>();
    const USaveComponent* Marker = Spot && Spot->GetOwner() ? Spot->GetOwner()->FindComponentByClass<USaveComponent>() : nullptr;
    if (!bRestoring && (!Saves || !Saves->IsRestoring()))
    {
        HiddenSpotId = bHidden && Marker ? Marker->PersistentID : FGuid();
        if (bHidden)
            if (const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>()) HiddenReturnTransform = Hiding->GetReturnTransform();
    }
    const FName Key = bHidden && Marker ? FName(*Marker->PersistentID.ToString(EGuidFormats::Digits)) : NAME_None;
    SetConcealed(bHidden, bHidden && Spot ? Spot->GetOwner() : nullptr, EntryLocation, Key);
    DistanceSinceStep = 0.f;
}
bool UFrameworkThreatTarget::ValidateSaveState(FString& Error) const
{
    const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>();
    const USaveSubsystem* Saves = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if ((!Saves || !Saves->IsRestoring()) && Hiding && Hiding->IsHiding() && !HiddenSpotId.IsValid())
    { Error = TEXT("A persistent hiding spot requires a valid SaveComponent identity."); return false; }
    if (!HiddenReturnTransform.IsValid()) { Error = TEXT("Invalid hiding return transform."); return false; }
    if (HiddenSpotId.IsValid() && Saves)
    {
        bool bFound = false;
        for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
        {
            const USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>();
            if (Marker && Marker->PersistentID == HiddenSpotId && Actor->FindComponentByClass<UHideSpotComponent>()) { bFound = true; break; }
        }
        if (!bFound) { Error = TEXT("Saved concealment references a missing hiding spot."); return false; }
    }
    return true;
}
void UFrameworkThreatTarget::OnSaveStateRestored()
{
    TGuardValue<bool> Restoring(bRestoring, true);
    const FGuid RestoredId = HiddenSpotId;
    const FTransform SafeReturn = HiddenReturnTransform;
    UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>();
    if (!Hiding) return;
    Hiding->ResetHiding();
    SetConcealed(false, nullptr, GetOwner()->GetActorLocation());
    if (RestoredId.IsValid())
    {
        const USaveSubsystem* Saves = GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>();
        bool bRestored = false;
        for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
        {
            const USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>();
            UHideSpotComponent* Spot = Actor->FindComponentByClass<UHideSpotComponent>();
            if (Marker && Marker->PersistentID == RestoredId && Spot) { bRestored = Hiding->Restore(Spot, SafeReturn); break; }
        }
        if (!bRestored) UE_LOG(LogHorrorThreat, Error, TEXT("Cannot restore hiding spot %s for %s."), *RestoredId.ToString(), *GetNameSafe(GetOwner()));
    }
    HiddenSpotId = RestoredId;
    HiddenReturnTransform = SafeReturn;
    PreviousLocation = GetOwner()->GetActorLocation();
    DistanceSinceStep = 0.f;
}
void UFrameworkThreatTarget::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(StepTimer);
    if (UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>())
        Hiding->OnHidingChanged.RemoveDynamic(this, &UFrameworkThreatTarget::HandleHidingChanged);
    if (Noise) Noise->DestroyComponent();
    Noise = nullptr;
    Super::EndPlay(Reason);
}
