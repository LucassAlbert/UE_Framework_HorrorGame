#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "DialogueNPC.generated.h"

class UDialogueDefinition;
class UInteractableComponent;
class UStaticMeshComponent;
class USceneComponent;
class USaveComponent;
struct FStreamableHandle;

UCLASS()
class HORRORGAMEFRAMEWORK_API ADialogueNPC : public AActor, public IInteractionInterface, public ISaveStateListener
{
    GENERATED_BODY()
public:
    ADialogueNPC();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Dialogue") TSoftObjectPtr<UDialogueDefinition> Dialogue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue") TObjectPtr<USceneComponent> LookTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue") TObjectPtr<UInteractableComponent> Interactable;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> SaveComponent;
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
    void LoadDefinition();
    void FinishLoading();
    UPROPERTY(Transient) TObjectPtr<UDialogueDefinition> LoadedDefinition;
    TSharedPtr<FStreamableHandle> LoadHandle;
};
