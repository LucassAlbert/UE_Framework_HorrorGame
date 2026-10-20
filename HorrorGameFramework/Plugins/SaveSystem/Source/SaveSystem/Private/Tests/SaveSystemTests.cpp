#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/SaveTestStateComponent.h"
#include "Components/SaveComponent.h"
#include "Registry/ActorRegistry.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorSaveRoundTripTest, "HorrorFramework.Save.ComponentRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorSaveRoundTripTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(FName(TEXT("HorrorSaveAutomation")));
    UWorld* World = GI->GetWorld();
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    if (!TestNotNull(TEXT("Save subsystem"), Save)) return false;

    AActor* Actor = World->SpawnActor<AActor>();
    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    USaveComponent* Marker = NewObject<USaveComponent>(Actor, TEXT("Save"));
    Marker->PersistentID = FGuid::NewGuid();
    Actor->AddInstanceComponent(Marker);
    Marker->RegisterComponent();
    USaveTestStateComponent* State = NewObject<USaveTestStateComponent>(Actor, TEXT("Puzzle"));
    Actor->AddInstanceComponent(State);
    State->RegisterComponent();
    State->PuzzleState = 7;
    State->RuntimeCounter = 11;
    const FVector SavedLocation(120.f, 80.f, 30.f);
    Actor->SetActorLocation(SavedLocation);
    Save->ActorRegistry->RegisterActor(Actor);
    Save->ActorRegistry->RegisterActor(Actor);
    TestEqual(TEXT("Registration is idempotent"), Save->ActorRegistry->GetRegisteredActors().Num(), 1);

    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture actor and component"), Save->CaptureWorld(Snapshot, Error));
    TestEqual(TEXT("One actor captured"), Snapshot->SavedActors.Num(), 1);
    TArray<uint8> Serialized;
    TestTrue(TEXT("Encode snapshot"), UGameplayStatics::SaveGameToMemory(Snapshot, Serialized));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Serialized));
    if (TestNotNull(TEXT("Decode snapshot"), Decoded))
    {
        Actor->SetActorLocation(FVector(900.f));
        State->PuzzleState = 99;
        State->RuntimeCounter = 42;
        TestTrue(TEXT("Restore snapshot"), Save->RestoreWorld(Decoded, Error));
        TestEqual(TEXT("Component SaveGame state restored"), State->PuzzleState, 7);
        TestEqual(TEXT("Transient state is not restored"), State->RuntimeCounter, 42);
        TestTrue(TEXT("Position restored"), Actor->GetActorLocation().Equals(SavedLocation));

        const FString OriginalLevel = Decoded->CurrentLevel;
        Decoded->CurrentLevel = TEXT("OtherMap");
        State->PuzzleState = 18;
        TestFalse(TEXT("Wrong map rejected before mutation"), Save->RestoreWorld(Decoded, Error));
        TestEqual(TEXT("Rejected load preserves puzzle state"), State->PuzzleState, 18);
        Decoded->CurrentLevel = OriginalLevel;

        State->PuzzleState = 0;
        USystemSaveGame* DefaultSnapshot = NewObject<USystemSaveGame>();
        TestTrue(TEXT("Capture state equal to class default"), Save->CaptureWorld(DefaultSnapshot, Error));
        State->PuzzleState = 23;
        TestTrue(TEXT("Restore default state"), Save->RestoreWorld(DefaultSnapshot, Error));
        TestEqual(TEXT("Default value overwrites current state"), State->PuzzleState, 0);

        Save->ActorRegistry->UnregisterActor(Actor);
        TestFalse(TEXT("Missing actor rejected"), Save->RestoreWorld(Decoded, Error));
        Save->ActorRegistry->RegisterActor(Actor);
    }

    AActor* Duplicate = World->SpawnActor<AActor>();
    USaveComponent* DuplicateMarker = NewObject<USaveComponent>(Duplicate, TEXT("Save"));
    DuplicateMarker->PersistentID = Marker->PersistentID;
    Duplicate->AddInstanceComponent(DuplicateMarker);
    DuplicateMarker->RegisterComponent();
    Save->ActorRegistry->RegisterActor(Duplicate);
    TestFalse(TEXT("Duplicate persistent IDs rejected"), Save->CaptureWorld(NewObject<USystemSaveGame>(), Error));
    Save->ActorRegistry->UnregisterActor(Duplicate);
    Duplicate->Destroy();

    for (int32 Index = 0; Index < USaveSubsystem::SlotCount; ++Index)
        TestEqual(TEXT("Slot names preserve the existing convention"), USaveSubsystem::GetSlotName(Index),
            FString::Printf(TEXT("SaveSlot_%d"), Index));
    TestFalse(TEXT("Negative slot rejected"), Save->SaveToSlot(-1));
    TestFalse(TEXT("Sixth slot rejected"), Save->LoadFromSlot(5));

    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
