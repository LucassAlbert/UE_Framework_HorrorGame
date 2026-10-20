#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorPuzzles, Log, All);
UItemPuzzleComponent::UItemPuzzleComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UItemPuzzleComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Definition.IsNull()) return;
    if (Definition.Get()) FinishLoading();
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Definition.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UItemPuzzleComponent::FinishLoading));
}

void UItemPuzzleComponent::FinishLoading()
{
    LoadedDefinition = Definition.Get();
    FText Error;
    if (!ValidateState(Error))
    {
        UE_LOG(LogHorrorPuzzles, Error, TEXT("%s: %s"), *GetNameSafe(GetOwner()), *Error.ToString());
        return;
    }
    OnPuzzleChanged.Broadcast(IsSolved(), false);
}

bool UItemPuzzleComponent::IsReady() const
{
    FText Error;
    return LoadedDefinition && LoadedDefinition == Definition.Get() && LoadedDefinition->Validate(Error);
}
bool UItemPuzzleComponent::IsSolved() const
{
    return IsReady() && FilledSlots.Num() == LoadedDefinition->Slots.Num();
}
bool UItemPuzzleComponent::IsSlotFilled(FName SlotId) const { return FilledSlots.Contains(SlotId); }

bool UItemPuzzleComponent::ValidateState(FText& Error) const
{
    const UItemPuzzleDefinition* Data = Definition.Get();
    if (!Data || !Data->Validate(Error))
    {
        if (Error.IsEmpty()) Error = NSLOCTEXT("Puzzles", "NotLoaded", "A definição do puzzle não foi carregada.");
        return false;
    }
    if (!PendingSlot.IsNone())
    {
        Error = NSLOCTEXT("Puzzles", "PendingInsertion", "Há uma inserção de item em andamento.");
        return false;
    }
    TSet<FName> IDs;
    for (FName Id : FilledSlots)
    {
        if (!Data->FindSlot(Id) || IDs.Contains(Id))
        {
            Error = NSLOCTEXT("Puzzles", "InvalidState", "O puzzle contém um encaixe inválido ou repetido.");
            return false;
        }
        IDs.Add(Id);
    }
    return true;
}

bool UItemPuzzleComponent::CanInsert(FName SlotId, FName ItemId, FText& Error) const
{
    if (!IsReady() || !ValidateState(Error)) return false;
    const FItemPuzzleSlot* Slot = LoadedDefinition->FindSlot(SlotId);
    if (!Slot || IsSlotFilled(SlotId) || IsSolved())
    {
        Error = NSLOCTEXT("Puzzles", "UnavailableSlot", "Este encaixe não está disponível.");
        return false;
    }
    if (Slot->RequiredItemId != ItemId)
    {
        Error = NSLOCTEXT("Puzzles", "WrongItem", "Este item não serve neste encaixe.");
        return false;
    }
    return true;
}

bool UItemPuzzleComponent::BeginInsertion(FName SlotId, FName ItemId, FText& Error)
{
    if (!CanInsert(SlotId, ItemId, Error)) return false;
    PendingSlot = SlotId;
    return true;
}

void UItemPuzzleComponent::CommitInsertion()
{
    if (PendingSlot.IsNone()) return;
    FilledSlots.Add(PendingSlot);
    PendingSlot = NAME_None;
    OnPuzzleChanged.Broadcast(IsSolved(), false);
}
void UItemPuzzleComponent::CancelInsertion() { PendingSlot = NAME_None; }

bool UItemPuzzleComponent::NotifyStateRestored(FText& Error)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    PendingSlot = NAME_None;
    LoadedDefinition = Definition.Get();
    if (!ValidateState(Error)) return false;
    OnPuzzleChanged.Broadcast(IsSolved(), true);
    return true;
}

void UItemPuzzleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    CancelInsertion();
    OnPuzzleChanged.Clear();
    LoadedDefinition = nullptr;
    Super::EndPlay(Reason);
}
