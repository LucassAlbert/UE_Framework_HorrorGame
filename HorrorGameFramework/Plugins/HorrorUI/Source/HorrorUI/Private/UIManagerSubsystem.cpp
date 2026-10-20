#include "UIManagerSubsystem.h"
#include "UIManagerSettings.h"
#include "UIPlaceholderScreen.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorUI, Log, All);

void UUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UUIManagerSubsystem::HandleWorldCleanup);
}

void UUIManagerSubsystem::Deinitialize()
{
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    if (ConfigLoadHandle) ConfigLoadHandle->CancelHandle();
    ClearScreens();
    ConfigLoadHandle.Reset();
    UIConfigAsset = nullptr;
    bReady = false;
    Super::Deinitialize();
}

void UUIManagerSubsystem::InitializeUI()
{
    if (bReady) { OnUIReady.Broadcast(true); return; }
    if (bInitializing) return;
    UIConfigAssetRef = GetDefault<UUIManagerSettings>()->UIConfigAsset;
    if (UIConfigAssetRef.IsNull())
    {
        UE_LOG(LogHorrorUI, Error, TEXT("Configure UIConfigAsset in Project Settings > Horror UI."));
        OnUIReady.Broadcast(false);
        return;
    }
    bInitializing = true;
    ConfigLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        UIConfigAssetRef.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UUIManagerSubsystem::OnUIConfigLoaded));
}

void UUIManagerSubsystem::OnUIConfigLoaded()
{
    bInitializing = false;
    UIConfigAsset = UIConfigAssetRef.Get();
    bReady = IsValid(UIConfigAsset);
    if (!bReady) UE_LOG(LogHorrorUI, Error, TEXT("Cannot load UI config: %s"), *UIConfigAssetRef.ToString());
    OnUIReady.Broadcast(bReady);
}

UUIScreenBase* UUIManagerSubsystem::PushScreen(TSubclassOf<UUIScreenBase> ScreenClass)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !ScreenClass) return nullptr;
    // Reopening the current screen must not duplicate it.
    if (GetTopScreen() && GetTopScreen()->GetClass() == ScreenClass) return GetTopScreen();
    UUIScreenBase* Screen = CreateWidget<UUIScreenBase>(PC, ScreenClass);
    if (!Screen) return nullptr;
    Screen->SetIsFocusable(true);
    Screen->SetOwningManager(this);
    ScreenStack.Add(Screen);
    Screen->AddToViewport(100 + ScreenStack.Num());
    Screen->OnScreenEnter();
    Screen->ReceiveScreenEnter();
    ApplyScreenState();
    return Screen;
}

void UUIManagerSubsystem::PopScreen()
{
    if (ScreenStack.IsEmpty()) return;
    RemoveScreen(GetTopScreen());
}

void UUIManagerSubsystem::RemoveScreen(UUIScreenBase* Screen)
{
    if (!IsValid(Screen) || !ScreenStack.Contains(Screen)) return;
    // A boot/menu root has no gameplay underneath. Back must not leave an empty world.
    if (ScreenStack.Num() == 1 && !GameplayScreen && Screen->bPauseWorld) return;
    UUIScreenBase* Top = Screen;
    ScreenStack.RemoveSingle(Screen);
    if (IsValid(Top))
    {
        Top->OnScreenExit();
        Top->ReceiveScreenExit();
        Top->RemoveFromParent();
    }
    ApplyScreenState();
}

void UUIManagerSubsystem::ClearScreens()
{
    while (!ScreenStack.IsEmpty())
    {
        UUIScreenBase* Top = ScreenStack.Pop();
        if (IsValid(Top))
        {
            Top->OnScreenExit();
            Top->ReceiveScreenExit();
            Top->RemoveFromParent();
        }
    }
    if (IsValid(GameplayScreen))
    {
        GameplayScreen->OnScreenExit();
        GameplayScreen->ReceiveScreenExit();
        GameplayScreen->RemoveFromParent();
    }
    GameplayScreen = nullptr;
    ApplyScreenState();
}

UUIScreenBase* UUIManagerSubsystem::GetTopScreen() const
{
    return ScreenStack.IsEmpty() ? nullptr : ScreenStack.Last().Get();
}

bool UUIManagerSubsystem::DoesScreenStackPauseWorld() const
{
    return ScreenStack.ContainsByPredicate([](const UUIScreenBase* Screen) { return IsValid(Screen) && Screen->bPauseWorld; });
}

