#include "Subsystems/GameFlowSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorFlow, Log, All);

void UGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UUIManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->OnScreenStackChanged.AddUniqueDynamic(
        this, &UGameFlowSubsystem::HandleScreenStackChanged);
}

void UGameFlowSubsystem::Deinitialize()
{
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        UI->OnScreenStackChanged.RemoveDynamic(this, &UGameFlowSubsystem::HandleScreenStackChanged);
    Super::Deinitialize();
}

void UGameFlowSubsystem::RestartLevel()
{
    LoadLevel(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void UGameFlowSubsystem::LoadLevel(FName LevelName)
{
    if (!GetWorld() || LevelName.IsNone()) return;
    SetGameState(EGameFlowState::Loading);
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UI->ClearScreens();
    UGameplayStatics::OpenLevel(this, LevelName);
}

void UGameFlowSubsystem::PauseGame()
{
    if (CurrentState != EGameFlowState::Playing && CurrentState != EGameFlowState::Paused) return;
    UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (UI->HasModalScreen()) UI->PopScreen();
    else UI->OpenPauseUI();
}

void UGameFlowSubsystem::HandleScreenStackChanged(bool bHasModalScreen)
{
    if (CurrentState == EGameFlowState::Playing || CurrentState == EGameFlowState::Paused)
    {
        const UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
        SetGameState(UI && UI->DoesScreenStackPauseWorld() ? EGameFlowState::Paused : EGameFlowState::Playing);
    }
}

void UGameFlowSubsystem::SetGameState(EGameFlowState NewState)
{
    if (CurrentState == NewState) return;
    CurrentState = NewState;
    UE_LOG(LogHorrorFlow, Log, TEXT("Game flow: %s"), *StaticEnum<EGameFlowState>()->GetNameStringByValue(static_cast<int64>(NewState)));
}
