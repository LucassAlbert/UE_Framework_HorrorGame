#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Inventory/InventoryCharacter.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "Documents/DocumentGameplayComponent.h"
#include "Documents/DocumentPickup.h"
#include "DocumentDefinition.h"
#include "DocumentCollectionComponent.h"
#include "InventoryComponent.h"
#include "ObjectiveDefinition.h"
#include "ObjectiveJournalComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDocumentWorldSaveTest, "HorrorFramework.Documents.WorldSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDocumentWorldSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("DocumentWorldSaveAutomation"));
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
    UDocumentDefinition* Document = NewObject<UDocumentDefinition>();
    Document->DocumentId = TEXT("PowerMemo");
    Document->Title = FText::FromString(TEXT("Power memo"));
    FDocumentPage Page;
    Page.Text = FText::FromString(TEXT("The power door requires two fuses."));
    Document->Pages = { Page, Page };
    Document->CollectionFact = TEXT("Document.Memo.Collected");
    Document->ReadFact = TEXT("Document.Memo.Read");
    UObjectiveDefinition* Find = NewObject<UObjectiveDefinition>();
    Find->ObjectiveId = TEXT("FindMemo");
    Find->Title = FText::FromString(TEXT("Find memo"));
    Find->CompletionFacts = { Document->CollectionFact };
    UObjectiveDefinition* Read = NewObject<UObjectiveDefinition>();
    Read->ObjectiveId = TEXT("ReadMemo");
    Read->Title = FText::FromString(TEXT("Read memo"));
    Read->ActivationFacts = Find->CompletionFacts;
    Read->CompletionFacts = { Document->ReadFact };
    Player->Objectives->Definitions = { Find, Read };
    ADocumentPickup* Pickup = World->SpawnActor<ADocumentPickup>();
    Pickup->Document = Document;
    Pickup->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Pickup->Mesh->SetRelativeScale3D(FVector(0.2f));
    Pickup->SaveComponent->PersistentKey = TEXT("TestDocument");
    World->GetWorldSettings()->NotifyBeginPlay();
    PC->PlayerCameraManager->UpdateCamera(0.f);
    FVector Eyes;
    FRotator View;
    PC->GetPlayerViewPoint(Eyes, View);
    Pickup->SetActorLocation(Eyes + View.Vector() * 120.f);
    Save->ActorRegistry->RegisterActor(Player);
    Save->ActorRegistry->RegisterActor(Pickup);
    TestTrue(TEXT("Documents button has an optional listener"), Player->InventoryGameplay->OnDocumentsRequested.IsBound());
    FString Error;
    USystemSaveGame* Empty = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture uncollected document"), Save->CaptureWorld(Empty, Error));
    Pickup->SetActorLocation(Eyes + View.Vector() * 1000.f);
    TestFalse(TEXT("Direct call still validates pickup distance"), Player->DocumentGameplay->CollectDocument(Pickup));
    Pickup->SetActorLocation(Eyes + View.Vector() * 120.f);
    TestTrue(TEXT("Valid interaction collects document"), Player->DocumentGameplay->CollectDocument(Pickup));
    TestTrue(TEXT("Pickup is hidden and unavailable"), Pickup->IsHidden() && !Pickup->CanInteract_Implementation());
    TestTrue(TEXT("Collection owns document"), Player->Documents->Contains(Document));
    TestFalse(TEXT("Headless collection without reader stays unread"), Player->Documents->IsRead(Document));
    TestTrue(TEXT("Acquisition updates objectives"), Player->Objectives->HasFact(Document->CollectionFact));
    for (const FInventorySlot& Slot : Player->Inventory->GetSlots()) TestTrue(TEXT("Documents do not occupy inventory slots"), Slot.IsEmpty());
    USystemSaveGame* Unread = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture collected unread document"), Save->CaptureWorld(Unread, Error));
    TestTrue(TEXT("Opening marks document read"), Player->Documents->MarkRead(Document));
    TestTrue(TEXT("Reading updates objectives"), Player->Objectives->HasFact(Document->ReadFact));
    TestTrue(TEXT("Read objective completed"), Player->Objectives->GetRecords()[1].State == EObjectiveState::Completed);
    USystemSaveGame* Complete = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture read document"), Save->CaptureWorld(Complete, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Serialize complete document snapshot"), UGameplayStatics::SaveGameToMemory(Complete, Bytes));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    TestTrue(TEXT("Restore empty collection"), Save->RestoreWorld(Empty, Error));
    TestTrue(TEXT("Pickup visible and interactable after rewind"), !Pickup->IsHidden() && Pickup->CanInteract_Implementation());
    TestFalse(TEXT("Collection rewound"), Player->Documents->Contains(Document));
    TestFalse(TEXT("Reading fact rewound"), Player->Objectives->HasFact(Document->ReadFact));
    TestTrue(TEXT("Restore acquired unread state"), Save->RestoreWorld(Unread, Error));
    TestTrue(TEXT("Pickup hidden again"), Pickup->bCollected && Pickup->IsHidden());
    TestTrue(TEXT("Ownership restored"), Player->Documents->Contains(Document));
    TestFalse(TEXT("Unread restored"), Player->Documents->IsRead(Document));
    // Changing notification facts makes any accidental replay during load observable.
    Document->CollectionFact = TEXT("UnexpectedReplay.Collected");
    Document->ReadFact = TEXT("UnexpectedReplay.Read");
    TestTrue(TEXT("Restore serialized read state"), Save->RestoreWorld(Decoded, Error));
    TestTrue(TEXT("Read flag restored"), Player->Documents->IsRead(Document));
    TestFalse(TEXT("Acquisition events are not replayed"), Player->Objectives->HasFact(Document->CollectionFact));
    TestFalse(TEXT("Reading events are not replayed"), Player->Objectives->HasFact(Document->ReadFact));
    World->EndPlay(EEndPlayReason::Quit);
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
