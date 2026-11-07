#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HorrorEnemyController.h"
#include "HorrorDoorNavigationComponent.h"
#include "HorrorDoorTraversalDefinition.h"
#include "HorrorAIDoor.h"
#include "Threat/FrameworkEnemy.h"
#include "Inventory/InventoryKeyDoor.h"
#include "Demo/IntegratedDemo.h"
#include "Components/SaveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEnemyDoorTest, "HorrorFramework.Threat.DoorContractAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEnemyDoorTest::RunTest(const FString& Parameters)
{
    auto* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("EnemyDoorAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    const FTransform EnemyPose(FRotator::ZeroRotator, FVector(-100, 100, 100));
    auto* Enemy = World->SpawnActorDeferred<AFrameworkEnemy>(AFrameworkEnemy::StaticClass(), EnemyPose);
    Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
    Enemy->Definition = NewObject<UHorrorEnemyDefinition>(Enemy);
    Enemy->Save->PersistentKey = TEXT("DoorTest.Enemy");
    Enemy->FinishSpawning(EnemyPose);
    auto* AI = World->SpawnActor<AHorrorEnemyController>();
    AI->Possess(Enemy);
    auto* Door = World->SpawnActor<AInventoryKeyDoor>(FVector(0, 0, 160), FRotator::ZeroRotator);
    Door->SaveComponent->PersistentKey = TEXT("DoorTest.Door");
    Door->SetActorScale3D(FVector(.2f, 6, 3.2f));
    auto* Definition = NewObject<UHorrorDoorTraversalDefinition>(Door);
    Definition->OpenDelaySeconds = .8f;
    Door->AINavigation->Definition = Definition;
    Door->AINavigation->StartOffset = FVector(-180, 100, -150);
    Door->AINavigation->EndOffset = FVector(180, 100, -150);
    auto* PC = World->SpawnActor<APlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    World->GetWorldSettings()->NotifyBeginPlay();
    USaveSubsystem* Saves = GI->GetSubsystem<USaveSubsystem>();
    Saves->ActorRegistry->RegisterActor(Door);
    Saves->ActorRegistry->RegisterActor(Enemy);
    TestTrue(TEXT("Default profile is valid"), Definition->Validate());
    Definition->OpenDelaySeconds = -1;
    TestFalse(TEXT("Negative opening delay is rejected"), Definition->Validate());
    Definition->OpenDelaySeconds = .8f;
    TestTrue(TEXT("Endpoints ignore nonuniform actor scale"), Door->AINavigation->GetStartPoint().Equals(FVector(-180,100,10), .01f)
        && Door->AINavigation->GetEndPoint().Equals(FVector(180,100,10), .01f));
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    TestFalse(TEXT("Locked door rejects pathfinding and opening"), AI->CanUseDoor(Door->AINavigation) || IHorrorAIDoor::Execute_OpenDoorForAI(Door, Enemy));
    TestFalse(TEXT("Missing agent cannot open a door"), IHorrorAIDoor::Execute_OpenDoorForAI(Door, nullptr));
    Door->bLocked = false;
    Door->AINavigation->RefreshDoorNavigation();
    TestTrue(TEXT("Unlocked door is traversable in chase"), AI->CanUseDoor(Door->AINavigation));
    TestTrue(TEXT("Start waiting at the door"), AI->BeginDoorTraversal(Door->AINavigation, AI->GetPathFollowingComponent(), FVector(180,100,10)));
    AI->UpdateDoorTraversal(.4f);
    TestTrue(TEXT("The first half of the delay keeps the door closed"), AI->IsWaitingForDoor() && !Door->bOpen);
    World->GetWorldSettings()->SetPauserPlayerState(PC->PlayerState);
    AI->UpdateBrain();
    TestTrue(TEXT("Paused brain preserves the pending opening"), AI->IsWaitingForDoor() && !Door->bOpen);
    World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    auto* Snapshot = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture the closed door and the waiting enemy"), Saves->CaptureWorld(Snapshot, Error));
    AI->UpdateDoorTraversal(.4f);
    TestTrue(TEXT("The configured delay opens without unlocking a lock"), Door->bOpen && !Door->bLocked && !AI->IsWaitingForDoor());
    TestTrue(TEXT("Repeated AI open stays open instead of toggling"), IHorrorAIDoor::Execute_OpenDoorForAI(Door, Enemy) && Door->bOpen);
    TestTrue(TEXT("Restore the snapshot from before opening"), Saves->RestoreWorld(Snapshot, Error));
    TestTrue(TEXT("Restore cancels the old pending operation"), !Door->bOpen && !AI->IsWaitingForDoor() && !AI->GetCurrentDoor());
    AI->UpdateDoorTraversal(2);
    TestFalse(TEXT("No delayed callback opens the restored door"), Door->bOpen);
    AI->ChangeEnemyState(EHorrorEnemyState::Chase);
    TestTrue(TEXT("Wait can start again after restoration"), AI->BeginDoorTraversal(Door->AINavigation, AI->GetPathFollowingComponent(), FVector(180,100,10)));
    Door->bLocked = true;
    Door->AINavigation->RefreshDoorNavigation();
    AI->UpdateDoorTraversal(2);
    TestTrue(TEXT("Locking during the wait cancels without opening"), !Door->bOpen && !AI->IsWaitingForDoor() && !AI->GetCurrentDoor());
    Door->bLocked = false;
    Door->AINavigation->RefreshDoorNavigation();
    TestTrue(TEXT("Wait can restart before an external state change"), AI->BeginDoorTraversal(Door->AINavigation, AI->GetPathFollowingComponent(), FVector(180,100,10)));
    AI->ChangeEnemyState(EHorrorEnemyState::Disabled);
    AI->UpdateDoorTraversal(2);
    TestTrue(TEXT("Disabling the enemy cancels the pending opening"), !Door->bOpen && !AI->GetCurrentDoor());
    auto* Exit = World->SpawnActor<AIntegratedDemoExit>(FVector(500,0,100), FRotator::ZeroRotator);
    Exit->AINavigation->Definition = Definition;
    Exit->bLocked = false;
    Exit->AINavigation->RefreshDoorNavigation();
    TestFalse(TEXT("Demo completion cannot be activated as an AI door"), IHorrorAIDoor::Execute_OpenDoorForAI(Exit, Enemy));
    return !HasAnyErrors();
}
#endif
