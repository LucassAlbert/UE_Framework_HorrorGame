#pragma once
#include "CoreMinimal.h"
#include "Threat/FrameworkThreatCharacter.h"
#include "Inventory/InventoryKeyDoor.h"
#include "InspectionComponent.h"
#include "UIScreenBase.h"
#include "IntegratedDemo.generated.h"

// Composition belonging to the demonstration; reusable plugins have no demo dependencies.
UCLASS()
class HORRORGAMEFRAMEWORK_API AIntegratedDemoCharacter : public AFrameworkThreatCharacter
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleInspection(EInspectionState State, FText Message);
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UDemoCompletionScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    virtual bool HandleBackAction() override { return true; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UFUNCTION() void Restart();
    UFUNCTION() void MainMenu();
};

// Final door shares the existing key-consumption transaction and validates progress on use.
UCLASS()
class HORRORGAMEFRAMEWORK_API AIntegratedDemoExit : public AInventoryKeyDoor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Demo") TArray<FName> RequiredFacts;
    UFUNCTION(BlueprintPure, Category="Demo") bool HasRequiredProgress(AActor* Player) const;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void OnActorLoaded_Implementation() override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void ClosePresentation();
    TWeakObjectPtr<UUIScreenBase> Presentation;
    bool bCompleting = false;
};
