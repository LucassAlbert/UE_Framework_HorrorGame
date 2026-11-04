#include "Controls/ControlSettingsScreen.h"
#include "HorrorControlSettingsSubsystem.h"
#include "UIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "HAL/PlatformTime.h"

namespace ControlsMenu
{
    void Font(UTextBlock* Label, int32 Size = 26)
    {
        auto Style = Label->GetFont(); Style.Size = Size; Label->SetFont(Style);
    }
    UButton* Button(UWidgetTree* Tree, const FText& Text)
    {
        auto* Result = Tree->ConstructWidget<UButton>();
        auto* Label = Tree->ConstructWidget<UTextBlock>(); Font(Label); Label->SetText(Text); Result->SetContent(Label); return Result;
    }
    UTextBlock* Text(UWidgetTree* Tree, UVerticalBox* Panel, const FText& Value)
    {
        auto* Label = Tree->ConstructWidget<UTextBlock>(); Font(Label); Label->SetText(Value); Label->SetAutoWrapText(true); Label->SetWrapTextAt(1400); Panel->AddChildToVerticalBox(Label); return Label;
    }
    USizeBox* Width(UWidgetTree* Tree, UWidget* Content, float Size)
    {
        auto* Box = Tree->ConstructWidget<USizeBox>(); Box->SetWidthOverride(Size); Box->SetContent(Content); return Box;
    }
    FText NumberLabel(uint8 Field)
    {
        switch (Field)
        {
            case 0: return NSLOCTEXT("ControlsMenu", "FOV", "Campo de visão (FOV)");
            case 1: return NSLOCTEXT("ControlsMenu", "MouseX", "Mouse: sensibilidade X");
            case 2: return NSLOCTEXT("ControlsMenu", "MouseY", "Mouse: sensibilidade Y");
            case 3: return NSLOCTEXT("ControlsMenu", "PadX", "Controle: sensibilidade X");
            default: return NSLOCTEXT("ControlsMenu", "PadY", "Controle: sensibilidade Y");
        }
    }
}
void UControlNumberRow::Configure(UControlSettingsScreen* Screen, uint8 InField) { OwnerScreen = Screen; Field = InField; Refresh(); }
TSharedRef<SWidget> UControlNumberRow::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    auto* Panel = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(52); Height->SetContent(Panel); WidgetTree->RootWidget = Height;
    Label = WidgetTree->ConstructWidget<UTextBlock>(); ControlsMenu::Font(Label); Panel->AddChildToHorizontalBox(ControlsMenu::Width(WidgetTree, Label, OwnerScreen.IsValid() && OwnerScreen->IsEmbeddedInScreen() ? 400 : 430));
    auto* Minus = ControlsMenu::Button(WidgetTree, FText::FromString(TEXT("  -  ")));
    Minus->OnClicked.AddDynamic(this, &UControlNumberRow::Decrease); Panel->AddChildToHorizontalBox(Minus);
    Slider = WidgetTree->ConstructWidget<USlider>(); Slider->OnValueChanged.AddDynamic(this, &UControlNumberRow::Change);
    Panel->AddChildToHorizontalBox(ControlsMenu::Width(WidgetTree, Slider, OwnerScreen.IsValid() && OwnerScreen->IsEmbeddedInScreen() ? 180 : 260));
    auto* Plus = ControlsMenu::Button(WidgetTree, FText::FromString(TEXT("  +  ")));
    Plus->OnClicked.AddDynamic(this, &UControlNumberRow::Increase); Panel->AddChildToHorizontalBox(Plus);
    ValueLabel = WidgetTree->ConstructWidget<UTextBlock>(); ControlsMenu::Font(ValueLabel); Panel->AddChildToHorizontalBox(ValueLabel);
    Refresh(); return Super::RebuildWidget();
}
void UControlNumberRow::Refresh()
{
    if (!OwnerScreen.IsValid() || !Slider) return;
    bRefreshing = true;
    const auto Limits = OwnerScreen->GetLimits(Field); const float Value = OwnerScreen->GetNumber(Field);
    Slider->SetMinValue(Limits.X); Slider->SetMaxValue(Limits.Y); Slider->SetStepSize(Field == 0 ? 1.f : .1f); Slider->SetValue(Value);
    Label->SetText(ControlsMenu::NumberLabel(Field));
    ValueLabel->SetText(FText::FromString(Field == 0 ? FString::Printf(TEXT("  %.0f°"), Value) : FString::Printf(TEXT("  %.2fx"), Value)));
    bRefreshing = false;
}
void UControlNumberRow::Decrease() { if (OwnerScreen.IsValid()) OwnerScreen->SetNumber(Field, OwnerScreen->GetNumber(Field) - (Field == 0 ? 1.f : .1f)); }
void UControlNumberRow::Increase() { if (OwnerScreen.IsValid()) OwnerScreen->SetNumber(Field, OwnerScreen->GetNumber(Field) + (Field == 0 ? 1.f : .1f)); }
void UControlNumberRow::Change(float Value) { if (!bRefreshing && OwnerScreen.IsValid()) OwnerScreen->SetNumber(Field, Value); }

