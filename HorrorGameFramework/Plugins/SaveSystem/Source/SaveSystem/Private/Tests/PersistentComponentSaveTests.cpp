#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/SaveTestStateComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorSaveComponentIdentityTest,"HorrorFramework.Save.PersistentComponentIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHorrorSaveComponentIdentityTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI=NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("PersistentComponentIdentity"));
    UWorld* World=GI->GetWorld();
    ON_SCOPE_EXIT { GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World); };
    USaveSubsystem* Saves=GI->GetSubsystem<USaveSubsystem>();
    ASaveTestRuntimeActor* Actor=World->SpawnActor<ASaveTestRuntimeActor>();
    Actor->Marker->PersistentID=FGuid::NewGuid();
    Actor->State->PuzzleState=31;
    Saves->ActorRegistry->RegisterActor(Actor);
    USystemSaveGame* Snapshot=NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture persistent component state"),Saves->CaptureWorld(Snapshot,Error));
    if (!TestEqual(TEXT("Unmarked scene/input components are excluded"),Snapshot->SavedActors[0].Components.Num(),1)) return false;
    FComponentSaveData EmptyLegacy;
    EmptyLegacy.ComponentName=TEXT("GeneratedTransientInput_OldSession");
    {
        FMemoryWriter Writer(EmptyLegacy.ByteData);
        FObjectAndNameAsStringProxyArchive Archive(Writer,false);
        Archive.ArIsSaveGame=true;Archive.ArNoDelta=true;
        UActorComponent::StaticClass()->GetDefaultObject()->Serialize(Archive);
    }
    Snapshot->SavedActors[0].Components.Add(EmptyLegacy);
    Actor->State->PuzzleState=99;
    TestTrue(TEXT("Missing empty component in an old snapshot does not block travel"),Saves->RestoreWorld(Snapshot,Error));
    TestEqual(TEXT("Actual persistent data still restores"),Actor->State->PuzzleState,31);
    Snapshot->SavedActors[0].Components[0].ComponentName=TEXT("MissingPersistentPuzzle");
    Actor->State->PuzzleState=42;
    TestFalse(TEXT("Missing persistent component remains an explicit failure"),Saves->RestoreWorld(Snapshot,Error));
    TestTrue(TEXT("Failure identifies the missing component"),Error.Contains(TEXT("MissingPersistentPuzzle")));
    TestEqual(TEXT("Rejected state does not modify the existing participant"),Actor->State->PuzzleState,42);
    return !HasAnyErrors();
}
#endif
