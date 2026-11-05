#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Threat/FrameworkThreatCharacter.h"
#include "Threat/FrameworkThreatTarget.h"
#include "Threat/FrameworkHidingSpot.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "HidingTransitionDefinition.h"
#include "Components/SaveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Core/HorrorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorHidingTransitionTest, "HorrorFramework.Threat.HidingTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorHidingTransitionTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("HidingTransitionAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    auto* Player = World->SpawnActor<AFrameworkThreatCharacter>(FVector(0, 0, 120), FRotator::ZeroRotator);
    auto* PC = World->SpawnActor<AHorrorPlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    PC->Possess(Player);
    auto* Spot = World->SpawnActor<AFrameworkHidingSpot>(FVector(150, 0, 120), FRotator(0, 90, 0));
    Spot->SetActorScale3D(FVector(1.3f, 1.3f, 2.3f));
    Spot->Save->PersistentKey = TEXT("TransitionTestSpot");
    auto* Profile = NewObject<UHidingTransitionDefinition>(Spot);
    Profile->EnterSeconds = 1;
    Profile->ExitSeconds = 1;
    Spot->HideSpot->TransitionDefinition = Profile;
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Saves = GI->GetSubsystem<USaveSubsystem>();
    Saves->ActorRegistry->RegisterActor(Player);
    Saves->ActorRegistry->RegisterActor(Spot);
    UHidingParticipantComponent* Hiding = Player->Hiding;
    auto Step = [&](float Seconds) { static_cast<UActorComponent*>(Hiding)->TickComponent(Seconds, LEVELTICK_All, nullptr); };
    PC->SetControlRotation(FRotator(10, 30, 0));
    const FTransform Entry = Player->GetActorTransform();
    const FRotator EntryLook = PC->GetControlRotation();
    TestTrue(TEXT("Idle participant does not tick"), !Hiding->IsComponentTickEnabled());
    TestTrue(TEXT("Valid profile starts animated entry"), Hiding->Enter(Spot->HideSpot));
    TestTrue(TEXT("Entry reserves spot without protection"), Hiding->GetState() == EHidingState::Entering && Spot->HideSpot->IsOccupied()
        && !Player->ThreatTarget->IsConcealed() && Player->ThreatTarget->IsEnteringConcealment());
    TestTrue(TEXT("Repeated entry and exit cannot interrupt entry"), !Hiding->Enter(Spot->HideSpot) && !Hiding->Exit());
    TestTrue(TEXT("Controller blocks input throughout entry"), PC->InAnimation);
    const FRotator LockedLook = PC->GetControlRotation();
    PC->Look(FInputActionValue(FVector2D(20, 20)));
    TestTrue(TEXT("Look command cannot override transition camera"), PC->GetControlRotation().Equals(LockedLook));
    Step(.5f);
    TestTrue(TEXT("Pawn moves between endpoints with original scale"), Player->GetActorLocation().X > 0 && Player->GetActorLocation().X < 150
        && Player->GetActorScale3D().Equals(Entry.GetScale3D()));
    TestTrue(TEXT("Door opens during movement"), Spot->HideSpot->GetDoorAmount() > .9f);
    const FVector PausedLocation = Player->GetActorLocation();
    World->GetWorldSettings()->SetPauserPlayerState(PC->PlayerState);
    Step(2);
    TestTrue(TEXT("Paused transition does not advance"), Hiding->GetState() == EHidingState::Entering && Player->GetActorLocation().Equals(PausedLocation));
    World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    FString Error;
    auto* EnterSave = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Save during entry"), Saves->CaptureWorld(EnterSave, Error));
    Step(.5f);
    TestTrue(TEXT("Entry completion grants concealment and stops ticking"), Hiding->IsFullyHidden() && Player->ThreatTarget->IsConcealed()
        && !Hiding->IsComponentTickEnabled() && FMath::IsNearlyZero(Spot->HideSpot->GetDoorAmount()));
    AActor* Obstacle = World->SpawnActor<AActor>();
    UBoxComponent* Blocker = NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Blocker);
    Blocker->SetBoxExtent(FVector(10, 100, 100));
    Blocker->SetCollisionProfileName(TEXT("BlockAll"));
    Blocker->RegisterComponent();
    Blocker->SetWorldLocation(Entry.GetLocation());
    TestTrue(TEXT("Blocked return rejects exit and preserves concealment"), !Hiding->Exit() && Hiding->IsFullyHidden() && Player->ThreatTarget->IsConcealed());
    Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Hidden state releases animation lock"), !PC->InAnimation);
    TestTrue(TEXT("Exit starts"), Hiding->Exit());
    TestTrue(TEXT("Exit locks controller again"), PC->InAnimation);
    TestTrue(TEXT("Protection ends immediately before exit movement"), Hiding->GetState() == EHidingState::Exiting && !Player->ThreatTarget->IsConcealed());
    Step(.5f);
    auto* ExitSave = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Save during exit"), Saves->CaptureWorld(ExitSave, Error));
    Step(.5f);
    TestTrue(TEXT("Exit returns pose and releases occupancy"), Hiding->GetState() == EHidingState::Outside && !Spot->HideSpot->IsOccupied()
        && Player->GetActorTransform().Equals(Entry, .01f) && PC->GetControlRotation().Equals(EntryLook, .01f)
        && Player->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Completed exit releases animation lock"), !PC->InAnimation);
    TestTrue(TEXT("Load entering snapshot"), Saves->RestoreWorld(EnterSave, Error));
    TestTrue(TEXT("Entering snapshot loads stable hidden"), Hiding->IsFullyHidden() && Player->ThreatTarget->IsConcealed()
        && !Hiding->IsComponentTickEnabled() && FMath::IsNearlyZero(Spot->HideSpot->GetDoorAmount()));
    TestTrue(TEXT("Load exiting snapshot while currently hidden"), Saves->RestoreWorld(ExitSave, Error));
    TestTrue(TEXT("Exiting snapshot loads stable outside"), Hiding->GetState() == EHidingState::Outside && !Spot->HideSpot->IsOccupied()
        && !Player->ThreatTarget->IsConcealed() && !Hiding->IsComponentTickEnabled()
        && Player->GetActorTransform().Equals(Entry, .01f) && PC->GetControlRotation().Equals(EntryLook, .01f));
    TestTrue(TEXT("Can enter after load"), Hiding->Enter(Spot->HideSpot));
    Step(.5f);
    PC->UnPossess();
    TestTrue(TEXT("Pawn loss cancels animation, occupancy and collision override"), !Hiding->IsHiding() && !Hiding->IsComponentTickEnabled()
        && !Spot->HideSpot->IsOccupied() && FMath::IsNearlyZero(Spot->HideSpot->GetDoorAmount())
        && Player->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
    PC->Possess(Player);
    Player->SetActorTransform(Entry);
    TestTrue(TEXT("Can enter before spot destruction"), Hiding->Enter(Spot->HideSpot));
    Step(.5f);
    Spot->Destroy();
    TestTrue(TEXT("Destroying the spot releases transition"), !Hiding->IsHiding() && !Hiding->IsComponentTickEnabled() && Player->GetActorTransform().Equals(Entry));
    return !HasAnyErrors();
}
#endif
