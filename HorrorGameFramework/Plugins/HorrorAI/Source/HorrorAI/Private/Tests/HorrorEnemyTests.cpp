#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HorrorEnemyDefinition.h"
#include "HorrorEnemyCharacter.h"
#include "HorrorEnemyController.h"
#include "HorrorAITargetComponent.h"
#include "HorrorNoiseDefinition.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEnemyBehaviorTest, "HorrorFramework.Threat.PerceptionAndMemory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorEnemyBehaviorTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("EnemyBehaviorAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    UHorrorEnemyDefinition* Definition = NewObject<UHorrorEnemyDefinition>(GI);
    FString Error;
    TestTrue(TEXT("Default enemy definition is valid"), Definition->Validate(Error));
    Definition->LoseSightRadius = 1.f;
    TestFalse(TEXT("Invalid sight hysteresis is rejected"), Definition->Validate(Error));
    Definition->LoseSightRadius = 1700.f;
    const FTransform EnemyTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 100.f));
    AHorrorEnemyCharacter* Enemy = World->SpawnActorDeferred<AHorrorEnemyCharacter>(AHorrorEnemyCharacter::StaticClass(), EnemyTransform);
    Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
    Enemy->Definition = Definition;
    Enemy->FinishSpawning(EnemyTransform);
    AHorrorEnemyController* AI = World->SpawnActor<AHorrorEnemyController>();
    AI->Possess(Enemy);
    if (UAIPerceptionSystem::GetCurrent(World))
        TestTrue(TEXT("Configured senses register a valid perception listener"), AI->Senses->GetListenerId().IsValid());
    APawn* Player = World->SpawnActor<APawn>(FVector(500.f, 0.f, 100.f), FRotator::ZeroRotator);
    USceneComponent* PlayerRoot = NewObject<USceneComponent>(Player);
    Player->SetRootComponent(PlayerRoot);
    PlayerRoot->RegisterComponent();
    Player->SetActorLocation(FVector(500.f, 0.f, 100.f));
    APlayerController* PC = World->SpawnActor<APlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    PC->Possess(Player);
    UHorrorAITargetComponent* Target = NewObject<UHorrorAITargetComponent>(Player);
    Target->RegisterComponent();
    Target->Activate(true);
    TestTrue(TEXT("A possessed player is an available target"), Target->IsAvailable());
    TestTrue(TEXT("Target ahead can be seen"), AI->HasSightAt(Player, Player->GetActorLocation()));
    TestFalse(TEXT("Target behind the enemy cannot be seen"), AI->HasSightAt(Player, FVector(-500.f, 0.f, 100.f)));
    AActor* Wall = World->SpawnActor<AActor>(FVector(250.f, 0.f, 100.f), FRotator::ZeroRotator);
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20.f, 200.f, 200.f));
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent();
    Box->SetWorldLocation(FVector(250.f, 0.f, 100.f));
    TestFalse(TEXT("Wall blocks sight and capture validation"), AI->HasSightAt(Player, Player->GetActorLocation()));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AI->SetTarget(Player);
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    AActor* HideAnchor = World->SpawnActor<AActor>(FVector(500.f, 0.f, 100.f), FRotator::ZeroRotator);
    const FName HideKey(TEXT("StableHideSpot"));
    // This fixture deliberately has no NavMesh; movement is exercised on the demonstration map.
    AddExpectedMessage(TEXT("Cannot navigate"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
    Target->SetConcealed(true, HideAnchor, Player->GetActorLocation(), HideKey);
    TestTrue(TEXT("Witnessed entry is remembered"), AI->WitnessedAnchor.Get() == HideAnchor && Enemy->Memory.WitnessedConcealmentKey == HideKey);
    Target->SetConcealed(false, nullptr, Player->GetActorLocation());
    TestFalse(TEXT("Leaving clears the witnessed anchor"), AI->WitnessedAnchor.IsValid());
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Target->SetConcealed(true, HideAnchor, Player->GetActorLocation(), HideKey);
    TestFalse(TEXT("An entry behind a wall is not witnessed"), AI->WitnessedAnchor.IsValid());
    TestTrue(TEXT("Unseen concealment leads to investigation"), Enemy->Memory.State == EHorrorEnemyState::Investigate);
    AI->ChangeEnemyState(EHorrorEnemyState::Search);
    Enemy->Memory.RemainingTime = 0.f;
    AI->UpdateBrain();
    TestTrue(TEXT("Expired search returns to patrol"), Enemy->Memory.State == EHorrorEnemyState::Patrol);
    UHorrorNoiseDefinition* Noise = NewObject<UHorrorNoiseDefinition>(GI);
    TestTrue(TEXT("Valid gameplay noise definition"), Noise->Validate());
    Noise->Range = -1.f;
    TestFalse(TEXT("Negative noise range rejected"), Noise->Validate());
    Enemy->Memory.LastKnownLocation = FVector(99.f, 42.f, 100.f);
    Enemy->Memory.bHasLastKnownLocation = true;
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    AI->ResumeAfterRestore();
    TestTrue(TEXT("Restoration does not invent a live target"), Enemy->Memory.State == EHorrorEnemyState::Investigate && !AI->TrackedTarget.IsValid());
    TestTrue(TEXT("Last known location survives restoration"), Enemy->Memory.LastKnownLocation.Equals(FVector(99.f, 42.f, 100.f)));
    Target->SetConcealed(false, nullptr, Player->GetActorLocation());
    Player->SetActorLocation(FVector(100.f, 0.f, 100.f));
    Box->SetWorldLocation(FVector(50.f, 0.f, 100.f));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    TestFalse(TEXT("Capture cannot cross a wall even inside capture range"), AI->CaptureIfClose(Target, Player->GetActorLocation(), nullptr));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Close visible target can be captured"), AI->CaptureIfClose(Target, Player->GetActorLocation(), nullptr));
    TestFalse(TEXT("Capture is emitted only once"), AI->CaptureIfClose(Target, Player->GetActorLocation(), nullptr));
    AI->UnPossess();
    TestFalse(TEXT("Unpossession stops the brain"), AI->bReady);
    return !HasAnyErrors();
}
#endif
