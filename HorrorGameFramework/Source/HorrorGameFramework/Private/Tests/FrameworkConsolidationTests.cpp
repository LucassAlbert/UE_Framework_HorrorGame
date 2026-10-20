#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UIManagerSubsystem.h"
#include "UIScreenBase.h"
#include "Core/HorrorPlayerController.h"
#include "Inventory/InventoryCharacter.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Widgets/SWindow.h"

namespace ConsolidationTest
{
    UWorld* CreateWorld(UGameInstance*& GI, const TCHAR* Name)
    {
        GI = NewObject<UGameInstance>(GEngine);
        GI->InitializeStandalone(FName(Name));
        UWorld* World = GI->GetWorld();
        FURL URL;
        URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
        World->SetGameMode(URL);
        World->InitializeActorsForPlay(URL);
        return World;
    }
    void DestroyWorld(UGameInstance* GI, UWorld* World)
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    UUIScreenBase* CreateScreen(UGameInstance* GI)
    {
        UUIScreenBase* Screen = CreateWidget<UUIScreenBase>(GI);
        Screen->SetIsFocusable(true);
        return Screen;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorUIControlOwnershipTest, "HorrorFramework.Consolidation.UIOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorUIControlOwnershipTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI;
    UWorld* World = ConsolidationTest::CreateWorld(GI, TEXT("UIOwnershipAutomation"));
    ON_SCOPE_EXIT { ConsolidationTest::DestroyWorld(GI, World); };
    APlayerController* First = World->SpawnActor<APlayerController>();
    First->PlayerState = World->SpawnActor<APlayerState>();
    UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>();
    World->GetWorldSettings()->NotifyBeginPlay();
    UUIScreenBase* Screen = ConsolidationTest::CreateScreen(GI);
    if (!TestNotNull(TEXT("Headless presentation fixture created"), Screen)) return false;
    // Only control ownership is under test; rendering/viewport input is exercised in PIE and the package.
    UI->ScreenStack.Add(Screen);
    UI->ApplyScreenState();
    TestTrue(TEXT("Modal owns move and look locks"), First->IsMoveInputIgnored() && First->IsLookInputIgnored());
    TestTrue(TEXT("Modal pauses the world"), World->IsPaused());
    UI->ApplyScreenState();
    UI->ApplyScreenState();
    UI->ClearScreens();
    TestFalse(TEXT("Reapplying the stack does not accumulate move locks"), First->IsMoveInputIgnored());
    TestFalse(TEXT("Reapplying the stack does not accumulate look locks"), First->IsLookInputIgnored());
    TestFalse(TEXT("Closing releases owned pause"), World->IsPaused());
    First->SetIgnoreMoveInput(true);
    First->SetIgnoreLookInput(true);
    First->SetPause(true);
    UI->ScreenStack.Add(Screen);
    UI->ApplyScreenState();
    UI->ClearScreens();
    TestTrue(TEXT("External move and look locks survive UI close"), First->IsMoveInputIgnored() && First->IsLookInputIgnored());
    TestTrue(TEXT("Externally owned pause survives UI close"), World->IsPaused());
    First->SetIgnoreMoveInput(false);
    First->SetIgnoreLookInput(false);
    First->SetPause(false);
    UI->ScreenStack.Add(Screen);
    UI->ApplyScreenState();
    World->RemoveController(First);
    UI->ClearScreens();
    TestFalse(TEXT("Missing current controller still releases original owner's input"), First->IsMoveInputIgnored() || First->IsLookInputIgnored());
    TestFalse(TEXT("Missing current controller still releases owned pause"), World->IsPaused());
    World->AddController(First);
    APlayerController* Second = World->SpawnActor<APlayerController>();
    Second->PlayerState = World->SpawnActor<APlayerState>();
    UI->ScreenStack.Add(Screen);
    UI->ApplyScreenState();
    World->RemoveController(First);
    UI->ApplyScreenState();
    TestFalse(TEXT("Switching controllers releases the original owner"), First->IsMoveInputIgnored() || First->IsLookInputIgnored());
    TestTrue(TEXT("The new controller receives its own locks"), Second->IsMoveInputIgnored() && Second->IsLookInputIgnored());
    UI->ClearScreens();
    TestFalse(TEXT("The new controller is released normally"), Second->IsMoveInputIgnored() || Second->IsLookInputIgnored());
    World->AddController(First);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorFrameworkLifecycleTest, "HorrorFramework.Consolidation.PlayerLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorFrameworkLifecycleTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI;
    UWorld* World = ConsolidationTest::CreateWorld(GI, TEXT("PlayerLifecycleAutomation"));
    ON_SCOPE_EXIT { ConsolidationTest::DestroyWorld(GI, World); };
    AHorrorPlayerController* PC = World->SpawnActor<AHorrorPlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    AInventoryCharacter* First = World->SpawnActor<AInventoryCharacter>();
    AInventoryCharacter* Second = World->SpawnActor<AInventoryCharacter>();
    // A single-player save cannot contain two actors with the same persistent player role.
    // This fixture tests possession only and never captures that two-pawn world.
    PC->Possess(First);
    World->GetWorldSettings()->NotifyBeginPlay();
    UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>();
    UGameFlowSubsystem* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
    Flow->SetGameState(EGameFlowState::Playing);
    PC->SetControlState(EPlayerControlState::Hidden);
    TestTrue(TEXT("Hiding begins a timed transition"), PC->InAnimation);
    PC->Possess(Second);
    TestFalse(TEXT("Possession cancels old hiding animation"), PC->InAnimation);
    TestFalse(TEXT("Possession clears old hiding timer"), World->GetTimerManager().IsTimerActive(PC->HideTransitionTimer));
    TestTrue(TEXT("New pawn resumes gameplay state"), PC->CurrentState == EPlayerControlState::Gameplay);
    UUIScreenBase* Screen = ConsolidationTest::CreateScreen(GI);
    if (!TestNotNull(TEXT("Modal fixture created"), Screen)) return false;
    UI->ScreenStack.Add(Screen);
    UI->ApplyScreenState();
    PC->HandleScreenStackChanged(true);
    PC->FinishHideExit();
    TestTrue(TEXT("Completing hide exit cannot bypass an open modal"), PC->CurrentState == EPlayerControlState::UI);
    UI->ReleasePlayerPresentation(PC);
    TestFalse(TEXT("Releasing the presentation removes stale screens"), UI->HasModalScreen());
    TestFalse(TEXT("Releasing the presentation restores move input"), PC->IsMoveInputIgnored());
    Second->GetCharacterMovement()->Velocity = FVector(450.f, 0.f, 0.f);
    Second->AddMovementInput(FVector::ForwardVector, 1.f);
    PC->SetControlState(EPlayerControlState::Hidden);
    Second->OnSaveStateRestored();
    TestTrue(TEXT("Restore discards old movement velocity"), Second->GetCharacterMovement()->Velocity.IsNearlyZero());
    TestTrue(TEXT("Restore discards pending movement input"), Second->GetPendingMovementInputVector().IsNearlyZero());
    TestFalse(TEXT("Restore cancels hiding transition"), PC->InAnimation);
    TestTrue(TEXT("Restore returns the view to the pawn"), PC->GetViewTarget() == Second);
    Flow->SetGameState(EGameFlowState::GameOver);
    PC->Move(FInputActionValue(FVector2D(0.f, 1.f)));
    TestTrue(TEXT("Game over rejects movement even without a modal"), Second->GetPendingMovementInputVector().IsNearlyZero());
    Flow->SetGameState(EGameFlowState::Loading);
    PC->Move(FInputActionValue(FVector2D(0.f, 1.f)));
    TestTrue(TEXT("Loading rejects movement"), Second->GetPendingMovementInputVector().IsNearlyZero());
    Flow->SetGameState(EGameFlowState::Playing);
    PC->Move(FInputActionValue(FVector2D(0.f, 1.f)));
    TestFalse(TEXT("Gameplay accepts movement again"), Second->GetPendingMovementInputVector().IsNearlyZero());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorFrameworkMenuFocusTest, "HorrorFramework.Consolidation.MenuFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorFrameworkMenuFocusTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI;
    UWorld* World = ConsolidationTest::CreateWorld(GI, TEXT("MenuFocusAutomation"));
    ON_SCOPE_EXIT { ConsolidationTest::DestroyWorld(GI, World); };
    APlayerController* PC = World->SpawnActor<APlayerController>();
    ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
    Player->SetControllerId(0);
    PC->SetPlayer(Player);
    UUIScreenBase* Screen = ConsolidationTest::CreateScreen(GI);
    Screen->SetOwningPlayer(PC);
    UVerticalBox* Layout = Screen->WidgetTree->ConstructWidget<UVerticalBox>();
    Screen->WidgetTree->RootWidget = Layout;
    UButton* Disabled = Screen->WidgetTree->ConstructWidget<UButton>();
    Disabled->SetIsEnabled(false);
    Layout->AddChildToVerticalBox(Disabled);
    UVerticalBox* Hidden = Screen->WidgetTree->ConstructWidget<UVerticalBox>();
    Hidden->SetVisibility(ESlateVisibility::Collapsed);
    Hidden->AddChildToVerticalBox(Screen->WidgetTree->ConstructWidget<UButton>());
    Layout->AddChildToVerticalBox(Hidden);
    UButton* First = Screen->WidgetTree->ConstructWidget<UButton>();
    UButton* Second = Screen->WidgetTree->ConstructWidget<UButton>();
    Layout->AddChildToVerticalBox(First);
    Layout->AddChildToVerticalBox(Second);
    TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(640.f, 480.f));
    Window->SetContent(Screen->TakeWidget());
    FSlateApplication& Slate = FSlateApplication::Get();
    Slate.RegisterVirtualWindow(Window);
    const TSharedPtr<SWidget> PreviousFocus = Slate.GetUserFocusedWidget(0);
    ON_SCOPE_EXIT
    {
        Slate.ClearUserFocus(0, EFocusCause::Cleared);
        Slate.UnregisterVirtualWindow(Window);
        if (PreviousFocus) Slate.SetUserFocus(0, PreviousFocus, EFocusCause::SetDirectly);
    };
    Window->SlatePrepass();
    TestTrue(TEXT("Opening selects an enabled visible button instead of the screen root"),
        Screen->GetNavigationFocusTarget() == First->TakeWidget());
    Slate.SetUserFocus(0, Second->TakeWidget(), EFocusCause::Navigation);
    TestTrue(TEXT("Fixture focuses the second button for the gamepad user"), Second->HasUserFocus(PC));
    Screen->RememberNavigationFocus();
    Screen->SetVisibility(ESlateVisibility::Collapsed);
    Slate.ClearUserFocus(0, EFocusCause::Cleared);
    Screen->SetVisibility(ESlateVisibility::Visible);
    TestTrue(TEXT("Returning to a screen restores the selected button"),
        Screen->GetNavigationFocusTarget() == Second->TakeWidget());
    Second->SetIsEnabled(false);
    TestTrue(TEXT("A disabled remembered button is replaced by an available button"),
        Screen->GetNavigationFocusTarget() == First->TakeWidget());
    Screen->bUseGameplayInput = true;
    TestTrue(TEXT("Dialogue retains its root focus for gameplay actions"), Screen->GetNavigationFocusTarget() == Screen->TakeWidget());
    return !HasAnyErrors();
}
#endif
