#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectiveDefinition.generated.h"

UCLASS(BlueprintType)
class HORROROBJECTIVES_API UObjectiveDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective") FName ObjectiveId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective") FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (MultiLine = "true")) FText Description;
    // Empty activation requirements mean active from the beginning.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective") TArray<FName> ActivationFacts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective") TArray<FName> CompletionFacts;
    bool Validate(FText& Error) const;
};
