#include "Controls/MainSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "UIManagerSubsystem.h"

void UMainSettingsScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UWidget* Video = GetWidgetFromName(TEXT("ScrollBox_VIDEO"));
    auto* Canvas = Video ? Cast<UCanvasPanel>(Video->GetParent()) : nullptr;
    auto* VideoSlot = Video ? Cast<UCanvasPanelSlot>(Video->Slot) : nullptr;
    if (!ensureMsgf(Canvas && VideoSlot, TEXT("Settings template requires its video canvas slot."))) return;

    PageHost = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FrameworkSettingsPage"));
    if (auto* VideoPanel = Cast<UBorder>(GetWidgetFromName(TEXT("Border"))))
    {
        PageHost->SetBrush(VideoPanel->Background);
        PageHost->SetBrushColor(VideoPanel->GetBrushColor());
    }
    PageHost->SetPadding(FMargin(0));
    PageHost->SetClipping(EWidgetClipping::ClipToBounds);
    UCanvasPanelSlot* HostSlot = Canvas->AddChildToCanvas(PageHost);
    HostSlot->SetLayout(VideoSlot->GetLayout());
    HostSlot->SetZOrder(VideoSlot->GetZOrder() + 1);
    PageHost->SetVisibility(ESlateVisibility::Collapsed);

    if (auto* VideoButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_Video"))))
    {
        // The native tab controller now owns visibility and selected-button state.
        VideoButton->OnClicked.Clear();
        VideoButton->OnClicked.AddUniqueDynamic(this, &UMainSettingsScreen::OpenVideo);
    }
    if (auto* Audio = Cast<UButton>(GetWidgetFromName(TEXT("Button_Audio"))))
        Audio->OnClicked.AddUniqueDynamic(this, &UMainSettingsScreen::OpenAudio);
    if (auto* Controls = Cast<UButton>(GetWidgetFromName(TEXT("Button_Controls"))))
        Controls->OnClicked.AddUniqueDynamic(this, &UMainSettingsScreen::OpenControls);
    if (auto* Back = Cast<UButton>(GetWidgetFromName(TEXT("Button_Back"))))
        Back->OnClicked.AddUniqueDynamic(this, &UMainSettingsScreen::CloseScreen);

    UpdateTabs(Cast<UButton>(GetWidgetFromName(TEXT("Button_Video"))));
}

void UMainSettingsScreen::OpenVideo()
{
    if (!Manager || Manager->GetTopScreen() != this) return;
    ClearPage();
    UpdateTabs(Cast<UButton>(GetWidgetFromName(TEXT("Button_Video"))));
    FocusPage();
}

void UMainSettingsScreen::OpenAudio()
{
    if (Manager && Manager->UIConfigAsset)
        OpenPage(Manager->UIConfigAsset->AudioSettings, Cast<UButton>(GetWidgetFromName(TEXT("Button_Audio"))));
}

void UMainSettingsScreen::OpenControls()
{
    if (Manager && Manager->UIConfigAsset)
        OpenPage(Manager->UIConfigAsset->ControlSettings, Cast<UButton>(GetWidgetFromName(TEXT("Button_Controls"))));
}

void UMainSettingsScreen::OpenPage(TSubclassOf<UUIScreenBase> PageClass, UButton* SelectedTab)
{
    if (!Manager || Manager->GetTopScreen() != this || !PageHost || !PageClass) return;
    if (ActivePage && ActivePage->GetClass() == PageClass) return;

    // Create before closing the current page so a failed allocation preserves the current tab.
    UUIScreenBase* Page = CreateWidget<UUIScreenBase>(GetOwningPlayer(), PageClass);
    if (!Page) return;
    ClearPage();
    ActivePage = Page;
    Page->SetIsFocusable(true);
    Page->SetOwningManager(Manager);
    Page->SetPresentationHost(this);
    PageHost->SetContent(Page);
    Page->TakeWidget();
    ApplyTemplateStyle(Page);
    UpdateTabs(SelectedTab);
    Page->OnScreenEnter();
    Page->ReceiveScreenEnter();
    FocusPage();
}

