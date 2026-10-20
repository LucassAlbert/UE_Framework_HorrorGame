#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "InspectionGameplayComponent.generated.h"

class UInventoryGameplayComponent;
class UInventoryItemDefinition;
class UInspectionScreen;
class UUIScreenBase;

// The project adapter connects independent inspection, inventory and UI contracts.
UCLASS(ClassGroup = (Inspection), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UInspectionGameplayComponent : public UActorComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UInspectionGameplayComponent();
    UPROPERTY(EditAnywhere, Category = "Inspection") bool bEnableInspection = true;
    UPROPERTY(EditAnywhere, Category = "Inspection") TSubclassOf<UInspectionScreen> ScreenClass;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void InspectItem(UInventoryItemDefinition* Item);
    void CloseScreens();
    TWeakObjectPtr<UInventoryGameplayComponent> InventoryGameplay;
    TWeakObjectPtr<UInspectionScreen> Screen;
    TWeakObjectPtr<UUIScreenBase> ParentScreen;
};
