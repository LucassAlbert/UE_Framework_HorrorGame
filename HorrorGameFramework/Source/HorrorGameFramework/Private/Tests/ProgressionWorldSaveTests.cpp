#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Inventory/InventoryCharacter.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "Progression/ProgressionArea.h"
#include "Progression/PuzzleDoor.h"
#include "Progression/PuzzleSocket.h"
#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"
#include "ObjectiveDefinition.h"
#include "ObjectiveJournalComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Templates/UnrealTemplate.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorProgressionWorldSaveTest, "HorrorFramework.Progression.WorldSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorProgressionWorldSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(FName(TEXT("ProgressionWorldSaveAutomation")));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    AInventoryCharacter* Player = World->SpawnActor<AInventoryCharacter>();
    Player->SetActorLocation(FVector(0.f, 0.f, 100.f));
    APlayerController* PC = World->SpawnActor<APlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    PC->Possess(Player);
    PC->SetViewTarget(Player);
    PC->SetControlRotation(FRotator::ZeroRotator);

    UObjectiveDefinition* First = NewObject<UObjectiveDefinition>();
    First->ObjectiveId = TEXT("RestorePower");
    First->Title = FText::FromString(TEXT("Restore power"));
    First->CompletionFacts = { FName(TEXT("Puzzle.Complete")) };
    UObjectiveDefinition* Second = NewObject<UObjectiveDefinition>();
    Second->ObjectiveId = TEXT("ExploreArea");
    Second->Title = FText::FromString(TEXT("Explore area"));
    Second->ActivationFacts = First->CompletionFacts;
    Second->CompletionFacts = { FName(TEXT("Area.Entered")) };
    Player->Objectives->Definitions = { First, Second };
    TestTrue(TEXT("Fact can arrive before definitions finish loading"), Player->Objectives->RecordFact(TEXT("Early.Fact")));
    Player->DispatchBeginPlay();
    TestTrue(TEXT("Journal initialized"), Player->Objectives->IsReady());
    TestTrue(TEXT("Early fact preserved"), Player->Objectives->HasFact(TEXT("Early.Fact")));
    TestFalse(TEXT("Repeated fact rejected"), Player->Objectives->RecordFact(TEXT("Early.Fact")));
    TestTrue(TEXT("Inventory has objective screen listener"), Player->InventoryGameplay->OnObjectivesRequested.IsBound());

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UInventoryItemDefinition* Left = NewObject<UInventoryItemDefinition>();
    Left->ItemId = TEXT("LeftFuse");
    Left->Mesh = Cube;
    Left->WorldScale = 0.1f;
    Left->MaxStackSize = 2;
    UInventoryItemDefinition* Right = NewObject<UInventoryItemDefinition>();
    Right->ItemId = TEXT("RightFuse");
    Right->Mesh = Cube;
    Right->WorldScale = 0.1f;
    TestTrue(TEXT("Add stacked first ingredient"), Player->Inventory->AddItem(Left, 2).bSuccess);
    TestTrue(TEXT("Add second ingredient"), Player->Inventory->AddItem(Right, 1).bSuccess);

    UItemPuzzleDefinition* Data = NewObject<UItemPuzzleDefinition>();
    Data->PuzzleId = TEXT("PowerDoor");
    Data->CompletionFact = TEXT("Puzzle.Complete");
    FItemPuzzleSlot LeftSlot;
    LeftSlot.SlotId = TEXT("Left");
    LeftSlot.RequiredItemId = Left->ItemId;
    FItemPuzzleSlot RightSlot;
    RightSlot.SlotId = TEXT("Right");
    RightSlot.RequiredItemId = Right->ItemId;
    Data->Slots = { LeftSlot, RightSlot };
    APuzzleDoor* Door = World->SpawnActor<APuzzleDoor>();
    Door->SetActorLocation(FVector(600.f, 0.f, 100.f));
    Door->Puzzle->Definition = Data;
    Door->SaveComponent->PersistentKey = TEXT("TestPuzzleDoor");
    Door->DispatchBeginPlay();
    APuzzleSocket* SocketA = World->SpawnActor<APuzzleSocket>();
    APuzzleSocket* SocketB = World->SpawnActor<APuzzleSocket>();
    auto ConfigureSocket = [Door, Cube](APuzzleSocket* Socket, FName SlotId, FName Key)
    {
        Socket->Door = Door;
        Socket->SlotId = SlotId;
        Socket->Frame->SetStaticMesh(Cube);
        Socket->Frame->SetRelativeScale3D(FVector(0.2f));
        Socket->SaveComponent->PersistentKey = Key;
        Socket->DispatchBeginPlay();
    };
    ConfigureSocket(SocketA, TEXT("Left"), TEXT("TestSocketA"));
    ConfigureSocket(SocketB, TEXT("Right"), TEXT("TestSocketB"));
    SocketB->SetActorLocation(FVector(250.f, 200.f, 100.f));
    FVector Eyes;
    FRotator View;
    PC->PlayerCameraManager->UpdateCamera(0.f);
    PC->GetPlayerViewPoint(Eyes, View);
    SocketA->SetActorLocation(Eyes + View.Vector() * 140.f);
    Player->ProgressionGameplay->SelectedSocket = SocketA;
    TestFalse(TEXT("Wrong ingredient rejected"), Player->ProgressionGameplay->InsertItem(1).bSuccess);
    TestEqual(TEXT("Wrong ingredient preserved"), Player->Inventory->GetSlots()[1].Quantity, 1);
    TestTrue(TEXT("Correct ingredient accepted"), Player->ProgressionGameplay->InsertItem(0).bSuccess);
    TestEqual(TEXT("Only one stacked unit consumed"), Player->Inventory->GetSlots()[0].Quantity, 1);
    TestTrue(TEXT("Inserted visual displayed"), SocketA->InsertedMesh->IsVisible());
    TestTrue(TEXT("Partial puzzle keeps door locked"), Door->bLocked);
    FText Detail;
    TestFalse(TEXT("Filled socket cannot be reused"), Door->Puzzle->BeginInsertion(TEXT("Left"), Left->ItemId, Detail));
    TestTrue(TEXT("Prepare other ingredient"), Door->Puzzle->BeginInsertion(TEXT("Right"), Right->ItemId, Detail));
    TestFalse(TEXT("Pending transaction cannot be saved"), Door->Puzzle->ValidateState(Detail));
    Door->Puzzle->CancelInsertion();
    TestFalse(TEXT("Cancelled insertion stays empty"), Door->Puzzle->IsSlotFilled(TEXT("Right")));

    AProgressionArea* Area = World->SpawnActor<AProgressionArea>();
    Area->EntryFact = TEXT("Area.Entered");
    Area->SetActorLocation(FVector(1000.f, 0.f, 100.f));
    Area->SaveComponent->PersistentKey = TEXT("TestProgressionArea");
    Area->DispatchBeginPlay();
    // Physics overlap notifications require a world that has entered play, not only initialized actors.
    World->GetWorldSettings()->NotifyBeginPlay();
    {
        TGuardValue<bool> Restoring(Save->bRestoring, true);
        Player->SetActorLocation(Area->GetActorLocation());
        TestFalse(TEXT("Restore-time overlaps cannot visit areas"), Area->bVisited);
        TestFalse(TEXT("Restore-time overlaps cannot record facts"), Player->Objectives->HasFact(Area->EntryFact));
    }
    Player->SetActorLocation(FVector(0.f, 0.f, 100.f));
    for (AActor* Actor : TArray<AActor*>{ Player, Door, SocketA, SocketB, Area }) Save->ActorRegistry->RegisterActor(Actor);
    USystemSaveGame* Partial = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(FString(TEXT("Capture partial progression: ")) + Error, Save->CaptureWorld(Partial, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Serialize progression snapshot"), UGameplayStatics::SaveGameToMemory(Partial, Bytes));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    SocketA->SetActorLocation(FVector(250.f, -200.f, 100.f));
    SocketB->SetActorLocation(Eyes + View.Vector() * 140.f);
    Player->ProgressionGameplay->SelectedSocket = SocketB;
    TestTrue(TEXT("Final ingredient accepted"), Player->ProgressionGameplay->InsertItem(1).bSuccess);
    TestTrue(TEXT("Puzzle solved"), Door->Puzzle->IsSolved());
    TestFalse(TEXT("Door unlocked"), Door->bLocked);
    TestFalse(TEXT("Door does not open automatically"), Door->bOpen);
    TestTrue(TEXT("Puzzle objective completed"), Player->Objectives->GetRecords()[0].State == EObjectiveState::Completed);
    TestTrue(TEXT("Next objective activated"), Player->Objectives->GetRecords()[1].State == EObjectiveState::Active);
    Door->Interact_Implementation(Player);
    TestTrue(TEXT("Explicit interaction opens door"), Door->bOpen);
    Player->SetActorLocation(Area->GetActorLocation());
    TestTrue(TEXT("World emits gameplay overlaps"), World->HasBegunPlay());
    TestTrue(TEXT("Character capsule generates overlap events"), Player->GetCapsuleComponent()->GetGenerateOverlapEvents());
    TestTrue(TEXT("Area overlaps the player capsule"), Area->Volume->IsOverlappingComponent(Player->GetCapsuleComponent()));
    TestTrue(TEXT("Area entrant is player controlled"), Player->IsPlayerControlled());
    TestTrue(TEXT("Entering area records fact"), Player->Objectives->HasFact(Area->EntryFact));
    TestTrue(TEXT("Area objective completed"), Player->Objectives->GetRecords()[1].State == EObjectiveState::Completed);
    TestTrue(FString(TEXT("Restore partial progression: ")) + Error, Save->RestoreWorld(Decoded, Error));
    TestTrue(TEXT("Restored puzzle partial"), Door->Puzzle->IsSlotFilled(TEXT("Left")) && !Door->Puzzle->IsSlotFilled(TEXT("Right")));
    TestTrue(TEXT("Restored door locked and closed"), Door->bLocked && !Door->bOpen);
    TestEqual(TEXT("Restored consumed quantity"), Player->Inventory->GetSlots()[0].Quantity, 1);
    TestEqual(TEXT("Restored unused ingredient"), Player->Inventory->GetSlots()[1].Quantity, 1);
    TestTrue(TEXT("Restored socket visuals"), SocketA->InsertedMesh->IsVisible() && !SocketB->InsertedMesh->IsVisible());
    TestFalse(TEXT("Area visit rewound"), Area->bVisited);
    TestFalse(TEXT("Completion fact rewound"), Player->Objectives->HasFact(Data->CompletionFact));
    TestTrue(TEXT("Second objective inactive after restore"), Player->Objectives->GetRecords()[1].State == EObjectiveState::Inactive);
    World->EndPlay(EEndPlayReason::Quit);
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
