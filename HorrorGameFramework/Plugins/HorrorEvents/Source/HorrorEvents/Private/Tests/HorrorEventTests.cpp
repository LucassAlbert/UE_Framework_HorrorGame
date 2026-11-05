#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "HorrorEventComponent.h"
#include "HorrorEventDefinition.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorEventSnapshotTest, "HorrorFramework.Events.DefinitionAndSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHorrorEventSnapshotTest::RunTest(const FString& Parameters)
{
    auto* Definition = NewObject<UHorrorEventDefinition>();
    Definition->EventId = TEXT("Test.Event");
    auto* Action = NewObject<UHorrorEventStateAction>(Definition);
    Action->TargetId = TEXT("Light");
    Action->State = TEXT("On");
    FHorrorEventStep Step;
    Step.Action = Action;
    Definition->Steps.Add(Step);
    FText Error;
    TestTrue(TEXT("Valid event definition"), Definition->Validate(Error));
    Definition->Steps[0].DelayBefore = -1;
    TestFalse(TEXT("Negative delays are rejected"), Definition->Validate(Error));
    Definition->Steps[0].DelayBefore = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite delays are rejected"), Definition->Validate(Error));
    Definition->Steps[0].DelayBefore = 0;
    Definition->Steps[0].Action = nullptr;
    TestFalse(TEXT("Missing actions are rejected"), Definition->Validate(Error));
    Definition->Steps[0].Action = Action;

    auto* Event = NewObject<UHorrorEventComponent>();
    Event->Definition = Definition;
    Event->Progress.EventId = Definition->EventId;
    Event->Progress.State = EHorrorEventState::Completed;
    Event->Progress.NextStep = 1;
    Event->Progress.CompletedCount = 1;
    TestTrue(TEXT("Completed snapshot is valid"), Event->ValidateState(Error));
    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Event->Serialize(Archive);
    }
    Event->Progress = FHorrorEventProgress();
    {
        FMemoryReader Reader(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Reader, true);
        Archive.ArIsSaveGame = true;
        Event->Serialize(Archive);
    }
    TestTrue(TEXT("Execution count and state survive SaveGame serialization"), Event->Progress.CompletedCount == 1 && Event->ValidateState(Error));
    Definition->Revision = 2;
    TestFalse(TEXT("Incompatible definition revision is rejected"), Event->ValidateState(Error));
    Definition->Revision = 1;
    Event->Progress.CompletedCount = 2;
    TestFalse(TEXT("A once-only snapshot cannot contain multiple completions"), Event->ValidateState(Error));
    return true;
}
#endif