void UControlBindingRow::Configure(UControlSettingsScreen* Screen, FName InId) { OwnerScreen = Screen; Id = InId; Refresh(); }
TSharedRef<SWidget> UControlBindingRow::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    auto* Panel = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(76); Height->SetContent(Panel); WidgetTree->RootWidget = Height;
    Label = WidgetTree->ConstructWidget<UTextBlock>(); ControlsMenu::Font(Label); Label->SetAutoWrapText(true); Label->SetWrapTextAt(OwnerScreen.IsValid() && OwnerScreen->IsEmbeddedInScreen() ? 400 : 520); Panel->AddChildToHorizontalBox(ControlsMenu::Width(WidgetTree, Label, OwnerScreen.IsValid() && OwnerScreen->IsEmbeddedInScreen() ? 420 : 540));
    Button = WidgetTree->ConstructWidget<UButton>(); KeyLabel = WidgetTree->ConstructWidget<UTextBlock>(); ControlsMenu::Font(KeyLabel); KeyLabel->SetAutoWrapText(true); KeyLabel->SetWrapTextAt(420); Button->SetContent(KeyLabel);
    Button->OnClicked.AddDynamic(this, &UControlBindingRow::Capture); Panel->AddChildToHorizontalBox(ControlsMenu::Width(WidgetTree, Button, OwnerScreen.IsValid() && OwnerScreen->IsEmbeddedInScreen() ? 330 : 440));
    Refresh(); return Super::RebuildWidget();
}
void UControlBindingRow::Refresh()
{
    auto* Settings = OwnerScreen.IsValid() ? OwnerScreen->GetSource() : nullptr;
    const auto* Binding = Settings && Settings->GetDefinition() ? Settings->GetDefinition()->FindBinding(Id) : nullptr;
    if (!Label || !Binding) return;
    Label->SetText(Binding->DisplayName);
    const auto Value = Settings->GetPreferences(); const FKey* Key = Value.Bindings.Find(Id);
    FText KeyText=Key?Key->GetDisplayName():FText::GetEmpty();
    if(Key)
    {
        if(*Key==EKeys::Gamepad_FaceButton_Bottom)KeyText=FText::FromString(TEXT("A / Cruz"));
        else if(*Key==EKeys::Gamepad_FaceButton_Right)KeyText=FText::FromString(TEXT("B / Círculo"));
        else if(*Key==EKeys::Gamepad_Special_Right)KeyText=FText::FromString(TEXT("Menu / Start"));
        else if(*Key==EKeys::Gamepad_Special_Left)KeyText=FText::FromString(TEXT("View / Select"));
    }
    KeyLabel->SetText(KeyText);
}
void UControlBindingRow::Capture() { if (OwnerScreen.IsValid()) OwnerScreen->BeginCapture(Id); }
void UControlBindingRow::FocusButton() { if (Button) Button->SetUserFocus(GetOwningPlayer()); }

