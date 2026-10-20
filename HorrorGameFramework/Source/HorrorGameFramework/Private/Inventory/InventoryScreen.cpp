#include "Inventory/InventoryScreen.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

UInventoryScreen::UInventoryScreen(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

UButton* UInventoryScreen::AddAction(UVerticalBox* Layout, const FText& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(Label);
    Button->SetContent(Text);
    Layout->AddChildToVerticalBox(Button);
    return Button;
}

TSharedRef<SWidget> UInventoryScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.92f));
    Background->SetPadding(FMargin(40.f));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Layout);
    WidgetTree->RootWidget = Background;
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("Inventory", "Title", "Inventário"));
    Layout->AddChildToVerticalBox(Title);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        UInventorySlotButton* Button = WidgetTree->ConstructWidget<UInventorySlotButton>();
        Button->SlotIndex = Index;
        Button->OnClicked.AddDynamic(Button, &UInventorySlotButton::Select);
        Button->OnSlotSelected.AddDynamic(this, &UInventoryScreen::SelectSlot);
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Button->SetContent(Text);
        SlotButtons.Add(Button);
        SlotLabels.Add(Text);
        Layout->AddChildToVerticalBox(Button);
    }
    Description = WidgetTree->ConstructWidget<UTextBlock>();
    Layout->AddChildToVerticalBox(Description);
    UseButton = AddAction(Layout, NSLOCTEXT("Inventory", "Use", "Usar"));
    EquipButton = AddAction(Layout, NSLOCTEXT("Inventory", "Equip", "Equipar / desequipar"));
    CombineButton = AddAction(Layout, NSLOCTEXT("Inventory", "Combine", "Combinar"));
    InspectButton = AddAction(Layout, NSLOCTEXT("Inventory", "Inspect", "Inspecionar"));
    DropButton = AddAction(Layout, NSLOCTEXT("Inventory", "Drop", "Dropar uma unidade"));
    DiscardButton = AddAction(Layout, NSLOCTEXT("Inventory", "Discard", "Descartar"));
    ObjectivesButton = AddAction(Layout, NSLOCTEXT("Inventory", "Objectives", "Objetivos"));
    DocumentsButton = AddAction(Layout, NSLOCTEXT("Inventory", "Documents", "Documentos"));
    UButton* Close = AddAction(Layout, NSLOCTEXT("Inventory", "Back", "Voltar"));
    UseButton->OnClicked.AddDynamic(this, &UInventoryScreen::Use);
    EquipButton->OnClicked.AddDynamic(this, &UInventoryScreen::Equip);
    CombineButton->OnClicked.AddDynamic(this, &UInventoryScreen::Combine);
    InspectButton->OnClicked.AddDynamic(this, &UInventoryScreen::Inspect);
    DropButton->OnClicked.AddDynamic(this, &UInventoryScreen::Drop);
    DiscardButton->OnClicked.AddDynamic(this, &UInventoryScreen::Discard);
    ObjectivesButton->OnClicked.AddDynamic(this, &UInventoryScreen::Objectives);
    DocumentsButton->OnClicked.AddDynamic(this, &UInventoryScreen::Documents);
    Close->OnClicked.AddDynamic(this, &UInventoryScreen::Back);
    Feedback = WidgetTree->ConstructWidget<UTextBlock>();
    Layout->AddChildToVerticalBox(Feedback);
    Refresh();
    return Super::RebuildWidget();
}

void UInventoryScreen::SetSource(UInventoryGameplayComponent* InSource)
{
    UnbindSource();
    Source = InSource;
    if (InSource)
    {
        InSource->OnFeedback.AddUniqueDynamic(this, &UInventoryScreen::ShowFeedback);
        if (UInventoryComponent* Inventory = InSource->GetInventory())
            Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UInventoryScreen::Refresh);
    }
    Refresh();
    if (SlotButtons.IsValidIndex(SelectedSlot)) SlotButtons[SelectedSlot]->SetKeyboardFocus();
}

void UInventoryScreen::UnbindSource()
{
    if (UInventoryGameplayComponent* Gameplay = Source.Get())
    {
        Gameplay->OnFeedback.RemoveDynamic(this, &UInventoryScreen::ShowFeedback);
        if (UInventoryComponent* Inventory = Gameplay->GetInventory())
            Inventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryScreen::Refresh);
    }
}

