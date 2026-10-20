#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Audio/AudioSettingsScreen.h"
#include "HorrorAudioSubsystem.h"
#include "HorrorSoundDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorAudioScreenTest, "HorrorFramework.Audio.ScreenLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorAudioScreenTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("AudioScreenAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    const TSubclassOf<UAudioSettingsScreen> ScreenClass = LoadClass<UAudioSettingsScreen>(nullptr,
        TEXT("/Game/_Project/Audio/UI/WBP_AudioSettings.WBP_AudioSettings_C"));
    if (!TestNotNull(TEXT("Configured audio screen is available"), ScreenClass.Get())) return false;
    UAudioSettingsScreen* Screen = CreateWidget<UAudioSettingsScreen>(GI, ScreenClass);
    if (!TestNotNull(TEXT("Audio screen created"), Screen)) return false;
    Screen->TakeWidget();
    TestTrue(TEXT("A nested volume control receives navigation focus"),
        Screen->GetNavigationFocusTarget() != Screen->TakeWidget());
    for (uint8 Index = 0; Index <= static_cast<uint8>(EHorrorAudioChannel::Music); ++Index)
    {
        UHorrorSoundDefinition* Definition = Screen->PreviewSounds.FindRef(static_cast<EHorrorAudioChannel>(Index));
        FText Error;
        if (TestNotNull(TEXT("Every volume group has a preview definition"), Definition))
        {
            TestTrue(TEXT("Preview definition is valid"), Definition->Validate(Error));
            TestTrue(TEXT("Preview remains audible while the world is paused"),
                Definition->PausePolicy == EHorrorAudioPausePolicy::ContinueWhilePaused);
        }
    }
    UHorrorAudioSubsystem* Audio = GI->GetSubsystem<UHorrorAudioSubsystem>();
    const FHorrorAudioVolumes Applied = Audio->GetAppliedVolumes();
    Screen->OnScreenEnter();
    Screen->SetVolume(EHorrorAudioChannel::Music, .23f);
    TestTrue(TEXT("Screen changes live playback volumes"), FMath::IsNearlyEqual(Audio->GetVolumes().Music, .23f));
    TestTrue(TEXT("Screen preview preserves applied preferences"), Audio->GetAppliedVolumes().Equals(Applied));
    Screen->OnScreenExit();
    TestTrue(TEXT("Exiting the screen restores the last applied volumes"), Audio->GetVolumes().Equals(Applied));
    TestTrue(TEXT("Exiting releases the exclusive edit owner"), Audio->BeginVolumeEdit(GI));
    Audio->CancelVolumeEdit(GI);
    Screen->RemoveFromParent();
    return !HasAnyErrors();
}
#endif
