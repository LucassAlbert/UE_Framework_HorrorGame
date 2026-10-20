#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "Components/Button.h"
#include "DocumentScreens.generated.h"

class UDocumentGameplayComponent;
class UDocumentDefinition;
class UTextBlock;
class UVerticalBox;
class UImage;
class USizeBox;
class UTexture2D;
class UScrollBox;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDocumentSelected, UDocumentDefinition*, Document);

UCLASS()
class UDocumentEntryButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UDocumentDefinition> Document;
    FOnDocumentSelected OnDocumentSelected;
    UFUNCTION() void Select() { OnDocumentSelected.Broadcast(Document); }
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UDocumentCollectionScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    void SetSource(UDocumentGameplayComponent* InSource);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Refresh();
    UFUNCTION() void Read(UDocumentDefinition* Document);
    UFUNCTION() void Back();
    void Unbind();
    TWeakObjectPtr<UDocumentGameplayComponent> Source;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Entries;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UDocumentReaderScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    void SetDocument(UDocumentDefinition* InDocument);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Previous();
    UFUNCTION() void Next();
    UFUNCTION() void Back();
    void RefreshPage();
    void FinishImageLoading(uint64 Generation);
    void CancelImageLoading();
    UPROPERTY(Transient) TObjectPtr<UDocumentDefinition> Document;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedImage;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Body;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> PageScroll;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PageNumber;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ImageStatus;
    UPROPERTY(Transient) TObjectPtr<UImage> PageImage;
    UPROPERTY(Transient) TObjectPtr<USizeBox> ImageContainer;
    UPROPERTY(Transient) TObjectPtr<UButton> PreviousButton;
    UPROPERTY(Transient) TObjectPtr<UButton> NextButton;
    int32 PageIndex = 0;
    uint64 ImageGeneration = 0;
    TSharedPtr<FStreamableHandle> ImageLoadHandle;
};
