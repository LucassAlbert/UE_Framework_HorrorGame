#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "InspectionComponent.h"
#include "InspectionScreen.generated.h"

class UImage;
class UTextBlock;
class USizeBox;

UCLASS()
class HORRORGAMEFRAMEWORK_API UInspectionScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UInspectionScreen(const FObjectInitializer& Initializer);
    void SetSource(UInspectionComponent* InSource, const FText& Name, const FText& Description);
    virtual void OnScreenExit() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
    virtual void NativeOnFocusLost(const FFocusEvent& Event) override;
private:
    UFUNCTION() void HandleState(EInspectionState State, FText Message);
    UFUNCTION() void ResetView();
    UFUNCTION() void Back();
    void ReleaseSource();
    void ResetInput();
    TWeakObjectPtr<UInspectionComponent> Source;
    FVector2D AnalogRotation = FVector2D::ZeroVector;
    float ZoomIn = 0.f;
    float ZoomOut = 0.f;
    bool bDragging = false;
    UPROPERTY(Transient) TObjectPtr<UImage> Preview;
    UPROPERTY(Transient) TObjectPtr<USizeBox> PreviewArea;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemDescription;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
};
