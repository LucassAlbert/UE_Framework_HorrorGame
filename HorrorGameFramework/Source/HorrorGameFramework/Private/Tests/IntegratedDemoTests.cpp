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
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Demo/IntegratedDemo.h"
#include "Streaming/StreamingEnemy.h"
#include "Streaming/FrameworkSavePoint.h"
#include "Streaming/SaveMenuScreens.h"
#include "Dialogue/DialogueNPC.h"
#include "Dialogue/DialogueGameplayComponent.h"
#include "DialogueSessionComponent.h"
#include "Documents/DocumentPickup.h"
#include "Documents/DocumentGameplayComponent.h"
#include "Inventory/InventoryWorldItem.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryRecipe.h"
#include "Progression/PuzzleSocket.h"
#include "Progression/PuzzleDoor.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "ItemPuzzleComponent.h"
#include "ObjectiveJournalComponent.h"
#include "HorrorEnemyController.h"
#include "HorrorWorldSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UIManagerSubsystem.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

namespace IntegratedDemoTest
{
    UWorld* World()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->IsGameWorld()) return Context.World();
        return nullptr;
    }
    template<class T> T* Find(UWorld* W, const TCHAR* Key)
    {
        for (TActorIterator<T> It(W); It; ++It)
            if (const USaveComponent* Save = It->template FindComponentByClass<USaveComponent>(); Save && Save->PersistentKey == Key) return *It;
        return nullptr;
    }
    void Aim(AIntegratedDemoCharacter* Player, APlayerController* PC, AActor* Target, FVector Offset = FVector(0,150,20))
    {
        FVector Location=Target->GetActorLocation()+Offset;
        Location.Z=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+.15f;
        Player->SetActorLocation(Location);
        PC->PlayerCameraManager->UpdateCamera(0.f);
        FVector Origin;FRotator Previous;PC->GetPlayerViewPoint(Origin,Previous);
        PC->SetControlRotation((Target->GetActorLocation()-Origin).Rotation());
        PC->PlayerCameraManager->UpdateCamera(0.f);
    }
    int32 Slot(AIntegratedDemoCharacter* Player, const TCHAR* Id)
    {
        for (int32 I=0;I<Player->Inventory->SlotCapacity;++I)
            if (const UInventoryItemDefinition* Item=Player->Inventory->GetItemAt(I);Item && Item->ItemId==Id) return I;
        return INDEX_NONE;
    }
    class FPlaythrough : public IAutomationLatentCommand
    {
    public:
        explicit FPlaythrough(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds()+90) {}
        virtual bool Update() override
        {
            const double Now=FPlatformTime::Seconds();
            if (Now>Deadline) {Test->AddError(FString::Printf(TEXT("Integrated demo timed out in phase %d"),Phase));return true;}
            UWorld* W=World();if(!W || !W->GetGameInstance()) return false;
            auto* GI=W->GetGameInstance();auto* UI=GI->GetSubsystem<UUIManagerSubsystem>();
            auto* Flow=GI->GetSubsystem<UGameFlowSubsystem>();auto* Saves=GI->GetSubsystem<USaveSubsystem>();
            auto* PC=W->GetFirstPlayerController();auto* Player=PC?Cast<AIntegratedDemoCharacter>(PC->GetPawn()):nullptr;
            auto Next=[&](){++Phase;Deadline=Now+90;};
            auto Check=[&](bool Value,const TCHAR* Message){Test->TestTrue(Message,Value);return Value;};
            if(Phase==19)
            {
                if(UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("BootScene") || !UI->GetTopScreen()) return false;
                if(!Check(Cast<UFrameworkMainMenuScreen>(UI->GetTopScreen())!=nullptr,TEXT("Completion returns to the real main menu")))return true;
                UI->GetTopScreen()->ProcessEvent(UI->GetTopScreen()->FindFunctionChecked(TEXT("NewGame")),nullptr);Next();return false;
            }
            if(!Player)return false;
            const bool Ready=!Saves->IsBusy() && Saves->IsWorldReady() && Flow->CurrentState==EGameFlowState::Playing;
            auto* Door=Find<APuzzleDoor>(W,TEXT("Integrated.CorridorPuzzleDoor"));
            switch(Phase)
            {
            case 0:
                if(!Ready || !Player->Objectives->IsReady())return false;
                {
                    AStreamingEnemy* Enemy=nullptr;
                    for(TActorIterator<AStreamingEnemy> It(W);It;++It)Enemy=*It;
                    if(!Enemy)return false;
                    if(!Check(Enemy->Memory.State!=EHorrorEnemyState::Disabled,TEXT("Enemy is active from the beginning")))return true;
                    // Only this automated route disables capture after verifying the authored initial state.
                    Enemy->Memory.State=EHorrorEnemyState::Disabled;
                    CastChecked<AHorrorEnemyController>(Enemy->GetController())->StopMovement();
                }
                if(!Check(Door && Door->bLocked,TEXT("Corridor starts locked")))return true;
                Next();return false;
            case 1:
            {
                auto* NPC=Find<ADialogueNPC>(W,TEXT("Integrated.Caretaker"));if(!NPC || !NPC->CanInteract_Implementation())return false;
                Aim(Player,PC,NPC);Next();return false;
            }
            case 2:
                if(!Check(Player->DialogueGameplay->StartDialogue(Find<ADialogueNPC>(W,TEXT("Integrated.Caretaker"))),TEXT("NPC interaction starts the live conversation")))return true;
                Check(!W->IsPaused(),TEXT("Conversation keeps the world running"));Next();return false;
            case 3:
                for(int32 I=0;I<8 && Player->Dialogue->IsConversationActive();++I)Player->DialogueGameplay->Advance();
                if(!Check(Player->Objectives->HasFact(TEXT("Demo.NPC.Talked")),TEXT("Dialogue advances the integrated objectives")))return true;
                Aim(Player,PC,Find<ADocumentPickup>(W,TEXT("Integrated.MaintenanceMemo")));Next();return false;
            case 4:
                if(!Check(Player->DocumentGameplay->CollectDocument(Find<ADocumentPickup>(W,TEXT("Integrated.MaintenanceMemo"))),TEXT("Collect the actual memo through its gameplay adapter")))return true;
                Check(Player->Objectives->HasFact(TEXT("Demo.Memo.Read")) && W->IsPaused(),TEXT("Automatic reading pauses and records the first reading"));
                UI->PopScreen();Next();return false;
            case 5:
                for(const TCHAR* Key : {TEXT("Integrated.KeyShaft"),TEXT("Integrated.KeyHead"),TEXT("Integrated.LeftFuse"),TEXT("Integrated.RightFuse"),TEXT("Integrated.Flashlight")})
                {
                    auto* Pickup=Find<AInventoryWorldItem>(W,Key);if(!Pickup || !Pickup->CanInteract_Implementation())return false;
                    Aim(Player,PC,Pickup);Pickup->Interact_Implementation(Player);
                }
                if(!Check(Player->Inventory->GetSlots().Num()==5,TEXT("The authored collection fits five slots")))return true;
                Player->InventoryGameplay->OpenInventory();
                if(!Check(Player->InventoryGameplay->InspectSlot(Slot(Player,TEXT("Integrated.KeyA"))).bSuccess,TEXT("Inspection opens from the real inventory")))return true;
                Next();return false;
            case 6:
                if(Player->Inspection->GetState()!=EInspectionState::Ready)return false;
                if(FParse::Param(FCommandLine::Get(),TEXT("IntegratedDemoCapture")))
                {
                    if(!bCapturedInspection) {bCapturedInspection=true;WaitUntil=Now+.4;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Integrated_Inspection.png"),true,false);return false;}
                    if(Now<WaitUntil)return false;
                }
                Check(Player->Objectives->HasFact(TEXT("Demo.Item.Inspected")),TEXT("Only a ready inspection records the examination fact"));
                while(UI->HasModalScreen())UI->PopScreen();
                // Preload the recipe only in the test; gameplay continues using its asynchronous loader.
                LoadObject<UInventoryRecipe>(nullptr,TEXT("/Game/_Project/Integrated/Data/DA_AssembleExitKey.DA_AssembleExitKey"));
                LoadObject<UInventoryItemDefinition>(nullptr,TEXT("/Game/_Project/Integrated/Data/DA_Key.DA_Key"));
                if(!Check(Player->InventoryGameplay->CombineSlots(Slot(Player,TEXT("Integrated.KeyA")),Slot(Player,TEXT("Integrated.KeyB"))).bSuccess,TEXT("Two ingredients produce the exit key")))return true;
                Aim(Player,PC,Find<APuzzleSocket>(W,TEXT("Integrated.SocketLeft")));Next();return false;
            case 7:
            {
                auto* Socket=Find<APuzzleSocket>(W,TEXT("Integrated.SocketLeft"));Socket->Interact_Implementation(Player);
                FVector Origin;FRotator Rotation;PC->GetPlayerViewPoint(Origin,Rotation);
                FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoTest),false,Player);
                W->LineTraceSingleByChannel(Hit,Origin,Origin+Rotation.Vector()*250,ECC_GameTraceChannel1,Query);
                Test->AddInfo(FString::Printf(TEXT("Socket trace: origin=%s target=%s hit=%s available=%d selected=%d slot=%d"),*Origin.ToString(),*Socket->GetActorLocation().ToString(),*GetNameSafe(Hit.GetActor()),Socket->CanInteract_Implementation(),Player->ProgressionGameplay->GetSelectedSocket()==Socket,Slot(Player,TEXT("Integrated.FuseA"))));
                Test->AddInfo(FString::Printf(TEXT("Socket geometry: rotation=%s component=%s mesh=%s collision=%d response=%d"),*Rotation.ToString(),*Socket->Frame->GetComponentLocation().ToString(),*GetNameSafe(Socket->Frame->GetStaticMesh()),int32(Socket->Frame->GetCollisionEnabled()),int32(Socket->Frame->GetCollisionResponseToChannel(ECC_GameTraceChannel1))));
                const auto Result=Player->ProgressionGameplay->InsertItem(Slot(Player,TEXT("Integrated.FuseA")));
                if(!Result.bSuccess)Test->AddError(Result.Message.ToString());
                if(!Check(Result.bSuccess,TEXT("Insert left fuse through the selection transaction")))return true;
                Aim(Player,PC,Find<APuzzleSocket>(W,TEXT("Integrated.SocketRight")),FVector(0,-150,0));Next();return false;
            }
            case 8:
            {
                auto* Socket=Find<APuzzleSocket>(W,TEXT("Integrated.SocketRight"));Socket->Interact_Implementation(Player);
                if(!Check(Player->ProgressionGameplay->InsertItem(Slot(Player,TEXT("Integrated.FuseB"))).bSuccess,TEXT("Insert right fuse and unlock the corridor")))return true;
                if(!Check(Door && !Door->bLocked && !Door->bOpen && Player->Objectives->HasFact(TEXT("Demo.Power.Restored")),TEXT("Puzzle unlocks without opening automatically")))return true;
                Door->Interact_Implementation(Player);
                if(!Check(Door->bOpen,TEXT("Interaction opens the unlocked passage")))return true;
                AFrameworkSavePoint* Point=nullptr;for(TActorIterator<AFrameworkSavePoint> It(W);It;++It)if(It->GetActorLocation().X < -1200)Point=*It;
                if(!Check(Point!=nullptr,TEXT("Maintenance wing has a manual save terminal")))return true;
                Aim(Player,PC,Point);Point->Interact_Implementation(Player);
                auto* Screen=Cast<UFrameworkSaveSlotsScreen>(UI->GetTopScreen());
                if(!Check(Screen!=nullptr,TEXT("Terminal opens manual slot selection")))return true;
                Screen->SelectSlot(0);Next();return false;
            }
            case 9:
                if(Saves->IsBusy())return false;
                if(!Check(UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(0),0),TEXT("Integrated progress is written to the isolated slot")))return true;
                while(UI->HasModalScreen())UI->PopScreen();Player->Inventory->RemoveFromSlot(Slot(Player,TEXT("Integrated.ExitKey")));
                Door->bLocked=true;
                if(!Check(Saves->LoadFromSlot(0),TEXT("Rewind starts after inventory and door mutations")))return true;
                Next();return false;
            case 10:
                if(!Ready)return false;
                if(!Check(Door && !Door->bLocked && Door->bOpen && Slot(Player,TEXT("Integrated.ExitKey"))!=INDEX_NONE && Player->Objectives->HasFact(TEXT("Demo.Item.Inspected")) && Player->Dialogue->HasCompleted(TEXT("Integrated.Caretaker")),TEXT("Load restores key, puzzle, inspection objective and conversation history together")))return true;
                Player->SetActorLocation(FVector(1000,0,100));Next();return false;
            case 11:
                if(!Ready || !W->GetSubsystem<UHorrorWorldSubsystem>()->IsAreaReady(TEXT("B")))return false;
                if(!Player->Objectives->HasFact(TEXT("Demo.East.Entered")))Player->SetActorLocation(FVector(1650,0,100));
                if(!Player->Objectives->HasFact(TEXT("Demo.East.Entered")))return false;
                {
                    auto* Exit=Find<AIntegratedDemoExit>(W,TEXT("Integrated.FinalExit"));if(!Exit)return false;
                    if(UNavigationSystemV1::IsNavigationBeingBuilt(W))return false;
                    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,FVector(-800,120,10),FVector(800,120,10));
                    if(!Check(Path && Path->IsValid() && !Path->IsPartial(),TEXT("Opened puzzle leaves a complete navigation path through the corridor")))return true;
                    Exit->RequiredFacts.Add(TEXT("Demo.UnsatisfiedTestFact"));
                    Check(!Exit->HasRequiredProgress(Player),TEXT("An unmet prerequisite blocks completion"));Exit->RequiredFacts.Pop();
                    Check(Exit->bLocked && Exit->HasRequiredProgress(Player),TEXT("Final exit requires the authored progress and remains key-locked"));
                    Aim(Player,PC,Exit,FVector(-150,0,-40));Next();return false;
                }
            case 12:
                Player->InventoryGameplay->OpenInventory();
                if(!Check(Player->InventoryGameplay->UseSlot(Slot(Player,TEXT("Integrated.ExitKey"))).bSuccess,TEXT("Using the combined key unlocks the actual exit")))return true;
                Check(Slot(Player,TEXT("Integrated.ExitKey"))==INDEX_NONE,TEXT("Unlocking consumes the exit key"));Next();return false;
            case 13:
            {
                auto* Exit=Find<AIntegratedDemoExit>(W,TEXT("Integrated.FinalExit"));Exit->Interact_Implementation(PC);
                if(!Check(Cast<UDemoCompletionScreen>(UI->GetTopScreen())!=nullptr && W->IsPaused(),TEXT("Exit presents the completion screen and pauses danger")))return true;
                Check(Player->Objectives->HasFact(TEXT("Demo.Exit.Reached")),TEXT("Victory completes the escape objective"));
                auto* Top=UI->GetTopScreen();Exit->Interact_Implementation(PC);
                Check(Top==UI->GetTopScreen() && Top->HandleBackAction(),TEXT("Completion is idempotent and Back cannot resume gameplay"));
                if(FParse::Param(FCommandLine::Get(),TEXT("IntegratedDemoCapture")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Integrated_Completed.png"),true,false);
                WaitUntil=Now+.4;Next();return false;
            }
            case 14:
                if(Now<WaitUntil)return false;
                UI->GetTopScreen()->ProcessEvent(UI->GetTopScreen()->FindFunctionChecked(TEXT("Restart")),nullptr);Phase=17;Next();return false;
            case 18:
                if(!Ready || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("FrameworkIntegrated_Demo"))return false;
                if(!Check(!Player->Objectives->HasFact(TEXT("Demo.Exit.Reached")) && Player->Inventory->GetItemAt(0)==nullptr && Door && Door->bLocked,TEXT("Restart resets the integrated demonstration")))return true;
                {
                    auto* Screen=UI->PushScreen(UDemoCompletionScreen::StaticClass());Screen->ProcessEvent(Screen->FindFunctionChecked(TEXT("MainMenu")),nullptr);Next();return false;
                }
            case 20:
                if(!Ready || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("FrameworkIntegrated_Demo"))return false;
                Check(true,TEXT("Main menu New Game launches the integrated demo"));return true;
            }
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Phase=0;
        double Deadline;
        double WaitUntil=0;
        bool bCapturedInspection=false;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorIntegratedDemoPlaythrough,"HorrorFramework.IntegratedDemo.Playthrough",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorIntegratedDemoPlaythrough::RunTest(const FString& Parameters)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("IntegratedDemoAutomation")))
    {AddInfo(TEXT("Opt-in playthrough: use IntegratedDemoAutomation and a fresh UserDir containing HorrorIntegratedTest."));return true;}
    FString UserDir;FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir);
    if(!UserDir.Contains(TEXT("HorrorIntegratedTest"))) {AddError(TEXT("An isolated test UserDir is required."));return false;}
    for(int32 I=0;I<USaveSubsystem::SlotCount;++I)if(UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(I),0))
    {AddError(TEXT("Existing slots will not be overwritten; use a fresh test directory."));return false;}
    AddCommand(new IntegratedDemoTest::FPlaythrough(this));return true;
}
#endif
