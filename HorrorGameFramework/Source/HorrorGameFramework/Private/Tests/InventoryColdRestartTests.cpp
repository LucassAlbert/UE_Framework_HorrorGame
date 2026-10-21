#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "Inventory/InventoryCharacter.h"
#include "Inventory/InventoryDroppedItem.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
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

namespace InventoryColdTest
{
    const TCHAR* DefinitionPath = TEXT("/Game/_Project/Inventory/Items/DA_DemoFlashlight.DA_DemoFlashlight");
    struct FState
    {
        TStrongObjectPtr<UGameInstance> Instance;
        TWeakObjectPtr<AInventoryCharacter> Player;
        FAutomationTestBase* Test = nullptr;
        FString Slot;
        double StartedAt = FPlatformTime::Seconds();
        void Cleanup()
        {
            UWorld* World = Instance->GetWorld();
            Instance->Shutdown();
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            Instance.Reset();
        }
    };

    class FWaitForLoad : public IAutomationLatentCommand
    {
    public:
        explicit FWaitForLoad(TSharedRef<FState> InState) : State(InState) {}
        virtual bool Update() override
        {
            USaveSubsystem* Save = State->Instance->GetSubsystem<USaveSubsystem>();
            if (Save->IsBusy() && FPlatformTime::Seconds() - State->StartedAt < 15.0) return false;
            State->Test->TestFalse(TEXT("Async load completed before timeout"), Save->IsBusy());
            State->Test->TestNotNull(TEXT("Cold snapshot accepted"), Save->CurrentSaveGame.Get());
            AInventoryCharacter* Player = State->Player.Get();
            State->Test->TestEqual(TEXT("Cold inventory quantity restored"), Player->Inventory->GetSlots()[0].Quantity, 1);
            State->Test->TestEqual(TEXT("Cold equipment restored"), Player->Inventory->GetEquippedSlot(), 0);
            int32 Drops = 0;
            for (AActor* Actor : Save->ActorRegistry->GetRegisteredActors())
            {
                AInventoryDroppedItem* Drop = Cast<AInventoryDroppedItem>(Actor);
                if (!Drop) continue;
                ++Drops;
                State->Test->TestEqual(TEXT("Cold drop item identity"), Drop->Item.ToSoftObjectPath().ToString(), FString(DefinitionPath));
                State->Test->TestTrue(TEXT("Cold drop physics rebuilt"), Drop->Mesh->IsSimulatingPhysics());
                State->Test->TestTrue(TEXT("Cold drop velocity restored"), Drop->Mesh->GetPhysicsLinearVelocity().Equals(FVector(5.f, 8.f, -12.f), 0.01f));
            }
            State->Test->TestEqual(TEXT("Exactly one missing drop recreated"), Drops, 1);
            State->Test->TestTrue(TEXT("Remove only the isolated test slot"), UGameplayStatics::DeleteGameInSlot(State->Slot, 0));
            State->Cleanup();
            return true;
        }
    private:
        TSharedRef<FState> State;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorInventoryColdRestartTest, "HorrorFramework.Inventory.ColdRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorInventoryColdRestartTest::RunTest(const FString& Parameters)
{
    FString Slot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("HorrorInventoryColdSlot="), Slot))
    {
        AddInfo(TEXT("Cold restart requires two separate processes with HorrorInventoryColdSlot=HorrorFramework_Automation_<GUID>; write phase also uses HorrorInventoryColdWrite."));
        return true;
    }
    FGuid Suffix;
    if (!Slot.RemoveFromStart(TEXT("HorrorFramework_Automation_")) || !FGuid::Parse(Slot, Suffix))
    {
        AddError(TEXT("Cold test refuses a non-isolated slot name."));
        return false;
    }
    Slot = TEXT("HorrorFramework_Automation_") + Slot;
    TSharedRef<InventoryColdTest::FState> State = MakeShared<InventoryColdTest::FState>();
    State->Test = this;
    State->Slot = Slot;
    State->Instance.Reset(NewObject<UGameInstance>(GEngine));
    State->Instance->InitializeStandalone(FName(TEXT("InventoryColdRestoreAutomation")));
    UWorld* World = State->Instance->GetWorld();
    World->InitializeActorsForPlay(FURL());
    USaveSubsystem* Save = State->Instance->GetSubsystem<USaveSubsystem>();
    AInventoryCharacter* Player = World->SpawnActor<AInventoryCharacter>();
    Player->DispatchBeginPlay();
    State->Player = Player;
    Save->ActorRegistry->RegisterActor(Player);

    if (FParse::Param(FCommandLine::Get(), TEXT("HorrorInventoryColdWrite")))
    {
        if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        { AddError(TEXT("Test slot already exists; refusing overwrite.")); State->Cleanup(); return false; }
        UInventoryItemDefinition* Definition = LoadObject<UInventoryItemDefinition>(nullptr, InventoryColdTest::DefinitionPath);
        if (!TestNotNull(TEXT("Demo definition available"), Definition)) { State->Cleanup(); return false; }
        Definition->Mesh.LoadSynchronous();
        Definition->EquipmentActorClass.LoadSynchronous();
        Player->Inventory->AddItem(Definition, 1);
        Player->Inventory->EquipSlot(0);
        AInventoryDroppedItem* Drop = World->SpawnActor<AInventoryDroppedItem>();
        Drop->Item = Definition;
        Drop->SaveComponent->PersistentID = FGuid::NewGuid();
        Drop->SetActorLocation(FVector(150.f, 100.f, 200.f));
        Drop->DispatchBeginPlay();
        Drop->Mesh->SetPhysicsLinearVelocity(FVector(5.f, 8.f, -12.f));
        Save->ActorRegistry->RegisterActor(Drop);
        USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
        FString Error;
        const bool bCaptured = Save->CaptureWorld(Snapshot, Error);
        TestTrue(FString(TEXT("Cold snapshot capture: ")) + Error, bCaptured);
        if (bCaptured) TestTrue(TEXT("Write cold restart snapshot"), UGameplayStatics::SaveGameToSlot(Snapshot, Slot, 0));
        State->Cleanup();
        return true;
    }

    TestNull(TEXT("Item definition starts unloaded in fresh process"), FSoftObjectPath(InventoryColdTest::DefinitionPath).ResolveObject());
    USaveGame* Snapshot = UGameplayStatics::LoadGameFromSlot(Slot, 0);
    if (!TestNotNull(TEXT("Previous process snapshot readable"), Snapshot)) { State->Cleanup(); return false; }
    Save->OperationWorld = World;
    Save->ActiveSlot = 0;
    Save->bBusy = Save->bLoadingSlot = true;
    Save->OnSlotLoaded(Slot, 0, Snapshot, ++Save->LoadGeneration);
    AddCommand(new InventoryColdTest::FWaitForLoad(State));
    return true;
}
#endif
