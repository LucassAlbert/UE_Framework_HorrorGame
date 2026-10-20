#include "Progression/ProgressionScreens.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "Progression/PuzzleSocket.h"
#include "Inventory/InventoryScreen.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "ObjectiveJournalComponent.h"
#include "ObjectiveDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Widgets/Layout/Anchors.h"

namespace ProgressionUI
{
    UVerticalBox* BuildPanel(UWidgetTree* Tree)
    {
        UBorder* Border = Tree->ConstructWidget<UBorder>();
        Border->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.92f));
        Border->SetPadding(FMargin(32.f));
        UVerticalBox* Layout = Tree->ConstructWidget<UVerticalBox>();
        Border->SetContent(Layout);
        Tree->RootWidget = Border;
        return Layout;
    }
    UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Layout, const FText& Text)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(Text);
        Label->SetAutoWrapText(true);
        Layout->AddChildToVerticalBox(Label);
        return Label;
    }
    UButton* AddBack(UWidgetTree* Tree, UVerticalBox* Layout)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(NSLOCTEXT("Progression", "Back", "Voltar"));
        Button->SetContent(Text);
        Layout->AddChildToVerticalBox(Button);
        return Button;
    }
}

TSharedRef<SWidget> UPuzzleItemSelectionScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Layout = ProgressionUI::BuildPanel(WidgetTree);
    Title = ProgressionUI::AddText(WidgetTree, Layout, NSLOCTEXT("Puzzles", "SelectionTitle", "Selecionar item"));
    SlotContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Layout->AddChildToVerticalBox(SlotContainer);
    Feedback = ProgressionUI::AddText(WidgetTree, Layout, FText::GetEmpty());
    ProgressionUI::AddBack(WidgetTree, Layout)->OnClicked.AddDynamic(this, &UPuzzleItemSelectionScreen::Back);
    return Super::RebuildWidget();
}

void UPuzzleItemSelectionScreen::SetSource(UProgressionGameplayComponent* InSource)
{
    Unbind();
    Source = InSource;
    if (!InSource || !InSource->GetInventory() || !SlotContainer) return;
    UInventoryComponent* Inventory = InSource->GetInventory();
    Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UPuzzleItemSelectionScreen::Refresh);
    SlotContainer->ClearChildren();
    Buttons.Reset();
    Labels.Reset();
    const int32 Count = Inventory->GetSlots().Num();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UInventorySlotButton* Button = WidgetTree->ConstructWidget<UInventorySlotButton>();
        Button->SlotIndex = Index;
        Button->OnClicked.AddDynamic(Button, &UInventorySlotButton::Select);
        Button->OnSlotSelected.AddDynamic(this, &UPuzzleItemSelectionScreen::Choose);
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Button->SetContent(Text);
        SlotContainer->AddChildToVerticalBox(Button);
        Buttons.Add(Button);
        Labels.Add(Text);
    }
    if (APuzzleSocket* Socket = InSource->GetSelectedSocket())
        Title->SetText(FText::Format(NSLOCTEXT("Puzzles", "InsertInto", "Inserir item: {0}"), Socket->GetSlotName()));
    Refresh();
}

void UPuzzleItemSelectionScreen::Refresh()
{
    UInventoryComponent* Inventory = Source.IsValid() ? Source->GetInventory() : nullptr;
    const TArray<FInventorySlot> Slots = Inventory ? Inventory->GetSlots() : TArray<FInventorySlot>();
    for (int32 Index = 0; Index < Labels.Num(); ++Index)
    {
        UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(Index) : nullptr;
        Labels[Index]->SetText(Item && Slots.IsValidIndex(Index)
            ? FText::Format(NSLOCTEXT("Puzzles", "ItemQuantity", "{0} ×{1}"), Item->DisplayName, Slots[Index].Quantity)
            : NSLOCTEXT("Puzzles", "EmptySlot", "Vazio"));
        Buttons[Index]->SetIsEnabled(Item != nullptr);
    }
}

