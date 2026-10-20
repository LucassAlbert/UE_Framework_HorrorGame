#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryRecipe.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorInventoryTransactionsTest, "HorrorFramework.Inventory.Transactions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorInventoryTransactionsTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(FName(TEXT("InventoryTransactionsAutomation")));
    UWorld* World = GI->GetWorld();
    AActor* Owner = World->SpawnActor<AActor>();
    UInventoryComponent* Inventory = NewObject<UInventoryComponent>(Owner);
    Inventory->RegisterComponent();
    UInventoryItemDefinition* Item = NewObject<UInventoryItemDefinition>();
    Item->ItemId = TEXT("Stackable");
    Item->MaxStackSize = 3;
    Item->bCanEquip = true;
    TestTrue(TEXT("Add across stacks"), Inventory->AddItem(Item, 7).bSuccess);
    TestEqual(TEXT("First stack is full"), Inventory->GetSlots()[0].Quantity, 3);
    TestEqual(TEXT("Third stack has remainder"), Inventory->GetSlots()[2].Quantity, 1);
    const FGuid FirstId = Inventory->GetSlots()[0].StackId;
    TestFalse(TEXT("Overflow rejects entire collection"), Inventory->AddItem(Item, 9).bSuccess);
    TestEqual(TEXT("No partial add on failure"), Inventory->GetSlots()[2].Quantity, 1);
    TestTrue(TEXT("Stack identity is stable"), Inventory->GetSlots()[0].StackId == FirstId);
    TestTrue(TEXT("Equip a specific stack"), Inventory->EquipSlot(0).bSuccess);
    TestTrue(TEXT("Remove one unit"), Inventory->RemoveFromSlot(0, 1).bSuccess);
    TestEqual(TEXT("Equipment survives partial removal"), Inventory->GetEquippedSlot(), 0);

    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Inventory->Serialize(Archive);
    }
    Inventory->RemoveFromSlot(0, 2);
    TestEqual(TEXT("Empty stack clears equipment"), Inventory->GetEquippedSlot(), INDEX_NONE);
    {
        FMemoryReader Reader(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        Archive.ArIsSaveGame = true;
        Inventory->Serialize(Archive);
    }
    TestTrue(TEXT("Restored inventory validates"), Inventory->NotifyStateRestored().bSuccess);
    TestEqual(TEXT("Equipment GUID restored"), Inventory->GetEquippedSlot(), 0);
    TestEqual(TEXT("Quantity restored"), Inventory->GetSlots()[0].Quantity, 2);
    TestFalse(TEXT("Discard defaults disabled"), Inventory->DiscardSlot(0).bSuccess);
    TestFalse(TEXT("Invalid index rejects"), Inventory->RemoveFromSlot(-1).bSuccess);
    TestFalse(TEXT("Negative add rejects"), Inventory->AddItem(Item, -1).bSuccess);

    UInventoryRecipe* Recipe = NewObject<UInventoryRecipe>();
    UInventoryItemDefinition* Result = NewObject<UInventoryItemDefinition>();
    Result->ItemId = TEXT("Result");
    Recipe->FirstIngredient = Item;
    Recipe->SecondIngredient = Item;
    Recipe->ResultItem = Result;
    TestTrue(TEXT("Same stack can supply two ingredients"), Inventory->CombineSlots(0, 0, Recipe).bSuccess);
    TestTrue(TEXT("Result uses newly freed slot"), Inventory->GetItemAt(0) == Result);
    TestEqual(TEXT("Consumed equipped stack clears equipment"), Inventory->GetEquippedSlot(), INDEX_NONE);

    UInventoryComponent* Full = NewObject<UInventoryComponent>(Owner);
    Full->SlotCapacity = 1;
    Full->RegisterComponent();
    Full->AddItem(Item, 3);
    TestFalse(TEXT("Combination needs space for result"), Full->CombineSlots(0, 0, Recipe).bSuccess);
    TestEqual(TEXT("Failed recipe preserves both ingredients"), Full->GetSlots()[0].Quantity, 3);
    Full->RemoveFromSlot(0, 2);
    TestFalse(TEXT("One unit cannot supply two ingredients"), Full->CombineSlots(0, 0, Recipe).bSuccess);
    TestEqual(TEXT("Insufficient ingredients preserved"), Full->GetSlots()[0].Quantity, 1);
    Inventory->UnregisterComponent();
    Full->UnregisterComponent();
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
