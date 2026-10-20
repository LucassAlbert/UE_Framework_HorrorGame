#include "Threat/FrameworkEnemy.h"
#include "Threat/ThreatGameOverScreen.h"
#include "Dialogue/DialogueGameplayComponent.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Components/SaveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HorrorEnemyController.h"
#include "TimerManager.h"

AFrameworkEnemy::AFrameworkEnemy() { Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save")); }
void AFrameworkEnemy::BeginPlay()
{
    Super::BeginPlay();
    OnCaptured.AddUniqueDynamic(this, &AFrameworkEnemy::HandleCaptured);
}
void AFrameworkEnemy::HandleCaptured(AActor* Enemy, APawn* Player)
{
    UGameFlowSubsystem* Flow = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();
    if (!IsValid(Player) || Flow->CurrentState != EGameFlowState::Playing) return;
    if (const USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>(); Saves && Saves->IsBusy()) return;
    Flow->SetGameState(EGameFlowState::GameOver);
    Player->ConsumeMovementInputVector();
    if (ACharacter* Character = Cast<ACharacter>(Player)) Character->GetCharacterMovement()->StopMovementImmediately();
    if (UDialogueGameplayComponent* Dialogue = Player->FindComponentByClass<UDialogueGameplayComponent>()) Dialogue->Cancel();
    UUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    UI->ClearScreens();
    UI->OpenScreen(EHorrorScreen::GameOver);
}
bool AFrameworkEnemy::ValidateSaveState(FString& Error) const
{
    if (!IsValid(Definition) || !Definition->Validate(Error) || !Memory.Validate() || Memory.State == EHorrorEnemyState::Captured)
    { if (Error.IsEmpty()) Error = TEXT("Invalid or captured enemy state cannot be saved."); return false; }
    for (const FVector& Location : PatrolLocations)
        if (Location.ContainsNaN()) { Error = TEXT("Patrol route contains an invalid location."); return false; }
    return true;
}
void AFrameworkEnemy::OnSaveStateRestored()
{
    if (AHorrorEnemyController* AI = Cast<AHorrorEnemyController>(GetController())) AI->StopMovement();
    // All participants, including the player's concealment, must finish their restoration first.
    RestoreTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AFrameworkEnemy::ResumeAfterRestore);
}
void AFrameworkEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
    OnCaptured.RemoveDynamic(this, &AFrameworkEnemy::HandleCaptured);
    GetWorldTimerManager().ClearTimer(RestoreTimer);
    Super::EndPlay(Reason);
}