TSharedRef<SWidget> UControlSettingsScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    if (auto* LocalUser = GetOwningLocalPlayer()) Source = LocalUser->GetSubsystem<UHorrorControlSettingsSubsystem>();
    auto* Background = WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(.015f,.02f,.025f,.97f)); Background->SetPadding(FMargin(32)); WidgetTree->RootWidget = Background;
    auto* Layout = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Layout);
    auto* Title=ControlsMenu::Text(WidgetTree, Layout, NSLOCTEXT("ControlsMenu", "Title", "Controles e câmera"));ControlsMenu::Font(Title,32);
    auto* HelpBox=WidgetTree->ConstructWidget<USizeBox>();HelpBox->SetHeightOverride(100);Layout->AddChildToVerticalBox(HelpBox);
    auto* Help=WidgetTree->ConstructWidget<UTextBlock>();ControlsMenu::Font(Help);Help->SetText(NSLOCTEXT("ControlsMenu", "Help", "Prévia imediata. Aplicar grava; Voltar descarta alterações não aplicadas.\nMenus: A confirma; B/Esc volta. Os analógicos continuam movendo e olhando."));Help->SetAutoWrapText(true);Help->SetWrapTextAt(IsEmbeddedInScreen() ? 900 : 1400);HelpBox->SetContent(Help); if (IsEmbeddedInScreen()) Help->SetText(NSLOCTEXT("ControlsMenu", "EmbeddedHelp", "Prévia imediata. Aplicar salva as alterações.\nTrocar de aba ou Voltar descarta alterações não aplicadas."));
    auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
    Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Panel);
    for (uint8 Index = 0; Index < 5; ++Index)
    {
        auto* Row = WidgetTree->ConstructWidget<UControlNumberRow>(); Row->Configure(this, Index); Panel->AddChildToVerticalBox(Row); Numbers.Add(Row);
    }
    const TCHAR* ToggleLabels[] = {TEXT("Mouse: inverter X"),TEXT("Mouse: inverter Y"),TEXT("Controle: inverter X"),TEXT("Controle: inverter Y")};
    for (uint8 Index = 0; Index < 4; ++Index)
    {
        auto* Toggle = WidgetTree->ConstructWidget<UCheckBox>(); auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); ControlsMenu::Font(Label); Label->SetText(FText::FromString(ToggleLabels[Index])); Toggle->SetContent(Label); auto* Height=WidgetTree->ConstructWidget<USizeBox>();Height->SetHeightOverride(44);Height->SetContent(Toggle);Panel->AddChildToVerticalBox(Height);Inversions.Add(Toggle);
    }
    Inversions[0]->OnCheckStateChanged.AddDynamic(this, &UControlSettingsScreen::MouseInvertX);
    Inversions[1]->OnCheckStateChanged.AddDynamic(this, &UControlSettingsScreen::MouseInvertY);
    Inversions[2]->OnCheckStateChanged.AddDynamic(this, &UControlSettingsScreen::GamepadInvertX);
    Inversions[3]->OnCheckStateChanged.AddDynamic(this, &UControlSettingsScreen::GamepadInvertY);
    if (Source.IsValid() && Source->IsReady()) for (const auto& Binding : Source->GetDefinition()->Bindings)
    {
        auto* Row = WidgetTree->ConstructWidget<UControlBindingRow>(); Row->Configure(this, Binding.Id); Panel->AddChildToVerticalBox(Row); Bindings.Add(Row);
    }
    auto* FeedbackBox=WidgetTree->ConstructWidget<USizeBox>();FeedbackBox->SetHeightOverride(IsEmbeddedInScreen() ? 72 : 110);Layout->AddChildToVerticalBox(FeedbackBox);
    Feedback=WidgetTree->ConstructWidget<UTextBlock>();ControlsMenu::Font(Feedback);Feedback->SetAutoWrapText(true);Feedback->SetWrapTextAt(IsEmbeddedInScreen() ? 900 : 1400);FeedbackBox->SetContent(Feedback);
    auto* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(); Layout->AddChildToVerticalBox(Footer);
    ApplyButton = ControlsMenu::Button(WidgetTree, NSLOCTEXT("ControlsMenu", "Apply", "Aplicar")); ApplyButton->OnClicked.AddDynamic(this, &UControlSettingsScreen::Apply); Footer->AddChildToHorizontalBox(ApplyButton);
    DefaultsButton = ControlsMenu::Button(WidgetTree, NSLOCTEXT("ControlsMenu", "Defaults", "Restaurar padrões")); DefaultsButton->OnClicked.AddDynamic(this, &UControlSettingsScreen::Defaults); Footer->AddChildToHorizontalBox(DefaultsButton);
    if (!IsEmbeddedInScreen())
    {
        auto* Close = ControlsMenu::Button(WidgetTree, NSLOCTEXT("ControlsMenu", "Back", "Voltar"));
        Close->OnClicked.AddDynamic(this, &UControlSettingsScreen::Back);
        Footer->AddChildToHorizontalBox(Close);
    }
    return Super::RebuildWidget();
}
void UControlSettingsScreen::OnScreenEnter()
{
    Super::OnScreenEnter();
    bEditing = Source.IsValid() && Source->BeginEdit(this);
    if (bEditing) { Source->OnPreferencesChanged.AddUniqueDynamic(this, &UControlSettingsScreen::Refresh); Feedback->SetText(Source->GetLoadError()); }
    else Feedback->SetText(NSLOCTEXT("ControlsMenu", "Unavailable", "As preferências não estão disponíveis ou já estão sendo editadas."));
    for (const auto& Row : Numbers) Row->SetIsEnabled(bEditing);
    for (const auto& Row : Bindings) Row->SetIsEnabled(bEditing);
    for (const auto& Toggle : Inversions) Toggle->SetIsEnabled(bEditing);
    ApplyButton->SetIsEnabled(bEditing); DefaultsButton->SetIsEnabled(bEditing); Refresh();
}
float UControlSettingsScreen::GetNumber(uint8 Field) const
{
    if (!Source.IsValid()) return Field == 0 ? 90.f : 1.f;
    const auto Value = Source->GetPreferences();
    switch (Field) { case 0:return Value.FieldOfView;case 1:return Value.Mouse.SensitivityX;case 2:return Value.Mouse.SensitivityY;case 3:return Value.Gamepad.SensitivityX;default:return Value.Gamepad.SensitivityY; }
}
FVector2D UControlSettingsScreen::GetLimits(uint8 Field) const
{
    const auto* Definition = Source.IsValid() ? Source->GetDefinition() : nullptr;
    return Definition ? (Field == 0 ? FVector2D(Definition->MinimumFOV,Definition->MaximumFOV) : FVector2D(Definition->MinimumSensitivity,Definition->MaximumSensitivity)) : (Field == 0 ? FVector2D(70,110) : FVector2D(.1,5));
}
void UControlSettingsScreen::SetNumber(uint8 Field, float NewValue)
{
    if (!bEditing || !Source.IsValid() || Field > 4 || !FMath::IsFinite(NewValue)) return;
    auto Value = Source->GetPreferences(); const auto Limits = GetLimits(Field); NewValue = FMath::Clamp(NewValue, float(Limits.X), float(Limits.Y));
    switch (Field) { case 0:Value.FieldOfView=NewValue;break;case 1:Value.Mouse.SensitivityX=NewValue;break;case 2:Value.Mouse.SensitivityY=NewValue;break;case 3:Value.Gamepad.SensitivityX=NewValue;break;default:Value.Gamepad.SensitivityY=NewValue;break; }
    FText Error; if (!Source->Preview(this,Value,Error)) Feedback->SetText(Error);
}
void UControlSettingsScreen::SetInversion(uint8 Field, bool NewValue)
{
    if (bRefreshing || !bEditing || !Source.IsValid()) return;
    auto Value = Source->GetPreferences();
    switch (Field) { case 0:Value.Mouse.bInvertX=NewValue;break;case 1:Value.Mouse.bInvertY=NewValue;break;case 2:Value.Gamepad.bInvertX=NewValue;break;default:Value.Gamepad.bInvertY=NewValue;break; }
    FText Error; if (!Source->Preview(this,Value,Error)) Feedback->SetText(Error);
}
void UControlSettingsScreen::MouseInvertX(bool Value) { SetInversion(0,Value); }
void UControlSettingsScreen::MouseInvertY(bool Value) { SetInversion(1,Value); }
void UControlSettingsScreen::GamepadInvertX(bool Value) { SetInversion(2,Value); }
void UControlSettingsScreen::GamepadInvertY(bool Value) { SetInversion(3,Value); }
void UControlSettingsScreen::Refresh()
{
    for (const auto& Row : Numbers) Row->Refresh(); for (const auto& Row : Bindings) Row->Refresh();
    if (!Source.IsValid() || Inversions.Num()!=4) return;
    const auto Value = Source->GetPreferences(); bRefreshing=true;
    Inversions[0]->SetIsChecked(Value.Mouse.bInvertX); Inversions[1]->SetIsChecked(Value.Mouse.bInvertY); Inversions[2]->SetIsChecked(Value.Gamepad.bInvertX); Inversions[3]->SetIsChecked(Value.Gamepad.bInvertY); bRefreshing=false;
}
void UControlSettingsScreen::BeginCapture(FName Id)
{
    if (!bEditing || !Source.IsValid() || !Source->GetDefinition()->FindBinding(Id)) return;
    CancelCapture(); PendingBinding=Id;
    Feedback->SetText(NSLOCTEXT("ControlsMenu", "Capture", "Pressione a nova tecla ou botão. Esc cancela; sem entrada, a captura termina em 10 s.\nDurante a captura, B pode ser atribuído ao gameplay."));
    SetUserFocus(GetOwningPlayer()); SetKeyboardFocus();
    CaptureDeadline=FPlatformTime::Seconds()+10.;
    CaptureTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){if(FPlatformTime::Seconds()<CaptureDeadline)return true;CaptureTicker.Reset();CancelCapture();return false;}));
}
bool UControlSettingsScreen::SubmitKey(FKey Key)
{
    if (!IsCapturing() || !Source.IsValid()) return false;
    FText Message;
    if (!Source->Remap(this,PendingBinding,Key,Message)) { Feedback->SetText(Message); return false; }
    CancelCapture();
    Feedback->SetText(Message.IsEmpty()?NSLOCTEXT("ControlsMenu", "PreviewKey", "Comando alterado na prévia. Clique em Aplicar para gravar."):Message);
    return true;
}
void UControlSettingsScreen::CancelCapture()
{
    if (CaptureTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(CaptureTicker);CaptureTicker.Reset(); }
    if (!PendingBinding.IsNone() && Feedback) Feedback->SetText(NSLOCTEXT("ControlsMenu", "Cancelled", "Captura encerrada."));
    const FName Previous=PendingBinding;PendingBinding=NAME_None;
    if(!Previous.IsNone())for(const auto& Row:Bindings)if(Row->GetBindingId()==Previous){Row->FocusButton();break;}
}
FReply UControlSettingsScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if (Event.GetKey()==ConsumedKey) return FReply::Handled();
    if (IsCapturing()) { ConsumedKey=Event.GetKey();if(Event.GetKey()==EKeys::Escape)CancelCapture();else SubmitKey(Event.GetKey());return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
FReply UControlSettingsScreen::NativeOnKeyUp(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if (ConsumedKey==Event.GetKey()) {ConsumedKey=FKey();return FReply::Handled();} return Super::NativeOnKeyUp(Geometry,Event);
}
FReply UControlSettingsScreen::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(IsCapturing()){ConsumedKey=Event.GetEffectingButton();SubmitKey(ConsumedKey);return FReply::Handled();}return Super::NativeOnPreviewMouseButtonDown(Geometry,Event);
}
FReply UControlSettingsScreen::NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(ConsumedKey==Event.GetEffectingButton()){ConsumedKey=FKey();return FReply::Handled();}return Super::NativeOnMouseButtonUp(Geometry,Event);
}
FReply UControlSettingsScreen::NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(IsCapturing()){SubmitKey(Event.GetWheelDelta()>0?EKeys::MouseScrollUp:EKeys::MouseScrollDown);return FReply::Handled();}return Super::NativeOnMouseWheel(Geometry,Event);
}
void UControlSettingsScreen::Apply() { if(bEditing && Source.IsValid()){FText Error;Feedback->SetText(Source->Apply(this,Error)?NSLOCTEXT("ControlsMenu","Saved","Preferências aplicadas."):Error);} }
void UControlSettingsScreen::Defaults() { if(bEditing && Source.IsValid()){CancelCapture();Source->RestoreDefaults(this);Feedback->SetText(NSLOCTEXT("ControlsMenu","DefaultsPreview","Padrões na prévia. Aplicar grava; Voltar restaura os últimos valores aplicados."));} }
void UControlSettingsScreen::Back() { CloseScreen(); }
void UControlSettingsScreen::EndEditing()
{
    CancelCapture();ConsumedKey=FKey();
    if(Source.IsValid()){Source->OnPreferencesChanged.RemoveDynamic(this,&UControlSettingsScreen::Refresh);if(bEditing)Source->Cancel(this);}bEditing=false;Source.Reset();
}
void UControlSettingsScreen::OnScreenExit() { EndEditing();Super::OnScreenExit(); }
void UControlSettingsScreen::NativeDestruct() { EndEditing();Super::NativeDestruct(); }