void UInventoryScreen::NativeDestruct()
{
    UnbindSource();
    Source.Reset();
    Super::NativeDestruct();
}

void UInventoryScreen::Refresh()
{
    UInventoryComponent* Inventory = Source.IsValid() ? Source->GetInventory() : nullptr;
    const TArray<FInventorySlot> Slots = Inventory ? Inventory->GetSlots() : TArray<FInventorySlot>();
    for (int32 Index = 0; Index < SlotLabels.Num(); ++Index)
    {
        UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(Index) : nullptr;
        const FString Prefix = Index == SelectedSlot ? TEXT("> ") : TEXT("  ");
        const FString Content = Item && Slots.IsValidIndex(Index)
            ? FString::Printf(TEXT("%s x%d%s"), *Item->DisplayName.ToString(), Slots[Index].Quantity,
                Inventory->GetEquippedSlot() == Index ? TEXT(" [equipado]") : TEXT("")) : TEXT("Vazio");
        SlotLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s%d: %s"), *Prefix, Index + 1, *Content)));
    }
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(SelectedSlot) : nullptr;
    if (Description) Description->SetText(Item ? Item->Description : FText::GetEmpty());
    if (UseButton) UseButton->SetIsEnabled(Item && Item->bCanUse);
    if (EquipButton) EquipButton->SetIsEnabled(Item && Item->bCanEquip);
    if (CombineButton) CombineButton->SetIsEnabled(Item && !Item->Recipes.IsEmpty());
    if (DropButton) DropButton->SetIsEnabled(Item && Item->bCanDrop);
    if (DiscardButton) DiscardButton->SetIsEnabled(Item && Item->bCanDiscard);
    if (InspectButton) InspectButton->SetIsEnabled(Item && Item->bCanInspect && Source.IsValid() && Source->OnInspectRequested.IsBound());
    if (ObjectivesButton) ObjectivesButton->SetVisibility(Source.IsValid() && Source->OnObjectivesRequested.IsBound()
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (DocumentsButton) DocumentsButton->SetVisibility(Source.IsValid() && Source->OnDocumentsRequested.IsBound()
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UInventoryScreen::SelectSlot(int32 Index)
{
    SelectedSlot = Index;
    if (FirstIngredient != INDEX_NONE && Source.IsValid())
    {
        const int32 First = FirstIngredient;
        FirstIngredient = INDEX_NONE;
        const FInventoryResult Result = Source->CombineSlots(First, SelectedSlot);
        Source->ReportResult(Result);
        if (Result.bSuccess)
            ShowFeedback(NSLOCTEXT("Inventory", "Combined", "Itens combinados."));
    }
    Refresh();
}

void UInventoryScreen::ShowFeedback(FText Message) { if (Feedback) Feedback->SetText(Message); }
void UInventoryScreen::Use() { if (Source.IsValid()) Source->ReportResult(Source->UseSlot(SelectedSlot)); }
void UInventoryScreen::Equip()
{
    if (!Source.IsValid() || !Source->GetInventory()) return;
    if (Source->GetInventory()->GetEquippedSlot() == SelectedSlot)
    {
        Source->GetInventory()->Unequip();
        CloseScreen();
    }
    else Source->ReportResult(Source->EquipSlot(SelectedSlot));
}
void UInventoryScreen::Combine()
{
    FirstIngredient = SelectedSlot;
    ShowFeedback(NSLOCTEXT("Inventory", "SelectSecond", "Selecione o segundo ingrediente."));
    if (SlotButtons.IsValidIndex(SelectedSlot)) SlotButtons[SelectedSlot]->SetKeyboardFocus();
}
void UInventoryScreen::Drop() { if (Source.IsValid()) Source->ReportResult(Source->DropSlot(SelectedSlot)); }
void UInventoryScreen::Discard() { if (Source.IsValid() && Source->GetInventory()) Source->ReportResult(Source->GetInventory()->DiscardSlot(SelectedSlot)); }
void UInventoryScreen::Inspect() { if (Source.IsValid()) Source->ReportResult(Source->InspectSlot(SelectedSlot)); }
void UInventoryScreen::Back() { CloseScreen(); }
void UInventoryScreen::Objectives() { if (Source.IsValid()) Source->RequestObjectives(); }
void UInventoryScreen::Documents() { if (Source.IsValid()) Source->RequestDocuments(); }
