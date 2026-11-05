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
#include "GameFramework/PlayerController.h"
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
    {
        Hiding->OnHidingChanged.AddUniqueDynamic(this, &UFrameworkThreatTarget::HandleHidingChanged);
        Hiding->OnStateChanged.AddUniqueDynamic(this, &UFrameworkThreatTarget::HandleHidingStateChanged);
    }
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
void UFrameworkThreatTarget::CacheHidingSnapshot()
{
    const USaveSubsystem* Saves = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if (bRestoring || (Saves && Saves->IsRestoring())) return;
    const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>();
    if (!Hiding) return;
    const EHidingState State = Hiding->GetState();
    const UHideSpotComponent* Spot = Hiding->GetSpot();
    const USaveComponent* Marker = Spot ? Spot->GetOwner()->FindComponentByClass<USaveComponent>() : nullptr;
    HiddenSpotId = (State == EHidingState::Entering || State == EHidingState::Hidden) && Marker ? Marker->PersistentID : FGuid();
    bExitInProgress = State == EHidingState::Exiting;
    HidingSnapshotVersion = 1;
    if (Hiding->IsHiding())
    {
        HiddenReturnTransform = Hiding->GetReturnTransform();
        SavedReturnLook = Hiding->GetReturnLook();
    }
}
void UFrameworkThreatTarget::Serialize(FArchive& Ar)
{
    if (Ar.IsSaveGame())
    {
        if (Ar.IsLoading())
        {
            // Older snapshots omit these properties, including on a warm load.
            HidingSnapshotVersion = 0;
            bExitInProgress = false;
            SavedReturnLook = FRotator::ZeroRotator;
        }
        else if (Ar.IsSaving()) CacheHidingSnapshot();
    }
    Super::Serialize(Ar);
}
void UFrameworkThreatTarget::HandleHidingStateChanged(EHidingState State, UHideSpotComponent* Spot)
{
    CacheHidingSnapshot();
    const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>();
    const FVector Entry = Hiding ? Hiding->GetReturnTransform().GetLocation() : GetOwner()->GetActorLocation();
    const USaveComponent* Marker = Spot ? Spot->GetOwner()->FindComponentByClass<USaveComponent>() : nullptr;
    const FName Key = Marker ? FName(*Marker->PersistentID.ToString(EGuidFormats::Digits)) : NAME_None;
    if (State == EHidingState::Entering) BeginConcealment(Spot->GetOwner(), Entry, Key);
    else SetConcealed(State == EHidingState::Hidden, State == EHidingState::Hidden && Spot ? Spot->GetOwner() : nullptr, Entry, Key);
    DistanceSinceStep = 0.f;
}
void UFrameworkThreatTarget::HandleHidingChanged(bool bHidden, UHideSpotComponent* Spot, FVector EntryLocation)
{
    if (const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>())
        HandleHidingStateChanged(Hiding->GetState(), Spot);
}
bool UFrameworkThreatTarget::ValidateSaveState(FString& Error) const
{
    const UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>();
    const USaveSubsystem* Saves = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if ((!Saves || !Saves->IsRestoring()) && Hiding && (Hiding->GetState() == EHidingState::Entering || Hiding->IsFullyHidden()) && !HiddenSpotId.IsValid())
    { Error = TEXT("A persistent hiding spot requires a valid SaveComponent identity."); return false; }
    if (HidingSnapshotVersion > 1 || (HidingSnapshotVersion == 1 && SavedReturnLook.ContainsNaN()))
    { Error = TEXT("Invalid hiding snapshot version or camera orientation."); return false; }
    if ((!Saves || !Saves->IsRestoring()) && bExitInProgress && Hiding && !Hiding->IsReturnLocationClear())
    { Error = TEXT("Cannot save an exit with a blocked return location."); return false; }
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
    const FRotator ReturnLook = SavedReturnLook;
    const bool bRestoreOutside = HidingSnapshotVersion == 1 && bExitInProgress;
    const bool bRestoreLook = HidingSnapshotVersion == 1;
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
    if (bRestoreLook && RestoredId.IsValid()) Hiding->SetReturnLook(ReturnLook);
    if (bRestoreOutside)
    {
        GetOwner()->SetActorTransform(SafeReturn, false, nullptr, ETeleportType::TeleportPhysics);
        if (const APawn* Pawn = Cast<APawn>(GetOwner()))
            if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController())) PC->SetControlRotation(ReturnLook);
    }
    bExitInProgress = false;
    HiddenSpotId = RestoredId;
    HiddenReturnTransform = SafeReturn;
    PreviousLocation = GetOwner()->GetActorLocation();
    DistanceSinceStep = 0.f;
}
void UFrameworkThreatTarget::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(StepTimer);
    if (UHidingParticipantComponent* Hiding = GetOwner()->FindComponentByClass<UHidingParticipantComponent>())
    {
        Hiding->OnHidingChanged.RemoveDynamic(this, &UFrameworkThreatTarget::HandleHidingChanged);
        Hiding->OnStateChanged.RemoveDynamic(this, &UFrameworkThreatTarget::HandleHidingStateChanged);
    }
    if (Noise) Noise->DestroyComponent();
    Noise = nullptr;
    Super::EndPlay(Reason);
}
