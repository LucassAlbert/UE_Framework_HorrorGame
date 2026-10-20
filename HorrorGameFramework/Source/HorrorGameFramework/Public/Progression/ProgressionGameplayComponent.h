#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "InventoryTypes.h"
#include "ObjectiveJournalComponent.h"
#include "ProgressionGameplayComponent.generated.h"

class APuzzleSocket;
class UInventoryComponent;
class UPuzzleItemSelectionScreen;
class UObjectivesScreen;
class UObjectiveNotice;
class UObjectiveDefinition;
class UInventoryScreen;

UCLASS(ClassGroup = (Progression), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UProgressionGameplayComponent : public UActorComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UProgressionGameplayComponent();
    UPROPERTY(EditAnywhere, Category = "Progression") bool bEnablePuzzles = true;
    UPROPERTY(EditAnywhere, Category = "Progression") bool bEnableObjectives = true;
    UPROPERTY(EditAnywhere, Category = "Progression") TSubclassOf<UPuzzleItemSelectionScreen> ItemSelectionClass;
    UPROPERTY(EditAnywhere, Category = "Progression") TSubclassOf<UObjectivesScreen> ObjectivesClass;
    UFUNCTION(BlueprintCallable, Category = "Progression") void OpenObjectives();
    void OpenItemSelection(APuzzleSocket* Socket);
    FInventoryResult InsertItem(int32 SlotIndex);
    UInventoryComponent* GetInventory() const;
    UObjectiveJournalComponent* GetJournal() const;
    APuzzleSocket* GetSelectedSocket() const { return SelectedSocket.Get(); }
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FHorrorProgressionWorldSaveTest;
    bool ValidateTarget(APuzzleSocket* Socket) const;
    void CloseScreens();
    UFUNCTION() void HandleObjectiveChanged(UObjectiveDefinition* Definition, EObjectiveState State);
    TWeakObjectPtr<APuzzleSocket> SelectedSocket;
    TWeakObjectPtr<UPuzzleItemSelectionScreen> SelectionScreen;
    TWeakObjectPtr<UObjectivesScreen> ObjectivesScreen;
    TWeakObjectPtr<UInventoryScreen> ObjectivesParentScreen;
    TWeakObjectPtr<UObjectiveNotice> Notice;
    bool bInserting = false;
};