TSharedRef<SWidget> UFrameworkSettingsHub::RebuildWidget()
{
    if(WidgetTree->RootWidget)return Super::RebuildWidget();
    auto* Background=WidgetTree->ConstructWidget<UBorder>();Background->SetBrushColor(FLinearColor(.015f,.02f,.025f,.97f));Background->SetPadding(FMargin(50));WidgetTree->RootWidget=Background;
    auto* Panel=WidgetTree->ConstructWidget<UVerticalBox>();Background->SetContent(Panel);
    ControlsMenu::Text(WidgetTree,Panel,NSLOCTEXT("ControlsMenu","Hub","Configurações"));
    auto* ControlButton=ControlsMenu::Button(WidgetTree,NSLOCTEXT("ControlsMenu","Controls","Controles e câmera"));ControlButton->OnClicked.AddDynamic(this,&UFrameworkSettingsHub::Controls);Panel->AddChildToVerticalBox(ControlButton);
    auto* AudioButton=ControlsMenu::Button(WidgetTree,NSLOCTEXT("ControlsMenu","Audio","Áudio"));AudioButton->OnClicked.AddDynamic(this,&UFrameworkSettingsHub::Audio);Panel->AddChildToVerticalBox(AudioButton);
    if(AdvancedSettingsClass){auto* OtherButton=ControlsMenu::Button(WidgetTree,NSLOCTEXT("ControlsMenu","Other","Outras configurações"));OtherButton->OnClicked.AddDynamic(this,&UFrameworkSettingsHub::Advanced);Panel->AddChildToVerticalBox(OtherButton);}
    auto* Close=ControlsMenu::Button(WidgetTree,NSLOCTEXT("ControlsMenu","Back","Voltar"));Close->OnClicked.AddDynamic(this,&UFrameworkSettingsHub::Back);Panel->AddChildToVerticalBox(Close);
    return Super::RebuildWidget();
}
void UFrameworkSettingsHub::Controls(){GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->OpenScreen(EHorrorScreen::ControlSettings);}
void UFrameworkSettingsHub::Audio(){GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->OpenAudioSettingsUI();}
void UFrameworkSettingsHub::Advanced(){if(AdvancedSettingsClass)GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()->PushScreen(AdvancedSettingsClass);}
void UFrameworkSettingsHub::Back(){CloseScreen();}
