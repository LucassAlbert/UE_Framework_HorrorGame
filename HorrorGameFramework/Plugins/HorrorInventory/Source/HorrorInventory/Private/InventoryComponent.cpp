#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryRecipe.h"

namespace InventoryErrors
{
    FInventoryResult InvalidSlot()
    {
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "InvalidSlot", "O item não está disponível."));
    }
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::OnRegister()
{
    Super::OnRegister();
    if (Slots.IsEmpty() && SlotCapacity > 0) Slots.SetNum(SlotCapacity);
}

const FInventorySlot* UInventoryComponent::FindSlot(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex) && !Slots[SlotIndex].IsEmpty() ? &Slots[SlotIndex] : nullptr;
}

UInventoryItemDefinition* UInventoryComponent::GetItemAt(int32 SlotIndex) const
{
    const FInventorySlot* Slot = FindSlot(SlotIndex);
    return Slot ? Slot->Item.Get() : nullptr;
}

int32 UInventoryComponent::GetEquippedSlot() const
{
    if (!EquippedStackId.IsValid()) return INDEX_NONE;
    return Slots.IndexOfByPredicate([this](const FInventorySlot& Slot) { return Slot.StackId == EquippedStackId; });
}

FInventoryResult UInventoryComponent::ValidateState() const
{
    if (SlotCapacity < 1 || Slots.Num() != SlotCapacity)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "InvalidCapacity", "A capacidade do inventário não corresponde ao estado salvo."));
    TSet<FGuid> IDs;
    for (const FInventorySlot& Slot : Slots)
    {
        if (Slot.IsEmpty())
        {
            if (Slot.StackId.IsValid()) return InventoryErrors::InvalidSlot();
            continue;
        }
        const UInventoryItemDefinition* Item = Slot.Item.Get();
        if (!IsValid(Item) || Item->ItemId.IsNone() || Item->MaxStackSize < 1 || Slot.Quantity < 1
            || Slot.Quantity > Item->MaxStackSize || !Slot.StackId.IsValid() || IDs.Contains(Slot.StackId))
        {
            return FInventoryResult::Failure(NSLOCTEXT("Inventory", "InvalidState", "O inventário contém um item não carregado ou dados inválidos."));
        }
        IDs.Add(Slot.StackId);
    }
    if (EquippedStackId.IsValid())
    {
        const UInventoryItemDefinition* Equipped = GetItemAt(GetEquippedSlot());
        if (!Equipped || !Equipped->bCanEquip) return InventoryErrors::InvalidSlot();
    }
    return FInventoryResult::Success();
}

FInventoryResult UInventoryComponent::PlanAdd(TArray<FInventorySlot>& Candidate, UInventoryItemDefinition* Item, int32 Quantity)
{
    if (!IsValid(Item) || Item->ItemId.IsNone() || Item->MaxStackSize < 1 || Quantity < 1)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "InvalidItem", "O item ou a quantidade é inválido."));
    const TSoftObjectPtr<UInventoryItemDefinition> Reference(Item);
    int32 Remaining = Quantity;
    for (FInventorySlot& Slot : Candidate)
    {
        if (Slot.Item != Reference || Slot.Quantity >= Item->MaxStackSize) continue;
        const int32 Added = FMath::Min(Remaining, Item->MaxStackSize - Slot.Quantity);
        Slot.Quantity += Added;
        Remaining -= Added;
        if (Remaining == 0) return FInventoryResult::Success();
    }
    for (FInventorySlot& Slot : Candidate)
    {
        if (!Slot.IsEmpty()) continue;
        Slot.Item = Item;
        Slot.StackId = FGuid::NewGuid();
        Slot.Quantity = FMath::Min(Remaining, Item->MaxStackSize);
        Remaining -= Slot.Quantity;
        if (Remaining == 0) return FInventoryResult::Success();
    }
    return FInventoryResult::Failure(NSLOCTEXT("Inventory", "Full", "Não há espaço suficiente no inventário."));
}

FInventoryResult UInventoryComponent::CanAddItem(UInventoryItemDefinition* Item, int32 Quantity) const
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    TArray<FInventorySlot> Candidate = Slots;
    return PlanAdd(Candidate, Item, Quantity);
}

