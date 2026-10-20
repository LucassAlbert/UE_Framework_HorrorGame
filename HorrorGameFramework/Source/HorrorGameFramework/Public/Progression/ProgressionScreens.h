#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "Blueprint/UserWidget.h"
#include "ProgressionScreens.generated.h"

class UProgressionGameplayComponent;
class UObjectiveJournalComponent;
class UInventorySlotButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class HORRORGAMEFRAMEWORK_API UPuzzleItemSelectionScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    void SetSource(UProgressionGameplayComponent* InSource);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Refresh();
    UFUNCTION() void Choose(int32 SlotIndex);
    UFUNCTION() void Back();
    void Unbind();
    TWeakObjectPtr<UProgressionGameplayComponent> Source;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> SlotContainer;
    UPROPERTY(Transient) TArray<TObjectPtr<UInventorySlotButton>> Buttons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UObjectivesScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    void SetSource(UObjectiveJournalComponent* InSource);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Refresh();
    UFUNCTION() void Back();
    void Unbind();
    TWeakObjectPtr<UObjectiveJournalComponent> Source;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Entries;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UObjectiveNotice : public UUserWidget
{
    GENERATED_BODY()
public:
    void Enqueue(const FText& Message);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual void NativeDestruct() override;
private:
    void ShowNext();
    TArray<FText> Pending;
    float RemainingSeconds = 0.f;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};
