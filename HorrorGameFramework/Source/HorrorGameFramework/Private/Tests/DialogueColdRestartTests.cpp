#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Inventory/InventoryCharacter.h"
#include "DialogueDefinition.h"
#include "DialogueSessionComponent.h"
#include "ObjectiveJournalComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDialogueColdRestartTest, "HorrorFramework.Dialogue.ColdRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDialogueColdRestartTest::RunTest(const FString& Parameters)
{
    const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("DialogueTestWrite"));
    const bool bRead = FParse::Param(FCommandLine::Get(), TEXT("DialogueTestRead"));
    if (!bWrite && !bRead)
    {
        AddInfo(TEXT("Run separate processes with DialogueTestWrite/DialogueTestRead and an isolated DialogueTestSlot."));
        return true;
    }
    FString Slot;
    FParse::Value(FCommandLine::Get(), TEXT("DialogueTestSlot="), Slot);
    if (bWrite == bRead || !Slot.StartsWith(TEXT("HorrorFramework_Dialogue_Test_")))
    {
        AddError(TEXT("Use exactly one phase and a dedicated HorrorFramework_Dialogue_Test_ slot."));
        return false;
    }
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("DialogueColdRestartAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    AInventoryCharacter* Player = World->SpawnActor<AInventoryCharacter>();
    World->GetWorldSettings()->NotifyBeginPlay();
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    Save->ActorRegistry->RegisterActor(Player);
    UDialogueDefinition* Definition = NewObject<UDialogueDefinition>();
    Definition->DialogueId = TEXT("CaretakerPersistentConversation");
    Definition->SpeakerName = FText::FromString(TEXT("Caretaker"));
    FDialogueLine Line;
    Line.Text = FText::FromString(TEXT("Find the fuses."));
    Definition->Lines = { Line };
    Definition->bRepeatable = false;
    Definition->CompletionFact = TEXT("Dialogue.Caretaker.Completed");
    FString Error;
    if (bWrite)
    {
        if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        {
            AddError(TEXT("Refusing to overwrite an existing file. Choose a new isolated test slot."));
            return false;
        }
        TestTrue(TEXT("Start conversation"), Player->Dialogue->Start(Definition));
        TestTrue(TEXT("Complete conversation"), Player->Dialogue->Advance());
        USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
        if (!TestTrue(TEXT("Capture completed conversation"), Save->CaptureWorld(Snapshot, Error))) return false;
        TestTrue(TEXT("Write dedicated test file"), UGameplayStatics::SaveGameToSlot(Snapshot, Slot, 0));
        return !HasAnyErrors();
    }
    TestFalse(TEXT("Fresh process has no completion"), Player->Dialogue->HasCompleted(Definition->DialogueId));
    USystemSaveGame* Snapshot = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!TestNotNull(TEXT("Read dedicated test file"), Snapshot)) return false;
    if (!TestTrue(TEXT("Restore in fresh process"), Save->RestoreWorld(Snapshot, Error))) return false;
    TestTrue(TEXT("Stable completion ID survives restart"), Player->Dialogue->HasCompleted(Definition->DialogueId));
    TestFalse(TEXT("One-time conversation remains unavailable"), Player->Dialogue->Start(Definition));
    TestTrue(TEXT("Objective fact survives restart"), Player->Objectives->HasFact(Definition->CompletionFact));
    TestFalse(TEXT("No conversation resumes halfway"), Player->Dialogue->IsConversationActive());
    if (!HasAnyErrors()) TestTrue(TEXT("Remove only dedicated test file"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
    return !HasAnyErrors();
}
#endif
