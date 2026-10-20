#pragma once

#include "Core/HorrorCharacter.h"
#include "InventoryCharacter.generated.h"

class UInventoryComponent;
class UInventoryGameplayComponent;
class UInspectionComponent;
class UInspectionGameplayComponent;
class UObjectiveJournalComponent;
class UProgressionGameplayComponent;
class UDocumentCollectionComponent;
class UDocumentGameplayComponent;
class UDialogueSessionComponent;
class UDialogueGameplayComponent;

// Composes optional capabilities without changing the reusable controller or movement system.
UCLASS()
class HORRORGAMEFRAMEWORK_API AInventoryCharacter : public AHorrorCharacter
{
    GENERATED_BODY()
public:
    AInventoryCharacter();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UInventoryComponent> Inventory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UInventoryGameplayComponent> InventoryGameplay;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspection")
    TObjectPtr<UInspectionComponent> Inspection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inspection")
    TObjectPtr<UInspectionGameplayComponent> InspectionGameplay;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression")
    TObjectPtr<UObjectiveJournalComponent> Objectives;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression")
    TObjectPtr<UProgressionGameplayComponent> ProgressionGameplay;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Documents")
    TObjectPtr<UDocumentCollectionComponent> Documents;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Documents")
    TObjectPtr<UDocumentGameplayComponent> DocumentGameplay;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue") TObjectPtr<UDialogueSessionComponent> Dialogue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue") TObjectPtr<UDialogueGameplayComponent> DialogueGameplay;
    virtual void OnInventory_Implementation() override;
};