void UPuzzleItemSelectionScreen::Choose(int32 SlotIndex)
{
    if (!Source.IsValid()) return;
    const FInventoryResult Result = Source->InsertItem(SlotIndex);
    if (!Result.bSuccess && Feedback) Feedback->SetText(Result.Message);
}
void UPuzzleItemSelectionScreen::Back() { CloseScreen(); }
void UPuzzleItemSelectionScreen::Unbind()
{
    if (Source.IsValid() && Source->GetInventory())
        Source->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UPuzzleItemSelectionScreen::Refresh);
    Source.Reset();
}
void UPuzzleItemSelectionScreen::NativeDestruct() { Unbind(); Super::NativeDestruct(); }

TSharedRef<SWidget> UObjectivesScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Layout = ProgressionUI::BuildPanel(WidgetTree);
    ProgressionUI::AddText(WidgetTree, Layout, NSLOCTEXT("Objectives", "Title", "Objetivos"));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Entries = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Entries);
    Layout->AddChildToVerticalBox(Scroll);
    ProgressionUI::AddBack(WidgetTree, Layout)->OnClicked.AddDynamic(this, &UObjectivesScreen::Back);
    return Super::RebuildWidget();
}

void UObjectivesScreen::SetSource(UObjectiveJournalComponent* InSource)
{
    Unbind();
    Source = InSource;
    if (InSource) InSource->OnJournalChanged.AddUniqueDynamic(this, &UObjectivesScreen::Refresh);
    Refresh();
}

void UObjectivesScreen::Refresh()
{
    if (!Entries) return;
    Entries->ClearChildren();
    if (!Source.IsValid() || !Source->IsReady())
    {
        ProgressionUI::AddText(WidgetTree, Entries, NSLOCTEXT("Objectives", "Loading", "Carregando objetivos…"));
        return;
    }
    int32 Count = 0;
    for (const FObjectiveRecord& Record : Source->GetRecords())
    {
        UObjectiveDefinition* Definition = Record.Definition.Get();
        if (!Definition || Record.State == EObjectiveState::Inactive) continue;
        const FText State = Record.State == EObjectiveState::Completed
            ? NSLOCTEXT("Objectives", "Completed", "Concluído") : NSLOCTEXT("Objectives", "Active", "Em andamento");
        ProgressionUI::AddText(WidgetTree, Entries, FText::Format(NSLOCTEXT("Objectives", "Entry", "{0} — {1}"), State, Definition->Title));
        ProgressionUI::AddText(WidgetTree, Entries, Definition->Description);
        ++Count;
    }
    if (Count == 0) ProgressionUI::AddText(WidgetTree, Entries, NSLOCTEXT("Objectives", "Empty", "Nenhum objetivo disponível."));
}
void UObjectivesScreen::Back() { CloseScreen(); }
void UObjectivesScreen::Unbind()
{
    if (Source.IsValid()) Source->OnJournalChanged.RemoveDynamic(this, &UObjectivesScreen::Refresh);
    Source.Reset();
}
void UObjectivesScreen::NativeDestruct() { Unbind(); Super::NativeDestruct(); }

TSharedRef<SWidget> UObjectiveNotice::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f));
    Border->SetPadding(FMargin(16.f));
    Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetWrapTextAt(340.f);
    Border->SetContent(Label);
    UCanvasPanelSlot* NoticeSlot = Canvas->AddChildToCanvas(Border);
    NoticeSlot->SetAnchors(FAnchors(1.f, 0.f));
    NoticeSlot->SetAlignment(FVector2D(1.f, 0.f));
    NoticeSlot->SetPosition(FVector2D(-24.f, 24.f));
    NoticeSlot->SetAutoSize(true);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return Super::RebuildWidget();
}

void UObjectiveNotice::Enqueue(const FText& Message)
{
    Pending.Add(Message);
    if (RemainingSeconds <= 0.f) ShowNext();
}

void UObjectiveNotice::ShowNext()
{
    if (Pending.IsEmpty()) { RemoveFromParent(); return; }
    if (Label) Label->SetText(Pending[0]);
    Pending.RemoveAt(0);
    constexpr float NoticeDurationSeconds = 4.f;
    RemainingSeconds = NoticeDurationSeconds;
}

void UObjectiveNotice::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    RemainingSeconds -= FMath::Max(0.f, DeltaSeconds);
    if (RemainingSeconds <= 0.f) ShowNext();
}
void UObjectiveNotice::NativeDestruct() { Pending.Reset(); RemainingSeconds = 0.f; Super::NativeDestruct(); }
