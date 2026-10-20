#include "Inspection/InspectionScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"

UInspectionScreen::UInspectionScreen(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UInspectionScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.96f));
    Background->SetPadding(FMargin(24.f));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Layout);
    WidgetTree->RootWidget = Background;
    ItemName = WidgetTree->ConstructWidget<UTextBlock>();
    Layout->AddChildToVerticalBox(ItemName);
    ItemDescription = WidgetTree->ConstructWidget<UTextBlock>();
    ItemDescription->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(ItemDescription);
    PreviewArea = WidgetTree->ConstructWidget<USizeBox>();
    PreviewArea->SetWidthOverride(460.f);
    PreviewArea->SetHeightOverride(460.f);
    Preview = WidgetTree->ConstructWidget<UImage>();
    Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
    PreviewArea->SetContent(Preview);
    Layout->AddChildToVerticalBox(PreviewArea)->SetHorizontalAlignment(HAlign_Center);
    Status = WidgetTree->ConstructWidget<UTextBlock>();
    Layout->AddChildToVerticalBox(Status);
    UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
    Hint->SetText(NSLOCTEXT("Inspection", "Controls", "Arraste com o botão esquerdo para girar • Roda: zoom • Controle: analógico direito e gatilhos • Esc/B: voltar"));
    Hint->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(Hint);
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    Layout->AddChildToVerticalBox(Actions);
    UButton* Reset = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* ResetLabel = WidgetTree->ConstructWidget<UTextBlock>();
    ResetLabel->SetText(NSLOCTEXT("Inspection", "Reset", "Restaurar posição (R / Y)"));
    Reset->SetContent(ResetLabel);
    Reset->OnClicked.AddDynamic(this, &UInspectionScreen::ResetView);
    Actions->AddChildToHorizontalBox(Reset);
    UButton* Close = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CloseLabel->SetText(NSLOCTEXT("Inspection", "Back", "Voltar ao inventário"));
    Close->SetContent(CloseLabel);
    Close->OnClicked.AddDynamic(this, &UInspectionScreen::Back);
    Actions->AddChildToHorizontalBox(Close);
    return Super::RebuildWidget();
}

void UInspectionScreen::SetSource(UInspectionComponent* InSource, const FText& Name, const FText& Description)
{
    ReleaseSource();
    Source = InSource;
    if (ItemName) ItemName->SetText(Name);
    if (ItemDescription) ItemDescription->SetText(Description);
    if (InSource)
    {
        InSource->OnStateChanged.AddUniqueDynamic(this, &UInspectionScreen::HandleState);
        HandleState(InSource->GetState(), FText::GetEmpty());
    }
}

void UInspectionScreen::HandleState(EInspectionState State, FText Message)
{
    if (Status) Status->SetText(Message);
    if (!Preview) return;
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetImageSize(FVector2D(768.f));
    Brush.SetResourceObject(State == EInspectionState::Ready && Source.IsValid() ? Source->GetRenderTarget() : nullptr);
    Preview->SetBrush(Brush);
}

void UInspectionScreen::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if (!Source.IsValid() || Source->GetState() != EInspectionState::Ready) return;
    // Slate ticks while gameplay is paused. Rates use elapsed UI time, not frame counts.
    const float Step = FMath::Max(DeltaSeconds, 0.f);
    Source->AdjustView(AnalogRotation * (100.f * Step), (ZoomIn - ZoomOut) * (4.f * Step));
    Source->CaptureIfDirty();
}

FReply UInspectionScreen::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::LeftMouseButton && PreviewArea
        && PreviewArea->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
    {
        bDragging = true;
        return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UInspectionScreen::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(Geometry, Event);
}

FReply UInspectionScreen::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (bDragging && Source.IsValid())
    {
        const FVector2D Delta = Event.GetCursorDelta() / Geometry.Scale;
        Source->AdjustView(FVector2D(-Delta.X, Delta.Y) * 0.4f, 0.f);
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(Geometry, Event);
}

FReply UInspectionScreen::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Source.IsValid())
    {
        Source->AdjustView(FVector2D::ZeroVector, Event.GetWheelDelta());
        return FReply::Handled();
    }
    return Super::NativeOnMouseWheel(Geometry, Event);
}

FReply UInspectionScreen::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    const FKey Key = Event.GetKey();
    const float Value = FMath::Abs(Event.GetAnalogValue()) < 0.15f ? 0.f : Event.GetAnalogValue();
    if (Key == EKeys::Gamepad_RightX) AnalogRotation.X = -Value;
    else if (Key == EKeys::Gamepad_RightY) AnalogRotation.Y = Value;
    else if (Key == EKeys::Gamepad_RightTriggerAxis) ZoomIn = FMath::Max(0.f, Value);
    else if (Key == EKeys::Gamepad_LeftTriggerAxis) ZoomOut = FMath::Max(0.f, Value);
    else return Super::NativeOnAnalogValueChanged(Geometry, Event);
    return FReply::Handled();
}

FReply UInspectionScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::R || Event.GetKey() == EKeys::Gamepad_FaceButton_Top)
    {
        ResetView();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}

void UInspectionScreen::ResetInput()
{
    bDragging = false;
    AnalogRotation = FVector2D::ZeroVector;
    ZoomIn = ZoomOut = 0.f;
}
void UInspectionScreen::NativeOnFocusLost(const FFocusEvent& Event) { ResetInput(); Super::NativeOnFocusLost(Event); }
void UInspectionScreen::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) { ResetInput(); Super::NativeOnMouseCaptureLost(Event); }
void UInspectionScreen::ResetView() { if (Source.IsValid()) Source->ResetView(); ResetInput(); SetKeyboardFocus(); }
void UInspectionScreen::Back() { CloseScreen(); }
void UInspectionScreen::ReleaseSource()
{
    ResetInput();
    if (Preview) Preview->SetBrush(FSlateBrush());
    if (Source.IsValid())
    {
        Source->OnStateChanged.RemoveDynamic(this, &UInspectionScreen::HandleState);
        Source->CloseInspection();
    }
    Source.Reset();
}
void UInspectionScreen::OnScreenExit() { ReleaseSource(); Super::OnScreenExit(); }
void UInspectionScreen::NativeDestruct() { ReleaseSource(); Super::NativeDestruct(); }
