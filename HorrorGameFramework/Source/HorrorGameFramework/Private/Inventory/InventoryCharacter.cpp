#include "Inventory/InventoryCharacter.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryComponent.h"
#include "InspectionComponent.h"
#include "Inspection/InspectionGameplayComponent.h"
#include "ObjectiveJournalComponent.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "DocumentCollectionComponent.h"
#include "Documents/DocumentGameplayComponent.h"
#include "DialogueSessionComponent.h"
#include "Dialogue/DialogueGameplayComponent.h"

AInventoryCharacter::AInventoryCharacter()
{
    Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
    InventoryGameplay = CreateDefaultSubobject<UInventoryGameplayComponent>(TEXT("InventoryGameplay"));
    Inspection = CreateDefaultSubobject<UInspectionComponent>(TEXT("Inspection"));
    InspectionGameplay = CreateDefaultSubobject<UInspectionGameplayComponent>(TEXT("InspectionGameplay"));
    Objectives = CreateDefaultSubobject<UObjectiveJournalComponent>(TEXT("Objectives"));
    ProgressionGameplay = CreateDefaultSubobject<UProgressionGameplayComponent>(TEXT("ProgressionGameplay"));
    Documents = CreateDefaultSubobject<UDocumentCollectionComponent>(TEXT("Documents"));
    DocumentGameplay = CreateDefaultSubobject<UDocumentGameplayComponent>(TEXT("DocumentGameplay"));
    Dialogue = CreateDefaultSubobject<UDialogueSessionComponent>(TEXT("Dialogue"));
    DialogueGameplay = CreateDefaultSubobject<UDialogueGameplayComponent>(TEXT("DialogueGameplay"));
}

void AInventoryCharacter::OnInventory_Implementation() { InventoryGameplay->OpenInventory(); }
