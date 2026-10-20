#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "AudioPreferenceStore.h"
#include "HorrorAudioSubsystem.h"
#include "HorrorSoundDefinition.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"

namespace AudioTests
{
    UGameInstance* CreateInstance()
    {
        UGameInstance* GI = NewObject<UGameInstance>(GEngine);
        GI->InitializeStandalone(TEXT("AudioAutomation"));
        GI->GetWorld()->InitializeActorsForPlay(FURL());
        return GI;
    }
    void DestroyInstance(UGameInstance* GI)
    {
        UWorld* World = GI->GetWorld();
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorAudioPreferencesTest, "HorrorFramework.Audio.Preferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorAudioPreferencesTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = AudioTests::CreateInstance();
    ON_SCOPE_EXIT { AudioTests::DestroyInstance(GI); };
    UHorrorAudioSubsystem* Audio = GI->GetSubsystem<UHorrorAudioSubsystem>();
    const FString Path = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("HorrorAudioPreferences_Test_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
    Audio->PreferencesPath = Path;
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Path, false, false, true); };
    UObject* Owner = NewObject<USceneComponent>(GI);
    UObject* Other = NewObject<USceneComponent>(GI);
    TestTrue(TEXT("Begin exclusive editing"), Audio->BeginVolumeEdit(Owner));
    TestFalse(TEXT("Another editor cannot overwrite a preview"), Audio->BeginVolumeEdit(Other));
    const FHorrorAudioVolumes Initial = Audio->GetAppliedVolumes();
    UHorrorSoundDefinition* GainDefinition = NewObject<UHorrorSoundDefinition>(GI);
    GainDefinition->Channel = EHorrorAudioChannel::Effects;
    GainDefinition->Volume = .8f;
    UAudioComponent* GainComponent = NewObject<UAudioComponent>(GI->GetWorld());
    FHorrorAudioPlayback GainPlayback;
    GainPlayback.Definition = GainDefinition;
    GainPlayback.Component = GainComponent;
    GainPlayback.World = GI->GetWorld();
    Audio->Playbacks.Add(FGuid::NewGuid(), GainPlayback);
    FHorrorAudioVolumes Preview = Initial;
    Preview.Master = .5f;
    Preview.Effects = .25f;
    TestTrue(TEXT("Preview accepted"), Audio->PreviewVolumes(Owner, Preview));
    TestTrue(TEXT("Existing components receive master, group and definition gain"), FMath::IsNearlyEqual(GainComponent->VolumeMultiplier, .1f));
    TestTrue(TEXT("Preview changes playback but not applied values"), Audio->GetVolumes().Equals(Preview) && Audio->GetAppliedVolumes().Equals(Initial));
    TestFalse(TEXT("Preview does not write a file"), IFileManager::Get().FileExists(*Path));
    TestTrue(TEXT("Master multiplies the selected group"), FMath::IsNearlyEqual(Preview.GetPlaybackGain(EHorrorAudioChannel::Effects), .125f));
    TestFalse(TEXT("Other owners cannot preview"), Audio->PreviewVolumes(Other, Initial));
    FHorrorAudioVolumes Invalid = Preview;
    Invalid.Music = 2.f;
    TestFalse(TEXT("Invalid values rejected without changing the preview"), Audio->PreviewVolumes(Owner, Invalid));
    TestTrue(TEXT("Preview remains valid"), Audio->GetVolumes().Equals(Preview));
    FHorrorAudioVolumes Muted = Preview;
    Muted.Master = 0.f;
    Audio->PreviewVolumes(Owner, Muted);
    TestTrue(TEXT("Master zero silences existing components"), FMath::IsNearlyZero(GainComponent->VolumeMultiplier));
    Audio->PreviewVolumes(Owner, Preview);
    TestTrue(TEXT("Restoring the master restores component gain"), FMath::IsNearlyEqual(GainComponent->VolumeMultiplier, .1f));
    Audio->CancelVolumeEdit(Owner);
    TestTrue(TEXT("Closing without Apply restores last applied values"), Audio->GetVolumes().Equals(Initial));
    TestTrue(TEXT("A new editing session may begin"), Audio->BeginVolumeEdit(Owner));
    Audio->PreviewVolumes(Owner, Preview);
    FText ApplyError;
    TestTrue(TEXT("Apply confirms successful file replacement"), Audio->ApplyVolumes(Owner, ApplyError));
    FHorrorAudioVolumes Loaded;
    FString Error;
    TestTrue(TEXT("Read persisted preferences"), HorrorAudioPreferences::Load(Path, Loaded, Error));
    TestTrue(TEXT("Applied values survive serialization"), Loaded.Equals(Preview));
    Audio->PreviewVolumes(Owner, Initial);
    Audio->CancelVolumeEdit(Owner);
    TestTrue(TEXT("Cancel after Apply retains the most recently applied values"), Audio->GetVolumes().Equals(Preview));
    FFileHelper::SaveStringToFile(TEXT("{\"version\":999}"), *Path);
    Loaded = Initial;
    TestFalse(TEXT("Unknown version is rejected"), HorrorAudioPreferences::Load(Path, Loaded, Error));
    TestTrue(TEXT("Rejected preferences do not partially update values"), Loaded.Equals(Initial));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorAudioPlaybackTest, "HorrorFramework.Audio.PlaybackLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorAudioPlaybackTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = AudioTests::CreateInstance();
    ON_SCOPE_EXIT { AudioTests::DestroyInstance(GI); };
    UWorld* World = GI->GetWorld();
    UHorrorAudioSubsystem* Audio = GI->GetSubsystem<UHorrorAudioSubsystem>();
    UHorrorSoundDefinition* Definition = NewObject<UHorrorSoundDefinition>(GI);
    FText Error;
    TestFalse(TEXT("Missing sound rejected"), Definition->Validate(Error));
    Definition->Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/SimpleSettingsMenu/Sounds/SFX/SFX_RevolverShot.SFX_RevolverShot")));
    Definition->bSpatialized = true;
    TestTrue(TEXT("Spatial settings can be authored directly in the definition"), Definition->Validate(Error));
    Definition->bUseAttenuationAsset = true;
    TestFalse(TEXT("Spatial sound requires explicit attenuation"), Definition->Validate(Error));
    Definition->bSpatialized = false;
    TestTrue(TEXT("Valid nonspatial definition accepted"), Definition->Validate(Error));
    const FGuid Pending = Audio->PlaySound(World, Definition, FVector::ZeroVector);
    TestTrue(TEXT("Request has a stable handle"), Pending.IsValid() && Audio->HasPlayback(Pending));
    Audio->StopSound(Pending, false);
    TestFalse(TEXT("Stopping during loading removes pending playback"), Audio->HasPlayback(Pending));
    Audio->FinishLoading(Pending);
    TestFalse(TEXT("A late callback cannot restart a cancelled request"), Audio->HasPlayback(Pending));
    AActor* Owner = World->SpawnActor<AActor>();
    USceneComponent* Attachment = NewObject<USceneComponent>(Owner);
    Attachment->RegisterComponentWithWorld(World);
    const FGuid Attached = Audio->PlaySound(World, Definition, FVector::ZeroVector, Attachment);
    Audio->HandleAttachmentEndPlay(Owner, EEndPlayReason::Destroyed);
    TestFalse(TEXT("Owner end play cancels attached requests"), Audio->HasPlayback(Attached));
    const FGuid BeforeCleanup = Audio->PlaySound(World, Definition, FVector::ZeroVector);
    Audio->HandleWorldCleanup(World, true, true);
    TestFalse(TEXT("Map cleanup cancels its audio requests"), Audio->HasPlayback(BeforeCleanup));
    TestTrue(TEXT("Cancelled loads release streamable handles"), Audio->PendingLoads.IsEmpty());
    TestTrue(TEXT("Managed channel routing is independent of legacy sound mixes"), Audio->ChannelClasses.Num() == 4);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorAudioColdRestartTest, "HorrorFramework.Audio.ColdRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorAudioColdRestartTest::RunTest(const FString& Parameters)
{
    const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("AudioTestWrite"));
    const bool bRead = FParse::Param(FCommandLine::Get(), TEXT("AudioTestRead"));
    if (!bWrite && !bRead) { AddInfo(TEXT("Run separate processes with AudioTestWrite/AudioTestRead and a unique -UserDir containing HorrorAudioTest_.")); return true; }
    if (bWrite == bRead || !FPaths::ProjectSavedDir().Contains(TEXT("HorrorAudioTest_")))
    { AddError(TEXT("Use exactly one phase and an isolated HorrorAudioTest_ UserDir.")); return false; }
    UGameInstance* GI = AudioTests::CreateInstance();
    ON_SCOPE_EXIT { AudioTests::DestroyInstance(GI); };
    UHorrorAudioSubsystem* Audio = GI->GetSubsystem<UHorrorAudioSubsystem>();
    FHorrorAudioVolumes Expected;
    Expected.Master = .37f;
    Expected.Ambience = .52f;
    Expected.Effects = .63f;
    Expected.Interface = .74f;
    Expected.Music = .85f;
    if (bWrite)
    {
        if (IFileManager::Get().FileExists(*UHorrorAudioSubsystem::GetPreferencesPath()))
        { AddError(TEXT("Refusing to overwrite an existing preferences file; use a fresh test UserDir.")); return false; }
        Audio->BeginVolumeEdit(GI);
        Audio->PreviewVolumes(GI, Expected);
        FText Error;
        TestTrue(TEXT("Write applied audio preferences"), Audio->ApplyVolumes(GI, Error));
    }
    else TestTrue(TEXT("Fresh process loads applied preferences without an editor or world save"), Audio->GetAppliedVolumes().Equals(Expected));
    return !HasAnyErrors();
}
#endif
