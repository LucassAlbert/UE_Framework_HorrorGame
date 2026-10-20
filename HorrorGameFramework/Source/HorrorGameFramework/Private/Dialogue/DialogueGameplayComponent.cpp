#include "Dialogue/DialogueGameplayComponent.h"
#include "Dialogue/DialogueNPC.h"
#include "Dialogue/DialogueScreen.h"
#include "DialogueDefinition.h"
#include "DialogueSessionComponent.h"
#include "ObjectiveJournalComponent.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "InteractionScannerComponent.h"
#include "Subsystems/InteractionSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "UIManagerSubsystem.h"
#include "InteractableComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogueGameplay, Log, All);

UDialogueGameplayComponent::UDialogueGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ScreenClass = UDialogueScreen::StaticClass();
}
UDialogueSessionComponent* UDialogueGameplayComponent::GetSession() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UDialogueSessionComponent>() : nullptr;
}
UUIManagerSubsystem* UDialogueGameplayComponent::GetUI() const
{
    if (ConversationUI.IsValid()) return ConversationUI.Get();
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = ConversationController.IsValid() ? ConversationController.Get() : (Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr);
    return PC && PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
}
void UDialogueGameplayComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UDialogueSessionComponent* Session = GetSession()) Session->OnEnded.AddUniqueDynamic(this, &UDialogueGameplayComponent::HandleEnded);
    if (APawn* Pawn = Cast<APawn>(GetOwner())) Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UDialogueGameplayComponent::HandleControllerChanged);
}
bool UDialogueGameplayComponent::CanStart(const ADialogueNPC* NPC) const
{
    UDialogueSessionComponent* Session = GetSession();
    UDialogueDefinition* Definition = IsValid(NPC) ? NPC->Dialogue.Get() : nullptr;
    FText Error;
    if (!bEnableDialogue || bStarting || bCleaning || !Session || Session->IsConversationActive() || !Definition || !Definition->Validate(Error)
        || (!Definition->bRepeatable && Session->HasCompleted(Definition->DialogueId))) return false;
    UUIManagerSubsystem* UI = GetUI();
    if (!UI || UI->HasModalScreen() || !GetWorld() || GetWorld()->IsPaused()) return false;
    const UObjectiveJournalComponent* Journal = GetOwner()->FindComponentByClass<UObjectiveJournalComponent>();
    if ((!Definition->RequiredFacts.IsEmpty() || !Definition->BlockingFacts.IsEmpty()) && (!Journal || !Journal->IsReady())) return false;
    for (FName Fact : Definition->RequiredFacts) if (!Journal->HasFact(Fact)) return false;
    for (FName Fact : Definition->BlockingFacts) if (Journal->HasFact(Fact)) return false;
    return true;
}
bool UDialogueGameplayComponent::ValidateTarget(const ADialogueNPC* NPC) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !IsValid(NPC)) return false;
    FVector Origin;
    FRotator Rotation;
    PC->GetPlayerViewPoint(Origin, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DialogueInteraction), false, GetOwner());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Rotation.Vector() * NPC->Interactable->InteractionDistance,
        ECC_GameTraceChannel1, Query) || Hit.GetActor() != NPC) return false;
    FHitResult Obstruction;
    return !GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, Hit.ImpactPoint, ECC_Visibility, Query) || Obstruction.GetActor() == NPC;
}
bool UDialogueGameplayComponent::StartDialogue(ADialogueNPC* NPC)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->GetLocalPlayer() || !ScreenClass || !CanStart(NPC) || !ValidateTarget(NPC)) return false;
    TGuardValue<bool> Starting(bStarting, true);
    ConversationController = PC;
    Speaker = NPC;
    if (!GetSession()->Start(NPC->Dialogue.Get())) { Speaker.Reset(); ConversationController.Reset(); return false; }
    UUIManagerSubsystem* UI = GetUI();
    UDialogueScreen* Widget = Cast<UDialogueScreen>(UI->PushScreen(ScreenClass));
    if (!Widget)
    {
        UE_LOG(LogDialogueGameplay, Error, TEXT("Cannot open dialogue presentation for %s."), *GetNameSafe(NPC));
        Cancel();
        return false;
    }
    Screen = Widget;
    ConversationUI = UI;
    Widget->SetSource(this);
    NPC->OnEndPlay.AddUniqueDynamic(this, &UDialogueGameplayComponent::HandleNPCExit);
    UI->OnScreenStackChanged.AddUniqueDynamic(this, &UDialogueGameplayComponent::HandleStackChanged);
    // Ignore input stops new movement; discard existing velocity/input so the player does not coast into the NPC.
    Pawn->ConsumeMovementInputVector();
    if (ACharacter* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
    for (AActor* Source : { static_cast<AActor*>(Pawn), static_cast<AActor*>(PC) })
    {
        TArray<UInteractionScannerComponent*> Scanners;
        Source->GetComponents(Scanners);
        for (UInteractionScannerComponent* Scanner : Scanners)
            if (Scanner->IsActive()) { SuspendedScanners.Add(Scanner); Scanner->Deactivate(); }
    }
    if (UInteractionSubsystem* Interaction = PC->GetGameInstance()->GetSubsystem<UInteractionSubsystem>()) Interaction->SetFocusedActor(nullptr);
    UpdateConversation();
    if (!GetSession()->IsConversationActive()) return false;
    GetWorld()->GetTimerManager().SetTimer(ConversationTimer, this, &UDialogueGameplayComponent::UpdateConversation, 0.03f, true);
    return true;
}
void UDialogueGameplayComponent::Advance()
{
    UpdateConversation();
    if (UDialogueSessionComponent* Session = GetSession(); Session && Session->IsConversationActive()) Session->Advance();
}
void UDialogueGameplayComponent::Cancel()
{
    if (UDialogueSessionComponent* Session = GetSession(); Session && Session->IsConversationActive()) Session->Cancel();
    else Cleanup();
}
void UDialogueGameplayComponent::UpdateConversation()
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = ConversationController.Get();
    ADialogueNPC* NPC = Speaker.Get();
    UUIManagerSubsystem* UI = GetUI();
    const UGameFlowSubsystem* Flow = PC && PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<UGameFlowSubsystem>() : nullptr;
    if (!PC || !Pawn || PC->GetPawn() != Pawn || !IsValid(NPC) || !UI || UI->GetTopScreen() != Screen.Get()
        || (Flow && (Flow->CurrentState == EGameFlowState::Loading || Flow->CurrentState == EGameFlowState::GameOver))) { Cancel(); return; }
    FVector Origin;
    FRotator Rotation;
    PC->GetPlayerViewPoint(Origin, Rotation);
    const FVector Direction = NPC->LookTarget->GetComponentLocation() - Origin;
    if (!Direction.IsNearlyZero()) PC->SetControlRotation(Direction.Rotation());
}
void UDialogueGameplayComponent::HandleEnded(UDialogueDefinition* Definition, bool bCompleted, bool bFirstCompletion)
{
    Cleanup();
    if (!bCompleted || !bFirstCompletion || !Definition || Definition->CompletionFact.IsNone()) return;
    const UProgressionGameplayComponent* Progression = GetOwner()->FindComponentByClass<UProgressionGameplayComponent>();
    if (Progression && !Progression->bEnableObjectives) return;
    if (UObjectiveJournalComponent* Journal = GetOwner()->FindComponentByClass<UObjectiveJournalComponent>()) Journal->RecordFact(Definition->CompletionFact);
}
void UDialogueGameplayComponent::HandleNPCExit(AActor* Actor, EEndPlayReason::Type Reason) { Cancel(); }
void UDialogueGameplayComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController) { Cancel(); }
void UDialogueGameplayComponent::HandleStackChanged(bool bHasScreen)
{
    if (!bStarting && !bCleaning && Screen.IsValid())
        if (UUIManagerSubsystem* UI = GetUI(); !UI || UI->GetTopScreen() != Screen.Get()) Cancel();
}
void UDialogueGameplayComponent::Cleanup()
{
    if (bCleaning) return;
    TGuardValue<bool> Cleaning(bCleaning, true);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ConversationTimer);
    if (Speaker.IsValid()) Speaker->OnEndPlay.RemoveDynamic(this, &UDialogueGameplayComponent::HandleNPCExit);
    if (UUIManagerSubsystem* UI = GetUI())
    {
        UI->OnScreenStackChanged.RemoveDynamic(this, &UDialogueGameplayComponent::HandleStackChanged);
        if (Screen.IsValid()) UI->RemoveScreen(Screen.Get());
    }
    for (const auto& Scanner : SuspendedScanners) if (Scanner.IsValid()) Scanner->Activate();
    SuspendedScanners.Reset();
    Speaker.Reset();
    Screen.Reset();
    ConversationController.Reset();
    ConversationUI.Reset();
}
bool UDialogueGameplayComponent::ValidateSaveState(FString& Error) const
{
    if (bStarting || bCleaning) { Error = TEXT("Cannot capture dialogue during a presentation transition."); return false; }
    return !GetSession() || GetSession()->ValidateState(Error);
}
void UDialogueGameplayComponent::OnSaveStateRestored() { Cancel(); }
void UDialogueGameplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Cancel();
    if (UDialogueSessionComponent* Session = GetSession()) Session->OnEnded.RemoveDynamic(this, &UDialogueGameplayComponent::HandleEnded);
    if (APawn* Pawn = Cast<APawn>(GetOwner())) Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UDialogueGameplayComponent::HandleControllerChanged);
    Super::EndPlay(Reason);
}
