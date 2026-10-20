#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DocumentDefinition.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct HORRORDOCUMENTS_API FDocumentPage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document", meta = (MultiLine = "true"))
    FText Text;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
    TSoftObjectPtr<UTexture2D> Image;
};

// Shared content; acquisition and reading progress belong to each player's collection.
UCLASS(BlueprintType)
class HORRORDOCUMENTS_API UDocumentDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
    FName DocumentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
    TArray<FDocumentPage> Pages;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
    FName CollectionFact;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
    FName ReadFact;

    bool Validate(FText& Error) const;
};
