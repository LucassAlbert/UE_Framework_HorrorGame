#include "DialogueSessionComponent.h"
#include "DialogueDefinition.h"
#include "Templates/UnrealTemplate.h"

UDialogueSessionComponent::UDialogueSessionComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UDialogueSessionComponent::Start(UDialogueDefinition* Definition)
{
    FText Error;
    if (bNotifying || IsConversationActive() || !IsValid(Definition) || !Definition->Validate(Error)
        || (!Definition->bRepeatable && HasCompleted(Definition->DialogueId))) return false;
    ActiveDefinition = Definition;
    LineIndex = 0;
    TGuardValue<bool> Notifying(bNotifying, true);
    OnLineChanged.Broadcast(Definition, LineIndex);
    return true;
}

bool UDialogueSessionComponent::Advance()
{
    if (bNotifying || !IsConversationActive()) return false;
    if (LineIndex + 1 >= ActiveDefinition->Lines.Num()) Finish(true);
    else
    {
        ++LineIndex;
        TGuardValue<bool> Notifying(bNotifying, true);
        OnLineChanged.Broadcast(ActiveDefinition, LineIndex);
    }
    return true;
}

void UDialogueSessionComponent::Cancel() { if (!bNotifying && IsConversationActive()) Finish(false); }

void UDialogueSessionComponent::Finish(bool bCompleted)
{
    UDialogueDefinition* Definition = ActiveDefinition;
    const bool bFirst = bCompleted && !HasCompleted(Definition->DialogueId);
    if (bFirst) CompletedDialogues.Add(Definition->DialogueId);
    ActiveDefinition = nullptr;
    LineIndex = INDEX_NONE;
    TGuardValue<bool> Notifying(bNotifying, true);
    OnEnded.Broadcast(Definition, bCompleted, bFirst);
}

bool UDialogueSessionComponent::ValidateState(FString& Error) const
{
    Error.Reset();
    TSet<FName> Ids;
    for (FName Id : CompletedDialogues)
    {
        if (Id.IsNone() || Ids.Contains(Id)) { Error = TEXT("Dialogue history contains an empty or duplicate ID."); return false; }
        Ids.Add(Id);
    }
    if (bNotifying) { Error = TEXT("Cannot capture dialogue history during a notification."); return false; }
    return true;
}

void UDialogueSessionComponent::NotifyStateRestored() { Cancel(); }
void UDialogueSessionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Cancel();
    OnLineChanged.Clear();
    OnEnded.Clear();
    Super::EndPlay(Reason);
}
