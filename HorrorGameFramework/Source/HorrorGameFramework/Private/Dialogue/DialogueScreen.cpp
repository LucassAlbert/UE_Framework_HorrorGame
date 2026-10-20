#include "Dialogue/DialogueScreen.h"
#include "Dialogue/DialogueGameplayComponent.h"
#include "DialogueSessionComponent.h"
#include "DialogueDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UDialogueScreen::UDialogueScreen(const FObjectInitializer& Initializer) : Super(Initializer)
{
    bPauseWorld = false;
    bUseGameplayInput = true;
}
TSharedRef<SWidget> UDialogueScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.93f));
    Border->SetPadding(FMargin(24.f));
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Border);
    PanelSlot->SetAnchors(FAnchors(0.05f, 0.62f, 0.95f, 0.96f));
    PanelSlot->SetOffsets(FMargin(0.f));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->SetContent(Layout);
    SpeakerText = WidgetTree->ConstructWidget<UTextBlock>();
    LineText = WidgetTree->ConstructWidget<UTextBlock>();
    LineText->SetAutoWrapText(true);
    ProgressText = WidgetTree->ConstructWidget<UTextBlock>();
    Layout->AddChildToVerticalBox(SpeakerText);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->AddChild(LineText);
    Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Layout->AddChildToVerticalBox(ProgressText);
    // Mouse interaction is unnecessary; unhandled keys reach Enhanced Input.
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return Super::RebuildWidget();
}
void UDialogueScreen::SetSource(UDialogueGameplayComponent* InSource)
{
    Unbind();
    Source = InSource;
    if (UDialogueSessionComponent* Session = InSource ? InSource->GetSession() : nullptr)
    {
        Session->OnLineChanged.AddUniqueDynamic(this, &UDialogueScreen::Refresh);
        Refresh(Session->GetDefinition(), Session->GetLineIndex());
    }
}
void UDialogueScreen::Refresh(UDialogueDefinition* Definition, int32 LineIndex)
{
    if (!Definition || !Definition->Lines.IsValidIndex(LineIndex) || !LineText) return;
    SpeakerText->SetText(Definition->SpeakerName);
    LineText->SetText(Definition->Lines[LineIndex].Text);
    ProgressText->SetText(FText::Format(NSLOCTEXT("Dialogue", "Progress", "Fala {0} de {1} — Interagir: avançar / Pausa: cancelar"), LineIndex + 1, Definition->Lines.Num()));
}
bool UDialogueScreen::HandlePrimaryAction() { if (Source.IsValid()) Source->Advance(); return true; }
bool UDialogueScreen::HandleBackAction() { if (Source.IsValid()) Source->Cancel(); return true; }
void UDialogueScreen::OnScreenExit() { if (Source.IsValid()) Source->Cancel(); Super::OnScreenExit(); }
void UDialogueScreen::Unbind()
{
    if (Source.IsValid() && Source->GetSession()) Source->GetSession()->OnLineChanged.RemoveDynamic(this, &UDialogueScreen::Refresh);
    Source.Reset();
}
void UDialogueScreen::NativeDestruct() { Unbind(); Super::NativeDestruct(); }
