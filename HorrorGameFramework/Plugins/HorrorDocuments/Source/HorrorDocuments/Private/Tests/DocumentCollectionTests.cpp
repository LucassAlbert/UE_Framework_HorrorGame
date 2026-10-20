#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DocumentDefinition.h"
#include "DocumentCollectionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDocumentCollectionTest, "HorrorFramework.Documents.Collection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDocumentCollectionTest::RunTest(const FString& Parameters)
{
    UDocumentCollectionComponent* Collection = NewObject<UDocumentCollectionComponent>();
    UDocumentDefinition* Document = NewObject<UDocumentDefinition>();
    Document->DocumentId = TEXT("Memo");
    Document->Title = FText::FromString(TEXT("Memo"));
    FText Error;
    TestFalse(TEXT("Document without pages rejected"), Collection->Collect(Document, Error));
    FDocumentPage Page;
    Document->Pages.Add(Page);
    TestFalse(TEXT("Document page requires readable text"), Collection->Collect(Document, Error));
    Document->Pages[0].Text = FText::FromString(TEXT("First page"));
    Page.Text = FText::FromString(TEXT("Second page"));
    Document->Pages.Add(Page);
    TestTrue(TEXT("Valid document collected"), Collection->Collect(Document, Error));
    TestFalse(TEXT("Collection does not mark a document read"), Collection->IsRead(Document));
    TestFalse(TEXT("Repeated acquisition rejected"), Collection->Collect(Document, Error));
    TestEqual(TEXT("No duplicate collection entry"), Collection->GetDocuments().Num(), 1);
    UDocumentDefinition* Collision = NewObject<UDocumentDefinition>();
    Collision->DocumentId = Document->DocumentId;
    Collision->Title = Document->Title;
    Collision->Pages = Document->Pages;
    TestFalse(TEXT("Different asset with same identifier rejected"), Collection->Collect(Collision, Error));
    TestFalse(TEXT("Unowned document cannot be read"), Collection->MarkRead(Collision));
    TestTrue(TEXT("First opening marks read without requiring every page"), Collection->MarkRead(Document));
    TestTrue(TEXT("Read state available"), Collection->IsRead(Document));
    TestFalse(TEXT("Reopening does not repeat read event"), Collection->MarkRead(Document));
    TestTrue(TEXT("Collection remains valid"), Collection->ValidateState(Error));
    return true;
}
#endif
