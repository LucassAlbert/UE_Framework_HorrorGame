#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DialogueDefinition.h"
#include "DialogueSessionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDialogueSessionTest, "HorrorFramework.Dialogue.Session",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDialogueSessionTest::RunTest(const FString& Parameters)
{
    UDialogueSessionComponent* Session = NewObject<UDialogueSessionComponent>();
    UDialogueDefinition* Definition = NewObject<UDialogueDefinition>();
    TestFalse(TEXT("Invalid definitions cannot start"), Session->Start(Definition));
    Definition->DialogueId = TEXT("CaretakerIntro");
    Definition->SpeakerName = FText::FromString(TEXT("Caretaker"));
    FDialogueLine Line;
    Line.Text = FText::FromString(TEXT("Find the two fuses."));
    Definition->Lines = { Line, Line };
    Definition->RequiredFacts = { TEXT("ReadMemo") };
    Definition->BlockingFacts = Definition->RequiredFacts;
    TestFalse(TEXT("Contradictory conditions are rejected"), Session->Start(Definition));
    Definition->BlockingFacts.Reset();
    TestTrue(TEXT("Valid sequential dialogue starts"), Session->Start(Definition));
    TestEqual(TEXT("First line starts at zero"), Session->GetLineIndex(), 0);
    TestFalse(TEXT("An active conversation cannot be replaced"), Session->Start(Definition));
    TestTrue(TEXT("Advance one line"), Session->Advance());
    TestTrue(TEXT("Reaching last line does not complete before confirmation"), Session->IsConversationActive());
    Session->Cancel();
    TestFalse(TEXT("Cancel never completes dialogue"), Session->HasCompleted(Definition->DialogueId));
    TestTrue(TEXT("Cancelled dialogue can restart"), Session->Start(Definition));
    TestEqual(TEXT("Restart begins at first line"), Session->GetLineIndex(), 0);
    Session->Advance();
    Session->Advance();
    TestFalse(TEXT("Final confirmation closes session"), Session->IsConversationActive());
    TestTrue(TEXT("Completion is recorded"), Session->HasCompleted(Definition->DialogueId));
    TestTrue(TEXT("Repeatable dialogue can reopen"), Session->Start(Definition));
    Session->NotifyStateRestored();
    TestFalse(TEXT("Restore cancels presentation"), Session->IsConversationActive());
    TestTrue(TEXT("Restore does not erase completion history"), Session->HasCompleted(Definition->DialogueId));
    Definition->bRepeatable = false;
    TestFalse(TEXT("One-time dialogue cannot reopen after completion"), Session->Start(Definition));
    UDialogueDefinition* SameId = NewObject<UDialogueDefinition>();
    SameId->DialogueId = Definition->DialogueId;
    SameId->SpeakerName = Definition->SpeakerName;
    SameId->Lines = Definition->Lines;
    SameId->bRepeatable = false;
    TestFalse(TEXT("History uses stable dialogue ID rather than asset pointer"), Session->Start(SameId));
    FString Error;
    TestTrue(TEXT("Final state remains valid"), Session->ValidateState(Error));
    return true;
}
#endif
