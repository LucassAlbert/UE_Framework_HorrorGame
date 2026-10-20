#include "ObjectiveDefinition.h"

bool UObjectiveDefinition::Validate(FText& Error) const
{
    auto ValidFacts = [](const TArray<FName>& Facts)
    {
        TSet<FName> Seen;
        for (FName Fact : Facts)
        {
            if (Fact.IsNone() || Seen.Contains(Fact)) return false;
            Seen.Add(Fact);
        }
        return true;
    };
    if (ObjectiveId.IsNone() || Title.IsEmpty() || CompletionFacts.IsEmpty()
        || !ValidFacts(ActivationFacts) || !ValidFacts(CompletionFacts))
    {
        Error = NSLOCTEXT("Objectives", "InvalidDefinition", "Objetivo sem identificação, título ou condições válidas.");
        return false;
    }
    return true;
}
