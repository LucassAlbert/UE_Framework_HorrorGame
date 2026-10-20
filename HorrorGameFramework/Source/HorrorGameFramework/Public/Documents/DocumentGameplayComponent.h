#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "DocumentGameplayComponent.generated.h"

class UDocumentDefinition;
class UDocumentCollectionComponent;
class UDocumentCollectionScreen;
class UDocumentReaderScreen;
class UUIScreenBase;
class UUIManagerSubsystem;
class ADocumentPickup;

UCLASS(ClassGroup = (Documents), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UDocumentGameplayComponent : public UActorComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UDocumentGameplayComponent();
    UPROPERTY(EditAnywhere, Category = "Documents") bool bEnableDocuments = true;
    UPROPERTY(EditAnywhere, Category = "Documents") TSubclassOf<UDocumentCollectionScreen> CollectionScreenClass;
    UPROPERTY(EditAnywhere, Category = "Documents") TSubclassOf<UDocumentReaderScreen> ReaderScreenClass;
    UFUNCTION(BlueprintCallable, Category = "Documents") void OpenCollection();
    UFUNCTION(BlueprintCallable, Category = "Documents") bool OpenReader(UDocumentDefinition* Document);
    bool CollectDocument(ADocumentPickup* Pickup);
    UDocumentCollectionComponent* GetCollection() const;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UUIManagerSubsystem* GetUI() const;
    bool ValidateTarget(ADocumentPickup* Pickup) const;
    void CloseScreens();
    void RecordFact(FName Fact);
    UFUNCTION() void HandleCollected(UDocumentDefinition* Document);
    UFUNCTION() void HandleRead(UDocumentDefinition* Document);
    TWeakObjectPtr<UDocumentCollectionScreen> CollectionScreen;
    TWeakObjectPtr<UDocumentReaderScreen> ReaderScreen;
    TWeakObjectPtr<UUIScreenBase> ParentScreen;
    bool bCollecting = false;
};
