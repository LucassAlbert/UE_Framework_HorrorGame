#include "HorrorEnemyController.h"
#include "HorrorEnemyCharacter.h"
#include "HorrorAITargetComponent.h"
#include "HorrorAIDoor.h"
#include "HorrorDoorNavigationComponent.h"
#include "HorrorDoorTraversalDefinition.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorAI, Log, All);

AHorrorEnemyController::AHorrorEnemyController()
{
    Senses = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SetPerceptionComponent(*Senses);
    Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
    Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
    Sight->DetectionByAffiliation.bDetectEnemies = true;
    Sight->DetectionByAffiliation.bDetectFriendlies = true;
    Sight->DetectionByAffiliation.bDetectNeutrals = true;
    Hearing->DetectionByAffiliation = Sight->DetectionByAffiliation;
    Sight->AutoSuccessRangeFromLastSeenLocation = -1.f;
    // OnRegister needs configured senses to create the perception listener.
    Senses->ConfigureSense(*Sight);
    Senses->ConfigureSense(*Hearing);
    Senses->SetDominantSense(Sight->GetSenseImplementation());
}
void AHorrorEnemyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    Enemy = Cast<AHorrorEnemyCharacter>(InPawn);
    AHorrorEnemyCharacter* User = Enemy.Get();
    FString Error;
    if (!User || !IsValid(User->Definition) || !User->Definition->Validate(Error))
    {
        UE_LOG(LogHorrorAI, Error, TEXT("Configure a valid enemy definition on %s: %s"), *GetNameSafe(InPawn), *Error);
        return;
    }
    const UHorrorEnemyDefinition* Definition = User->Definition;
    Sight->SightRadius = Definition->SightRadius;
    Sight->LoseSightRadius = Definition->LoseSightRadius;
    Sight->PeripheralVisionAngleDegrees = Definition->SightHalfAngle;
    Sight->SetMaxAge(Definition->SearchSeconds);
    Hearing->HearingRange = Definition->HearingRange;
    Hearing->SetMaxAge(Definition->SearchSeconds);
    Senses->ConfigureSense(*Sight);
    Senses->ConfigureSense(*Hearing);
    Senses->SetDominantSense(Sight->GetSenseImplementation());
    Senses->RequestStimuliListenerUpdate();
    Senses->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AHorrorEnemyController::HandlePerception);
    bReady = true;
    LastUpdateTime = GetWorld()->GetTimeSeconds();
    if (User->Memory.State == EHorrorEnemyState::Patrol && !User->Memory.bPatrolWaiting && User->Memory.RemainingTime <= 0.f)
        User->Memory.RemainingTime = Definition->MoveTimeoutSeconds;
    ResumeAfterRestore();
    GetWorldTimerManager().SetTimer(BrainTimer, this, &AHorrorEnemyController::UpdateBrain, Definition->UpdateInterval, true);
}
void AHorrorEnemyController::SetTarget(AActor* Actor)
{
    UHorrorAITargetComponent* NewTarget = IsValid(Actor) ? Actor->FindComponentByClass<UHorrorAITargetComponent>() : nullptr;
    if (TrackedTarget.Get() == NewTarget) return;
    if (TrackedTarget.IsValid())
    {
        TrackedTarget->OnConcealmentChanged.RemoveDynamic(this, &AHorrorEnemyController::HandleConcealment);
        TrackedTarget->OnConcealmentStarted.RemoveDynamic(this, &AHorrorEnemyController::HandleConcealmentStarted);
    }
    TrackedTarget = NewTarget;
    WitnessedAnchor.Reset();
    if (NewTarget)
    {
        NewTarget->OnConcealmentChanged.AddUniqueDynamic(this, &AHorrorEnemyController::HandleConcealment);
        NewTarget->OnConcealmentStarted.AddUniqueDynamic(this, &AHorrorEnemyController::HandleConcealmentStarted);
    }
}
bool AHorrorEnemyController::HasSightAt(AActor* Actor, FVector Location) const
{
    const AHorrorEnemyCharacter* User = Enemy.Get();
    if (!User || !IsValid(Actor) || Location.ContainsNaN()) return false;
    FVector Origin;
    FRotator Rotation;
    User->GetActorEyesViewPoint(Origin, Rotation);
    const FVector Direction = Location - Origin;
    if (Direction.SizeSquared() > FMath::Square(User->Definition->LoseSightRadius)
        || FVector::DotProduct(Rotation.Vector(), Direction.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(User->Definition->SightHalfAngle))) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySightValidation), false, User);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, Location, ECC_Visibility, Params) || Hit.GetActor() == Actor;
}
void AHorrorEnemyController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!bReady || !User || GetWorld()->IsPaused() || User->Memory.State >= EHorrorEnemyState::Captured || !IsValid(Actor) || Actor == User) return;
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed()
        && User->Memory.State != EHorrorEnemyState::Chase && !Stimulus.StimulusLocation.ContainsNaN())
    {
        User->Memory.LastKnownLocation = Stimulus.StimulusLocation;
        User->Memory.bHasLastKnownLocation = true;
        ChangeEnemyState(EHorrorEnemyState::Investigate);
    }
}
void AHorrorEnemyController::HandleConcealmentStarted(AActor* Actor, FVector EntryLocation)
{
    HandleConcealment(Actor, true, EntryLocation);
}
void AHorrorEnemyController::HandleConcealment(AActor* Actor, bool bConcealed, FVector EntryLocation)
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!bReady || !User || !TrackedTarget.IsValid() || TrackedTarget->GetOwner() != Actor) return;
    // Completing an entry must not erase a previously witnessed attempt.
    if (bConcealed && WitnessedAnchor.IsValid() && WitnessedAnchor.Get() == TrackedTarget->GetConcealmentAnchor()) return;
    if (bConcealed && User->Memory.State == EHorrorEnemyState::Chase && HasSightAt(Actor, EntryLocation))
    {
        RememberConcealment(EntryLocation);
    }
    else
    {
        WitnessedAnchor.Reset();
        if (User->Memory.State == EHorrorEnemyState::Chase) ChangeEnemyState(EHorrorEnemyState::Investigate);
    }
}
void AHorrorEnemyController::RememberConcealment(FVector EntryLocation)
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!User || !TrackedTarget.IsValid()) return;
    WitnessedAnchor = TrackedTarget->GetConcealmentAnchor();
    WitnessedEntry = EntryLocation;
    User->Memory.WitnessedConcealmentKey = TrackedTarget->GetConcealmentKey();
    User->Memory.WitnessedEntry = EntryLocation;
    User->Memory.LastKnownLocation = EntryLocation;
    User->Memory.RemainingTime = User->Definition->MoveTimeoutSeconds;
    RequestDestination(EntryLocation);
}
void AHorrorEnemyController::ChangeEnemyState(EHorrorEnemyState State)
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!User) return;
    CancelDoorInteraction();
    StopMovement();
    bMoveRequested = false;
    User->Memory.State = State;
    if (State != EHorrorEnemyState::Chase) User->Memory.WitnessedConcealmentKey = NAME_None;
    User->Memory.bPatrolWaiting = false;
    User->Memory.RemainingTime = State == EHorrorEnemyState::Search ? User->Definition->SearchSeconds : User->Definition->MoveTimeoutSeconds;
    const float Speed = State == EHorrorEnemyState::Chase ? User->Definition->ChaseSpeed
        : (State == EHorrorEnemyState::Patrol ? User->Definition->PatrolSpeed : User->Definition->InvestigateSpeed);
    User->GetCharacterMovement()->MaxWalkSpeed = Speed;
    User->OnStateChanged.Broadcast(State);
}
void AHorrorEnemyController::RequestDestination(FVector Goal)
{
    const AHorrorEnemyCharacter* User = Enemy.Get();
    if (!User || Goal.ContainsNaN() || DoorUseState != EDoorUseState::None) return;
    if (bMoveRequested && FVector::DistSquared(MoveGoal, Goal) < 2500.f && GetMoveStatus() == EPathFollowingStatus::Moving) return;
    MoveGoal = Goal;
    bMoveRequested = true;
    // A chase must approach inside the capture radius instead of stopping at patrol tolerance.
    const float Acceptance = User->Memory.State == EHorrorEnemyState::Chase
        ? FMath::Min(User->Definition->ArrivalRadius, User->Definition->CaptureDistance * .5f)
        : User->Definition->ArrivalRadius;
    // Investigation may approach the reachable side of a blocked last-known location.
    const bool bAllowPartial = User->Memory.State == EHorrorEnemyState::Investigate;
    const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, Acceptance, false, true, false, false, nullptr, bAllowPartial);
    if (Result == EPathFollowingRequestResult::Failed && !bReportedNavigationFailure)
    {
        UE_LOG(LogHorrorAI, Warning, TEXT("Cannot navigate %s to %s. Check NavMesh bounds, route and obstacles."), *GetNameSafe(User), *Goal.ToCompactString());
        bReportedNavigationFailure = true;
    }
    else if (Result != EPathFollowingRequestResult::Failed) bReportedNavigationFailure = false;
}
void AHorrorEnemyController::AdvancePatrol()
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!User) return;
    if (!User->PatrolLocations.IsEmpty()) User->Memory.PatrolIndex = (User->Memory.PatrolIndex + 1) % User->PatrolLocations.Num();
    ChangeEnemyState(EHorrorEnemyState::Patrol);
}
void AHorrorEnemyController::BeginSearch()
{
    if (const AHorrorEnemyCharacter* User = Enemy.Get()) SearchOrigin = User->GetActorLocation();
    ChangeEnemyState(EHorrorEnemyState::Search);
    ChooseSearchGoal();
}
void AHorrorEnemyController::ChooseSearchGoal()
{
    const AHorrorEnemyCharacter* User = Enemy.Get();
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Location;
    if (Navigation && User && Navigation->GetRandomReachablePointInRadius(SearchOrigin, User->Definition->SearchRadius, Location)) RequestDestination(Location.Location);
}
bool AHorrorEnemyController::CaptureIfClose(UHorrorAITargetComponent* Target, FVector Location, AActor* Anchor)
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    APawn* Player = Target ? Cast<APawn>(Target->GetOwner()) : nullptr;
    if (!User || !Player || !Target->CanBeCaptured() || GetWorld()->IsPaused()
        || User->Memory.State >= EHorrorEnemyState::Captured
        || FVector::Dist(User->GetActorLocation(), Location) > User->Definition->CaptureDistance
        || !HasSightAt(Anchor ? Anchor : Player, Location)) return false;
    ChangeEnemyState(EHorrorEnemyState::Captured);
    User->OnCaptured.Broadcast(User, Player);
    return true;
}
void AHorrorEnemyController::UpdateBrain()
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!bReady || !User || GetWorld()->IsPaused()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const float Elapsed = FMath::Max(0.f, static_cast<float>(Now - LastUpdateTime));
    LastUpdateTime = Now;
    if (User->Memory.State >= EHorrorEnemyState::Captured) { CancelDoorInteraction(); return; }
    if (UpdateDoorTraversal(Elapsed)) return;
    User->Memory.RemainingTime = FMath::Max(0.f, User->Memory.RemainingTime - Elapsed);
    TArray<AActor*> Visible;
    Senses->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
    for (AActor* Actor : Visible)
    {
        UHorrorAITargetComponent* Target = IsValid(Actor) ? Actor->FindComponentByClass<UHorrorAITargetComponent>() : nullptr;
        if (!Target || !Target->IsAvailable() || Target->IsConcealed() || !HasSightAt(Actor, Actor->GetActorLocation())) continue;
        SetTarget(Actor);
        User->Memory.LastKnownLocation = Actor->GetActorLocation();
        User->Memory.bHasLastKnownLocation = true;
        if (User->Memory.State != EHorrorEnemyState::Chase) ChangeEnemyState(EHorrorEnemyState::Chase);
        if (Target->IsEnteringConcealment() && !WitnessedAnchor.IsValid())
            RememberConcealment(Target->GetConcealmentEntry());
        if (!CaptureIfClose(Target, Actor->GetActorLocation(), nullptr)) RequestDestination(User->Memory.LastKnownLocation);
        return;
    }
    if (User->Memory.State == EHorrorEnemyState::Chase)
    {
        UHorrorAITargetComponent* Target = TrackedTarget.Get();
        if (Target && Target->IsAvailable() && (Target->IsConcealed() || Target->IsEnteringConcealment()) && WitnessedAnchor.IsValid())
        {
            if (CaptureIfClose(Target, WitnessedEntry, WitnessedAnchor.Get())) return;
            if (User->Memory.RemainingTime <= 0.f) { WitnessedAnchor.Reset(); BeginSearch(); }
            else RequestDestination(WitnessedEntry);
            return;
        }
        ChangeEnemyState(EHorrorEnemyState::Investigate);
    }
    if (User->Memory.State == EHorrorEnemyState::Search)
    {
        if (User->Memory.RemainingTime <= 0.f) { SetTarget(nullptr); ChangeEnemyState(EHorrorEnemyState::Patrol); }
        else if (bMoveRequested && GetMoveStatus() != EPathFollowingStatus::Moving) ChooseSearchGoal();
        return;
    }
    if (User->Memory.State == EHorrorEnemyState::Investigate)
    {
        if (!User->Memory.bHasLastKnownLocation) { ChangeEnemyState(EHorrorEnemyState::Patrol); return; }
        if (FVector::Dist2D(User->GetActorLocation(), User->Memory.LastKnownLocation) <= User->Definition->ArrivalRadius
            || (bMoveRequested && GetMoveStatus() == EPathFollowingStatus::Idle)
            || User->Memory.RemainingTime <= 0.f) { BeginSearch(); return; }
        RequestDestination(User->Memory.LastKnownLocation);
        return;
    }
    if (User->PatrolLocations.IsEmpty()) return;
    User->Memory.PatrolIndex %= User->PatrolLocations.Num();
    const FVector Goal = User->PatrolLocations[User->Memory.PatrolIndex];
    if (User->Memory.bPatrolWaiting)
    {
        if (User->Memory.RemainingTime <= 0.f) AdvancePatrol();
    }
    else if (FVector::Dist2D(User->GetActorLocation(), Goal) <= User->Definition->ArrivalRadius || User->Memory.RemainingTime <= 0.f)
    {
        StopMovement();
        User->Memory.bPatrolWaiting = true;
        User->Memory.RemainingTime = User->Definition->PatrolWaitSeconds;
    }
    else RequestDestination(Goal);
}
void AHorrorEnemyController::ResumeAfterRestore()
{
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!bReady || !User) return;
    CancelDoorInteraction();
    SearchOrigin = User->GetActorLocation();
    SetTarget(nullptr);
    Senses->ForgetAll();
    CancelDoorInteraction();
    StopMovement();
    bMoveRequested = false;
    LastUpdateTime = GetWorld()->GetTimeSeconds();
    const FHorrorEnemyMemory Restored = User->Memory;
    bool bRestoreWitness = false;
    if (Restored.State == EHorrorEnemyState::Chase && !Restored.WitnessedConcealmentKey.IsNone())
    {
        const APlayerController* PC = GetWorld()->GetFirstPlayerController();
        APawn* Player = PC ? PC->GetPawn() : nullptr;
        UHorrorAITargetComponent* Target = Player ? Player->FindComponentByClass<UHorrorAITargetComponent>() : nullptr;
        if (Target && Target->IsConcealed() && Target->GetConcealmentKey() == Restored.WitnessedConcealmentKey)
        {
            SetTarget(Player);
            WitnessedAnchor = Target->GetConcealmentAnchor();
            WitnessedEntry = Restored.WitnessedEntry;
            bRestoreWitness = WitnessedAnchor.IsValid();
        }
    }
    const EHorrorEnemyState State = Restored.State == EHorrorEnemyState::Chase && !bRestoreWitness ? EHorrorEnemyState::Investigate : Restored.State;
    ChangeEnemyState(State);
    User->Memory.RemainingTime = Restored.RemainingTime;
    User->Memory.bPatrolWaiting = Restored.bPatrolWaiting;
}
void AHorrorEnemyController::Shutdown()
{
    bReady = false;
    CancelDoorInteraction();
    GetWorldTimerManager().ClearTimer(BrainTimer);
    SetTarget(nullptr);
    Senses->OnTargetPerceptionUpdated.RemoveDynamic(this, &AHorrorEnemyController::HandlePerception);
    Senses->ForgetAll();
    StopMovement();
    Enemy.Reset();
}
void AHorrorEnemyController::OnUnPossess() { Shutdown(); Super::OnUnPossess(); }
void AHorrorEnemyController::EndPlay(const EEndPlayReason::Type Reason) { Shutdown(); Super::EndPlay(Reason); }

