#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Core/HorrorPlayerController.h"
#include "Threat/FrameworkHidingSpot.h"
#include "Threat/FrameworkThreatTarget.h"
#include "Threat/FrameworkEnemy.h"
#include "HidingParticipantComponent.h"
#include "HideSpotComponent.h"
#include "HorrorEnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SaveComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

namespace HidingRuntimeTest
{
    class FProtocol : public IAutomationLatentCommand
    {
    public:
        FProtocol(FAutomationTestBase* InTest, bool bInCold) : Test(InTest), bCold(bInCold), Deadline(FPlatformTime::Seconds() + 120) {}
        bool Update() override
        {
            if (FPlatformTime::Seconds() > Deadline) { Test->AddError(FString::Printf(TEXT("Hiding protocol timeout at phase %d"), Phase)); return true; }
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.World() && Context.World()->IsGameWorld()) { World = Context.World(); break; }
            auto* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Player = PC ? PC->GetPawn() : nullptr;
            auto* GI = World ? World->GetGameInstance() : nullptr;
            if (!GI || !Player) return false;
            auto* Saves = GI->GetSubsystem<USaveSubsystem>();
            if (!Saves->IsWorldReady() || Saves->IsBusy() || GI->GetSubsystem<UGameFlowSubsystem>()->CurrentState != EGameFlowState::Playing) return false;
            auto* Hiding = Player->FindComponentByClass<UHidingParticipantComponent>();
            auto* Target = Player->FindComponentByClass<UFrameworkThreatTarget>();
            AFrameworkHidingSpot* Spot = nullptr;
            for (TActorIterator<AFrameworkHidingSpot> It(World); It; ++It)
                if (It->HideSpot->TransitionDefinition && It->Save->PersistentKey == TEXT("Integrated.HidingMaintenance")) { Spot = *It; break; }
            if (!Spot || !Hiding || !Target) return false;
            auto Check = [&](bool Value, const TCHAR* Message) { return Test->TestTrue(Message, Value); };
            auto Screenshot = [&](const TCHAR* Name)
            {
                if (FParse::Param(FCommandLine::Get(), TEXT("HidingCaptureRender")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name, true, false);
            };
            if (Phase == 0)
            {
                for (TActorIterator<AHorrorEnemyCharacter> It(World); It; ++It)
                {
                    It->Memory.State = EHorrorEnemyState::Disabled;
                    if (AController* AI = It->GetController()) AI->StopMovement();
                }
                if (bCold)
                {
                    if (!Check(Saves->LoadFromSlot(0), TEXT("Cold process loads entry checkpoint"))) return true;
                    Phase = 10; return false;
                }
                FVector Outside = Spot->GetActorLocation() + Spot->GetActorForwardVector() * 190;
                Outside.Z = Player->GetActorLocation().Z;
                Player->SetActorLocation(Outside, false, nullptr, ETeleportType::TeleportPhysics);
                PC->SetControlRotation((Spot->GetActorLocation() - Outside).Rotation());
                Entry = Player->GetActorTransform(); EntryLook = PC->GetControlRotation();
                if (!Check(Hiding->Enter(Spot->HideSpot), TEXT("Authored cabinet accepts animated entry"))) return true;
                Check(!Target->IsConcealed(), TEXT("Runtime entry has no protection"));
                Phase = 1; return false;
            }
            if (Phase == 1)
            {
                if (Spot->HideSpot->GetDoorAmount() < .8f) return false;
                Check(!Spot->DoorPivot->GetRelativeRotation().IsNearlyZero(), TEXT("Door mesh follows transition signal"));
                Screenshot(TEXT("HidingEntering.png"));
                if (!Check(Saves->SaveToSlot(0), TEXT("Persist entry to disk"))) return true;
                Phase = 2; return false;
            }
            if (Phase == 2)
            {
                if (!Hiding->IsFullyHidden()) return false;
                Check(Target->IsConcealed() && !Hiding->IsComponentTickEnabled(), TEXT("Runtime finishes hidden without active transition"));
                Screenshot(TEXT("HidingInside.png"));
                if (!Check(Hiding->Exit(), TEXT("Runtime starts exit"))) return true;
                Check(!Target->IsConcealed(), TEXT("Runtime exit removes protection immediately"));
                Phase = 3; return false;
            }
            if (Phase == 3)
            {
                if (Spot->HideSpot->GetDoorAmount() < .8f) return false;
                Screenshot(TEXT("HidingExiting.png"));
                if (!Check(Saves->SaveToSlot(1), TEXT("Persist exit to disk"))) return true;
                Phase = 4; return false;
            }
            if (Phase == 4)
            {
                if (Hiding->IsHiding()) return false;
                Check(Player->GetActorLocation().Equals(Entry.GetLocation(), 5) && PC->GetControlRotation().Equals(EntryLook, .1f),
                    *FString::Printf(TEXT("Runtime returns to entry pose: actual %s / %s, expected %s / %s"),
                        *Player->GetActorLocation().ToString(), *PC->GetControlRotation().ToString(), *Entry.GetLocation().ToString(), *EntryLook.ToString()));
                if (!Check(Saves->LoadFromSlot(0), TEXT("Warm process loads entry checkpoint"))) return true;
                Phase = 10; return false;
            }
            if (Phase == 10)
            {
                Check(Hiding->IsFullyHidden() && Target->IsConcealed() && !Hiding->IsComponentTickEnabled(), TEXT("Disk entry restores fully hidden without replay"));
                Check(FMath::IsNearlyZero(Spot->HideSpot->GetDoorAmount()), TEXT("Restored cabinet is closed"));
                if (!Check(Saves->LoadFromSlot(1), TEXT("Load exit checkpoint"))) return true;
                Phase = 11; return false;
            }
            Check(!Hiding->IsHiding() && !Target->IsConcealed() && !Hiding->IsComponentTickEnabled(), TEXT("Disk exit restores outside without replay"));
            Check(!Spot->HideSpot->IsOccupied() && Spot->DoorPivot->GetRelativeRotation().IsNearlyZero(), TEXT("Disk exit leaves door closed and spot available"));
            Check(FVector::Dist2D(Player->GetActorLocation(), Spot->GetActorLocation()) > 150, TEXT("Disk exit restores safe return position instead of midpoint"));
            Screenshot(TEXT("HidingCaptureInterrupt.png"));
            if (!Check(Hiding->Enter(Spot->HideSpot), TEXT("Entry can restart after disk restore"))) return true;
            for (TActorIterator<AFrameworkEnemy> It(World); It; ++It)
            {
                It->OnCaptured.Broadcast(*It, Player);
                break;
            }
            Check(GI->GetSubsystem<UGameFlowSubsystem>()->CurrentState == EGameFlowState::GameOver, TEXT("Capture interrupts an entering player"));
            Check(!Hiding->IsHiding() && !Hiding->IsComponentTickEnabled() && !Spot->HideSpot->IsOccupied(), TEXT("Capture releases transition and occupancy"));
            return true;
        }
    private:
        FAutomationTestBase* Test;
        bool bCold;
        int32 Phase = 0;
        double Deadline;
        FTransform Entry;
        FRotator EntryLook;
    };
    bool Run(FAutomationTestBase* Test, bool bCold)
    {
        if (!FParse::Param(FCommandLine::Get(), bCold ? TEXT("HidingColdRead") : TEXT("HidingRuntimeTest")))
        { Test->AddInfo(TEXT("Opt-in hiding protocol requires isolated HorrorHidingTest UserDir.")); return true; }
        FString UserDir;
        FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir);
        if (!UserDir.Contains(TEXT("HorrorHidingTest"))) { Test->AddError(TEXT("Use an isolated HorrorHidingTest directory.")); return false; }
        for (int32 Slot=0; Slot<2; ++Slot)
            if (UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Slot), 0) != bCold)
            { Test->AddError(TEXT("Runtime requires fresh slots; cold read requires both snapshots.")); return false; }
        Test->AddCommand(new FProtocol(Test, bCold));
        return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorHidingRuntimeTest, "HorrorFramework.Hiding.Runtime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorHidingRuntimeTest::RunTest(const FString& Parameters) { return HidingRuntimeTest::Run(this, false); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorHidingColdReadTest, "HorrorFramework.Hiding.ColdRead", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorHidingColdReadTest::RunTest(const FString& Parameters) { return HidingRuntimeTest::Run(this, true); }
#endif
