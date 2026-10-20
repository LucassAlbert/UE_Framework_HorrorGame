#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "DocumentPickup.generated.h"

class UDocumentDefinition;
class UStaticMeshComponent;
class USaveComponent;
class UInteractableComponent;
struct FStreamableHandle;

UCLASS()
class HORRORGAMEFRAMEWORK_API ADocumentPickup : public AActor, public IInteractionInterface, public ISaveStateListener
{
    GENERATED_BODY()
public:
    ADocumentPickup();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Document") TSoftObjectPtr<UDocumentDefinition> Document;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Document") bool bCollected = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> SaveComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction") TObjectPtr<UInteractableComponent> Interactable;
    void SetCollected(bool bValue);
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void RefreshPresentation();
    void FinishLoading();
    UPROPERTY(Transient) TObjectPtr<UDocumentDefinition> LoadedDocument;
    TSharedPtr<FStreamableHandle> LoadHandle;
};
