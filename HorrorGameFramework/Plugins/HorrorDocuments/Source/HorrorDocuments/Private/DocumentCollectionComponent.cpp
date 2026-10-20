#include "DocumentCollectionComponent.h"
#include "DocumentDefinition.h"
#include "Templates/UnrealTemplate.h"

UDocumentCollectionComponent::UDocumentCollectionComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UDocumentCollectionComponent::Contains(UDocumentDefinition* Document) const
{
    return IsValid(Document) && Documents.ContainsByPredicate([Document](const FCollectedDocument& Entry)
        { return Entry.Definition.Get() == Document; });
}
bool UDocumentCollectionComponent::IsRead(UDocumentDefinition* Document) const
{
    const FCollectedDocument* Entry = Documents.FindByPredicate([Document](const FCollectedDocument& Candidate)
        { return Document && Candidate.Definition.Get() == Document; });
    return Entry && Entry->bRead;
}

bool UDocumentCollectionComponent::Collect(UDocumentDefinition* Document, FText& Error)
{
    if (bNotifying)
    {
        Error = NSLOCTEXT("Documents", "BusyCollection", "A coleção está notificando uma alteração. Tente novamente após essa operação.");
        return false;
    }
    if (!ValidateState(Error)) return false;
    if (!IsValid(Document) || !Document->Validate(Error))
    {
        if (Error.IsEmpty()) Error = NSLOCTEXT("Documents", "MissingDocument", "O documento não está disponível.");
        return false;
    }
    for (const FCollectedDocument& Entry : Documents)
        if (Entry.Definition.Get()->DocumentId == Document->DocumentId)
        {
            Error = NSLOCTEXT("Documents", "DuplicateDocument", "Este documento já foi coletado ou possui identificação repetida.");
            return false;
        }
    FCollectedDocument Entry;
    Entry.Definition = Document;
    Documents.Add(Entry);
    LoadedDefinitions.Add(Document);
    TGuardValue<bool> Notifying(bNotifying, true);
    OnDocumentCollected.Broadcast(Document);
    OnCollectionChanged.Broadcast();
    return true;
}

bool UDocumentCollectionComponent::MarkRead(UDocumentDefinition* Document)
{
    FText Error;
    if (bNotifying || !ValidateState(Error)) return false;
    FCollectedDocument* Entry = Documents.FindByPredicate([Document](const FCollectedDocument& Candidate)
        { return Document && Candidate.Definition.Get() == Document; });
    if (!Entry || Entry->bRead) return false;
    Entry->bRead = true;
    TGuardValue<bool> Notifying(bNotifying, true);
    OnDocumentRead.Broadcast(Document);
    OnCollectionChanged.Broadcast();
    return true;
}

bool UDocumentCollectionComponent::ValidateState(FText& Error) const
{
    Error = FText::GetEmpty();
    TSet<FName> Ids;
    for (const FCollectedDocument& Entry : Documents)
    {
        UDocumentDefinition* Document = Entry.Definition.Get();
        if (!Document || !Document->Validate(Error) || Ids.Contains(Document->DocumentId))
        {
            if (Error.IsEmpty()) Error = NSLOCTEXT("Documents", "InvalidCollection", "A coleção contém um documento ausente ou repetido.");
            return false;
        }
        Ids.Add(Document->DocumentId);
    }
    return true;
}

bool UDocumentCollectionComponent::NotifyStateRestored(FText& Error)
{
    if (!ValidateState(Error)) return false;
    LoadedDefinitions.Reset();
    for (const FCollectedDocument& Entry : Documents) LoadedDefinitions.Add(Entry.Definition.Get());
    // Restoration refreshes presentation without replaying acquisition or reading events.
    TGuardValue<bool> Notifying(bNotifying, true);
    OnCollectionChanged.Broadcast();
    return true;
}

void UDocumentCollectionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    OnDocumentCollected.Clear();
    OnDocumentRead.Clear();
    OnCollectionChanged.Clear();
    LoadedDefinitions.Reset();
    Super::EndPlay(Reason);
}