FInventoryResult UInventoryComponent::AddItem(UInventoryItemDefinition* Item, int32 Quantity)
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    TArray<FInventorySlot> Candidate = Slots;
    const FInventoryResult Result = PlanAdd(Candidate, Item, Quantity);
    if (Result.bSuccess) Commit(MoveTemp(Candidate));
    return Result;
}

void UInventoryComponent::RemoveUnits(TArray<FInventorySlot>& Candidate, int32 SlotIndex, int32 Quantity)
{
    Candidate[SlotIndex].Quantity -= Quantity;
    if (Candidate[SlotIndex].Quantity == 0) Candidate[SlotIndex] = FInventorySlot();
}

FInventoryResult UInventoryComponent::RemoveFromSlot(int32 SlotIndex, int32 Quantity)
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    const FInventorySlot* Slot = FindSlot(SlotIndex);
    if (!Slot || Quantity < 1 || Quantity > Slot->Quantity) return InventoryErrors::InvalidSlot();
    TArray<FInventorySlot> Candidate = Slots;
    RemoveUnits(Candidate, SlotIndex, Quantity);
    Commit(MoveTemp(Candidate));
    return FInventoryResult::Success();
}

FInventoryResult UInventoryComponent::EquipSlot(int32 SlotIndex)
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    const UInventoryItemDefinition* Item = GetItemAt(SlotIndex);
    if (!Item || !Item->bCanEquip) return InventoryErrors::InvalidSlot();
    EquippedStackId = Slots[SlotIndex].StackId;
    OnInventoryChanged.Broadcast();
    return FInventoryResult::Success();
}

FInventoryResult UInventoryComponent::Unequip()
{
    EquippedStackId.Invalidate();
    OnInventoryChanged.Broadcast();
    return FInventoryResult::Success();
}

FInventoryResult UInventoryComponent::CombineSlots(int32 FirstSlot, int32 SecondSlot, UInventoryRecipe* Recipe)
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    const FInventorySlot* First = FindSlot(FirstSlot);
    const FInventorySlot* Second = FindSlot(SecondSlot);
    if (!First || !Second || !IsValid(Recipe) || !Recipe->ResultItem.Get()
        || (FirstSlot == SecondSlot && First->Quantity < 2)) return InventoryErrors::InvalidSlot();
    const bool bMatches = (First->Item == Recipe->FirstIngredient && Second->Item == Recipe->SecondIngredient)
        || (First->Item == Recipe->SecondIngredient && Second->Item == Recipe->FirstIngredient);
    if (!bMatches) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "NoRecipe", "Esses itens não podem ser combinados."));

    TArray<FInventorySlot> Candidate = Slots;
    RemoveUnits(Candidate, FirstSlot, 1);
    RemoveUnits(Candidate, SecondSlot, 1);
    const FInventoryResult Result = PlanAdd(Candidate, Recipe->ResultItem.Get(), 1);
    if (Result.bSuccess) Commit(MoveTemp(Candidate));
    return Result;
}

FInventoryResult UInventoryComponent::DiscardSlot(int32 SlotIndex)
{
    const UInventoryItemDefinition* Item = GetItemAt(SlotIndex);
    if (!Item || !Item->bCanDiscard)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DiscardDisabled", "Descartar está desativado para este item."));
    return RemoveFromSlot(SlotIndex, 1);
}

void UInventoryComponent::KeepDefinitionsAlive()
{
    LoadedDefinitions.Reset();
    for (const FInventorySlot& Slot : Slots)
    {
        if (UInventoryItemDefinition* Item = Slot.Item.Get()) LoadedDefinitions.AddUnique(Item);
    }
}

void UInventoryComponent::Commit(TArray<FInventorySlot>&& Candidate)
{
    Slots = MoveTemp(Candidate);
    if (GetEquippedSlot() == INDEX_NONE) EquippedStackId.Invalidate();
    KeepDefinitionsAlive();
    OnInventoryChanged.Broadcast();
}

FInventoryResult UInventoryComponent::NotifyStateRestored()
{
    const FInventoryResult Validation = ValidateState();
    if (!Validation.bSuccess) return Validation;
    KeepDefinitionsAlive();
    OnInventoryChanged.Broadcast();
    return FInventoryResult::Success();
}