void UUIManagerSubsystem::ApplyScreenState()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (ControlOwner.Get() != PC) ReleaseControlState();
    if (!PC)
    {
        OnScreenStackChanged.Broadcast(HasModalScreen());
        return;
    }
    // The HUD remains visible, but must not receive focus/actions behind a modal screen.
    if (IsValid(GameplayScreen)) GameplayScreen->SetIsEnabled(!HasModalScreen());
    for (UUIScreenBase* Screen : ScreenStack)
    {
        Screen->RememberNavigationFocus();
        Screen->SetVisibility(Screen == GetTopScreen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (UUIScreenBase* Top = GetTopScreen())
    {
        ControlOwner = PC;
        if (DoesScreenStackPauseWorld())
        {
            if (!World->IsPaused()) bPauseOwned = PC->SetPause(true);
        }
        else if (bPauseOwned) { PC->SetPause(false); bPauseOwned = false; }
        if (!bInputOwned)
        {
            PC->SetIgnoreMoveInput(true);
            PC->SetIgnoreLookInput(true);
            bInputOwned = true;
        }
        PC->bShowMouseCursor = !Top->bUseGameplayInput;
        if (Top->bUseGameplayInput)
        {
            // Let the focused screen consume Back (including the Editor's PIE Escape shortcut).
            // Unhandled actions continue to Enhanced Input in the controller.
            FInputModeGameAndUI Mode;
            Mode.SetWidgetToFocus(Top->GetNavigationFocusTarget());
            PC->SetInputMode(Mode);
        }
        else
        {
            FInputModeUIOnly Mode;
            Mode.SetWidgetToFocus(Top->GetNavigationFocusTarget());
            PC->SetInputMode(Mode);
        }
    }
    else
    {
        ReleaseControlState();
    }
    OnScreenStackChanged.Broadcast(HasModalScreen());
}

void UUIManagerSubsystem::ReleaseControlState()
{
    if (APlayerController* Owner = ControlOwner.Get())
    {
        // Other systems may own ignore-input increments; release exactly our increment.
        if (bInputOwned)
        {
            Owner->SetIgnoreMoveInput(false);
            Owner->SetIgnoreLookInput(false);
        }
        if (bPauseOwned && Owner->GetWorld()) Owner->SetPause(false);
        Owner->bShowMouseCursor = false;
        if (Owner->GetWorld()) Owner->SetInputMode(FInputModeGameOnly());
    }
    bInputOwned = false;
    bPauseOwned = false;
    ControlOwner.Reset();
}

void UUIManagerSubsystem::ReleasePlayerPresentation(APlayerController* Controller)
{
    if (!Controller) return;
    const bool bOwnsPresentation = ControlOwner.Get() == Controller
        || (GameplayScreen && GameplayScreen->GetOwningPlayer() == Controller)
        || ScreenStack.ContainsByPredicate([Controller](const UUIScreenBase* Screen)
            { return IsValid(Screen) && Screen->GetOwningPlayer() == Controller; });
    if (bOwnsPresentation) ClearScreens();
}

void UUIManagerSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    if (World && World->GetGameInstance() == GetGameInstance()) ClearScreens();
}

TSubclassOf<UUIScreenBase> UUIManagerSubsystem::GetScreenClass(EHorrorScreen ScreenType) const
{
    if (!UIConfigAsset) return nullptr;
    switch (ScreenType)
    {
        case EHorrorScreen::MainMenu: return UIConfigAsset->MainMenu;
        case EHorrorScreen::Settings: return UIConfigAsset->Settings;
        case EHorrorScreen::Pause: return UIConfigAsset->PauseGame;
        case EHorrorScreen::Inventory: return UIConfigAsset->Inventory;
        case EHorrorScreen::Inspection: return UIConfigAsset->Inspection;
        case EHorrorScreen::Documents: return UIConfigAsset->Documents;
        case EHorrorScreen::Objectives: return UIConfigAsset->Objectives;
        case EHorrorScreen::SaveSlots: return UIConfigAsset->SaveSlots;
        case EHorrorScreen::GameOver: return UIConfigAsset->GameOver;
        case EHorrorScreen::AudioSettings: return UIConfigAsset->AudioSettings;
    }
    return nullptr;
}

UUIScreenBase* UUIManagerSubsystem::OpenScreen(EHorrorScreen ScreenType)
{
    if (TSubclassOf<UUIScreenBase> ScreenClass = GetScreenClass(ScreenType)) return PushScreen(ScreenClass);
    if (!UIConfigAsset || !UIConfigAsset->bEnablePlaceholderScreens)
    {
        UE_LOG(LogHorrorUI, Error, TEXT("Screen %d has no configured class."), static_cast<int32>(ScreenType));
        return nullptr;
    }

    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC) return nullptr;
    UUIPlaceholderScreen* Screen = CreateWidget<UUIPlaceholderScreen>(PC, UUIPlaceholderScreen::StaticClass());
    if (!Screen) return nullptr;
    Screen->ScreenTitle = FText::FromString(StaticEnum<EHorrorScreen>()->GetNameStringByValue(static_cast<int64>(ScreenType)));
    Screen->SetIsFocusable(true);
    Screen->SetOwningManager(this);
    ScreenStack.Add(Screen);
    Screen->AddToViewport(100 + ScreenStack.Num());
    Screen->OnScreenEnter();
    Screen->ReceiveScreenEnter();
    UE_LOG(LogHorrorUI, Display, TEXT("Opened prototype placeholder: %s"), *Screen->ScreenTitle.ToString());
    ApplyScreenState();
    return Screen;
}

void UUIManagerSubsystem::OpenMainMenuUI() { OpenScreen(EHorrorScreen::MainMenu); }
void UUIManagerSubsystem::OpenSettingsUI() { OpenScreen(EHorrorScreen::Settings); }
void UUIManagerSubsystem::OpenAudioSettingsUI() { OpenScreen(EHorrorScreen::AudioSettings); }
void UUIManagerSubsystem::OpenPauseUI() { OpenScreen(EHorrorScreen::Pause); }

void UUIManagerSubsystem::OpenGameplayUI()
{
    if (IsValid(GameplayScreen)) return;
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !UIConfigAsset || !UIConfigAsset->GameplayHUD) return;
    GameplayScreen = CreateWidget<UUIScreenBase>(PC, UIConfigAsset->GameplayHUD);
    if (!GameplayScreen) return;
    GameplayScreen->SetOwningManager(this);
    GameplayScreen->AddToViewport(0);
    GameplayScreen->SetIsEnabled(!HasModalScreen());
    GameplayScreen->OnScreenEnter();
    GameplayScreen->ReceiveScreenEnter();
}
