#include "Subsystems/Boot/BootManagerSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorBoot, Log, All);

void UBootManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UUIManagerSubsystem>();
    Collection.InitializeDependency<USaveSubsystem>();
    Collection.InitializeDependency<UGameFlowSubsystem>();
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    Saves->OnLoadStarted.AddUniqueDynamic(this, &UBootManagerSubsystem::HandleSaveLoadStarted);
    Saves->OnLoadCompleted.AddUniqueDynamic(this, &UBootManagerSubsystem::HandleSaveLoaded);
    Saves->OnWorldReady.AddUniqueDynamic(this, &UBootManagerSubsystem::HandleSaveWorldReady);
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UBootManagerSubsystem::HandleWorldLoaded);
}

void UBootManagerSubsystem::Deinitialize()
{
    if (USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>())
    {
        Saves->OnLoadStarted.RemoveDynamic(this, &UBootManagerSubsystem::HandleSaveLoadStarted);
        Saves->OnLoadCompleted.RemoveDynamic(this, &UBootManagerSubsystem::HandleSaveLoaded);
        Saves->OnWorldReady.RemoveDynamic(this, &UBootManagerSubsystem::HandleSaveWorldReady);
    }
    ReleasePreparationPause();
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        UI->OnUIReady.RemoveDynamic(this, &UBootManagerSubsystem::HandleUIReady);
    Super::Deinitialize();
}

void UBootManagerSubsystem::SetState(EBootState NewState)
{
    CurrentState = NewState;
    OnBootStateChanged.Broadcast(NewState);
}

void UBootManagerSubsystem::StartBoot()
{
    if (CurrentState != EBootState::None) return;
    SetState(EBootState::PreInit);
    SetState(EBootState::InitCore);
    SetState(EBootState::InitSystems);
    SetState(EBootState::LoadAssets);
    UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    UI->OnUIReady.AddUniqueDynamic(this, &UBootManagerSubsystem::HandleUIReady);
    UI->InitializeUI();
}

void UBootManagerSubsystem::HandleUIReady(bool bSuccess)
{
    bUIReady = bSuccess;
    if (!bSuccess)
    {
        UE_LOG(LogHorrorBoot, Error, TEXT("Boot stopped: UI configuration could not be loaded."));
        return;
    }
    SetState(EBootState::InitUI);
    HandleInitUI();
}

void UBootManagerSubsystem::HandleWorldLoaded(UWorld* World)
{
    if (World && World->GetGameInstance() == GetGameInstance() && bUIReady) HandleInitUI();
}

void UBootManagerSubsystem::HandleInitUI()
{
    UWorld* World = GetWorld();
    if (!World || !World->GetFirstPlayerController()) return;
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    if (Saves->HasPendingLoad() || !Saves->IsWorldReady())
    {
        GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()->SetGameState(EGameFlowState::Loading);
        HoldPreparationPause();
        return;
    }
    ReleasePreparationPause();
    UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    UGameFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();
    const FString StartupMap = FPackageName::GetShortName(UGameMapsSettings::GetGameDefaultMap());
    const bool bStartupWorld = UGameplayStatics::GetCurrentLevelName(this, true) == StartupMap;
    UI->ClearScreens();
    Flow->SetGameState(bStartupWorld ? EGameFlowState::MainMenu : EGameFlowState::Playing);
    if (bStartupWorld)
    {
        UI->OpenMainMenuUI();
        SetupMenuCamera();
    }
    else UI->OpenGameplayUI();
    if (!FinishBoot)
    {
        FinishBoot = true;
        SetState(EBootState::Ready);
        UE_LOG(LogHorrorBoot, Log, TEXT("Boot completed; UI configuration and player controller are ready."));
        OnBootCompleted.Broadcast();
    }
}

void UBootManagerSubsystem::SetupMenuCamera()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(TEXT("MenuCam"))) { PC->SetViewTargetWithBlend(*It, 0.f); return; }
    }
}

void UBootManagerSubsystem::SetupInputUI()
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    PC->bShowMouseCursor = true;
    PC->SetInputMode(FInputModeUIOnly());
}

void UBootManagerSubsystem::HoldPreparationPause()
{
    UWorld* World = GetWorld();
    if (!World) return;
    if (PreparationWorld.Get() != World) { bPreparationPauseOwned = false; PreparationWorld = World; }
    APlayerController* PC = World->GetFirstPlayerController();
    if (PC && !World->IsPaused()) bPreparationPauseOwned = PC->SetPause(true);
}
void UBootManagerSubsystem::ReleasePreparationPause()
{
    if (bPreparationPauseOwned && PreparationWorld.IsValid())
        if (APlayerController* PC = PreparationWorld->GetFirstPlayerController()) PC->SetPause(false);
    bPreparationPauseOwned = false;
    PreparationWorld.Reset();
}
void UBootManagerSubsystem::HandleSaveLoadStarted()
{
    if (!bUIReady) return;
    UGameFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();
    BeforeLoadState = Flow->CurrentState;
    BeforeLoadWorld = GetWorld();
    Flow->SetGameState(EGameFlowState::Loading);
    HoldPreparationPause();
}
void UBootManagerSubsystem::HandleSaveLoaded(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (!bUIReady) return;
    if (bSuccess) { HandleInitUI(); return; }
    ReleasePreparationPause();
    UGameFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();
    const EGameFlowState State = BeforeLoadWorld.Get() == GetWorld() ? BeforeLoadState : EGameFlowState::MainMenu;
    Flow->SetGameState(State);
    if (State == EGameFlowState::MainMenu)
    {
        UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
        UI->ClearScreens();
        UI->OpenMainMenuUI();
    }
}
void UBootManagerSubsystem::HandleSaveWorldReady() { if (bUIReady) HandleInitUI(); }
