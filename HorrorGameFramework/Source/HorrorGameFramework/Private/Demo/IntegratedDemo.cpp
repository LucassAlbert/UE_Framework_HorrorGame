#include "Demo/IntegratedDemo.h"
#include "ObjectiveJournalComponent.h"
#include "InteractableComponent.h"
#include "UIManagerSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "Subsystems/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

void AIntegratedDemoCharacter::BeginPlay()
{
    Super::BeginPlay();
    Inspection->OnStateChanged.AddUniqueDynamic(this, &AIntegratedDemoCharacter::HandleInspection);
}
void AIntegratedDemoCharacter::HandleInspection(EInspectionState State, FText Message)
{
    const USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    if (State == EInspectionState::Ready && !Saves->IsRestoring()) Objectives->RecordFact(TEXT("Demo.Item.Inspected"));
}
void AIntegratedDemoCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    Inspection->OnStateChanged.RemoveDynamic(this, &AIntegratedDemoCharacter::HandleInspection);
    Super::EndPlay(Reason);
}

TSharedRef<SWidget> UDemoCompletionScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.015f,.025f,.025f,.98f));
    Background->SetPadding(FMargin(70.f));
    WidgetTree->RootWidget = Background;
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Panel);
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("IntegratedDemo", "Completed", "Demonstração concluída"));
    Panel->AddChildToVerticalBox(Title);
    UButton* RestartButton = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RestartLabel = WidgetTree->ConstructWidget<UTextBlock>();
    RestartLabel->SetText(NSLOCTEXT("IntegratedDemo", "Restart", "Reiniciar demonstração"));
    RestartButton->SetContent(RestartLabel);
    RestartButton->OnClicked.AddDynamic(this, &UDemoCompletionScreen::Restart);
    Panel->AddChildToVerticalBox(RestartButton);
    UButton* MenuButton = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* MenuLabel = WidgetTree->ConstructWidget<UTextBlock>();
    MenuLabel->SetText(NSLOCTEXT("IntegratedDemo", "Menu", "Voltar ao menu"));
    MenuButton->SetContent(MenuLabel);
    MenuButton->OnClicked.AddDynamic(this, &UDemoCompletionScreen::MainMenu);
    Panel->AddChildToVerticalBox(MenuButton);
    return Super::RebuildWidget();
}
void UDemoCompletionScreen::Restart() { GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()->RestartLevel(); }
void UDemoCompletionScreen::MainMenu() { GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()->LoadLevel(TEXT("/HorrorCore/Core/BootScene")); }
FReply UDemoCompletionScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right) return FReply::Handled();
    return Super::NativeOnKeyDown(Geometry, Event);
}

bool AIntegratedDemoExit::HasRequiredProgress(AActor* Player) const
{
    const UObjectiveJournalComponent* Journal = IsValid(Player) ? Player->FindComponentByClass<UObjectiveJournalComponent>() : nullptr;
    if (!Journal || !Journal->IsReady()) return false;
    for (FName Fact : RequiredFacts) if (Fact.IsNone() || !Journal->HasFact(Fact)) return false;
    return true;
}
bool AIntegratedDemoExit::CanInteract_Implementation() const
{
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    return !bCompleting && !bLocked && PC && HasRequiredProgress(PC->GetPawn());
}
void AIntegratedDemoExit::Interact_Implementation(AActor* Interactor)
{
    const AController* Controller = Cast<AController>(Interactor);
    APawn* Pawn = Cast<APawn>(Interactor);
    if (Controller) Pawn = Controller->GetPawn();
    Interactor = Pawn;
    UGameInstance* GI = GetGameInstance();
    if (!GI || !Pawn || Pawn->GetWorld() != GetWorld() || !Pawn->IsPlayerControlled() || bCompleting || bLocked || !HasRequiredProgress(Interactor)) return;
    if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance)) return;
    USaveSubsystem* Saves = GI->GetSubsystem<USaveSubsystem>();
    UGameFlowSubsystem* Flow = GI->GetSubsystem<UGameFlowSubsystem>();
    if (Saves->IsBusy() || Flow->CurrentState != EGameFlowState::Playing) return;
    bCompleting = true;
    Presentation = GI->GetSubsystem<UUIManagerSubsystem>()->PushScreen(UDemoCompletionScreen::StaticClass());
    if (!Presentation.IsValid()) { bCompleting = false; return; }
    Interactor->FindComponentByClass<UObjectiveJournalComponent>()->RecordFact(TEXT("Demo.Exit.Reached"));
}
FText AIntegratedDemoExit::GetInteractionText_Implementation() const
{
    return NSLOCTEXT("IntegratedDemo", "Leave", "Sair do complexo");
}
void AIntegratedDemoExit::ClosePresentation()
{
    if (Presentation.IsValid() && GetGameInstance()) GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->RemoveScreen(Presentation.Get());
    Presentation.Reset();
    bCompleting = false;
}
void AIntegratedDemoExit::OnActorLoaded_Implementation() { ClosePresentation(); Super::OnActorLoaded_Implementation(); }
void AIntegratedDemoExit::EndPlay(const EEndPlayReason::Type Reason) { ClosePresentation(); Super::EndPlay(Reason); }
