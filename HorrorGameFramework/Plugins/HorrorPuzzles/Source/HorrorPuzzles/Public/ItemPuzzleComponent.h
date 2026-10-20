#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemPuzzleComponent.generated.h"

class UItemPuzzleDefinition;
struct FStreamableHandle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemPuzzleChanged, bool, bSolved, bool, bRestored);

UCLASS(ClassGroup = (Puzzles), meta = (BlueprintSpawnableComponent))
class HORRORPUZZLES_API UItemPuzzleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UItemPuzzleComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Puzzle")
    TSoftObjectPtr<UItemPuzzleDefinition> Definition;
    UPROPERTY(BlueprintAssignable, Category = "Puzzle") FOnItemPuzzleChanged OnPuzzleChanged;
    UFUNCTION(BlueprintPure, Category = "Puzzle") bool IsReady() const;
    UFUNCTION(BlueprintPure, Category = "Puzzle") bool IsSolved() const;
    UFUNCTION(BlueprintPure, Category = "Puzzle") bool IsSlotFilled(FName SlotId) const;
    UFUNCTION(BlueprintPure, Category = "Puzzle") UItemPuzzleDefinition* GetDefinition() const { return LoadedDefinition; }
    bool CanInsert(FName SlotId, FName ItemId, FText& Error) const;
    // No callbacks run between prepare and commit, allowing an adapter to consume inventory atomically.
    bool BeginInsertion(FName SlotId, FName ItemId, FText& Error);
    void CommitInsertion();
    void CancelInsertion();
    bool ValidateState(FText& Error) const;
    bool NotifyStateRestored(FText& Error);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void FinishLoading();
    UPROPERTY(SaveGame) TArray<FName> FilledSlots;
    UPROPERTY(Transient) TObjectPtr<UItemPuzzleDefinition> LoadedDefinition;
    FName PendingSlot;
    TSharedPtr<FStreamableHandle> LoadHandle;
};
