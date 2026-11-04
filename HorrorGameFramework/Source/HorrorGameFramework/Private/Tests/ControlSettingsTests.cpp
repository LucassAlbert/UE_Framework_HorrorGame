#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "HorrorControlSettingsSubsystem.h"
#include "ControlLookModifier.h"
#include "Controls/ControlSettingsScreen.h"
#include "Controls/MainSettingsScreen.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Audio/AudioSettingsScreen.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UIManagerSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Streaming/StreamingEnemy.h"
#include "HorrorEnemyController.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

namespace ControlSettingsTest
{
    UWorld* World()
    {
        for(const FWorldContext& Context:GEngine->GetWorldContexts())if(Context.World() && Context.World()->IsGameWorld())return Context.World();return nullptr;
    }
    const FHorrorBindingDefinition* Binding(const UControlPreferencesDefinition* Definition,FKey Key)
    {
        return Definition->Bindings.FindByPredicate([Key](const auto& Row){return Row.DefaultKey==Key;});
    }
    void SlateKey(FKey Key)
    {
        const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
        FSlateApplication::Get().ProcessKeyDownEvent(Event);FSlateApplication::Get().ProcessKeyUpEvent(Event);
    }
    class FSettingsTabs : public IAutomationLatentCommand
    {
    public:
        explicit FSettingsTabs(FAutomationTestBase* InTest) : Test(InTest) {}

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Now < WaitUntil) return false;
            UWorld* W = World();
            UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
            auto* UI = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
            auto* Flow = GI ? GI->GetSubsystem<UGameFlowSubsystem>() : nullptr;
            auto* Saves = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
            if (!UI || !Flow || !Saves || !Saves->IsWorldReady()) return false;
            if (Phase == 0)
            {
                if (Flow->CurrentState != EGameFlowState::Playing) return false;
                for (TActorIterator<AStreamingEnemy> It(W); It; ++It)
                {
                    It->Memory.State = EHorrorEnemyState::Disabled;
                    CastChecked<AHorrorEnemyController>(It->GetController())->StopMovement();
                }
                UI->OpenPauseUI();
                UI->OpenSettingsUI();
                Hub = Cast<UMainSettingsScreen>(UI->GetTopScreen());
                if (!Test->TestNotNull(TEXT("Settings template exists"), Hub.Get())) return true;
                Capture(TEXT("SettingsVideo.png"));
                Phase = 1;
                WaitUntil = Now + .5;
                return false;
            }
            if (!Hub.IsValid())
            {
                Test->AddError(TEXT("Settings host disappeared during tab switching."));
                return true;
            }
            auto* Audio = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Audio")));
            auto* Controls = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Controls")));
            auto* Back = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Back")));
            if (!Audio || !Controls || !Back)
            {
                Test->AddError(TEXT("Settings navigation buttons are missing."));
                return true;
            }
            Test->TestTrue(TEXT("Sidebar remains visible across all tabs"), Audio->IsVisible() && Controls->IsVisible() && Back->IsVisible() && Hub->IsVisible());
            Test->TestTrue(TEXT("Tab switching keeps one settings screen in the UI stack"), UI->GetTopScreen() == Hub.Get());
            if (Phase == 1)
            {
                Audio->OnClicked.Broadcast();
                Capture(TEXT("SettingsAudio.png"));
            }
            else if (Phase == 2)
            {
                CheckPageGeometry(Back);
                Controls->OnClicked.Broadcast();
                Capture(TEXT("SettingsControls.png"));
            }
            else
            {
                CheckPageGeometry(Back);
                UUIScreenBase* Page = Hub->GetActivePage();
                if (Test->TestNotNull(TEXT("Embedded controls page exists"), Page))
                {
                    auto* LocalUser = W->GetFirstPlayerController()->GetLocalPlayer();
                    auto* Preferences = LocalUser->GetSubsystem<UHorrorControlSettingsSubsystem>();
                    auto* SettingsPage = CastChecked<UControlSettingsScreen>(Page);
                    SettingsPage->SetNumber(0, 80);
                    SettingsPage->BeginCapture(Preferences->GetDefinition()->Bindings[0].Id);
                    Audio->OnClicked.Broadcast();
                    Test->TestTrue(TEXT("Switching tabs cancels camera preview and key capture"), FMath::IsNearlyEqual(Preferences->GetPreferences().FieldOfView, 90.f));
                    UObject* Owner = NewObject<UControlPreferencesSave>(GI);
                    Test->TestTrue(TEXT("Switching tabs releases control editing ownership"), Preferences->BeginEdit(Owner));
                    Preferences->Cancel(Owner);
                }
                Back->OnClicked.Broadcast();
                UI->PopScreen();
                Test->TestTrue(TEXT("Closing settings and pause restores gameplay input ownership"), !UI->HasModalScreen() && !W->IsPaused());
                return true;
            }
            ++Phase;
            WaitUntil = Now + .5;
            return false;
        }

    private:
        void Capture(const TCHAR* Filename)
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("ControlsCaptureRender")))
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Filename, true, false);
        }
        void CheckPageGeometry(UButton* Back)
        {
            UUIScreenBase* Page = Hub->GetActivePage();
            if (!Test->TestNotNull(TEXT("Active page is hosted in the template"), Page)) return;
            Test->TestTrue(TEXT("Hosted page uses the template panel instead of a viewport layer"), Page->IsEmbeddedInScreen() && !Page->IsInViewport() && Page->GetParent() != nullptr);
            if (FParse::Param(FCommandLine::Get(), TEXT("ControlsCaptureRender")))
            {
                const FGeometry& Nav = Back->GetCachedGeometry();
                const FGeometry& Content = Page->GetCachedGeometry();
                Test->TestTrue(TEXT("Navigation and content occupy separate visible columns"), Content.GetLocalSize().X > 0 && Nav.LocalToAbsolute(Nav.GetLocalSize()).X <= Content.LocalToAbsolute(FVector2D::ZeroVector).X + 2.f);
            }
            auto* Reference = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Back")));
            Page->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Widget)
            {
                if (auto* Button = Cast<UButton>(Widget))
                    Test->TestTrue(TEXT("Embedded buttons inherit the template outline and fill"), Button->GetStyle().Normal == Reference->GetStyle().Normal);
            });
        }
        FAutomationTestBase* Test;
        TWeakObjectPtr<UMainSettingsScreen> Hub;
        int32 Phase = 0;
        double WaitUntil = 0;
    };

    class FPreferences : public IAutomationLatentCommand
    {
    public:
        FPreferences(FAutomationTestBase* InTest,bool bInCold) : Test(InTest), bCold(bInCold), Deadline(FPlatformTime::Seconds()+100) {}
        virtual bool Update() override
        {
            const double Now=FPlatformTime::Seconds();
            if(Now>Deadline){Test->AddError(FString::Printf(TEXT("Controls timed out in phase %d"),Phase));return true;}
            UWorld* W=World();auto* GI=W?W->GetGameInstance():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
            auto* LocalUser=PC?PC->GetLocalPlayer():nullptr;APawn* Player=PC?PC->GetPawn():nullptr;
            if(!GI || !LocalUser || !Player)return false;
            auto* Settings=LocalUser->GetSubsystem<UHorrorControlSettingsSubsystem>();auto* Input=LocalUser->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
            auto* UI=GI->GetSubsystem<UUIManagerSubsystem>();auto* Flow=GI->GetSubsystem<UGameFlowSubsystem>();auto* Saves=GI->GetSubsystem<USaveSubsystem>();
            if(!Saves->IsWorldReady() || Saves->IsBusy())return false;
            if(!Settings || !Settings->IsReady()){Test->AddError(TEXT("Control preferences Data Asset is missing or invalid."));return true;}
            auto Check=[&](bool Value,const TCHAR* Message){Test->TestTrue(Message,Value);return Value;};
            const auto* Definition=Settings->GetDefinition();const auto* Interact=Binding(Definition,EKeys::E);const auto* PadInteract=Binding(Definition,EKeys::Gamepad_FaceButton_Bottom);
            auto* Camera=Player->FindComponentByClass<UCameraComponent>();
            auto* Source=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/_Project/Inventory/Input/IMC_InventoryDemo.IMC_InventoryDemo"));
            auto* Runtime=Settings->GetRuntimeContext(Source);
            if(!Check(Interact && PadInteract && Camera && Runtime,TEXT("Settings configuration, camera and local mapping are available")))return true;
            if(bCold || Phase==2)
            {
                if(Flow->CurrentState!=EGameFlowState::Playing)return false;
                Check(FMath::IsNearlyEqual(Settings->GetPreferences().FieldOfView,105.f) && FMath::IsNearlyEqual(Camera->FieldOfView,105.f),TEXT("Applied FOV is restored on the newly created pawn"));
                Check(Settings->GetPreferences().Bindings.FindChecked(Interact->Id)==EKeys::C,TEXT("Applied command survives world/process restart"));
                Check(Settings->GetPreferences().Mouse.bInvertY && FMath::IsNearlyEqual(Settings->GetPreferences().Mouse.SensitivityX,2.f),TEXT("Mouse preferences survive independently"));
                Check(Settings->GetPreferences().Bindings.FindChecked(PadInteract->Id)==EKeys::Gamepad_FaceButton_Right && Settings->GetPreferences().Gamepad.bInvertX && FMath::IsNearlyEqual(Settings->GetPreferences().Gamepad.SensitivityX,1.4f) && FMath::IsNearlyEqual(Settings->GetPreferences().Gamepad.SensitivityY,.8f),TEXT("Gamepad remapping, inversion and independent X/Y sensitivity survive restart"));
                Check(Input->QueryKeysMappedToAction(Interact->Action.Get()).Contains(EKeys::Gamepad_FaceButton_Right),TEXT("Restored gamepad binding is active in Enhanced Input"));
                Check(!Input->HasMappingContext(Source) && Input->HasMappingContext(Runtime),TEXT("Only the player-owned context is active after restart"));
                Check(!UI->HasModalScreen() && !W->IsPaused(),TEXT("Restart releases menu pause and editing"));
                return true;
            }
            if(Phase==0)
            {
                if(Flow->CurrentState!=EGameFlowState::Playing)return false;
                for(TActorIterator<AStreamingEnemy> It(W);It;++It){It->Memory.State=EHorrorEnemyState::Disabled;CastChecked<AHorrorEnemyController>(It->GetController())->StopMovement();}
                Check(Runtime!=Source && !Input->HasMappingContext(Source),TEXT("Input uses an isolated clone without original context duplication"));
                UI->OpenPauseUI();
                UUIScreenBase* Pause = UI->GetTopScreen();
                UI->OpenSettingsUI();
                auto* Hub = Cast<UMainSettingsScreen>(UI->GetTopScreen());
                if (!Check(Hub && Hub->GetClass()->GetName() == TEXT("WBP_MainSettings_C"), TEXT("Settings opens the original template"))) return true;
                auto* BackButton = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Back")));
                if (!Check(BackButton != nullptr, TEXT("Template exposes its Back button"))) return true;
                BackButton->OnClicked.Broadcast();
                if (!Check(UI->GetTopScreen() == Pause && W->IsPaused(), TEXT("Template Back restores the pause screen"))) return true;

                UI->OpenSettingsUI();
                Hub = Cast<UMainSettingsScreen>(UI->GetTopScreen());
                auto* AudioButton = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Audio")));
                auto* VideoButton = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Video")));
                auto* ControlsButton = Cast<UButton>(Hub->GetWidgetFromName(TEXT("Button_Controls")));
                if (!Check(AudioButton && VideoButton && ControlsButton, TEXT("Template exposes all settings categories"))) return true;
                VideoButton->OnClicked.Broadcast();
                UWidget* Video = Hub->GetWidgetFromName(TEXT("ScrollBox_VIDEO"));
                Check(Video && Video->IsVisible() && UI->GetTopScreen() == Hub, TEXT("Video stays in the template"));

                AudioButton->OnClicked.Broadcast();
                auto* Audio = Cast<UAudioSettingsScreen>(Hub->GetActivePage());
                if (!Check(Audio && W->IsPaused(), TEXT("Template Audio opens the framework volume screen"))) return true;
                const float AppliedVolume = Audio->GetVolume(EHorrorAudioChannel::Master);
                const float PreviewVolume = AppliedVolume > 0.5f ? 0.25f : 0.75f;
                Audio->SetVolume(EHorrorAudioChannel::Master, PreviewVolume);
                Check(FMath::IsNearlyEqual(Audio->GetVolume(EHorrorAudioChannel::Master), PreviewVolume), TEXT("Audio previews volume through the existing subsystem"));
                VideoButton->OnClicked.Broadcast();
                if (!Check(UI->GetTopScreen() == Hub && W->IsPaused(), TEXT("Audio tab closes inside the persistent template"))) return true;
                AudioButton->OnClicked.Broadcast();
                Audio = Cast<UAudioSettingsScreen>(Hub->GetActivePage());
                if (!Check(Audio && FMath::IsNearlyEqual(Audio->GetVolume(EHorrorAudioChannel::Master), AppliedVolume), TEXT("Leaving Audio without Apply rolls back volume"))) return true;
                VideoButton->OnClicked.Broadcast();
                ControlsButton->OnClicked.Broadcast();
                auto* Screen=Cast<UControlSettingsScreen>(Hub->GetActivePage());if(!Check(Screen && W->IsPaused() && UI->GetTopScreen()==Hub,TEXT("Controls are embedded inside the paused settings template")))return true;
                auto* OtherOwner=NewObject<UControlPreferencesSave>(GI);Check(!Settings->BeginEdit(OtherOwner),TEXT("Concurrent edits cannot steal the transaction"));
                Screen->SetNumber(0,105);Screen->SetNumber(1,2);Screen->SetNumber(2,1.5f);Screen->SetNumber(3,1.4f);Screen->SetNumber(4,.8f);
                auto Value=Settings->GetPreferences();Value.Mouse.bInvertY=true;Value.Gamepad.bInvertX=true;FText Error;Check(Settings->Preview(Screen,Value,Error),TEXT("Preview inversion with the screen's edit ownership"));
                Check(FMath::IsNearlyEqual(Camera->FieldOfView,105.f),TEXT("FOV preview reaches the camera while paused"));
                Screen->BeginCapture(Interact->Id);if(!Check(Screen->SubmitKey(EKeys::C),TEXT("Keyboard capture swaps a conflicting command")))return true;
                Screen->BeginCapture(PadInteract->Id);SlateKey(EKeys::Gamepad_FaceButton_Right);
                if(!Check(!Screen->IsCapturing() && Settings->GetPreferences().Bindings[PadInteract->Id]==EKeys::Gamepad_FaceButton_Right && UI->GetTopScreen()==Hub,TEXT("B can be captured for gameplay without navigating Back")))return true;
                Screen->BeginCapture(Interact->Id);SlateKey(EKeys::Escape);
                Check(!Screen->IsCapturing() && UI->GetTopScreen()==Hub,TEXT("Escape cancels only the pending key capture"));
                const auto Keys=Input->QueryKeysMappedToAction(Interact->Action.Get());
                Check(Keys.Contains(EKeys::C) && !Keys.Contains(EKeys::E),TEXT("Enhanced Input exposes the new physical mapping, not the previous key"));
                Check(Source->GetMappings().ContainsByPredicate([&](const auto& Mapping){return Mapping.Action==Interact->Action.Get() && Mapping.Key==EKeys::E;}),TEXT("Original shared mapping asset stays unchanged"));
                const auto* Look=Runtime->GetMappings().FindByPredicate([&](const auto& Mapping){return Mapping.Action && FSoftObjectPath(Mapping.Action)==Definition->LookAction.ToSoftObjectPath() && !Mapping.Key.IsGamepadKey();});
                Check(Look && Look->Modifiers.ContainsByPredicate([](const auto& Modifier){return Modifier && Modifier->IsA(UControlLookModifier::StaticClass());}),TEXT("Look preferences are applied at the mapping's device boundary"));
                for(int32 Index=0;Index<Source->GetMappings().Num();++Index)
                {
                    const auto& Original=Source->GetMappings()[Index];
                    if(!Original.Action || FSoftObjectPath(Original.Action)!=Definition->LookAction.ToSoftObjectPath())continue;
                    for(float Delta:{1.f/15,1.f/60,1.f/120})
                    {
                        FInputActionValue Baseline(FVector2D(.6,.35)),Actual=Baseline;
                        for(const auto& Modifier:Original.Modifiers)if(Modifier)Baseline=Modifier->ModifyRaw(Input->GetPlayerInput(),Baseline,Delta);
                        for(const auto& Modifier:Runtime->GetMappings()[Index].Modifiers)if(Modifier)Actual=Modifier->ModifyRaw(Input->GetPlayerInput(),Actual,Delta);
                        const auto Preferences=Settings->GetPreferences();
                        const FVector Expected=UControlLookModifier::Transform(Original.Key.IsGamepadKey()?Preferences.Gamepad:Preferences.Mouse,Baseline.Get<FVector>());
                        Check(Actual.Get<FVector>().Equals(Expected,.0001),TEXT("Mouse/gamepad modifiers preserve source filtering and timing at different frame rates"));
                    }
                }
                SlateKey(EKeys::Gamepad_FaceButton_Right);Check(UI->GetTopScreen()==Pause,TEXT("Outside capture, B closes settings and restores the pause menu"));
                Check(FMath::IsNearlyEqual(Camera->FieldOfView,90.f) && Settings->GetPreferences().Bindings[Interact->Id]==EKeys::E && !Settings->GetPreferences().Mouse.bInvertY,TEXT("Back restores FOV, key swap and inversion together"));
                Screen=Cast<UControlSettingsScreen>(UI->OpenScreen(EHorrorScreen::ControlSettings));
                Screen->BeginCapture(Interact->Id);
                if(FParse::Param(FCommandLine::Get(),TEXT("ControlsCaptureRender")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ControlSettings.png"),true,false);
                WaitUntil=Now+10.3;Phase=1;return false;
            }
            if(Phase==1)
            {
                if(Now<WaitUntil)return false;
                auto* Screen=Cast<UControlSettingsScreen>(UI->GetTopScreen());if(!Check(Screen && !Screen->IsCapturing() && W->IsPaused(),TEXT("Capture times out even while gameplay is paused")))return true;
                Screen->SetNumber(0,105);Screen->SetNumber(1,2);Screen->SetNumber(3,1.4f);Screen->SetNumber(4,.8f);auto Value=Settings->GetPreferences();Value.Mouse.bInvertY=true;Value.Gamepad.bInvertX=true;FText Error;Settings->Preview(Screen,Value,Error);
                Screen->BeginCapture(Interact->Id);Screen->SubmitKey(EKeys::C);
                Screen->BeginCapture(PadInteract->Id);Screen->SubmitKey(EKeys::Gamepad_FaceButton_Right);
                Screen->ProcessEvent(Screen->FindFunctionChecked(TEXT("Apply")),nullptr);
                if(!Check(UGameplayStatics::DoesSaveGameExist(Settings->GetStorageSlot(),0),TEXT("Apply writes a separate control-preferences file")))return true;
                auto* Saved=Cast<UControlPreferencesSave>(UGameplayStatics::LoadGameFromSlot(Settings->GetStorageSlot(),0));
                Check(Saved && Saved->Version==1 && FMath::IsNearlyEqual(Saved->Preferences.FieldOfView,105.f),TEXT("Applied values are actually serialized"));
                for(int32 I=0;I<USaveSubsystem::SlotCount;++I)Check(!UGameplayStatics::DoesSaveGameExist(USaveSubsystem::GetSlotName(I),0),TEXT("Control preferences do not occupy progress slots"));
                Screen->ProcessEvent(Screen->FindFunctionChecked(TEXT("Defaults")),nullptr);Check(FMath::IsNearlyEqual(Camera->FieldOfView,90.f),TEXT("Restore defaults is previewed without saving"));
                UI->PopScreen();Check(FMath::IsNearlyEqual(Camera->FieldOfView,105.f) && Settings->GetPreferences().Bindings[Interact->Id]==EKeys::C,TEXT("Cancelling defaults returns to the last applied values"));
                Screen=Cast<UControlSettingsScreen>(UI->OpenScreen(EHorrorScreen::ControlSettings));Screen->SetNumber(0,80);
                Flow->RestartLevel();Phase=2;Deadline=Now+100;return false;
            }
            return false;
        }
    private:
        FAutomationTestBase* Test;
        bool bCold;
        int32 Phase=0;
        double Deadline,WaitUntil=0;
    };
    bool Run(FAutomationTestBase* Test,bool bCold)
    {
        const TCHAR* Flag=bCold?TEXT("ControlsColdRead"):TEXT("ControlsRuntimeTest");
        if(!FParse::Param(FCommandLine::Get(),Flag)){Test->AddInfo(TEXT("Opt-in controls protocol: use an isolated HorrorControlsTest UserDir and runtime/cold flag."));return true;}
        FString UserDir;FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir);
        if(!UserDir.Contains(TEXT("HorrorControlsTest"))){Test->AddError(TEXT("An isolated HorrorControlsTest directory is required."));return false;}
        if(!bCold && UGameplayStatics::DoesSaveGameExist(TEXT("HorrorControlPreferences_0"),0)){Test->AddError(TEXT("Existing preferences will not be overwritten. Use a fresh test UserDir."));return false;}
        if(bCold && !UGameplayStatics::DoesSaveGameExist(TEXT("HorrorControlPreferences_0"),0)){Test->AddError(TEXT("Cold read requires the previous isolated runtime test."));return false;}
        if (!bCold) Test->AddCommand(new FSettingsTabs(Test));
        Test->AddCommand(new FPreferences(Test,bCold));return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorControlsRuntimeTest,"HorrorFramework.Controls.Runtime",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorControlsRuntimeTest::RunTest(const FString& Parameters){return ControlSettingsTest::Run(this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorControlsColdTest,"HorrorFramework.Controls.ColdRead",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorControlsColdTest::RunTest(const FString& Parameters){return ControlSettingsTest::Run(this,true);}
#endif
