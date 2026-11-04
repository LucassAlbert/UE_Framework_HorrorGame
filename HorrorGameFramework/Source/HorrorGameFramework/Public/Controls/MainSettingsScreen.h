#pragma once

#include "UIScreenBase.h"
#include "MainSettingsScreen.generated.h"

class UBorder;
class UButton;

UCLASS()
class HORRORGAMEFRAMEWORK_API UMainSettingsScreen : public UUIScreenBase
{
    GENERATED_BODY()

public:
    virtual void CloseScreen() override;
    virtual void OnScreenExit() override;
    UUIScreenBase* GetActivePage() const { return ActivePage; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

private:
    UFUNCTION() void OpenVideo();
    UFUNCTION() void OpenAudio();
    UFUNCTION() void OpenControls();
    void OpenPage(TSubclassOf<UUIScreenBase> PageClass, UButton* SelectedTab);
    void ClearPage();
    void UpdateTabs(UButton* SelectedTab);
    void ApplyTemplateStyle(UUIScreenBase* Page);
    void FocusPage();

    UPROPERTY(Transient) TObjectPtr<UBorder> PageHost;
    UPROPERTY(Transient) TObjectPtr<UUIScreenBase> ActivePage;
};
