#include "DocumentDefinition.h"

bool UDocumentDefinition::Validate(FText& Error) const
{
    Error = FText::GetEmpty();
    if (DocumentId.IsNone() || Title.IsEmpty() || Pages.IsEmpty())
    {
        Error = NSLOCTEXT("Documents", "InvalidDefinition", "O documento precisa de identificação, título e páginas.");
        return false;
    }
    for (const FDocumentPage& Page : Pages)
    {
        if (Page.Text.IsEmpty())
        {
            Error = NSLOCTEXT("Documents", "MissingText", "Cada página do documento precisa de um texto legível.");
            return false;
        }
    }
    return true;
}
