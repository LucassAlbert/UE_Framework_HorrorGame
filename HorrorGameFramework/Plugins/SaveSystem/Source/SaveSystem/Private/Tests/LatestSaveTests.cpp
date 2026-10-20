#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

namespace LatestSaveTest
{
    struct FFixture
    {
        UGameInstance* GI = nullptr;
        double StartTime = FPlatformTime::Seconds();
        FAutomationTestBase* Test = nullptr;
    };
    class FWaitForLatest : public IAutomationLatentCommand
    {
    public:
        explicit FWaitForLatest(TSharedRef<FFixture> InFixture) : Fixture(InFixture) {}
        virtual bool Update() override
        {
            USaveSubsystem* Save = Fixture->GI->GetSubsystem<USaveSubsystem>();
            if (Save->IsBusy() && FPlatformTime::Seconds() - Fixture->StartTime < 20.0) return false;
            Fixture->Test->TestFalse(TEXT("Latest-save operation completes"), Save->IsBusy());
            Fixture->Test->TestTrue(TEXT("Newest matching-map snapshot wins over slot order and newer foreign map"),
                Save->CurrentSaveGame && Save->CurrentSaveGame->SavedAt == FDateTime(2026, 1, 2));
            UWorld* World = Fixture->GI->GetWorld();
            World->EndPlay(EEndPlayReason::Quit);
            Fixture->GI->Shutdown();
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            Fixture->GI->RemoveFromRoot();
            return true;
        }
    private:
        TSharedRef<FFixture> Fixture;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorLatestSaveTest, "HorrorFramework.Threat.LatestSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorLatestSaveTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ThreatTestSaves")))
    { AddInfo(TEXT("Run with -ThreatTestSaves and a fresh -UserDir containing HorrorThreatTest_ to test disk selection.")); return true; }
    if (!FPaths::ProjectSavedDir().Contains(TEXT("HorrorThreatTest_")))
    { AddError(TEXT("Refusing disk test without an isolated HorrorThreatTest_ UserDir.")); return false; }
    for (int32 Slot = 0; Slot < USaveSubsystem::SlotCount; ++Slot)
        if (UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(Slot), 0))
        { AddError(TEXT("Use a fresh UserDir; test refuses to overwrite existing slots.")); return false; }
    TSharedRef<LatestSaveTest::FFixture> Fixture = MakeShared<LatestSaveTest::FFixture>();
    Fixture->Test = this;
    Fixture->GI = NewObject<UGameInstance>(GEngine);
    Fixture->GI->AddToRoot();
    Fixture->GI->InitializeStandalone(TEXT("LatestSaveAutomation"));
    UWorld* World = Fixture->GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    for (int32 Slot : { 0, 2, 4 })
    {
        USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
        Snapshot->CurrentLevel = Slot == 0 ? TEXT("OtherMap") : UGameplayStatics::GetCurrentLevelName(Fixture->GI, true);
        Snapshot->SavedAt = Slot == 0 ? FDateTime(2026, 1, 3) : (Slot == 2 ? FDateTime(2026, 1, 2) : FDateTime(2026, 1, 1));
        TestTrue(TEXT("Write isolated candidate snapshot"), UGameplayStatics::SaveGameToSlot(Snapshot, USaveSubsystem::GetSlotName(Slot), 0));
    }
    TestTrue(TEXT("Start latest-map selection"), Fixture->GI->GetSubsystem<USaveSubsystem>()->LoadLatestForCurrentLevel());
    ADD_LATENT_AUTOMATION_COMMAND(LatestSaveTest::FWaitForLatest(Fixture));
    return !HasAnyErrors();
}
#endif
