#include "Core/HorrorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Interfaces/HorrorInputReceiver.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/InteractionSubsystem.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "UIManagerSubsystem.h"
#include "UIScreenBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorInput, Log, All);

AHorrorPlayerController::AHorrorPlayerController()
{
    bShowMouseCursor = false;
}

void AHorrorPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UI->OnScreenStackChanged.AddUniqueDynamic(this, &AHorrorPlayerController::HandleScreenStackChanged);
        HandleScreenStackChanged(UI->HasModalScreen());
    }
}

void AHorrorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UI->ReleasePlayerPresentation(this);
    GetWorldTimerManager().ClearTimer(HideTransitionTimer);
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        UI->OnScreenStackChanged.RemoveDynamic(this, &AHorrorPlayerController::HandleScreenStackChanged);
    if (UInteractionSubsystem* Interaction = GetGameInstance()->GetSubsystem<UInteractionSubsystem>())
        Interaction->SetFocusedActor(nullptr);
    Super::EndPlay(EndPlayReason);
}

bool AHorrorPlayerController::IsGameplayFlowActive() const
{
    const UGameFlowSubsystem* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
    return Flow && Flow->CurrentState == EGameFlowState::Playing;
}

void AHorrorPlayerController::ResetTransientControlState()
{
    if (GetPawn())
        if (UHidingParticipantComponent* Hiding = GetPawn()->FindComponentByClass<UHidingParticipantComponent>()) Hiding->ResetHiding();
    GetWorldTimerManager().ClearTimer(HideTransitionTimer);
    InAnimation = false;
    CurrentState = EPlayerControlState::Gameplay;
    StateBeforeUI = EPlayerControlState::Gameplay;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UInteractionSubsystem* Interaction = GI->GetSubsystem<UInteractionSubsystem>()) Interaction->SetFocusedActor(nullptr);
        if (UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>()) HandleScreenStackChanged(UI->HasModalScreen());
    }
    if (GetPawn()) SetViewTarget(GetPawn());
}

void AHorrorPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ResetTransientControlState();
    if (InPawn)
        if (UHidingParticipantComponent* Hiding = InPawn->FindComponentByClass<UHidingParticipantComponent>())
            Hiding->OnHidingChanged.AddUniqueDynamic(this, &AHorrorPlayerController::HandleHidingChanged);
    if (IsLocalController() && IsGameplayFlowActive())
        if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>(); UI && UI->IsReady()) UI->OpenGameplayUI();
}

void AHorrorPlayerController::OnUnPossess()
{
    if (GetPawn())
        if (UHidingParticipantComponent* Hiding = GetPawn()->FindComponentByClass<UHidingParticipantComponent>())
        {
            Hiding->ResetHiding();
            Hiding->OnHidingChanged.RemoveDynamic(this, &AHorrorPlayerController::HandleHidingChanged);
        }
    if (UGameInstance* GI = GetGameInstance())
        if (UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>()) UI->ReleasePlayerPresentation(this);
    Super::OnUnPossess();
    ResetTransientControlState();
}

void AHorrorPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
    if (!Input)
    {
        UE_LOG(LogHorrorInput, Error, TEXT("Configure EnhancedInputComponent for the player controller."));
        return;
    }
    if (InteractAction) Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AHorrorPlayerController::HandleInteract);
    if (MoveAction) Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHorrorPlayerController::Move);
    if (LookAction) Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHorrorPlayerController::Look);
    if (CrouchAction) Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHorrorPlayerController::Crouch);
    if (InventoryAction) Input->BindAction(InventoryAction, ETriggerEvent::Started, this, &AHorrorPlayerController::CallInventory);
    if (PauseGameAction) Input->BindAction(PauseGameAction, ETriggerEvent::Started, this, &AHorrorPlayerController::CallPauseGame);
}

void AHorrorPlayerController::HandleScreenStackChanged(bool bHasModalScreen)
{
    if (bHasModalScreen)
    {
        if (CurrentState != EPlayerControlState::UI && CurrentState != EPlayerControlState::Paused)
            StateBeforeUI = CurrentState;
        CurrentState = EPlayerControlState::UI;
        if (UInteractionSubsystem* Interaction = GetGameInstance()->GetSubsystem<UInteractionSubsystem>())
            Interaction->SetFocusedActor(nullptr);
    }
    else if (CurrentState == EPlayerControlState::UI || CurrentState == EPlayerControlState::Paused)
    {
        CurrentState = StateBeforeUI;
    }
}

void AHorrorPlayerController::SetControlState(EPlayerControlState NewState)
{
    CurrentState = NewState;
    switch (NewState)
    {
        case EPlayerControlState::Hidden: HideSpotEnter(); break;
        case EPlayerControlState::Gameplay:
            SetInputMode(FInputModeGameOnly());
            bShowMouseCursor = false;
            break;
        case EPlayerControlState::UI:
            SetInputMode(FInputModeUIOnly());
            bShowMouseCursor = true;
            break;
        case EPlayerControlState::Paused:
            if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UI->OpenPauseUI();
            break;
    }
}

