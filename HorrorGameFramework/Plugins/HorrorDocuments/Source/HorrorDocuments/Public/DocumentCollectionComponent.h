#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DocumentCollectionComponent.generated.h"

class UDocumentDefinition;

USTRUCT(BlueprintType)
struct HORRORDOCUMENTS_API FCollectedDocument
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Document") TSoftObjectPtr<UDocumentDefinition> Definition;
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Document") bool bRead = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDocumentEvent, UDocumentDefinition*, Document);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDocumentCollectionChanged);

UCLASS(ClassGroup = (Documents), meta = (BlueprintSpawnableComponent))
class HORRORDOCUMENTS_API UDocumentCollectionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDocumentCollectionComponent();
    UPROPERTY(BlueprintAssignable, Category = "Documents") FOnDocumentEvent OnDocumentCollected;
    UPROPERTY(BlueprintAssignable, Category = "Documents") FOnDocumentEvent OnDocumentRead;
    UPROPERTY(BlueprintAssignable, Category = "Documents") FOnDocumentCollectionChanged OnCollectionChanged;
    UFUNCTION(BlueprintCallable, Category = "Documents") bool Collect(UDocumentDefinition* Document, FText& Error);
    UFUNCTION(BlueprintCallable, Category = "Documents") bool MarkRead(UDocumentDefinition* Document);
    UFUNCTION(BlueprintPure, Category = "Documents") bool Contains(UDocumentDefinition* Document) const;
    UFUNCTION(BlueprintPure, Category = "Documents") bool IsRead(UDocumentDefinition* Document) const;
    UFUNCTION(BlueprintPure, Category = "Documents") TArray<FCollectedDocument> GetDocuments() const { return Documents; }
    bool ValidateState(FText& Error) const;
    bool NotifyStateRestored(FText& Error);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(SaveGame) TArray<FCollectedDocument> Documents;
    UPROPERTY(Transient) TArray<TObjectPtr<UDocumentDefinition>> LoadedDefinitions;
    bool bNotifying = false;
};
