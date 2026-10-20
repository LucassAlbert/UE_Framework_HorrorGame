#include "Audio/AudioSettingsScreen.h"
#include "HorrorAudioSubsystem.h"
#include "HorrorSoundDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

namespace AudioMenu
{
    UButton* AddButton(UWidgetTree* Tree, const FText& Label)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(Label);
        Button->SetContent(Text);
        return Button;
    }
    FText ChannelLabel(EHorrorAudioChannel Channel)
    {
        switch (Channel)
        {
            case EHorrorAudioChannel::Master: return NSLOCTEXT("AudioMenu", "Master", "Geral");
            case EHorrorAudioChannel::Ambience: return NSLOCTEXT("AudioMenu", "Ambience", "Ambiente");
            case EHorrorAudioChannel::Effects: return NSLOCTEXT("AudioMenu", "Effects", "Efeitos");
            case EHorrorAudioChannel::Interface: return NSLOCTEXT("AudioMenu", "Interface", "Interface");
            case EHorrorAudioChannel::Music: return NSLOCTEXT("AudioMenu", "Music", "Música");
        }
        return FText::GetEmpty();
    }
}
void UAudioVolumeRow::Configure(UAudioSettingsScreen* Screen, EHorrorAudioChannel InChannel)
{
    OwnerScreen = Screen;
    Channel = InChannel;
    Refresh();
}
TSharedRef<SWidget> UAudioVolumeRow::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UHorizontalBox* Layout = WidgetTree->ConstructWidget<UHorizontalBox>();
    WidgetTree->RootWidget = Layout;
    USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
    LabelBox->SetWidthOverride(160.f);
    LabelText = WidgetTree->ConstructWidget<UTextBlock>();
    LabelBox->SetContent(LabelText);
    Layout->AddChildToHorizontalBox(LabelBox);
    UButton* Minus = AudioMenu::AddButton(WidgetTree, FText::FromString(TEXT("  -  ")));
    Minus->OnClicked.AddDynamic(this, &UAudioVolumeRow::Decrease);
    Layout->AddChildToHorizontalBox(Minus);
    USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>();
    SliderBox->SetWidthOverride(280.f);
    Slider = WidgetTree->ConstructWidget<USlider>();
    Slider->SetStepSize(.05f);
    Slider->OnValueChanged.AddDynamic(this, &UAudioVolumeRow::Change);
    SliderBox->SetContent(Slider);
    Layout->AddChildToHorizontalBox(SliderBox);
    UButton* Plus = AudioMenu::AddButton(WidgetTree, FText::FromString(TEXT("  +  ")));
    Plus->OnClicked.AddDynamic(this, &UAudioVolumeRow::Increase);
    Layout->AddChildToHorizontalBox(Plus);
    USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
    ValueBox->SetWidthOverride(80.f);
    ValueText = WidgetTree->ConstructWidget<UTextBlock>();
    ValueBox->SetContent(ValueText);
    Layout->AddChildToHorizontalBox(ValueBox);
    UButton* Test = AudioMenu::AddButton(WidgetTree, NSLOCTEXT("AudioMenu", "Preview", "Ouvir teste"));
    Test->OnClicked.AddDynamic(this, &UAudioVolumeRow::Preview);
    Layout->AddChildToHorizontalBox(Test);
    Refresh();
    return Super::RebuildWidget();
}
void UAudioVolumeRow::Refresh()
{
    bRefreshing = true;
    const float Volume = OwnerScreen.IsValid() ? OwnerScreen->GetVolume(Channel) : 1.f;
    if (Slider) Slider->SetValue(Volume);
    FNumberFormattingOptions PercentFormat;
    PercentFormat.MaximumFractionalDigits = 0;
    if (ValueText) ValueText->SetText(FText::AsPercent(Volume, &PercentFormat));
    if (LabelText) LabelText->SetText(AudioMenu::ChannelLabel(Channel));
    bRefreshing = false;
}
void UAudioVolumeRow::Decrease() { if (OwnerScreen.IsValid()) Change(OwnerScreen->GetVolume(Channel) - .05f); }
void UAudioVolumeRow::Increase() { if (OwnerScreen.IsValid()) Change(OwnerScreen->GetVolume(Channel) + .05f); }
void UAudioVolumeRow::Change(float Value)
{
    if (!bRefreshing && OwnerScreen.IsValid()) OwnerScreen->SetVolume(Channel, FMath::Clamp(Value, 0.f, 1.f));
}
void UAudioVolumeRow::Preview() { if (OwnerScreen.IsValid()) OwnerScreen->PreviewChannel(Channel); }

