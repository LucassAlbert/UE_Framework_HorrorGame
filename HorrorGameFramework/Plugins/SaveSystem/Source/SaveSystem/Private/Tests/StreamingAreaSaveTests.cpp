#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/SaveTestStateComponent.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/SaveSlotCatalog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorSaveAreaCacheTest, "HorrorFramework.Save.StreamingAreaCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHorrorSaveAreaCacheTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("StreamingAreaCacheAutomation"));
    UWorld* World = GI->GetWorld();
    ON_SCOPE_EXIT { GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    auto Spawn = [World, Save](FName Area, int32 Value)
    {
        ASaveTestRuntimeActor* Actor = World->SpawnActor<ASaveTestRuntimeActor>();
        Actor->Marker->PersistentID = FGuid::NewGuid();
        Actor->Marker->StreamingAreaId = Area;
        Actor->State->PuzzleState = Value;
        Save->ActorRegistry->RegisterActor(Actor);
        return Actor;
    };
    ASaveTestRuntimeActor* A = Spawn(TEXT("A"), 42);
    ASaveTestRuntimeActor* B = Spawn(TEXT("B"), 13);
    const FGuid Id = A->Marker->PersistentID;
    FString Error;
    TestTrue(TEXT("Cache the area before eviction"), Save->CacheStreamingArea(TEXT("A"), Error));
    Save->EvictStreamingRuntimeActors(TEXT("A"));
    TestTrue(TEXT("Eviction destroys runtime resident"), A->IsActorBeingDestroyed());
    TestFalse(TEXT("Streaming eviction is not permanent destruction"), Save->DestroyedActors.Contains(Id));
    TestFalse(TEXT("Eviction leaves other areas intact"), B->IsActorBeingDestroyed());
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture includes an unloaded area cache"), Save->CaptureWorld(Snapshot, Error));
    TestEqual(TEXT("Both resident and evicted records persist"), Snapshot->SavedActors.Num(), 2);
    bool bCompleted = false;
    TestTrue(TEXT("Begin cached area reconstruction"), Save->RestoreStreamingArea(TEXT("A"), FSaveAreaRestored::CreateLambda(
        [this, &bCompleted](bool bSuccess, FText Message) { TestTrue(TEXT("Cached restore succeeds"), bSuccess); bCompleted = true; })));
    TestTrue(TEXT("Loaded native dependencies complete immediately"), bCompleted);
    ASaveTestRuntimeActor* Restored = nullptr;
    for (AActor* Actor : Save->ActorRegistry->GetRegisteredActors())
        if (ASaveTestRuntimeActor* Candidate = Cast<ASaveTestRuntimeActor>(Actor); Candidate && Candidate->Marker->PersistentID == Id) Restored = Candidate;
    if (TestNotNull(TEXT("Same GUID is reconstructed"), Restored))
    {
        TestEqual(TEXT("Unloaded puzzle state survives"), Restored->State->PuzzleState, 42);
        TestTrue(TEXT("Area ownership survives"), Restored->Marker->StreamingAreaId == TEXT("A"));
        Save->RecordDestroyedActor(Id);
        TestFalse(TEXT("Permanent removal also clears stale cache"), Save->CachedAreaActors.Contains(Id));
    }
    TestEqual(TEXT("Restoring one area preserves another"), B->State->PuzzleState, 13);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorSaveCatalogTest, "HorrorFramework.Save.SlotMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorSaveCatalogTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Missing slot is not readable"), USaveSlotCatalog::Describe(0, false, nullptr).bReadable);
    TestFalse(TEXT("Unreadable existing file is not offered for loading"), USaveSlotCatalog::Describe(1, true, nullptr).bReadable);
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    Snapshot->CurrentLevel = TEXT("AnotherMap");
    Snapshot->SavedAt = FDateTime(2026, 10, 20, 10, 0);
    for (int32 Version = 1; Version <= 3; ++Version)
    {
        Snapshot->FormatVersion = Version;
        const FSaveSlotInfo Info = USaveSlotCatalog::Describe(4, true, Snapshot);
        TestTrue(TEXT("Legacy and current metadata supported"), Info.bReadable && Info.SavedAt == Snapshot->SavedAt && Info.MapName == Snapshot->CurrentLevel);
    }
    Snapshot->FormatVersion = 99;
    TestFalse(TEXT("Future format rejected"), USaveSlotCatalog::Describe(4, true, Snapshot).bReadable);
    return !HasAnyErrors();
}
#endif
