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
#include "Threat/FrameworkThreatCharacter.h"
#include "Threat/FrameworkThreatTarget.h"
#include "Threat/FrameworkHidingSpot.h"
#include "Threat/ThreatGameOverScreen.h"
#include "Streaming/StreamingEnemy.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "HorrorEnemyDefinition.h"
#include "Components/SaveComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogFrameworkValidation, Log, All);

namespace LiveThreatValidation
{
    class FProtocol : public IAutomationLatentCommand
    {
    public:
        explicit FProtocol(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds() + 120) {}
        bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Now > Deadline)
            { Test->AddError(FString::Printf(TEXT("Live pursuit timed out in phase %d"), Phase)); return true; }
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.World() && Context.World()->IsGameWorld()) { World = Context.World(); break; }
            auto* PC = World ? World->GetFirstPlayerController() : nullptr;
            auto* Player = PC ? Cast<AFrameworkThreatCharacter>(PC->GetPawn()) : nullptr;
            if (!Player || !World->GetGameInstance()) return false;
            auto* GI = World->GetGameInstance();
            auto* Saves = GI->GetSubsystem<USaveSubsystem>();
            auto* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
            auto* UI = GI->GetSubsystem<UUIManagerSubsystem>();
            if (Saves->IsBusy() || !Saves->IsWorldReady()) return false;
            AFrameworkHidingSpot* Spot = nullptr;
            AStreamingEnemy* Enemy = nullptr;
            for (TActorIterator<AFrameworkHidingSpot> It(World); It; ++It)
                if (It->Save->PersistentKey == TEXT("Integrated.HidingMaintenance")) { Spot = *It; break; }
            for (TActorIterator<AStreamingEnemy> It(World); It; ++It) { Enemy = *It; break; }
            if (!Spot || !Enemy || !Enemy->Definition || !Enemy->GetController()) return false;
            auto* Hiding = Player->Hiding.Get();
            auto* Target = Player->ThreatTarget.Get();
            if (Now - LastTrace > 3)
            {
                LastTrace = Now;
                FVector Eyes; FRotator EyesRotation; Enemy->GetActorEyesViewPoint(Eyes, EyesRotation);
                FHitResult SightHit;
                FCollisionQueryParams SightQuery(SCENE_QUERY_STAT(LiveAudit), false, Enemy);
                World->LineTraceSingleByChannel(SightHit, Eyes, Hiding->GetReturnTransform().GetLocation(), ECC_Visibility, SightQuery);
                UE_LOG(LogFrameworkValidation, Display, TEXT("LiveAudit distance=%.2f capture=%.2f arrival=%.2f sightHit=%s rotation=%s"),
                    FVector::Dist(Enemy->GetActorLocation(), Hiding->GetReturnTransform().GetLocation()),
                    Enemy->Definition->CaptureDistance, Enemy->Definition->ArrivalRadius, *GetNameSafe(SightHit.GetActor()), *EyesRotation.ToString());
                UE_LOG(LogFrameworkValidation, Display, TEXT("LiveAudit phase=%d enemy=%s state=%d witness=%s player=%s hiding=%d flow=%d"),
                    Phase, *Enemy->GetActorLocation().ToString(), int32(Enemy->Memory.State), *Enemy->Memory.WitnessedConcealmentKey.ToString(),
                    *Player->GetActorLocation().ToString(), int32(Hiding->GetState()), int32(Flow->CurrentState));
            }
            auto Check = [&](bool Value, const TCHAR* Message) { return Test->TestTrue(Message, Value); };
            auto Next = [&]() { ++Phase; Deadline = Now + 45; };
            const bool bCaptured = Flow->CurrentState == EGameFlowState::GameOver;
            if (Phase == 0)
            {
                if (Flow->CurrentState != EGameFlowState::Playing || UNavigationSystemV1::IsNavigationBeingBuilt(World)) return false;
                Check(Enemy->Memory.State != EHorrorEnemyState::Disabled, TEXT("Authored enemy starts enabled"));
                // Only this instance is slowed to let entry finish before a witnessed capture.
                auto* FixtureDefinition = DuplicateObject<UHorrorEnemyDefinition>(Enemy->Definition, Enemy);
                FixtureDefinition->ChaseSpeed = 180;
                Enemy->Definition = FixtureDefinition;
                Enemy->PatrolLocations.Reset();
                Enemy->SetActorLocation(FVector(-2800, 520, 100), false, nullptr, ETeleportType::TeleportPhysics);
                Enemy->Memory.State = EHorrorEnemyState::Patrol;
                Enemy->ResumeAfterRestore();
                FVector Entry = Spot->GetActorLocation() + Spot->GetActorForwardVector() * 190;
                Entry.Z = Player->GetActorLocation().Z;
                Player->SetActorLocation(Entry, false, nullptr, ETeleportType::TeleportPhysics);
                PC->SetControlRotation((Spot->GetActorLocation() - Entry).Rotation());
                if (!Check(Saves->SaveToSlot(0), TEXT("Write baseline before pursuit"))) return true;
                Next(); return false;
            }
            if (Phase == 1)
            {
                Enemy->SetActorLocation(Player->GetActorLocation() + FVector(500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
                Enemy->SetActorRotation(FRotator(0, 180, 0));
                Enemy->Memory.State = EHorrorEnemyState::Patrol;
                Enemy->ResumeAfterRestore();
                Next(); return false;
            }
            if (Phase == 2)
            {
                if (Enemy->Memory.State != EHorrorEnemyState::Chase) return false;
                auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Enemy->GetActorLocation(), Player->GetActorLocation());
                if (!Check(Path && Path->IsValid() && !Path->IsPartial(), TEXT("Real pursuer has a complete navigation path to the cabinet entry"))) return true;
                if (!Check(Hiding->Enter(Spot->HideSpot), TEXT("Player starts a witnessed entry during real pursuit"))) return true;
                Check(!Target->IsConcealed(), TEXT("Entering is still vulnerable"));
                Check(!Enemy->Memory.WitnessedConcealmentKey.IsNone(), TEXT("Active AI remembers the witnessed entry"));
                Next(); return false;
            }
            if (Phase == 3)
            {
                if (bCaptured) { Test->AddError(TEXT("Fixture pursuer reached the player before entry finished")); return true; }
                if (!Hiding->IsFullyHidden()) return false;
                Check(Target->IsConcealed(), TEXT("Entry completes before the witness reaches the cabinet"));
                if (!Check(Saves->SaveToSlot(1), TEXT("Persist hidden player together with witnessed pursuit"))) return true;
                Next(); return false;
            }
            if (Phase == 4 || Phase == 6)
            {
                if (!bCaptured) return false;
                Check(Enemy->Memory.State == EHorrorEnemyState::Captured && Cast<UThreatGameOverScreen>(UI->GetTopScreen()) && World->IsPaused(),
                    TEXT("AI navigation and capture open the actual paused Game Over"));
                Check(!Hiding->IsHiding() && !Hiding->IsComponentTickEnabled() && !Spot->HideSpot->IsOccupied(), TEXT("Real capture cleans the hiding transition and occupancy"));
                if (Phase == 4)
                {
                    UI->GetTopScreen()->ProcessEvent(UI->GetTopScreen()->FindFunctionChecked(TEXT("LoadLatest")), nullptr);
                    Next(); return false;
                }
                if (!Check(Saves->LoadFromSlot(0), TEXT("Rewind to baseline after capture"))) return true;
                Next(); return false;
            }
            if (Phase == 5)
            {
                if (Flow->CurrentState != EGameFlowState::Playing || !Hiding->IsFullyHidden()) return false;
                Check(!UI->HasModalScreen() && !World->IsPaused(), TEXT("Load Latest from real Game Over returns to hidden gameplay"));
                if (Enemy->Memory.State != EHorrorEnemyState::Chase) return false;
                Check(!Enemy->Memory.WitnessedConcealmentKey.IsNone(), TEXT("Restore retains the witnessed entry and resumes pursuit"));
                Next(); return false;
            }
            if (Phase == 7)
            {
                if (Flow->CurrentState != EGameFlowState::Playing) return false;
                Check(!Hiding->IsHiding() && !Target->IsConcealed(), TEXT("Baseline clears hidden/captured state"));
                if (!Check(Hiding->Enter(Spot->HideSpot), TEXT("Enter with enemy behind the authored divider"))) return true;
                Next(); return false;
            }
            if (Phase == 8)
            {
                if (bCaptured) { Test->AddError(TEXT("Unwitnessed entry caused capture")); return true; }
                if (!Hiding->IsFullyHidden()) return false;
                Check(Enemy->Memory.WitnessedConcealmentKey.IsNone(), TEXT("Occluded entry leaves no witnessed memory"));
                Enemy->PatrolLocations.Reset();
                Enemy->SetActorLocation(Hiding->GetReturnTransform().GetLocation() + FVector(200, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
                Enemy->SetActorRotation(FRotator(0, 180, 0));
                Enemy->Memory.State = EHorrorEnemyState::Patrol;
                Enemy->ResumeAfterRestore();
                WaitUntil = Now + 2;
                Next(); return false;
            }
            if (Phase == 9)
            {
                if (!Check(!bCaptured && Hiding->IsFullyHidden(), TEXT("An unwitnessed hidden player survives nearby real perception"))) return true;
                if (Now < WaitUntil) return false;
                if (!Check(Hiding->Exit(), TEXT("Hidden player starts exit near an active enemy"))) return true;
                Check(!Target->IsConcealed(), TEXT("Exit removes protection before movement completes"));
                Next(); return false;
            }
            if (!bCaptured) return false;
            Check(Cast<UThreatGameOverScreen>(UI->GetTopScreen()) != nullptr && !Hiding->IsTransitioning(), TEXT("Real enemy captures the exposed exiting player without leaving input locked"));
            return true;
        }
    private:
        FAutomationTestBase* Test;
        int32 Phase = 0;
        double Deadline;
        double WaitUntil = 0;
        double LastTrace = 0;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorLiveThreatValidation, "HorrorFramework.Validation.LivePursuit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorLiveThreatValidation::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("FrameworkLiveAudit")))
    { AddInfo(TEXT("Live pursuit protocol requires FrameworkLiveAudit and a fresh HorrorAuditLive UserDir.")); return true; }
    FString UserDir;
    FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir);
    if (!UserDir.Contains(TEXT("HorrorAuditLive"))) { AddError(TEXT("Use an isolated HorrorAuditLive UserDir.")); return false; }
    for (int32 Slot = 0; Slot < USaveSubsystem::SlotCount; ++Slot)
        if (UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Slot), 0))
        { AddError(TEXT("Use a fresh test directory; existing saves will not be overwritten.")); return false; }
    AddCommand(new LiveThreatValidation::FProtocol(this));
    return true;
}
#endif