void UMainSettingsScreen::ClearPage()
{
    if (!ActivePage) return;
    ActivePage->OnScreenExit();
    ActivePage->ReceiveScreenExit();
    ActivePage->SetPresentationHost(nullptr);
    ActivePage->SetOwningManager(nullptr);
    PageHost->ClearChildren();
    ActivePage = nullptr;
    PageHost->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainSettingsScreen::UpdateTabs(UButton* SelectedTab)
{
    for (const FName Name : {FName(TEXT("Button_Video")), FName(TEXT("Button_Audio")), FName(TEXT("Button_Controls"))})
    {
        if (auto* Button = Cast<UButton>(GetWidgetFromName(Name)))
            Button->SetIsEnabled(Button != SelectedTab);
    }
    const ESlateVisibility VideoVisibility = ActivePage ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
    if (UWidget* Video = GetWidgetFromName(TEXT("ScrollBox_VIDEO"))) Video->SetVisibility(VideoVisibility);
    if (UWidget* Optimize = GetWidgetFromName(TEXT("Button_Optimize"))) Optimize->SetVisibility(VideoVisibility);
    if (PageHost) PageHost->SetVisibility(ActivePage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainSettingsScreen::ApplyTemplateStyle(UUIScreenBase* Page)
{
    auto* ReferenceButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_Back")));
    auto* ReferenceText = ReferenceButton ? Cast<UTextBlock>(ReferenceButton->GetContent()) : nullptr;
    auto* ReferenceSlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_SFX_Volume")));
    auto* ReferenceScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_VIDEO")));
    if (!ensure(ReferenceButton && ReferenceText && Page->WidgetTree)) return;

    const FButtonStyle ButtonStyle = ReferenceButton->GetStyle();
    const FSlateFontInfo Font = ReferenceText->GetFont();
    Page->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Widget)
    {
        if (auto* Button = Cast<UButton>(Widget))
        {
            Button->SetStyle(ButtonStyle);
            Button->SetColorAndOpacity(FLinearColor::White);
            Button->SetBackgroundColor(FLinearColor::White);
        }
        else if (auto* Text = Cast<UTextBlock>(Widget))
        {
            Text->SetFont(Font);
            Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        }
        else if (auto* Slider = Cast<USlider>(Widget); Slider && ReferenceSlider)
        {
            Slider->SetWidgetStyle(ReferenceSlider->GetWidgetStyle());
            Slider->SetSliderBarColor(FLinearColor::White);
            Slider->SetSliderHandleColor(FLinearColor::White);
        }
        else if (auto* Scroll = Cast<UScrollBox>(Widget); Scroll && ReferenceScroll)
        {
            Scroll->SetWidgetStyle(ReferenceScroll->GetWidgetStyle());
            Scroll->SetWidgetBarStyle(ReferenceScroll->GetWidgetBarStyle());
        }
    });

    if (auto* Background = Cast<UBorder>(Page->WidgetTree->RootWidget))
    {
        Background->SetBrushColor(FLinearColor::Transparent);
        Background->SetPadding(FMargin(8));
        if (auto* Layout = Cast<UVerticalBox>(Background->GetContent()))
        {
            if (auto* Title = Cast<UTextBlock>(Layout->GetChildAt(0)))
            {
                FSlateFontInfo TitleFont = Font;
                TitleFont.Size = 50;
                Title->SetFont(TitleFont);
                Title->SetJustification(ETextJustify::Center);
            }
        }
    }
}

void UMainSettingsScreen::FocusPage()
{
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller) return;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(ActivePage ? ActivePage->GetNavigationFocusTarget() : GetNavigationFocusTarget());
    Controller->SetInputMode(Mode);
}

void UMainSettingsScreen::CloseScreen()
{
    ClearPage();
    Super::CloseScreen();
}

void UMainSettingsScreen::OnScreenExit()
{
    ClearPage();
    Super::OnScreenExit();
}

void UMainSettingsScreen::NativeDestruct()
{
    ClearPage();
    Super::NativeDestruct();
}
