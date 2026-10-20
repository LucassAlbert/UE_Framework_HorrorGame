#include "ItemPuzzleDefinition.h"

const FItemPuzzleSlot* UItemPuzzleDefinition::FindSlot(FName SlotId) const
{
    return Slots.FindByPredicate([SlotId](const FItemPuzzleSlot& Slot) { return Slot.SlotId == SlotId; });
}

bool UItemPuzzleDefinition::Validate(FText& Error) const
{
    if (PuzzleId.IsNone() || Slots.IsEmpty())
    {
        Error = NSLOCTEXT("Puzzles", "InvalidDefinition", "O puzzle precisa de uma identificação e pelo menos um encaixe.");
        return false;
    }
    TSet<FName> IDs;
    for (const FItemPuzzleSlot& Slot : Slots)
    {
        if (Slot.SlotId.IsNone() || Slot.RequiredItemId.IsNone() || IDs.Contains(Slot.SlotId))
        {
            Error = NSLOCTEXT("Puzzles", "InvalidSlot", "Existem encaixes sem identificação, repetidos ou sem item requerido.");
            return false;
        }
        IDs.Add(Slot.SlotId);
    }
    return true;
}
