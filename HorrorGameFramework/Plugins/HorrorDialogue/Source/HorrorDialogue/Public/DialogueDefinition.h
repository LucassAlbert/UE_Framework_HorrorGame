#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DialogueDefinition.generated.h"

USTRUCT(BlueprintType)
struct HORRORDIALOGUE_API FDialogueLine
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = true))
    FText Text;
};

UCLASS(BlueprintType)
class HORRORDIALOGUE_API UDialogueDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue") FName DialogueId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue") FText SpeakerName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue") TArray<FDialogueLine> Lines;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue") bool bRepeatable = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Conditions") TArray<FName> RequiredFacts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Conditions") TArray<FName> BlockingFacts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Progression") FName CompletionFact;
    bool Validate(FText& Error) const;
};
