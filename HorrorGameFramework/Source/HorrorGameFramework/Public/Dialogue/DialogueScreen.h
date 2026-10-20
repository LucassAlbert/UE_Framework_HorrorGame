#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "DialogueScreen.generated.h"

class UDialogueGameplayComponent;
class UDialogueDefinition;
class UTextBlock;

UCLASS()
class HORRORGAMEFRAMEWORK_API UDialogueScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UDialogueScreen(const FObjectInitializer& Initializer);
    void SetSource(UDialogueGameplayComponent* InSource);
    virtual bool HandlePrimaryAction() override;
    virtual bool HandleBackAction() override;
    virtual void OnScreenExit() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Refresh(UDialogueDefinition* Definition, int32 LineIndex);
    void Unbind();
    TWeakObjectPtr<UDialogueGameplayComponent> Source;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeakerText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LineText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ProgressText;
};
