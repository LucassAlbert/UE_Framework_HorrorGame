#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIScreenBase.generated.h"

class UUIManagerSubsystem;

UCLASS()
class HORRORUI_API UUIScreenBase : public UUserWidget
{
    GENERATED_BODY()
public:
    // Menus keep their existing defaults; live conversations opt into gameplay input routing.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Input") bool bPauseWorld = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Input") bool bUseGameplayInput = false;
    virtual bool HandlePrimaryAction() { return false; }
    virtual bool HandleBackAction() { return false; }
    virtual void OnScreenEnter() {}
    virtual void OnScreenExit() {}

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ReceiveScreenEnter();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ReceiveScreenExit();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CloseScreen();

    void SetOwningManager(UUIManagerSubsystem* InManager) { Manager = InManager; }

    // Called before hiding a screen and when restoring its input mode.
    void RememberNavigationFocus();
    TSharedRef<SWidget> GetNavigationFocusTarget();

protected:
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
    TObjectPtr<UUIManagerSubsystem> Manager;

private:
    TWeakPtr<SWidget> LastNavigationFocus;
};
