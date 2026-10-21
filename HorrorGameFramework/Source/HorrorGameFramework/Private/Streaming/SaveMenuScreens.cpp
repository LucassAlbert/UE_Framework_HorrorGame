#include "Streaming/SaveMenuScreens.h"
#include "Subsystems/SaveSubsystem.h"
#include "Subsystems/SaveSlotCatalog.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

namespace
{
    UVerticalBox* CreatePanel(UWidgetTree* Tree)
    {
        UBorder* Background = Tree->ConstructWidget<UBorder>();
        Background->SetBrushColor(FLinearColor(.02f, .025f, .03f, .96f));
        Background->SetPadding(FMargin(60.f));
        Tree->RootWidget = Background;
        UVerticalBox* Panel = Tree->ConstructWidget<UVerticalBox>();
        Background->SetContent(Panel);
        return Panel;
    }
    UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Panel, const FText& Text)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(Text);
        Label->SetAutoWrapText(true);
        Panel->AddChildToVerticalBox(Label);
        return Label;
    }
    UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Panel, const FText& Text, UTextBlock** OutLabel = nullptr)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(Text);
        Button->SetContent(Label);
        Panel->AddChildToVerticalBox(Button);
        if (OutLabel) *OutLabel = Label;
        return Button;
    }
}
void USaveSlotButtonAction::Activate() { if (Screen.IsValid()) Screen->SelectSlot(SlotIndex); }
TSharedRef<SWidget> UFrameworkSaveSlotsScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Panel = CreatePanel(WidgetTree);
    Title = AddText(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Load", "Carregar jogo"));
    for (int32 Index = 0; Index < USaveSubsystem::SlotCount; ++Index)
    {
        UTextBlock* Label;
        UButton* Button = AddButton(WidgetTree, Panel, FText::AsNumber(Index + 1), &Label);
        USaveSlotButtonAction* Action = NewObject<USaveSlotButtonAction>(this);
        Action->Screen = this;
        Action->SlotIndex = Index;
        Button->OnClicked.AddDynamic(Action, &USaveSlotButtonAction::Activate);
        Buttons.Add(Button); Labels.Add(Label); Actions.Add(Action);
    }
    Feedback = AddText(WidgetTree, Panel, FText::GetEmpty());
    ConfirmButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Overwrite", "Confirmar sobrescrita"));
    ConfirmButton->OnClicked.AddDynamic(this, &UFrameworkSaveSlotsScreen::ConfirmOverwrite);
    ConfirmButton->SetVisibility(ESlateVisibility::Collapsed);
    BackButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Back", "Voltar"));
    BackButton->OnClicked.AddDynamic(this, &UFrameworkSaveSlotsScreen::Back);
    return Super::RebuildWidget();
}
void UFrameworkSaveSlotsScreen::OnScreenEnter()
{
    Super::OnScreenEnter();
    USaveSlotCatalog* Catalog = GetGameInstance()->GetSubsystem<USaveSlotCatalog>();
    Catalog->OnRefreshed.AddUniqueDynamic(this, &UFrameworkSaveSlotsScreen::RefreshRows);
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    Saves->OnSaveCompleted.AddUniqueDynamic(this, &UFrameworkSaveSlotsScreen::HandleSaved);
    Saves->OnLoadCompleted.AddUniqueDynamic(this, &UFrameworkSaveSlotsScreen::HandleLoaded);
    Catalog->Refresh();
    RefreshRows();
}
void UFrameworkSaveSlotsScreen::OnScreenExit()
{
    bBusy = false;
    GetGameInstance()->GetSubsystem<USaveSlotCatalog>()->OnRefreshed.RemoveDynamic(this, &UFrameworkSaveSlotsScreen::RefreshRows);
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    Saves->OnSaveCompleted.RemoveDynamic(this, &UFrameworkSaveSlotsScreen::HandleSaved);
    Saves->OnLoadCompleted.RemoveDynamic(this, &UFrameworkSaveSlotsScreen::HandleLoaded);
    Super::OnScreenExit();
}
void UFrameworkSaveSlotsScreen::SetSaveMode()
{
    bSaveMode = true;
    OverwriteSlot = INDEX_NONE;
    Title->SetText(NSLOCTEXT("SaveMenu", "Save", "Salvar jogo"));
    RefreshRows();
}
void UFrameworkSaveSlotsScreen::RefreshRows()
{
    const USaveSlotCatalog* Catalog = GetGameInstance()->GetSubsystem<USaveSlotCatalog>();
    for (const FSaveSlotInfo& Info : Catalog->GetEntries())
    {
        if (!Labels.IsValidIndex(Info.SlotIndex)) continue;
        FText Detail = NSLOCTEXT("SaveMenu", "Empty", "Vazio");
        if (Info.bReadable)
            Detail = FText::Format(NSLOCTEXT("SaveMenu", "Metadata", "{0} | {1}"), FText::FromString(Info.MapName), FText::AsDateTime(Info.SavedAt));
        else if (Info.bExists) Detail = Info.Error;
        Labels[Info.SlotIndex]->SetText(FText::Format(NSLOCTEXT("SaveMenu", "SlotIndex", "Slot {0} — {1}"), FText::AsNumber(Info.SlotIndex + 1), Detail));
        Buttons[Info.SlotIndex]->SetIsEnabled(!bBusy && !Catalog->IsReading() && (bSaveMode || Info.bReadable));
    }
}
void UFrameworkSaveSlotsScreen::SetBusy(bool bValue)
{
    bBusy = bValue;
    ConfirmButton->SetIsEnabled(!bBusy);
    BackButton->SetIsEnabled(!bBusy);
    RefreshRows();
}
void UFrameworkSaveSlotsScreen::SelectSlot(int32 Index)
{
    if (bBusy || !USaveSubsystem::IsValidSlot(Index)) return;
    const USaveSlotCatalog* Catalog = GetGameInstance()->GetSubsystem<USaveSlotCatalog>();
    if (Catalog->IsReading()) return;
    const TArray<FSaveSlotInfo> Entries = Catalog->GetEntries();
    if (!Entries.IsValidIndex(Index) || (!bSaveMode && !Entries[Index].bReadable)) return;
    if (bSaveMode && Entries[Index].bExists)
    {
        OverwriteSlot = Index;
        Feedback->SetText(FText::Format(NSLOCTEXT("SaveMenu", "OverwritePrompt", "Sobrescrever o slot {0}?"), FText::AsNumber(Index + 1)));
        ConfirmButton->SetVisibility(ESlateVisibility::Visible);
        ConfirmButton->SetKeyboardFocus();
        return;
    }
    BeginOperation(Index);
}
void UFrameworkSaveSlotsScreen::ConfirmOverwrite() { if (!bBusy && OverwriteSlot != INDEX_NONE) BeginOperation(OverwriteSlot); }
void UFrameworkSaveSlotsScreen::BeginOperation(int32 SlotIndex)
{
    SetBusy(true);
    Feedback->SetText(NSLOCTEXT("SaveMenu", "Working", "Aguarde..."));
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    const bool bStarted = bSaveMode ? Saves->SaveToSlot(SlotIndex) : Saves->LoadFromSlot(SlotIndex);
    if (!bStarted && bBusy)
    {
        SetBusy(false);
        Feedback->SetText(NSLOCTEXT("SaveMenu", "Unavailable", "Não foi possível iniciar. Aguarde o término da operação atual e tente novamente."));
    }
}
void UFrameworkSaveSlotsScreen::HandleSaved(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (!bSaveMode || !bBusy) return;
    SetBusy(false);
    OverwriteSlot = INDEX_NONE;
    ConfirmButton->SetVisibility(ESlateVisibility::Collapsed);
    Feedback->SetText(Message);
}
void UFrameworkSaveSlotsScreen::HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (bSaveMode || !bBusy) return;
    // Boot owns the successful transition, including cross-map UI reconstruction.
    SetBusy(false);
    if (!bSuccess) Feedback->SetText(Message);
}
void UFrameworkSaveSlotsScreen::Back()
{
    if (bBusy) return;
    if (OverwriteSlot != INDEX_NONE)
    {
        OverwriteSlot = INDEX_NONE;
        ConfirmButton->SetVisibility(ESlateVisibility::Collapsed);
        Feedback->SetText(FText::GetEmpty());
        BackButton->SetKeyboardFocus();
    }
    else CloseScreen();
}
FReply UFrameworkSaveSlotsScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right) { Back(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry, Event);
}
TSharedRef<SWidget> UFrameworkMainMenuScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Panel = CreatePanel(WidgetTree);
    AddText(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Title", "Horror Game Framework"));
    ContinueButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Continue", "Continuar"));
    ContinueButton->OnClicked.AddDynamic(this, &UFrameworkMainMenuScreen::ContinueGame);
    MenuButtons.Add(ContinueButton);
    UButton* NewButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "New", "Novo jogo"));
    NewButton->OnClicked.AddDynamic(this, &UFrameworkMainMenuScreen::NewGame); MenuButtons.Add(NewButton);
    UButton* LoadButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Load", "Carregar jogo"));
    LoadButton->OnClicked.AddDynamic(this, &UFrameworkMainMenuScreen::LoadGame); MenuButtons.Add(LoadButton);
    UButton* SettingsButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Settings", "Configurações"));
    SettingsButton->OnClicked.AddDynamic(this, &UFrameworkMainMenuScreen::Settings); MenuButtons.Add(SettingsButton);
    UButton* QuitButton = AddButton(WidgetTree, Panel, NSLOCTEXT("SaveMenu", "Quit", "Sair"));
    QuitButton->OnClicked.AddDynamic(this, &UFrameworkMainMenuScreen::Quit); MenuButtons.Add(QuitButton);
    Feedback = AddText(WidgetTree, Panel, FText::GetEmpty());
    return Super::RebuildWidget();
}
void UFrameworkMainMenuScreen::OnScreenEnter()
{
    Super::OnScreenEnter();
    USaveSlotCatalog* Catalog = GetGameInstance()->GetSubsystem<USaveSlotCatalog>();
    Catalog->OnRefreshed.AddUniqueDynamic(this, &UFrameworkMainMenuScreen::RefreshContinue);
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->OnLoadCompleted.AddUniqueDynamic(this, &UFrameworkMainMenuScreen::HandleLoaded);
    Feedback->SetText(GetGameInstance()->GetSubsystem<USaveSubsystem>()->LastLoadError);
    Catalog->Refresh();
    RefreshContinue();
}
void UFrameworkMainMenuScreen::OnScreenExit()
{
    bLoading = false;
    GetGameInstance()->GetSubsystem<USaveSlotCatalog>()->OnRefreshed.RemoveDynamic(this, &UFrameworkMainMenuScreen::RefreshContinue);
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->OnLoadCompleted.RemoveDynamic(this, &UFrameworkMainMenuScreen::HandleLoaded);
    Super::OnScreenExit();
}
void UFrameworkMainMenuScreen::RefreshContinue()
{
    const USaveSlotCatalog* Catalog = GetGameInstance()->GetSubsystem<USaveSlotCatalog>();
    ContinueButton->SetIsEnabled(!bLoading && !Catalog->IsReading() && Catalog->GetLatestSlot() != INDEX_NONE);
}
void UFrameworkMainMenuScreen::ContinueGame()
{
    if (bLoading) return;
    bLoading = true;
    for (UButton* Button : MenuButtons) Button->SetIsEnabled(false);
    Feedback->SetText(NSLOCTEXT("SaveMenu", "Working", "Aguarde..."));
    if (!GetGameInstance()->GetSubsystem<USaveSubsystem>()->LoadLatest() && bLoading)
        HandleLoaded(INDEX_NONE, false, NSLOCTEXT("SaveMenu", "Busy", "Aguarde a operação atual e tente novamente."));
}
void UFrameworkMainMenuScreen::HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message)
{
    if (!bLoading) return;
    bLoading = false;
    if (!bSuccess)
    {
        Feedback->SetText(Message);
        for (UButton* Button : MenuButtons) Button->SetIsEnabled(true);
        RefreshContinue();
    }
}
void UFrameworkMainMenuScreen::NewGame()
{
    if (bLoading) return;
    if (NewGameMap.IsNull()) { Feedback->SetText(NSLOCTEXT("SaveMenu", "NoMap", "Configure o mapa inicial deste jogo.")); return; }
    GetGameInstance()->GetSubsystem<UGameFlowSubsystem>()->LoadLevel(FName(*NewGameMap.ToSoftObjectPath().GetLongPackageName()));
}
void UFrameworkMainMenuScreen::LoadGame() { if (!bLoading) GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->OpenScreen(EHorrorScreen::SaveSlots); }
void UFrameworkMainMenuScreen::Settings() { if (!bLoading) GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->OpenSettingsUI(); }
void UFrameworkMainMenuScreen::Quit() { if (!bLoading) UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }
