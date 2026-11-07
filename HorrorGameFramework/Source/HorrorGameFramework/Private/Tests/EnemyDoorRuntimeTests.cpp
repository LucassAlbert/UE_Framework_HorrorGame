#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "Progression/PuzzleDoor.h"
#include "Progression/PuzzleSocket.h"
#include "Inventory/InventoryWorldItem.h"
#include "InventoryItemDefinition.h"
#include "Streaming/StreamingEnemy.h"
#include "HorrorEnemyController.h"
#include "HorrorDoorNavigationComponent.h"
#include "HorrorDoorTraversalDefinition.h"
#include "HorrorAIDoor.h"
#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"
#include "Components/SaveComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogDoorTraversalValidation, Log, All);
namespace EnemyDoorRuntime
{
    class FProtocol : public IAutomationLatentCommand
    {
    public:
        FProtocol(FAutomationTestBase* InTest, bool bInCold) : Test(InTest), bCold(bInCold), Deadline(FPlatformTime::Seconds()+120) {}
        bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Now > Deadline) { Test->AddError(FString::Printf(TEXT("Door traversal timed out at phase %d"), Phase)); return true; }
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.World() && Context.World()->IsGameWorld()) { World=Context.World(); break; }
            auto* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Player = PC ? PC->GetPawn() : nullptr;
            if (!Player || !World->GetGameInstance()) return false;
            auto* GI = World->GetGameInstance();
            auto* Saves = GI->GetSubsystem<USaveSubsystem>();
            auto* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
            if (Saves->IsBusy() || !Saves->IsWorldReady()) return false;
            APuzzleDoor* Door = nullptr;
            AStreamingEnemy* Enemy = nullptr;
            for (TActorIterator<APuzzleDoor> It(World); It; ++It)
                if (It->SaveComponent->PersistentKey==TEXT("Integrated.CorridorPuzzleDoor")) { Door=*It; break; }
            for (TActorIterator<AStreamingEnemy> It(World); It; ++It) { Enemy=*It; break; }
            if (!Door || !Enemy || !Door->Puzzle->IsReady()) return false;
            auto* AI = Cast<AHorrorEnemyController>(Enemy->GetController());
            if (!AI || !Door->AINavigation->HasValidConfiguration()) return false;
            auto Check = [&](bool Value, const TCHAR* Message) { return Test->TestTrue(Message,Value); };
            auto Next = [&]() { ++Phase; Deadline=Now+45; };
            auto Disable = [&]() { Enemy->Memory.State=EHorrorEnemyState::Disabled; Enemy->ResumeAfterRestore(); };
            auto Capture = [&](const TCHAR* Name)
            {
                if (FParse::Param(FCommandLine::Get(),TEXT("DoorCaptureRender")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);
            };
            if (Now-LastTrace>2)
            {
                LastTrace=Now;
                UE_LOG(LogDoorTraversalValidation,Display,TEXT("phase=%d enemy=%s state=%d waiting=%d doorOpen=%d locked=%d"),
                    Phase,*Enemy->GetActorLocation().ToString(),int32(Enemy->Memory.State),AI->IsWaitingForDoor(),Door->bOpen,Door->bLocked);
            }
            if (Flow->CurrentState==EGameFlowState::GameOver) { Test->AddError(TEXT("Door fixture was captured before its checkpoint")); return true; }
            if (Phase==0)
            {
                if (Flow->CurrentState!=EGameFlowState::Playing || UNavigationSystemV1::IsNavigationBeingBuilt(World)) return false;
                Disable();
                if (bCold)
                {
                    if (!Check(Saves->LoadFromSlot(1),TEXT("New process loads the snapshot of a closed door and waiting enemy"))) return true;
                    Phase=100; return false;
                }
                Check(Door->bLocked && !Door->Puzzle->IsSolved(),TEXT("Authored puzzle initially blocks the enemy"));
                if (!Check(Saves->SaveToSlot(0),TEXT("Persist original locked puzzle"))) return true;
                Next(); return false;
            }
            if (Phase==1)
            {
                FText Error;
                for (const FItemPuzzleSlot& Slot : Door->Puzzle->GetDefinition()->Slots)
                {
                    if (!Check(Door->Puzzle->BeginInsertion(Slot.SlotId,Slot.RequiredItemId,Error),TEXT("Fill the authored puzzle slot"))) return true;
                    APuzzleSocket* Socket=nullptr;
                    UInventoryItemDefinition* Item=nullptr;
                    for (TActorIterator<APuzzleSocket> It(World);It;++It)
                        if (It->Door.Get()==Door && It->SlotId==Slot.SlotId) {Socket=*It;break;}
                    for (TActorIterator<AInventoryWorldItem> It(World);It;++It)
                    {
                        auto* Candidate=It->Item.LoadSynchronous();
                        if (Candidate && Candidate->ItemId==Slot.RequiredItemId) {Item=Candidate;break;}
                    }
                    if (!Check(Socket && Item,TEXT("Puzzle fixture includes the matching socket item"))) return true;
                    Socket->SetInsertedItem(Item);
                    Door->Puzzle->CommitInsertion();
                }
                Check(!Door->bLocked && !Door->bOpen,TEXT("Puzzle unlocks without automatically opening"));
                Door->Interact_Implementation(Player);
                Player->SetActorLocation(FVector(650,100,90.15),false,nullptr,ETeleportType::TeleportPhysics);
                PC->SetControlRotation(FRotator(0,180,0));
                Enemy->SetActorLocation(FVector(-650,100,90.15),false,nullptr,ETeleportType::TeleportPhysics);
                Enemy->SetActorRotation(FRotator::ZeroRotator);
                Enemy->PatrolLocations.Reset();
                Enemy->Memory.State=EHorrorEnemyState::Patrol;
                Enemy->ResumeAfterRestore();
                Next(); return false;
            }
            if (Phase==2)
            {
                if (Enemy->Memory.State!=EHorrorEnemyState::Chase) return false;
                Door->Interact_Implementation(Player);
                if (!Check(!Door->bOpen,TEXT("Player closes the door in front of a live pursuer"))) return true;
                Next(); return false;
            }
            if (Phase==3)
            {
                if (!AI->IsWaitingForDoor()) return false;
                Check(!Door->bOpen && AI->GetCurrentDoor()==Door,TEXT("Navigation reaches the closed door and starts its configured wait"));
                WaitStart=World->GetTimeSeconds();
                PausedPosition=Enemy->GetActorLocation();
                PC->SetPause(true);
                WaitUntil=Now+1;
                Capture(TEXT("EnemyDoorWaiting.png"));
                Next(); return false;
            }
            if (Phase==4)
            {
                if (Now<WaitUntil) return false;
                Check(AI->IsWaitingForDoor() && !Door->bOpen && Enemy->GetActorLocation().Equals(PausedPosition,.01f),TEXT("Pause freezes door wait and enemy movement"));
                PC->SetPause(false);
                if (!Check(Saves->SaveToSlot(1),TEXT("Persist the enemy waiting at an unlocked closed door"))) return true;
                Next(); return false;
            }
            if (Phase==5 || Phase==101)
            {
                if (!Door->bOpen) return false;
                Check(!Door->bLocked && Door->Puzzle->IsSolved(),TEXT("Enemy opening preserves puzzle progress"));
                if (Phase==5) Check(World->GetTimeSeconds()-WaitStart >= Door->AINavigation->Definition->OpenDelaySeconds-.2f,TEXT("Opening honours the configured delay in game time"));
                if (!bCapturedOpen) { bCapturedOpen=true; Capture(TEXT("EnemyDoorOpen.png")); }
                if (Enemy->GetActorLocation().X<220) return false;
                Check(Door->bOpen,TEXT("Door remains open after actual physical traversal"));
                Disable();
                if (Phase==101) return true;
                if (!Check(Saves->LoadFromSlot(1),TEXT("Rewind to the closed door mid-action snapshot"))) return true;
                Next(); return false;
            }
            if (Phase==6 || Phase==100)
            {
                Check(!Door->bOpen && !Door->bLocked && Door->Puzzle->IsSolved(),TEXT("Mid-action snapshot restores a logically closed unlocked door"));
                if (Phase==100) { Phase=101; return false; }
                Disable();
                WaitUntil=Now+1.2;
                Next(); return false;
            }
            if (Phase==7)
            {
                if (Now<WaitUntil) return false;
                Check(!Door->bOpen && !AI->GetCurrentDoor(),TEXT("Loading and disabling leave no delayed opening callback"));
                if (!Check(Saves->LoadFromSlot(0),TEXT("Restore the original unsolved puzzle"))) return true;
                Next(); return false;
            }
            if (Phase==8)
            {
                if (!Check(Door->bLocked && !Door->bOpen && !Door->Puzzle->IsSolved(),TEXT("Unsolved lock restores before blocked-route test"))) return true;
                Player->SetActorLocation(FVector(650,0,90.15),false,nullptr,ETeleportType::TeleportPhysics);
                Enemy->SetActorLocation(FVector(-650,0,90.15),false,nullptr,ETeleportType::TeleportPhysics);
                Enemy->PatrolLocations.Reset();
                Enemy->Memory.State=EHorrorEnemyState::Investigate;
                Enemy->Memory.LastKnownLocation=Player->GetActorLocation();
                Enemy->Memory.bHasLastKnownLocation=true;
                Enemy->ResumeAfterRestore();
                WaitUntil=Now+3;
                Next(); return false;
            }
            if (Phase==9)
            {
                if (!Check(Door->bLocked && !Door->bOpen && !AI->IsWaitingForDoor() && Enemy->GetActorLocation().X<0,TEXT("Locked puzzle cannot be opened or crossed by the enemy"))) return true;
                if (Now<WaitUntil) return false;
                Check(Enemy->Memory.State==EHorrorEnemyState::Investigate || Enemy->Memory.State==EHorrorEnemyState::Search,TEXT("Unreachable last-known position becomes investigation/search on the accessible side"));
                Disable();
                // Only the runtime fixture narrows the panel to expose a physical alternate route.
                Door->SetActorScale3D(FVector(.2f,2,3.2f));
                Door->AINavigation->RefreshDoorNavigation();
                Next(); return false;
            }
            if (Phase==10)
            {
                if (UNavigationSystemV1::IsNavigationBeingBuilt(World)) return false;
                auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,FVector(-650,0,90.15),Player->GetActorLocation(),Enemy);
                if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
                Enemy->SetActorLocation(FVector(-650,0,90.15),false,nullptr,ETeleportType::TeleportPhysics);
                Enemy->SetActorRotation(FRotator::ZeroRotator);
                Enemy->Memory.State=EHorrorEnemyState::Investigate;
                Enemy->Memory.LastKnownLocation=Player->GetActorLocation();
                Enemy->Memory.bHasLastKnownLocation=true;
                Enemy->ResumeAfterRestore();
                Next(); return false;
            }
            if (Enemy->GetActorLocation().X<220) return false;
            Check(Door->bLocked && !Door->bOpen && !Door->Puzzle->IsSolved(),TEXT("Alternate route reaches the far side while respecting the locked puzzle"));
            return true;
        }
    private:
        FAutomationTestBase* Test;
        bool bCold;
        bool bCapturedOpen=false;
        int32 Phase=0;
        double Deadline,WaitUntil=0,LastTrace=0,WaitStart=0;
        FVector PausedPosition=FVector::ZeroVector;
    };
    bool Run(FAutomationTestBase* Test,bool bCold)
    {
        if (!FParse::Param(FCommandLine::Get(),bCold?TEXT("EnemyDoorsColdRead"):TEXT("EnemyDoorsRuntimeTest")))
        { Test->AddInfo(TEXT("Opt-in enemy-door protocol requires isolated HorrorDoorTest UserDir.")); return true; }
        FString UserDir; FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir);
        if (!UserDir.Contains(TEXT("HorrorDoorTest"))) {Test->AddError(TEXT("Use isolated HorrorDoorTest saves.")); return false;}
        for (int32 Slot=0;Slot<2;++Slot)
            if (UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Slot),0)!=bCold)
            {Test->AddError(TEXT("Use fresh runtime slots or preceding snapshots for cold read.")); return false;}
        Test->AddCommand(new FProtocol(Test,bCold)); return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEnemyDoorsRuntime,"HorrorFramework.Doors.Runtime",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEnemyDoorsRuntime::RunTest(const FString& Parameters) { return EnemyDoorRuntime::Run(this,false); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEnemyDoorsColdRead,"HorrorFramework.Doors.ColdRead",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEnemyDoorsColdRead::RunTest(const FString& Parameters) { return EnemyDoorRuntime::Run(this,true); }
#endif
