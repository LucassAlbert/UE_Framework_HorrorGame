#include "Threat/ThreatGameOverScreen.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

UThreatGameOverScreen::UThreatGameOverScreen(const FObjectInitializer& Initializer) : Super(Initializer) { SetIsFocusable(true); }
TSharedRef<SWidget> UThreatGameOverScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.025f, .005f, .005f, .96f));
    Background->SetPadding(FMargin(70.f));
    WidgetTree->RootWidget = Background;
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Layout);
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("Threat", "GameOver", "Você foi capturado"));
    Layout->AddChildToVerticalBox(Title);
    LoadButton = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* LoadText = WidgetTree->ConstructWidget<UTextBlock>();
    LoadText->SetText(NSLOCTEXT("Threat", "LoadLatest", "Carregar último save"));
    LoadButton->SetContent(LoadText);
    LoadButton->OnClicked.AddDynamic(this, &UThreatGameOverScreen::LoadLatest);
    Layout->AddChildToVerticalBox(LoadButton);
    RestartButton = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RestartText = WidgetTree->ConstructWidget<UTextBlock>();
    RestartText->SetText(NSLOCTEXT("Threat", "Restart", "Reiniciar mapa"));
    RestartButton->SetContent(RestartText);
    RestartButton->OnClicked.AddDynamic(this, &UThreatGameOverScreen::Restart);
    Layout->AddChildToVerticalBox(RestartButton);
    Feedback = WidgetTree->ConstructWidget<UTextBlock>();
    Feedback->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(Feedback);
    return Super::RebuildWidget();
}
void UThreatGameOverScreen::OnScreenEnter()
{
    Super::OnScreenEnter();
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->OnLoadCompleted.AddUniqueDynamic(this, &UThreatGameOverScreen::HandleLoaded);
}
void UThreatGameOverScreen::OnScreenExit()
{
    bLoading = false;
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->OnLoadCompleted.RemoveDynamic(this, &UThreatGameOverScreen::HandleLoaded);
    Super::OnScreenExit();
}
void UThreatGameOverScreen::LoadLatest()
{
    if (bLoading) return;
    bLoading = true;
    LoadButton->SetIsEnabled(false);
    RestartButton->SetIsEnabled(false);
    Feedback->SetText(NSLOCTEXT("Threat", "Loading", "Procurando o save mais recente..."));
    if (!GetGameInstance()->GetSubsystem<USaveSubsystem>()->LoadLatest() && bLoading)
        HandleLoaded(INDEX_NONE, false, NSLOCTEXT("Threat", "Busy", "Há uma operação de save em andamento. Tente novamente."));
}
void UThreatGameOverScreen::HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (!bLoading) return;
    bLoading = false;
    if (!bSuccess)
    {
        Feedback->SetText(Message);
        LoadButton->SetIsEnabled(true);
        RestartButton->SetIsEnabled(true);
        return;
    }
    // Boot finalizes the presentation after the destination world is ready.
}
void UThreatGameOverScreen::Restart()
{
    if (!bLoading && !GetGameInstance()->GetSubsystem<USaveSubsystem>()->IsBusy()) GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()->RestartLevel();
}
FReply UThreatGameOverScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right) return FReply::Handled();
    return Super::NativeOnKeyDown(Geometry, Event);
}
