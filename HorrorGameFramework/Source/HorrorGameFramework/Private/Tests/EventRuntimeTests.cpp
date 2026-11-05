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
#include "Components/SaveComponent.h"
#include "Components/BoxComponent.h"
#include "Events/FrameworkEventActor.h"
#include "Events/FrameworkEventActions.h"
#include "HorrorEventComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "ObjectiveJournalComponent.h"
#include "ObjectiveDefinition.h"
#include "Streaming/StreamingEnemy.h"
#include "HorrorEnemyController.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

namespace EventRuntimeTest
{
    UWorld* GameWorld()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->IsGameWorld()) return Context.World();
        return nullptr;
    }
    class FProtocol : public IAutomationLatentCommand
    {
    public:
        FProtocol(FAutomationTestBase* InTest, bool bInCold) : Test(InTest), bCold(bInCold), Deadline(FPlatformTime::Seconds() + 150) {}
        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Now > Deadline)
            {
                Test->AddError(FString::Printf(TEXT("Event protocol timed out in phase %d"), Phase));
                return true;
            }
            UWorld* World = GameWorld();
            auto* GI = World ? World->GetGameInstance() : nullptr;
            auto* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Player = PC ? PC->GetPawn() : nullptr;
            if (!GI || !Player) return false;
            auto* Saves = GI->GetSubsystem<USaveSubsystem>();
            auto* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
            if (!Saves->IsWorldReady() || Saves->IsBusy() || Flow->CurrentState != EGameFlowState::Playing) return false;
            AFrameworkEventActor* Scare = nullptr;
            for (TActorIterator<AFrameworkEventActor> It(World); It; ++It)
                if (It->Event->Definition && It->Event->Definition->EventId == TEXT("Integrated.CorridorScare")) Scare = *It;
            if (!Scare || !Scare->Event->IsReady()) return false;
            auto* Light = Cast<AFrameworkEventProp>(Scare->Event->GetTarget(TEXT("Light")));
            auto* Door = Cast<AFrameworkEventProp>(Scare->Event->GetTarget(TEXT("Door")));
            auto* Journal = Player->FindComponentByClass<UObjectiveJournalComponent>();
            if (!Light || !Door || !Journal || !Journal->IsReady()) return false;
            auto Check = [&](bool Value, const TCHAR* Message) { return Test->TestTrue(Message, Value); };
            if (Phase == 0)
            {
                for (TActorIterator<AStreamingEnemy> It(World); It; ++It)
                {
                    Scare->Event->Targets.Add(TEXT("TestEnemy"), *It);
                    auto* EnemyAction = NewObject<UFrameworkEventEnemyAction>();
                    EnemyAction->TargetId = TEXT("TestEnemy");
                    FText Error;
                    EnemyAction->bEnemyEnabled = false;
                    Check(EnemyAction->CanExecute(Scare->Event, Error) && EnemyAction->Execute(Scare->Event, Error), TEXT("Event action can disable a configured enemy"));
                    Check(It->Memory.State == EHorrorEnemyState::Disabled, TEXT("Enemy is disabled"));
                    EnemyAction->bEnemyEnabled = true;
                    Check(EnemyAction->Execute(Scare->Event, Error) && It->Memory.State == EHorrorEnemyState::Patrol, TEXT("Event action reactivates patrol"));
                    const float RemainingTime = It->Memory.RemainingTime;
                    Check(EnemyAction->RestoreFinalState(Scare->Event, Error) && It->Memory.RemainingTime == RemainingTime, TEXT("Restoring enable preserves an already active enemy"));
                    EnemyAction->bEnemyEnabled = false;
                    EnemyAction->Execute(Scare->Event, Error);
                    Scare->Event->Targets.Remove(TEXT("TestEnemy"));
                }
                if (bCold)
                {
                    if (!Check(Saves->LoadFromSlot(2), TEXT("Cold process starts loading the completed-event snapshot"))) return true;
                    Phase = 20;
                    return false;
                }
                Check(Scare->Event->Progress.State == EHorrorEventState::Idle, TEXT("A new game starts with an unconsumed event"));
                Scare->Event->SetEventsEnabled(false);
                Check(!Scare->TryTrigger(Player), TEXT("Disabled events cannot trigger"));
                Scare->Event->SetEventsEnabled(true);
                PC->SetPause(true);
                Check(!Scare->TryTrigger(Player), TEXT("Events cannot start behind a paused menu"));
                PC->SetPause(false);
                if (!Check(Saves->SaveToSlot(0), TEXT("Snapshot before the event starts"))) return true;
                Phase = 1;
                return false;
            }
            if (Phase == 1)
            {
                Player->SetActorLocation(FVector(-1000, 0, 96), false, nullptr, ETeleportType::TeleportPhysics);
                Phase = 2;
                return false;
            }
            if (Phase == 2)
            {
                if (Scare->Event->Progress.State != EHorrorEventState::Running || Scare->Event->Progress.NextStep < 1) return false;
                Check(!Scare->TryTrigger(Player), TEXT("An overlap cannot reenter an active sequence"));
                const auto LightBinding = Scare->Event->Targets.FindChecked(TEXT("Light"));
                Scare->Event->Targets.Remove(TEXT("Light"));
                FString SaveError;
                Check(!Scare->ValidateSaveState(SaveError), TEXT("An in-flight snapshot rejects missing final-state targets before restoration"));
                Scare->Event->Targets.Add(TEXT("Light"), LightBinding);
                if (FParse::Param(FCommandLine::Get(), TEXT("EventsCaptureRender")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/EventRunning.png"), true, false);
                if (!Check(Saves->SaveToSlot(1), TEXT("Snapshot while the sequence is running"))) return true;
                Phase = 3;
                return false;
            }
            if (Phase == 3)
            {
                if (Scare->Event->Progress.State != EHorrorEventState::Completed) return false;
                if (!CheckFinal(Scare, Light, Door, Journal)) return true;
                if (FParse::Param(FCommandLine::Get(), TEXT("EventsCaptureRender")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/EventCompleted.png"), true, false);
                if (!Check(Saves->SaveToSlot(2), TEXT("Snapshot after the event completes"))) return true;
                Phase = 4;
                return false;
            }
            if (Phase == 4)
            {
                if (!Check(Saves->LoadFromSlot(0), TEXT("Rewind to the snapshot before entry"))) return true;
                Phase = 5;
                return false;
            }
            if (Phase == 5)
            {
                Check(Scare->Event->Progress.State == EHorrorEventState::Idle && Scare->Event->Progress.CompletedCount == 0,
                    TEXT("Rewinding restores unconsumed execution state"));
                Check(Door->CurrentState == TEXT("Open") && Light->CurrentState == TEXT("On") && !Journal->HasFact(TEXT("Demo.CorridorScare.Completed")),
                    TEXT("Presentation and facts rewind with the event"));
                if (!Check(Saves->LoadFromSlot(1), TEXT("Load the snapshot captured mid-sequence"))) return true;
                Phase = 6;
                return false;
            }
            if (Phase == 6)
            {
                if (!CheckFinal(Scare, Light, Door, Journal)) return true;
                auto* Inventory = Player->FindComponentByClass<UInventoryComponent>();
                auto* Item = LoadObject<UInventoryItemDefinition>(nullptr, TEXT("/Game/_Project/Integrated/Data/DA_FuseA.DA_FuseA"));
                auto* Definition = NewObject<UHorrorEventDefinition>(GetTransientPackage());
                Definition->EventId = TEXT("Test.InteractionEvent");
                auto* RequiredFact = NewObject<UFrameworkEventFactCondition>(Definition);
                RequiredFact->RequiredFacts.Add(TEXT("Test.Event.Ready"));
                auto* RequiredItem = NewObject<UFrameworkEventItemCondition>(Definition);
                RequiredItem->Item = Item;
                RequiredItem->Quantity = 2;
                Definition->Conditions = {RequiredFact, RequiredItem};
                auto* Action = NewObject<UHorrorEventStateAction>(Definition);
                Action->TargetId = TEXT("Light"); Action->State = TEXT("On");
                FHorrorEventStep Step; Step.DelayBefore = .1f; Step.Action = Action; Definition->Steps.Add(Step);
                Probe = SpawnProbe(World, Player, Definition, Scare, EFrameworkEventTrigger::Interaction, TEXT("Tests.EventInteraction"));
                if (!Check(Probe.IsValid() && Inventory && Item, TEXT("Interaction fixture and item definition exist"))) return true;
                Check(!IInteractionInterface::Execute_CanInteract(Probe.Get()), TEXT("Missing facts and items hide the interaction"));
                Journal->RecordFact(TEXT("Test.Event.Ready"));
                Check(Probe->Event->Progress.State == EHorrorEventState::Idle, TEXT("Interaction mode waits for an explicit action"));
                Check(Inventory->AddItem(Item, 1).bSuccess, TEXT("First required item added"));
                Check(!IInteractionInterface::Execute_CanInteract(Probe.Get()), TEXT("The quantity condition counts actual inventory units"));
                Check(Inventory->AddItem(Item, 1).bSuccess && IInteractionInterface::Execute_CanInteract(Probe.Get()), TEXT("Fact and inventory conditions jointly enable interaction"));
                IInteractionInterface::Execute_Interact(Probe.Get(), Player);
                Phase = 7;
                return false;
            }
            if (Phase == 7)
            {
                if (!Probe.IsValid() || Probe->Event->Progress.State != EHorrorEventState::Completed) return false;
                Probe->Destroy();
                auto* Definition = NewObject<UHorrorEventDefinition>(GetTransientPackage());
                Definition->EventId = TEXT("Test.ProgressionEvent");
                auto* Condition = NewObject<UFrameworkEventFactCondition>(Definition);
                Condition->RequiredFacts.Add(TEXT("Test.Event.Progressed"));
                Definition->Conditions.Add(Condition);
                auto* Action = NewObject<UHorrorEventStateAction>(Definition);
                Action->TargetId = TEXT("Light"); Action->State = TEXT("Off");
                FHorrorEventStep Step; Step.Action = Action; Definition->Steps.Add(Step);
                Probe = SpawnProbe(World, Player, Definition, Scare, EFrameworkEventTrigger::Progression, TEXT("Tests.EventProgression"));
                Check(Probe->Event->Progress.State == EHorrorEventState::Idle, TEXT("Progression trigger waits for its condition"));
                Journal->RecordFact(TEXT("Test.Event.Progressed"));
                Check(Probe->Event->Progress.State == EHorrorEventState::Running, TEXT("Journal changes trigger progression events without polling"));
                const auto Records = Journal->GetRecords();
                if (!Records.IsEmpty())
                {
                    auto* Objective = NewObject<UFrameworkEventObjectiveCondition>();
                    Objective->Objective = Records[0].Definition;
                    Objective->ExpectedState = Records[0].State;
                    FText Error;
                    Check(Objective->Evaluate(Scare->Event, Player, Error), TEXT("Objective condition observes the actual journal state"));
                    Objective->ExpectedState = Records[0].State == EObjectiveState::Completed ? EObjectiveState::Active : EObjectiveState::Completed;
                    Check(!Objective->Evaluate(Scare->Event, Player, Error), TEXT("A different objective state rejects the event"));
                }
                Phase = 8;
                return false;
            }
            if (Phase == 8)
            {
                if (Probe->Event->Progress.State != EHorrorEventState::Completed) return false;
                Check(Light->CurrentState == TEXT("Off"), TEXT("Progression action updates the bound target"));
                Probe->Destroy(); Probe.Reset();
                if (!Check(Saves->LoadFromSlot(2), TEXT("Restore the completed-event snapshot"))) return true;
                Phase = 9;
                return false;
            }
            if (Phase == 9)
            {
                if (!CheckFinal(Scare, Light, Door, Journal)) return true;
                Flow->RestartLevel();
                Phase = 10;
                return false;
            }
            if (Phase == 10)
            {
                Check(Scare->Event->Progress.CompletedCount == 0 && Door->CurrentState == TEXT("Open"), TEXT("A restarted game resets event progress and presentation"));
                return true;
            }
            if (Phase == 20)
            {
                CheckFinal(Scare, Light, Door, Journal);
                return true;
            }
            return false;
        }
    private:
        bool CheckFinal(AFrameworkEventActor* Scare, AFrameworkEventProp* Light, AFrameworkEventProp* Door, UObjectiveJournalComponent* Journal)
        {
            const bool bValid = Scare->Event->Progress.State == EHorrorEventState::Completed && Scare->Event->Progress.CompletedCount == 1
                && Light->CurrentState == TEXT("On") && Door->CurrentState == TEXT("Closed") && Journal->HasFact(TEXT("Demo.CorridorScare.Completed"));
            Test->TestTrue(TEXT("Completed snapshot has final target states, completion fact and one execution"), bValid);
            Test->TestFalse(TEXT("A consumed event cannot be repeated"), Scare->TryTrigger(Journal->GetOwner()));
            return bValid;
        }
        AFrameworkEventActor* SpawnProbe(UWorld* World, APawn* Player, UHorrorEventDefinition* Definition, AFrameworkEventActor* Source, EFrameworkEventTrigger Trigger, FName Key)
        {
            const FTransform Transform(Player->GetActorLocation() + FVector(180, 0, 0));
            auto* Actor = World->SpawnActorDeferred<AFrameworkEventActor>(AFrameworkEventActor::StaticClass(), Transform);
            Actor->Trigger = Trigger;
            Actor->Event->Definition = Definition;
            Actor->Event->Targets = Source->Event->Targets;
            Actor->Save->PersistentKey = Key;
            Actor->FinishSpawning(Transform);
            return Actor;
        }
        FAutomationTestBase* Test;
        bool bCold;
        int32 Phase = 0;
        double Deadline;
        TWeakObjectPtr<AFrameworkEventActor> Probe;
    };
    bool Run(FAutomationTestBase* Test, bool bCold)
    {
        if (!FParse::Param(FCommandLine::Get(), bCold ? TEXT("EventsColdRead") : TEXT("EventsRuntimeTest")))
        {
            Test->AddInfo(TEXT("Opt-in event protocol requires an isolated HorrorEventsTest UserDir."));
            return true;
        }
        FString UserDir;
        FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir);
        if (!UserDir.Contains(TEXT("HorrorEventsTest"))) { Test->AddError(TEXT("Use an isolated HorrorEventsTest directory.")); return false; }
        for (int32 Slot = 0; Slot < 3; ++Slot)
        {
            const bool bExists = UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Slot), 0);
            if ((!bCold && bExists) || (bCold && Slot == 2 && !bExists))
            { Test->AddError(TEXT("Fresh runtime saves or the preceding cold-read snapshot are required.")); return false; }
        }
        Test->AddCommand(new FProtocol(Test, bCold));
        return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEventRuntimeTest, "HorrorFramework.Events.Runtime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEventRuntimeTest::RunTest(const FString& Parameters) { return EventRuntimeTest::Run(this, false); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEventColdReadTest, "HorrorFramework.Events.ColdRead", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEventColdReadTest::RunTest(const FString& Parameters) { return EventRuntimeTest::Run(this, true); }
#endif
