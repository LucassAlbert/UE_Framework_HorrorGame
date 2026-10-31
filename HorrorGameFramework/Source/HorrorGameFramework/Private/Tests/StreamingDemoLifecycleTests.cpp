#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Engine/LevelStreaming.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/SaveSlotCatalog.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UIManagerSubsystem.h"
#include "HorrorWorldSubsystem.h"
#include "Streaming/SaveMenuScreens.h"
#include "Streaming/FrameworkSavePoint.h"
#include "Streaming/StreamingEnemy.h"
#include "Documents/DocumentPickup.h"
#include "Inventory/InventoryKeyDoor.h"
#include "Inventory/InventoryWorldItem.h"
#include "Inventory/InventoryDroppedItem.h"
#include "InventoryItemDefinition.h"
#include "HorrorEnemyController.h"
#include "Interfaces/SaveStateListener.h"

DEFINE_LOG_CATEGORY_STATIC(LogStreamingValidation, Log, All);

namespace StreamingDemoTest
{
    const FName Root(TEXT("/Game/Maps/FrameworkStreaming_Demo"));
    UWorld* GameWorld()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->IsGameWorld()) return Context.World();
        return nullptr;
    }
    AActor* Find(USaveSubsystem* Saves, const TCHAR* Key)
    {
        for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
            if (USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>(); Marker && Marker->PersistentKey == Key) return Actor;
        return nullptr;
    }
    template<class T> T* ById(USaveSubsystem* Saves, const FGuid& Id)
    {
        for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
            if (T* Typed = Cast<T>(Actor))
                if (USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>(); Marker && Marker->PersistentID == Id) return Typed;
        return nullptr;
    }
    bool IsUnloaded(UWorld* World, const TCHAR* Package)
    {
        for (ULevelStreaming* Level : World->GetStreamingLevels())
            if (Level->GetWorldAssetPackageName() == Package) return !Level->GetLoadedLevel() && !Level->HasLoadRequestPending();
        return false;
    }
    class FLifecycle : public IAutomationLatentCommand
    {
    public:
        explicit FLifecycle(FAutomationTestBase* InTest, bool bColdRead = false) : Test(InTest), Phase(bColdRead ? 15 : 0), Deadline(FPlatformTime::Seconds() + 90.0) {}
        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Now > Deadline) { Test->AddError(FString::Printf(TEXT("Streaming demo timed out in phase %d"), Phase)); return true; }
            UWorld* World = GameWorld();
            UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Player = PC ? PC->GetPawn() : nullptr;
            if (!GI || !Player) return false;
            USaveSubsystem* Saves = GI->GetSubsystem<USaveSubsystem>();
            UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>();
            UGameFlowSubsystem* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
            UHorrorWorldSubsystem* Streaming = World->GetSubsystem<UHorrorWorldSubsystem>();
            const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
            const bool bReady = !Saves->IsBusy() && Saves->IsWorldReady() && Flow->CurrentState == EGameFlowState::Playing;
            auto Next = [this, Now]() { ++Phase; Deadline = Now + 90.0; };
            auto Assert = [this](bool bValue, const TCHAR* Message) { Test->TestTrue(Message, bValue); return bValue; };
            switch (Phase)
            {
            case 0:
                if (Map != TEXT("FrameworkStreaming_Demo") || !bReady) return false;
                for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
                    if (AStreamingEnemy* Enemy = Cast<AStreamingEnemy>(Actor))
                    {
                        EnemyId = Enemy->Save->PersistentID;
                        Enemy->Memory.State = EHorrorEnemyState::Disabled;
                        CastChecked<AHorrorEnemyController>(Enemy->GetController())->StopMovement();
                    }
                if (!Assert(EnemyId.IsValid(), TEXT("Demo spawns a stable persistent enemy"))) return true;
                Player->SetActorLocation(FVector(-2400,0,100));
                Next(); return false;
            case 1:
            {
                if (!bReady || !Streaming->IsAreaReady(TEXT("A"))) return false;
                ADocumentPickup* Doc = Cast<ADocumentPickup>(Find(Saves, TEXT("StreamingDemo.Document.A")));
                AInventoryKeyDoor* Door = Cast<AInventoryKeyDoor>(Find(Saves, TEXT("StreamingDemo.Door.A")));
                AInventoryWorldItem* Key = Cast<AInventoryWorldItem>(Find(Saves, TEXT("StreamingDemo.Key.A")));
                if (!Doc || !Door || !Key || !Key->Item.Get() || !Key->Item.Get()->Mesh.Get()) return false;
                Doc->SetCollected(true); Door->bLocked = false; Door->bOpen = true; Door->OnActorLoaded_Implementation();
                AInventoryDroppedItem* Drop = World->SpawnActorDeferred<AInventoryDroppedItem>(AInventoryDroppedItem::StaticClass(), FTransform(FVector(-2400,100,140)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                DropId = FGuid::NewGuid(); Drop->SaveComponent->PersistentID = DropId; Drop->Item = Key->Item;
                Drop->FinishSpawning(FTransform(FVector(-2400,100,140)));
                Player->SetActorLocation(FVector(3000,0,100));
                Next(); return false;
            }
            case 2:
            {
                if (!bReady || !IsUnloaded(World, TEXT("/Game/Maps/Streaming/FrameworkArea_A"))) return false;
                Assert(!Find(Saves, TEXT("StreamingDemo.Document.A")), TEXT("Distant area removes its placed participants"));
                Assert(!ById<AStreamingEnemy>(Saves, EnemyId), TEXT("Inactive distant enemy is evicted"));
                AFrameworkSavePoint* Point = nullptr;
                for (TActorIterator<AFrameworkSavePoint> It(World); It; ++It) if (It->GetActorLocation().X > 1200) Point = *It;
                if (!Assert(Point != nullptr, TEXT("Loaded area provides a manual save point"))) return true;
                Player->SetActorLocation(Point->GetActorLocation() + FVector(0,100,30));
                Point->Interact_Implementation(Player);
                UFrameworkSaveSlotsScreen* Screen = Cast<UFrameworkSaveSlotsScreen>(UI->GetTopScreen());
                if (!Assert(Screen != nullptr, TEXT("Save point opens the five-slot screen"))) return true;
                Screen->SelectSlot(0);
                Next(); return false;
            }
            case 3:
                if (Saves->IsBusy() || GI->GetSubsystem<USaveSlotCatalog>()->IsReading()) return false;
                if (!Assert(Saves->CurrentSaveGame && Saves->CurrentSaveGame->FormatVersion == 3, TEXT("Manual screen writes a version-three snapshot"))) return true;
                Assert(GI->GetSubsystem<USaveSlotCatalog>()->GetEntries().Num() == 5, TEXT("Catalog supplies all five slots"));
                if (UFrameworkSaveSlotsScreen* Screen = Cast<UFrameworkSaveSlotsScreen>(UI->GetTopScreen()))
                {
                    Screen->SelectSlot(0);
                    Assert(!Saves->IsBusy(), TEXT("Occupied slot waits for overwrite confirmation"));
                    Screen->ProcessEvent(Screen->FindFunctionChecked(TEXT("Back")), nullptr);
                    Assert(UI->GetTopScreen() == Screen, TEXT("Back cancels overwrite and keeps slot selection open"));
                }
                UI->PopScreen();
                Player->SetActorLocation(FVector(-2400,0,100));
                Next(); return false;
            case 4:
            {
                if (!bReady || !Streaming->IsAreaReady(TEXT("A"))) return false;
                ADocumentPickup* Doc = Cast<ADocumentPickup>(Find(Saves, TEXT("StreamingDemo.Document.A")));
                AInventoryKeyDoor* Door = Cast<AInventoryKeyDoor>(Find(Saves, TEXT("StreamingDemo.Door.A")));
                if (!Assert(Doc && Doc->bCollected && Door && Door->bOpen && !Door->bLocked, TEXT("Returning restores document and door state"))) return true;
                Assert(ById<AInventoryDroppedItem>(Saves, DropId) != nullptr, TEXT("Returning reconstructs the physics drop with its GUID"));
                Assert(ById<AStreamingEnemy>(Saves, EnemyId) != nullptr, TEXT("Returning reconstructs the enemy without changing identity"));
                if (!Assert(Saves->LoadFromSlot(0), TEXT("Begin rewind from a save with area A unloaded"))) return true;
                Next(); return false;
            }
            case 5:
                if (!bReady) return false;
                Assert(Player->GetActorLocation().X > 1200, TEXT("Rewind restores the saved player area"));
                Assert(IsUnloaded(World, TEXT("/Game/Maps/Streaming/FrameworkArea_A")), TEXT("Rewind keeps the distant saved area unloaded"));
                Assert(!UI->HasModalScreen() && !World->IsPaused(), TEXT("Loading returns to gameplay and releases its pause"));
                Player->SetActorLocation(FVector(-2400,0,100));
                Next(); return false;
            case 6:
            {
                if (!bReady || !Streaming->IsAreaReady(TEXT("A"))) return false;
                ADocumentPickup* Doc = Cast<ADocumentPickup>(Find(Saves,TEXT("StreamingDemo.Document.A")));
                if (!Assert(Doc && Doc->bCollected && ById<AInventoryDroppedItem>(Saves,DropId), TEXT("Disk snapshot preserves cached area state and runtime drops"))) return true;
                Player->SetActorLocation(FVector(0,0,100));
                Next(); return false;
            }
            case 7:
            {
                if (!bReady || !Streaming->IsAreaReady(TEXT("B"))) return false;
                AStreamingEnemy* Enemy = ById<AStreamingEnemy>(Saves, EnemyId);
                if (!Assert(Enemy != nullptr, TEXT("Enemy survives proximity preload"))) return true;
                UE_LOG(LogStreamingValidation, Display, TEXT("Pursuit setup: player=%s enemy=%s home=%s persistent=%d"),
                    *Player->GetActorLocation().ToString(), *Enemy->GetActorLocation().ToString(),
                    *Enemy->Residency->HomeAreaId.ToString(), Enemy->GetLevel() == World->PersistentLevel);
                OriginalSpeed = Enemy->Definition->ChaseSpeed;
                Enemy->Definition->ChaseSpeed = 200.f;
                Enemy->SetActorLocation(FVector(-1000,0,100)); Enemy->SetActorRotation(FRotator::ZeroRotator);
                Enemy->Memory.State = EHorrorEnemyState::Patrol; Enemy->ResumeAfterRestore();
                Next(); return false;
            }
            case 8:
            {
                AStreamingEnemy* Enemy = ById<AStreamingEnemy>(Saves,EnemyId);
                if (!Enemy || Enemy->Memory.State != EHorrorEnemyState::Chase) return false;
                LastMovement = Now; Next(); return false;
            }
            case 9:
            {
                AStreamingEnemy* Enemy = ById<AStreamingEnemy>(Saves, EnemyId);
                if (!Assert(Enemy != nullptr, TEXT("Active pursuer remains resident"))) return true;
                if (Now - LastTrace > 1.0)
                {
                    LastTrace = Now;
                    UE_LOG(LogStreamingValidation, Display, TEXT("Pursuit: player=%s enemy=%s state=%d area=%s home=%s dt=%f"),
                        *Player->GetActorLocation().ToString(), *Enemy->GetActorLocation().ToString(), static_cast<int32>(Enemy->Memory.State),
                        *Enemy->Save->StreamingAreaId.ToString(), *Enemy->Residency->HomeAreaId.ToString(), World->GetDeltaSeconds());
                }
                const double Elapsed = FMath::Min(Now - LastMovement, .1); LastMovement = Now;
                FVector Position = Player->GetActorLocation(); Position.X = FMath::Min(2400.0, Position.X + Elapsed * 180.0);
                Player->SetActorLocation(Position);
                if (Enemy->GetActorLocation().X < 1250) return false;
                Assert(Enemy->Memory.State == EHorrorEnemyState::Chase, TEXT("Pursuer crosses the corridor into area B while chasing"));
                Assert(Streaming->IsAreaReady(TEXT("A")), TEXT("Active pursuit retains its connected source area"));
                Enemy->Definition->ChaseSpeed = OriginalSpeed;
                Enemy->Memory.State = EHorrorEnemyState::Disabled;
                CastChecked<AHorrorEnemyController>(Enemy->GetController())->StopMovement();
                Flow->LoadLevel(TEXT("/Game/Maps/FrameworkThreat_Demo"));
                Next(); return false;
            }
            case 10:
                if (Map != TEXT("FrameworkThreat_Demo") || !bReady) return false;
                // Soft definitions of the old demo initialize independently from its UI boot.
                for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
                {
                    FString Error;
                    if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Actor))
                        if (!Listener->ValidateSaveState(Error)) return false;
                    TInlineComponentArray<UActorComponent*> Components(Actor);
                    for (UActorComponent* Component : Components)
                        if (const ISaveStateListener* Listener = Cast<ISaveStateListener>(Component))
                            if (!Listener->ValidateSaveState(Error)) return false;
                }
                if (!Assert(Saves->SaveToSlot(4), TEXT("Save another persistent map in slot five"))) return true;
                Next(); return false;
            case 11:
                if (Saves->IsBusy()) return false;
                if (!Assert(Saves->LoadFromSlot(0), TEXT("Load the streaming snapshot from another map"))) return true;
                Next(); return false;
            case 12:
                if (Map != TEXT("FrameworkStreaming_Demo") || !bReady) return false;
                Assert(Player->GetActorLocation().X > 1200, TEXT("Cross-map restore recovers streaming area and player"));
                UI->OpenMainMenuUI();
                Next(); return false;
            case 13:
                if (GI->GetSubsystem<USaveSlotCatalog>()->IsReading()) return false;
                if (!Assert(GI->GetSubsystem<USaveSlotCatalog>()->GetLatestSlot() == 4, TEXT("Catalog selects newest slot across maps"))) return true;
                if (UUIScreenBase* Screen = UI->GetTopScreen())
                {
                    UFunction* Continue = Screen->FindFunction(TEXT("ContinueGame"));
                    if (!Assert(Continue != nullptr, TEXT("Configured menu exposes Continue"))) return true;
                    Screen->ProcessEvent(Continue, nullptr);
                }
                Next(); return false;
            case 14:
                if (Map != TEXT("FrameworkThreat_Demo") || !bReady) return false;
                Assert(!World->IsPaused() && !UI->HasModalScreen(), TEXT("Continue returns to gameplay in the newest saved map"));
                Flow->LoadLevel(Root);
                Phase = 18; Deadline = Now + 90.0;
                return false;
            case 18:
                if (Map != TEXT("FrameworkStreaming_Demo") || !bReady) return false;
                Flow->SetGameState(EGameFlowState::GameOver);
                UI->ClearScreens();
                if (UUIScreenBase* Screen = UI->OpenScreen(EHorrorScreen::GameOver))
                    Screen->ProcessEvent(Screen->FindFunctionChecked(TEXT("LoadLatest")), nullptr);
                else { Test->AddError(TEXT("Configured Game Over screen is unavailable")); return true; }
                Next(); return false;
            case 19:
                if (Map != TEXT("FrameworkThreat_Demo") || !bReady) return false;
                Assert(!World->IsPaused() && !UI->HasModalScreen(), TEXT("Game Over loads the newest save across maps and releases control"));
                Test->AddInfo(TEXT("Completed streaming, cache/disk restore, pursuit, five-slot selection, overwrite cancellation, cross-map Continue and Game Over using isolated saves."));
                return true;
            case 15:
                if (Map != TEXT("FrameworkStreaming_Demo") || !bReady) return false;
                if (!Assert(Saves->LoadFromSlot(0), TEXT("Fresh process begins reading the previous process snapshot"))) return true;
                Next(); return false;
            case 16:
                if (!bReady) return false;
                if (!Assert(Saves->CurrentSaveGame && Player->GetActorLocation().X > 1200, TEXT("Fresh process restores the saved player in area B"))) return true;
                Assert(IsUnloaded(World, TEXT("/Game/Maps/Streaming/FrameworkArea_A")), TEXT("Fresh process keeps the cached distant area unloaded"));
                for (const FActorSaveData& Data : Saves->CurrentSaveGame->SavedActors)
                    if (Data.StreamingAreaId == TEXT("A") && Data.RuntimeClass.ToString().Contains(TEXT("InventoryDroppedItem"))) DropId = Data.ActorID;
                if (!Assert(DropId.IsValid(), TEXT("File contains the cached physics drop"))) return true;
                Player->SetActorLocation(FVector(-2400,0,100));
                Next(); return false;
            case 17:
            {
                if (!bReady || !Streaming->IsAreaReady(TEXT("A"))) return false;
                ADocumentPickup* Doc = Cast<ADocumentPickup>(Find(Saves,TEXT("StreamingDemo.Document.A")));
                AInventoryKeyDoor* Door = Cast<AInventoryKeyDoor>(Find(Saves,TEXT("StreamingDemo.Door.A")));
                AInventoryDroppedItem* Drop = ById<AInventoryDroppedItem>(Saves,DropId);
                Assert(Doc && Doc->bCollected && Door && Door->bOpen && !Door->bLocked,
                    TEXT("Cold restart restores the unloaded document and door states"));
                Assert(Drop && Drop->Mesh->IsSimulatingPhysics(), TEXT("Cold restart reconstructs the cached drop and its physics"));
                Assert(!World->IsPaused() && !UI->HasModalScreen(), TEXT("Cold restart returns control to gameplay"));
                return true;
            }
            }
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Phase = 0;
        double Deadline;
        double LastMovement = 0;
        double LastTrace = 0;
        float OriginalSpeed = 0;
        FGuid EnemyId;
        FGuid DropId;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorStreamingDemoLifecycleTest,"HorrorFramework.Streaming.DemoLifecycle",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorStreamingDemoLifecycleTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("StreamingDemoAutomation")))
    { AddInfo(TEXT("Integration protocol: run with StreamingDemoAutomation and a fresh isolated HorrorStage7 UserDir.")); return true; }
    FString UserDir;
    FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir);
    if (!FParse::Param(FCommandLine::Get(),TEXT("StreamingDemoAutomation")) || !UserDir.Contains(TEXT("HorrorStage7")))
    { AddError(TEXT("This integration test requires -StreamingDemoAutomation and an isolated -UserDir containing HorrorStage7.")); return false; }
    if (UWorld* World=StreamingDemoTest::GameWorld())
        for (int32 Index=0;Index<USaveSubsystem::SlotCount;++Index)
            if (UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Index),0))
            { AddError(TEXT("Use a fresh isolated user directory; existing saves will not be overwritten."));return false; }
    AddCommand(new StreamingDemoTest::FLifecycle(this));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorStreamingColdRestoreTest,"HorrorFramework.Streaming.ColdRestore",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorStreamingColdRestoreTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("StreamingDemoColdRead")))
    { AddInfo(TEXT("Cold-restore protocol: run in a fresh process using the isolated UserDir of DemoLifecycle.")); return true; }
    FString UserDir;
    FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir);
    if (!UserDir.Contains(TEXT("HorrorStage7")) || !UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(0),0))
    { AddError(TEXT("Cold restore requires a previous isolated HorrorStage7 snapshot in slot zero.")); return false; }
    AddCommand(new StreamingDemoTest::FLifecycle(this,true));
    return true;
}
#endif
