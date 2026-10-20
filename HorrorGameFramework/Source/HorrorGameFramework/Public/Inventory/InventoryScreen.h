#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "Components/Button.h"
#include "InventoryScreen.generated.h"

class UInventoryGameplayComponent;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotSelected, int32, SlotIndex);

UCLASS()
class UInventorySlotButton : public UButton
{
    GENERATED_BODY()
public:
    int32 SlotIndex = INDEX_NONE;
    FOnInventorySlotSelected OnSlotSelected;
    UFUNCTION()
    void Select() { OnSlotSelected.Broadcast(SlotIndex); }
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UInventoryScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UInventoryScreen(const FObjectInitializer& Initializer);
    void SetSource(UInventoryGameplayComponent* InSource);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UButton* AddAction(class UVerticalBox* Layout, const FText& Label);
    UFUNCTION() void Refresh();
    UFUNCTION() void SelectSlot(int32 Index);
    UFUNCTION() void ShowFeedback(FText Message);
    UFUNCTION() void Use();
    UFUNCTION() void Equip();
    UFUNCTION() void Combine();
    UFUNCTION() void Drop();
    UFUNCTION() void Discard();
    UFUNCTION() void Inspect();
    UFUNCTION() void Objectives();
    UFUNCTION() void Documents();
    UFUNCTION() void Back();
    void UnbindSource();
    TWeakObjectPtr<UInventoryGameplayComponent> Source;
    int32 SelectedSlot = 0;
    int32 FirstIngredient = INDEX_NONE;
    UPROPERTY(Transient) TArray<TObjectPtr<UInventorySlotButton>> SlotButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotLabels;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Description;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UButton> UseButton;
    UPROPERTY(Transient) TObjectPtr<UButton> EquipButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CombineButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DropButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DiscardButton;
    UPROPERTY(Transient) TObjectPtr<UButton> InspectButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ObjectivesButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DocumentsButton;
};
