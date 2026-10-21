#pragma once
#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "SaveMenuScreens.generated.h"
class UButton;
class UTextBlock;
class UFrameworkSaveSlotsScreen;
// Dynamic button delegates carry their slot through a small presentation object.
UCLASS()
class HORRORGAMEFRAMEWORK_API USaveSlotButtonAction : public UObject
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UFrameworkSaveSlotsScreen> Screen;
    int32 SlotIndex = INDEX_NONE;
    UFUNCTION() void Activate();
};
UCLASS()
class HORRORGAMEFRAMEWORK_API UFrameworkSaveSlotsScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    virtual void OnScreenEnter() override;
    virtual void OnScreenExit() override;
    void SetSaveMode();
    void SelectSlot(int32 Index);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UFUNCTION() void RefreshRows();
    UFUNCTION() void HandleSaved(int32 SlotIndex, bool bSuccess, FText Message);
    UFUNCTION() void HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message);
    UFUNCTION() void ConfirmOverwrite();
    UFUNCTION() void Back();
    void BeginOperation(int32 SlotIndex);
    void SetBusy(bool bValue);
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> Buttons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
    UPROPERTY(Transient) TArray<TObjectPtr<USaveSlotButtonAction>> Actions;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
    UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
    int32 OverwriteSlot = INDEX_NONE;
    bool bSaveMode = false;
    bool bBusy = false;
};
UCLASS()
class HORRORGAMEFRAMEWORK_API UFrameworkMainMenuScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Game") TSoftObjectPtr<UWorld> NewGameMap;
    virtual void OnScreenEnter() override;
    virtual void OnScreenExit() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void RefreshContinue();
    UFUNCTION() void ContinueGame();
    UFUNCTION() void NewGame();
    UFUNCTION() void LoadGame();
    UFUNCTION() void Settings();
    UFUNCTION() void Quit();
    UFUNCTION() void HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message);
    UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> MenuButtons;
    bool bLoading = false;
};
