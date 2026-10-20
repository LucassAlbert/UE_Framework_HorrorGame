#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "UIPlaceholderScreen.generated.h"

UCLASS()
class HORRORUI_API UUIPlaceholderScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UUIPlaceholderScreen(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    FText ScreenTitle;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UFUNCTION()
    void HandleClose();
};