void AHorrorPlayerController::HandleInteract(const FInputActionValue& InputValue)
{
    if (!IsGameplayFlowActive()) return;
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        if (UUIScreenBase* Top = UI->GetTopScreen(); Top && Top->bUseGameplayInput)
        {
            Top->HandlePrimaryAction();
            return;
        }
    if (InAnimation || CurrentState == EPlayerControlState::UI || CurrentState == EPlayerControlState::Paused) return;
    if (CurrentState == EPlayerControlState::Hidden) { HideSpotExit(); return; }
    UInteractionSubsystem* Interaction = GetGameInstance()->GetSubsystem<UInteractionSubsystem>();
    if (!Interaction) return;
    TWeakObjectPtr<AActor> Target = Interaction->GetFocusedActor();
    if (Interaction->TryInteract(this) && Target.IsValid()) CheckHideSpot(Target.Get());
}

void AHorrorPlayerController::Move(const FInputActionValue& InputValue)
{
    if (!IsGameplayFlowActive() || CurrentState != EPlayerControlState::Gameplay || InAnimation || IsMoveInputIgnored()) return;
    APawn* ControlledPawn = GetPawn();
    if (IsValid(ControlledPawn) && ControlledPawn->Implements<UHorrorInputReceiver>()) IHorrorInputReceiver::Execute_OnMove(ControlledPawn, InputValue);
}

void AHorrorPlayerController::Look(const FInputActionValue& InputValue)
{
    if (IsGameplayFlowActive() && CurrentState == EPlayerControlState::Hidden && !InAnimation && !IsLookInputIgnored())
    {
        if (GetPawn())
            if (UHidingParticipantComponent* Hiding = GetPawn()->FindComponentByClass<UHidingParticipantComponent>()) Hiding->Look(InputValue.Get<FVector2D>());
        return;
    }
    if (!IsGameplayFlowActive() || CurrentState != EPlayerControlState::Gameplay || InAnimation || IsLookInputIgnored()) return;
    APawn* ControlledPawn = GetPawn();
    if (IsValid(ControlledPawn) && ControlledPawn->Implements<UHorrorInputReceiver>()) IHorrorInputReceiver::Execute_OnLook(ControlledPawn, InputValue);
}

void AHorrorPlayerController::Crouch(const FInputActionValue& InputValue)
{
    if (!IsGameplayFlowActive() || CurrentState != EPlayerControlState::Gameplay || InAnimation) return;
    APawn* ControlledPawn = GetPawn();
    if (IsValid(ControlledPawn) && ControlledPawn->Implements<UHorrorInputReceiver>()) IHorrorInputReceiver::Execute_OnCrouch(ControlledPawn, InputValue);
}

void AHorrorPlayerController::CheckHideSpot(AActor* InteractiveActor)
{
    if (GetPawn() && GetPawn()->FindComponentByClass<UHidingParticipantComponent>()) return;
    if (IsValid(InteractiveActor) && InteractiveActor->FindComponentByClass<UHideSpotComponent>())
        SetControlState(EPlayerControlState::Hidden);
}

void AHorrorPlayerController::HideSpotEnter()
{
    InAnimation = true;
    GetWorldTimerManager().SetTimer(HideTransitionTimer, this, &AHorrorPlayerController::FinishHideEnter, 1.5f, false);
}

void AHorrorPlayerController::FinishHideEnter() { InAnimation = false; }

void AHorrorPlayerController::HideSpotExit()
{
    if (GetPawn())
        if (UHidingParticipantComponent* Hiding = GetPawn()->FindComponentByClass<UHidingParticipantComponent>(); Hiding && Hiding->IsHiding())
        {
            Hiding->Exit();
            return;
        }
    InAnimation = true;
    if (GetPawn()) SetViewTargetWithBlend(GetPawn(), 1.f);
    GetWorldTimerManager().SetTimer(HideTransitionTimer, this, &AHorrorPlayerController::FinishHideExit, 1.5f, false);
}

void AHorrorPlayerController::FinishHideExit()
{
    StateBeforeUI = EPlayerControlState::Gameplay;
    const UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    CurrentState = UI && UI->HasModalScreen() ? EPlayerControlState::UI : EPlayerControlState::Gameplay;
    InAnimation = false;
}

void AHorrorPlayerController::SetHideSpotCamera(AActor* HideSpotActor)
{
    if (IsValid(HideSpotActor)) SetViewTargetWithBlend(HideSpotActor, 1.f);
}

void AHorrorPlayerController::HandleHidingChanged(bool bNowHidden, UHideSpotComponent* Spot, FVector EntryLocation)
{
    GetWorldTimerManager().ClearTimer(HideTransitionTimer);
    InAnimation = false;
    StateBeforeUI = bNowHidden ? EPlayerControlState::Hidden : EPlayerControlState::Gameplay;
    const UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    CurrentState = UI && UI->HasModalScreen() ? EPlayerControlState::UI : StateBeforeUI;
}

void AHorrorPlayerController::CallPauseGame(const FInputActionValue& InputValue)
{
    if (UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        if (UUIScreenBase* Top = UI->GetTopScreen(); Top && Top->bUseGameplayInput && Top->HandleBackAction()) return;
    if (UGameFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()) Flow->PauseGame();
}

void AHorrorPlayerController::CallInventory(const FInputActionValue& InputValue)
{
    if (!IsGameplayFlowActive() || CurrentState != EPlayerControlState::Gameplay || InAnimation) return;
    APawn* ControlledPawn = GetPawn();
    if (IsValid(ControlledPawn) && ControlledPawn->Implements<UHorrorInputReceiver>())
        IHorrorInputReceiver::Execute_OnInventory(ControlledPawn);
}