bool AHorrorEnemyController::CanUseDoor(const UHorrorDoorNavigationComponent* Link) const
{
    const AHorrorEnemyCharacter* User = Enemy.Get();
    if (!bReady || !User || !IsValid(Link) || Link->GetWorld() != GetWorld() || !Link->HasValidConfiguration()) return false;
    AActor* Door = Link->GetOwner();
    if (IHorrorAIDoor::Execute_IsAIDoorLocked(Door)) return false;
    if (IHorrorAIDoor::Execute_IsAIDoorOpen(Door)) return User->Memory.State < EHorrorEnemyState::Captured;
    const bool bFollowingProgress = User->Memory.State == EHorrorEnemyState::Chase
        || User->Memory.State == EHorrorEnemyState::Investigate || User->Memory.State == EHorrorEnemyState::Search;
    return bFollowingProgress && IHorrorAIDoor::Execute_CanAIOpenDoor(Door, Enemy.Get());
}
bool AHorrorEnemyController::IsWaitingForDoor() const { return DoorUseState == EDoorUseState::Waiting; }
AActor* AHorrorEnemyController::GetCurrentDoor() const { return DoorLink.IsValid() ? DoorLink->GetOwner() : nullptr; }
bool AHorrorEnemyController::BeginDoorTraversal(UHorrorDoorNavigationComponent* Link, UPathFollowingComponent* Path, FVector ExitPoint)
{
    if (DoorUseState != EDoorUseState::None || !CanUseDoor(Link) || !Path || Path != GetPathFollowingComponent()
        || ExitPoint.ContainsNaN() || GetWorld()->IsPaused()) return false;
    const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), Link->GetOwner()->GetActorLocation());
    if (Distance > Link->Definition->InteractionDistance) return false;
    DoorLink = Link;
    DoorPath = Path;
    DoorExit = ExitPoint;
    LastUpdateTime = GetWorld()->GetTimeSeconds();
    if (IHorrorAIDoor::Execute_IsAIDoorOpen(Link->GetOwner()))
    {
        DoorUseState = EDoorUseState::Crossing;
        DoorRemaining = Link->Definition->CrossingTimeoutSeconds;
    }
    else
    {
        DoorUseState = EDoorUseState::Waiting;
        DoorRemaining = Link->Definition->OpenDelaySeconds;
        // PauseMove resets velocity while preserving the active navigation request.
        Path->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Reset);
    }
    return true;
}
void AHorrorEnemyController::NotifyDoorTraversalFinished(UHorrorDoorNavigationComponent* Link)
{
    if (DoorLink.Get() != Link) return;
    DoorUseState = EDoorUseState::None;
    DoorLink.Reset();
    DoorPath.Reset();
    DoorRemaining = 0;
    bMoveRequested = false;
}
void AHorrorEnemyController::FinishDoorTraversal(bool bAbortMove)
{
    UHorrorDoorNavigationComponent* Link = DoorLink.Get();
    UPathFollowingComponent* Path = DoorPath.Get();
    DoorUseState = EDoorUseState::None;
    DoorLink.Reset();
    DoorPath.Reset();
    DoorRemaining = 0;
    bMoveRequested = false;
    if (Path && Link) Path->FinishUsingCustomLink(Link);
    if (bAbortMove) StopMovement();
}
void AHorrorEnemyController::CancelDoorInteraction()
{
    if (DoorUseState != EDoorUseState::None) FinishDoorTraversal(true);
}
bool AHorrorEnemyController::UpdateDoorTraversal(float Elapsed)
{
    if (DoorUseState == EDoorUseState::None) return false;
    UHorrorDoorNavigationComponent* Link = DoorLink.Get();
    UPathFollowingComponent* Path = DoorPath.Get();
    AHorrorEnemyCharacter* User = Enemy.Get();
    if (!Link || !Path || !User || !CanUseDoor(Link)
        || (TrackedTarget.IsValid() && !TrackedTarget->IsAvailable())
        || FVector::Dist(User->GetActorLocation(), Link->GetOwner()->GetActorLocation()) > Link->Definition->InteractionDistance)
    { CancelDoorInteraction(); return false; }
    UHorrorAITargetComponent* Target = TrackedTarget.Get();
    if (Target && !Target->IsConcealed() && CaptureIfClose(Target, Target->GetOwner()->GetActorLocation(), nullptr)) return true;
    if (DoorUseState == EDoorUseState::Waiting)
    {
        DoorRemaining = FMath::Max(0.f, DoorRemaining - Elapsed);
        AActor* Door = Link->GetOwner();
        const bool bOpen = IHorrorAIDoor::Execute_IsAIDoorOpen(Door);
        if (!bOpen && DoorRemaining > 0) return true;
        if (!bOpen && !IHorrorAIDoor::Execute_OpenDoorForAI(Door, User)) { CancelDoorInteraction(); return false; }
        if (DoorLink.Get() != Link || DoorUseState == EDoorUseState::None) return true;
        DoorUseState = EDoorUseState::Crossing;
        DoorRemaining = Link->Definition->CrossingTimeoutSeconds;
        Path->ResumeMove(FAIRequestID::CurrentRequest);
    }
    else
    {
        DoorRemaining -= Elapsed;
        if (FVector::DistSquared2D(User->GetActorLocation(), DoorExit) <= FMath::Square(50.f)) FinishDoorTraversal(false);
        else if (DoorRemaining <= 0) FinishDoorTraversal(true);
    }
    return true;
}
