#pragma once
#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "ThreatGameOverScreen.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class HORRORGAMEFRAMEWORK_API UThreatGameOverScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UThreatGameOverScreen(const FObjectInitializer& Initializer);
    virtual void OnScreenEnter() override;
    virtual void OnScreenExit() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
private:
    UFUNCTION() void LoadLatest();
    UFUNCTION() void Restart();
    UFUNCTION() void HandleLoaded(int32 SlotIndex, bool bSuccess, FText Message);
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UButton> LoadButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RestartButton;
    bool bLoading = false;
};
