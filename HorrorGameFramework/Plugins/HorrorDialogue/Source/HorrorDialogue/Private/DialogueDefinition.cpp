#include "DialogueDefinition.h"

bool UDialogueDefinition::Validate(FText& Error) const
{
    Error = FText::GetEmpty();
    if (DialogueId.IsNone() || SpeakerName.IsEmpty() || Lines.IsEmpty()
        || Lines.ContainsByPredicate([](const FDialogueLine& Line) { return Line.Text.IsEmpty(); }))
    {
        Error = NSLOCTEXT("Dialogue", "InvalidDefinition", "Configure identificação, nome do NPC e pelo menos uma fala com texto.");
        return false;
    }
    for (FName Fact : RequiredFacts)
        if (Fact.IsNone() || BlockingFacts.Contains(Fact))
        {
            Error = NSLOCTEXT("Dialogue", "InvalidConditions", "As condições do diálogo contêm um fato vazio ou contraditório.");
            return false;
        }
    if (BlockingFacts.Contains(NAME_None))
    {
        Error = NSLOCTEXT("Dialogue", "EmptyBlockingFact", "Uma condição de bloqueio está vazia.");
        return false;
    }
    return true;
}