UAudioSettingsScreen::UAudioSettingsScreen(const FObjectInitializer& Initializer) : Super(Initializer) { SetIsFocusable(true); }
TSharedRef<SWidget> UAudioSettingsScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.015f, .015f, .015f, .96f));
    Background->SetPadding(FMargin(40.f));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Layout);
    WidgetTree->RootWidget = Background;
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("AudioMenu", "Title", "Áudio"));
    Layout->AddChildToVerticalBox(Title);
    UTextBlock* Help = WidgetTree->ConstructWidget<UTextBlock>();
    Help->SetText(NSLOCTEXT("AudioMenu", "Help", "Prévia imediata. Aplicar grava os volumes. Voltar cancela alterações não aplicadas."));
    Help->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(Help);
    for (uint8 Index = 0; Index <= static_cast<uint8>(EHorrorAudioChannel::Music); ++Index)
    {
        UAudioVolumeRow* Row = WidgetTree->ConstructWidget<UAudioVolumeRow>();
        Row->Configure(this, static_cast<EHorrorAudioChannel>(Index));
        USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>();
        RowBox->SetHeightOverride(65.f);
        RowBox->SetContent(Row);
        Layout->AddChildToVerticalBox(RowBox);
        Rows.Add(Row);
    }
    UButton* ApplyButton = AudioMenu::AddButton(WidgetTree, NSLOCTEXT("AudioMenu", "Apply", "Aplicar"));
    ApplyButton->OnClicked.AddDynamic(this, &UAudioSettingsScreen::Apply);
    Layout->AddChildToVerticalBox(ApplyButton);
    UButton* Stop = AudioMenu::AddButton(WidgetTree, NSLOCTEXT("AudioMenu", "Stop", "Parar teste"));
    Stop->OnClicked.AddDynamic(this, &UAudioSettingsScreen::StopPreview);
    Layout->AddChildToVerticalBox(Stop);
    UButton* Close = AudioMenu::AddButton(WidgetTree, NSLOCTEXT("AudioMenu", "Back", "Voltar"));
    Close->OnClicked.AddDynamic(this, &UAudioSettingsScreen::Back);
    Layout->AddChildToVerticalBox(Close);
    Feedback = WidgetTree->ConstructWidget<UTextBlock>();
    Feedback->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(Feedback);
    return Super::RebuildWidget();
}
void UAudioSettingsScreen::OnScreenEnter()
{
    Super::OnScreenEnter();
    UHorrorAudioSubsystem* Source = GetGameInstance()->GetSubsystem<UHorrorAudioSubsystem>();
    if (!Source || !Source->BeginVolumeEdit(this))
    {
        if (Feedback) Feedback->SetText(NSLOCTEXT("AudioMenu", "Busy", "O áudio está sendo editado por outra tela."));
        return;
    }
    Audio = Source;
    bEditing = true;
    Source->OnVolumesChanged.AddUniqueDynamic(this, &UAudioSettingsScreen::RefreshRows);
    Source->OnPlaybackFailed.AddUniqueDynamic(this, &UAudioSettingsScreen::HandlePreviewFailure);
    RefreshRows();
}
float UAudioSettingsScreen::GetVolume(EHorrorAudioChannel Channel) const { return Audio.IsValid() ? Audio->GetVolumes().Get(Channel) : 1.f; }
void UAudioSettingsScreen::SetVolume(EHorrorAudioChannel Channel, float Value)
{
    if (!bEditing || !Audio.IsValid()) return;
    FHorrorAudioVolumes Volumes = Audio->GetVolumes();
    if (Volumes.Set(Channel, Value) && Audio->PreviewVolumes(this, Volumes) && Feedback)
        Feedback->SetText(NSLOCTEXT("AudioMenu", "Unapplied", "Prévia: clique em Aplicar para salvar."));
}
void UAudioSettingsScreen::RefreshRows() { for (UAudioVolumeRow* Row : Rows) if (Row) Row->Refresh(); }
void UAudioSettingsScreen::Apply()
{
    if (!bEditing || !Audio.IsValid()) return;
    FText Error;
    const bool bSuccess = Audio->ApplyVolumes(this, Error);
    if (Feedback) Feedback->SetText(bSuccess ? NSLOCTEXT("AudioMenu", "Saved", "Volumes salvos.") : Error);
}
void UAudioSettingsScreen::PreviewChannel(EHorrorAudioChannel Channel)
{
    if (!bEditing || !Audio.IsValid()) return;
    StopPreview();
    UHorrorSoundDefinition* Definition = PreviewSounds.FindRef(Channel);
    if (!Definition)
    {
        if (Feedback) Feedback->SetText(NSLOCTEXT("AudioMenu", "NoPreview", "Nenhum som de teste configurado para este grupo."));
        return;
    }
    PreviewHandle = Audio->PlaySound(this, Definition, FVector::ZeroVector);
    if (!PreviewHandle.IsValid() && Feedback)
        Feedback->SetText(NSLOCTEXT("AudioMenu", "PreviewRejected", "Não foi possível iniciar o som de teste."));
}
void UAudioSettingsScreen::StopPreview()
{
    if (Audio.IsValid()) Audio->StopSound(PreviewHandle, false);
    PreviewHandle.Invalidate();
}
void UAudioSettingsScreen::HandlePreviewFailure(FGuid Handle, FText Error) { if (Handle == PreviewHandle && Feedback) Feedback->SetText(Error); }
void UAudioSettingsScreen::Back() { CloseScreen(); }
void UAudioSettingsScreen::EndEditing()
{
    StopPreview();
    if (Audio.IsValid())
    {
        Audio->OnVolumesChanged.RemoveDynamic(this, &UAudioSettingsScreen::RefreshRows);
        Audio->OnPlaybackFailed.RemoveDynamic(this, &UAudioSettingsScreen::HandlePreviewFailure);
        if (bEditing) Audio->CancelVolumeEdit(this);
    }
    Audio.Reset();
    bEditing = false;
}
void UAudioSettingsScreen::OnScreenExit() { EndEditing(); Super::OnScreenExit(); }
void UAudioSettingsScreen::NativeDestruct() { EndEditing(); Super::NativeDestruct(); }
