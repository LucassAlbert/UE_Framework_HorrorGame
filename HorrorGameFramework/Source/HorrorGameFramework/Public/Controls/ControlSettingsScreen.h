#pragma once
#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "Containers/Ticker.h"
#include "ControlSettingsScreen.generated.h"
class UButton;
class USlider;
class UTextBlock;
class UCheckBox;
class UHorrorControlSettingsSubsystem;
class UControlSettingsScreen;

UCLASS()
class HORRORGAMEFRAMEWORK_API UControlNumberRow : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UControlSettingsScreen* Screen, uint8 InField);
    void Refresh();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void Decrease();
    UFUNCTION() void Increase();
    UFUNCTION() void Change(float Value);
    TWeakObjectPtr<UControlSettingsScreen> OwnerScreen;
    uint8 Field = 0;
    bool bRefreshing = false;
    UPROPERTY(Transient) TObjectPtr<USlider> Slider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueLabel;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UControlBindingRow : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UControlSettingsScreen* Screen, FName InId);
    void Refresh();
    void FocusButton();
    FName GetBindingId() const { return Id; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void Capture();
    TWeakObjectPtr<UControlSettingsScreen> OwnerScreen;
    FName Id;
    UPROPERTY(Transient) TObjectPtr<UButton> Button;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> KeyLabel;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UControlSettingsScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    virtual void OnScreenEnter() override;
    virtual void OnScreenExit() override;
    float GetNumber(uint8 Field) const;
    FVector2D GetLimits(uint8 Field) const;
    void SetNumber(uint8 Field, float Value);
    UHorrorControlSettingsSubsystem* GetSource() const { return Source.Get(); }
    void BeginCapture(FName Id);
    bool SubmitKey(FKey Key);
    bool IsCapturing() const { return !PendingBinding.IsNone(); }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Refresh();
    UFUNCTION() void Apply();
    UFUNCTION() void Defaults();
    UFUNCTION() void Back();
    UFUNCTION() void MouseInvertX(bool Value);
    UFUNCTION() void MouseInvertY(bool Value);
    UFUNCTION() void GamepadInvertX(bool Value);
    UFUNCTION() void GamepadInvertY(bool Value);
    void SetInversion(uint8 Field, bool Value);
    void CancelCapture();
    void EndEditing();
    UPROPERTY(Transient) TArray<TObjectPtr<UControlNumberRow>> Numbers;
    UPROPERTY(Transient) TArray<TObjectPtr<UControlBindingRow>> Bindings;
    UPROPERTY(Transient) TArray<TObjectPtr<UCheckBox>> Inversions;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UButton> ApplyButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DefaultsButton;
    TWeakObjectPtr<UHorrorControlSettingsSubsystem> Source;
    FName PendingBinding;
    FKey ConsumedKey;
    FTSTicker::FDelegateHandle CaptureTicker;
    double CaptureDeadline = 0;
    bool bEditing = false;
    bool bRefreshing = false;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UFrameworkSettingsHub : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Settings") TSubclassOf<UUIScreenBase> AdvancedSettingsClass;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void Controls();
    UFUNCTION() void Audio();
    UFUNCTION() void Advanced();
    UFUNCTION() void Back();
};
