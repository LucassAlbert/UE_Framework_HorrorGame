#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/SaveTestStateComponent.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorRuntimeActorSaveTest, "HorrorFramework.Save.RuntimeActorReconstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorRuntimeActorSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(FName(TEXT("RuntimeActorSaveAutomation")));
    UWorld* World = GI->GetWorld();
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    ASaveTestRuntimeActor* Original = World->SpawnActor<ASaveTestRuntimeActor>();
    const FGuid ID = FGuid::NewGuid();
    Original->Marker->PersistentID = ID;
    Original->State->PuzzleState = 31;
    const FVector Location(40.f, 50.f, 60.f);
    Original->SetActorLocation(Location);
    Save->ActorRegistry->RegisterActor(Original);
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture runtime class"), Save->CaptureWorld(Snapshot, Error));
    TestTrue(TEXT("Class recorded only with opt-in"), Snapshot->SavedActors[0].RuntimeClass.Get() == Original->GetClass());

    // Exercise disk encoding with an isolated GUID name, never a player save slot.
    const FString TestSlot = TEXT("HorrorFramework_Automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TestTrue(TEXT("Write isolated test snapshot"), UGameplayStatics::SaveGameToSlot(Snapshot, TestSlot, 0));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSlot, 0));
    TestTrue(TEXT("Delete only the generated test snapshot"), UGameplayStatics::DeleteGameInSlot(TestSlot, 0));
    if (!TestNotNull(TEXT("Read snapshot from disk"), Decoded)) return false;
    Save->ActorRegistry->UnregisterActor(Original);
    Original->Destroy();
    ASaveTestRuntimeActor* Extra = World->SpawnActor<ASaveTestRuntimeActor>();
    Extra->Marker->PersistentID = FGuid::NewGuid();
    Save->ActorRegistry->RegisterActor(Extra);
    TestTrue(TEXT("Restore reconstructs a missing runtime actor"), Save->RestoreWorld(Decoded, Error));
    ASaveTestRuntimeActor* Restored = nullptr;
    for (AActor* Actor : Save->ActorRegistry->GetRegisteredActors())
    {
        ASaveTestRuntimeActor* Candidate = Cast<ASaveTestRuntimeActor>(Actor);
        if (Candidate && Candidate->Marker->PersistentID == ID) Restored = Candidate;
    }
    if (TestNotNull(TEXT("Actor with original stable GUID exists"), Restored))
    {
        TestEqual(TEXT("Runtime component restored"), Restored->State->PuzzleState, 31);
        TestTrue(TEXT("Runtime position restored"), Restored->GetActorLocation().Equals(Location));
        Restored->State->PuzzleState = 99;
        TestTrue(TEXT("Repeated restore works"), Save->RestoreWorld(Decoded, Error));
        TestEqual(TEXT("Repeated restore rewinds state"), Restored->State->PuzzleState, 31);
        const FActorSaveData DuplicateRecord = Decoded->SavedActors[0];
        Decoded->SavedActors.Add(DuplicateRecord);
        Restored->State->PuzzleState = 42;
        TestFalse(TEXT("Duplicate snapshot ID rejected"), Save->RestoreWorld(Decoded, Error));
        TestEqual(TEXT("Rejected snapshot does not modify participants"), Restored->State->PuzzleState, 42);
    }
    TestTrue(TEXT("Runtime actor created after snapshot removed"), Extra->IsActorBeingDestroyed());
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
