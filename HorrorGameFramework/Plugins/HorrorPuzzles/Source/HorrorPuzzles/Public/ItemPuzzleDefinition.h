#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemPuzzleDefinition.generated.h"

USTRUCT(BlueprintType)
struct HORRORPUZZLES_API FItemPuzzleSlot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SlotId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RequiredItemId;
};

UCLASS(BlueprintType)
class HORRORPUZZLES_API UItemPuzzleDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FName PuzzleId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FName CompletionFact;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") TArray<FItemPuzzleSlot> Slots;
    bool Validate(FText& Error) const;
    const FItemPuzzleSlot* FindSlot(FName SlotId) const;
};
