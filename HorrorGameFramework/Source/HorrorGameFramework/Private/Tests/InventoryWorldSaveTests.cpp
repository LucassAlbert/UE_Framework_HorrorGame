#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Inventory/InventoryCharacter.h"
#include "Inventory/InventoryDroppedItem.h"
#include "Inventory/InventoryFlashlight.h"
#include "Inventory/InventoryKeyDoor.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InteractionScannerComponent.h"
#include "Subsystems/InteractionSubsystem.h"
#include "Interfaces/InteractionInterface.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorInventoryWorldSaveTest, "HorrorFramework.Inventory.WorldSavePhysics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorInventoryWorldSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(FName(TEXT("InventoryWorldSaveAutomation")));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    AInventoryCharacter* Player = World->SpawnActor<AInventoryCharacter>();
    Player->FindComponentByClass<USaveComponent>()->PersistentID = FGuid::NewGuid();
    Save->ActorRegistry->RegisterActor(Player);
    UInventoryItemDefinition* Item = NewObject<UInventoryItemDefinition>();
    Item->ItemId = TEXT("Flashlight");
    Item->MaxStackSize = 3;
    Item->bCanEquip = true;
    Item->EquipmentActorClass = AInventoryFlashlight::StaticClass();
    Item->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Item->WorldScale = 0.1f;
    Player->Inventory->AddItem(Item, 2);
    Player->Inventory->EquipSlot(0);

    AInventoryDroppedItem* Drop = World->SpawnActor<AInventoryDroppedItem>();
    Drop->Item = Item;
    Drop->SaveComponent->PersistentID = FGuid::NewGuid();
    Drop->SetActorLocation(FVector(100.f, 100.f, 100.f));
    Drop->RefreshPresentation();
    TestTrue(TEXT("Drop simulates physics"), Drop->Mesh->IsSimulatingPhysics());
    Drop->Mesh->SetPhysicsLinearVelocity(FVector(10.f, 20.f, -30.f));
    Drop->Mesh->SetPhysicsAngularVelocityInRadians(FVector(0.f, 0.f, 2.f));
    Save->ActorRegistry->RegisterActor(Drop);
    const FGuid DropId = Drop->SaveComponent->PersistentID;
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture inventory and physical drop"), Save->CaptureWorld(Snapshot, Error));
    const FActorSaveData* Record = Snapshot->SavedActors.FindByPredicate([DropId](const FActorSaveData& Data) { return Data.ActorID == DropId; });
    if (TestNotNull(TEXT("Drop recorded"), Record)) TestTrue(TEXT("Physics state is stored"), Record->bHasPhysicsState);
    TArray<uint8> Bytes;
    TestTrue(TEXT("Encode combined snapshot"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    Player->Inventory->RemoveFromSlot(0, 2);
    Save->ActorRegistry->UnregisterActor(Drop);
    Drop->Destroy();
    const bool bRestored = Save->RestoreWorld(Decoded, Error);
    TestTrue(FString(TEXT("Restore actual inventory and dropped item: ")) + Error, bRestored);
    TestEqual(TEXT("Inventory quantity rewound"), Player->Inventory->GetSlots()[0].Quantity, 2);
    TestEqual(TEXT("Equipment rewound"), Player->Inventory->GetEquippedSlot(), 0);
    AInventoryDroppedItem* Restored = nullptr;
    for (AActor* Actor : Save->ActorRegistry->GetRegisteredActors())
    {
        AInventoryDroppedItem* Candidate = Cast<AInventoryDroppedItem>(Actor);
        if (Candidate && Candidate->SaveComponent->PersistentID == DropId) Restored = Candidate;
    }
    if (TestNotNull(TEXT("Dropped item reconstructed"), Restored))
    {
        TestTrue(TEXT("Item definition restored"), Restored->Item.Get() == Item);
        TestEqual(TEXT("Drop quantity restored"), Restored->Quantity, 1);
        TestTrue(TEXT("Restored root simulates physics"), Restored->Mesh->IsSimulatingPhysics());
        TestTrue(TEXT("Linear velocity restored"), Restored->Mesh->GetPhysicsLinearVelocity().Equals(FVector(10.f, 20.f, -30.f), 0.01f));
        TestTrue(TEXT("Angular velocity restored"), Restored->Mesh->GetPhysicsAngularVelocityInRadians().Equals(FVector(0.f, 0.f, 2.f), 0.01f));
    }
    AInventoryKeyDoor* Door = World->SpawnActor<AInventoryKeyDoor>();
    Door->RequiredKey = TEXT("Key");
    TestFalse(TEXT("Wrong key cannot unlock"), Door->ApplyInventoryItem_Implementation(Item, Player));
    Item->ItemId = TEXT("Key");
    TestTrue(TEXT("Matching key unlocks"), Door->ApplyInventoryItem_Implementation(Item, Player));
    TestFalse(TEXT("Already unlocked door rejects repeated key use"), Door->ApplyInventoryItem_Implementation(Item, Player));
    Door->SaveComponent->PersistentID = FGuid::NewGuid();
    Save->ActorRegistry->RegisterActor(Door);
    USystemSaveGame* DoorSnapshot = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture unlocked but closed door"), Save->CaptureWorld(DoorSnapshot, Error));
    Door->Interact_Implementation(Player);
    TestTrue(TEXT("Door opens before rewind"), Door->bOpen);
    TestTrue(TEXT("Restore door state"), Save->RestoreWorld(DoorSnapshot, Error));
    TestFalse(TEXT("Door remains unlocked"), Door->bLocked);
    TestFalse(TEXT("Door returns to closed state"), Door->bOpen);
    TestTrue(TEXT("Door presentation follows restored state"), Door->Mesh->GetRelativeRotation().Equals(FRotator::ZeroRotator));
    AInventoryWorldItem* Pickup = World->SpawnActor<AInventoryWorldItem>();
    Pickup->Item = Item;
    Pickup->SaveComponent->PersistentID = FGuid::NewGuid();
    Pickup->RefreshPresentation();
    Save->ActorRegistry->RegisterActor(Pickup);
    UInteractionScannerComponent* Scanner = NewObject<UInteractionScannerComponent>(Player, TEXT("Scanner"));
    Player->AddInstanceComponent(Scanner);
    Scanner->RegisterComponent();
    TestTrue(TEXT("Scanner is active by default"), Scanner->IsActive());
    FVector Eyes;
    FRotator View;
    Player->GetActorEyesViewPoint(Eyes, View);
    Pickup->SetActorLocation(Eyes + View.Vector() * 100.f);
    TestTrue(TEXT("Native interface exposes ready pickup"), IInteractionInterface::Execute_CanInteract(Pickup));
    USystemSaveGame* PickupSnapshot = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture uncollected authored pickup"), Save->CaptureWorld(PickupSnapshot, Error));
    UInteractionSubsystem* Interaction = GI->GetSubsystem<UInteractionSubsystem>();
    Interaction->SetFocusedActor(Pickup);
    TestTrue(TEXT("Validated trace executes pickup through interface"), Interaction->TryInteract(Player));
    TestFalse(TEXT("Collected authored pickup is unavailable"), Pickup->CanInteract_Implementation());
    TestTrue(TEXT("Restore collection state"), Save->RestoreWorld(PickupSnapshot, Error));
    TestTrue(TEXT("Uncollected default rewinds availability"), Pickup->CanInteract_Implementation());
    TestFalse(TEXT("Restored pickup becomes visible"), Pickup->IsHidden());
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
