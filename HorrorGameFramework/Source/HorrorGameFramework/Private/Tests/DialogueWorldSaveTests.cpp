#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Inventory/InventoryCharacter.h"
#include "Dialogue/DialogueGameplayComponent.h"
#include "Dialogue/DialogueNPC.h"
#include "Components/SaveComponent.h"
#include "DialogueDefinition.h"
#include "DialogueSessionComponent.h"
#include "ObjectiveJournalComponent.h"
#include "ObjectiveDefinition.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDialogueWorldSaveTest, "HorrorFramework.Dialogue.WorldSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDialogueWorldSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("DialogueWorldSaveAutomation"));
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
    APlayerController* PC = World->SpawnActor<APlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    PC->Possess(Player);
    UDialogueDefinition* Definition = NewObject<UDialogueDefinition>();
    Definition->DialogueId = TEXT("Caretaker");
    Definition->SpeakerName = FText::FromString(TEXT("Caretaker"));
    FDialogueLine Line;
    Line.Text = FText::FromString(TEXT("Read the power memo."));
    Definition->Lines = { Line, Line };
    Definition->CompletionFact = TEXT("Dialogue.Caretaker.Completed");
    UObjectiveDefinition* Objective = NewObject<UObjectiveDefinition>();
    Objective->ObjectiveId = TEXT("TalkToCaretaker");
    Objective->Title = FText::FromString(TEXT("Talk to caretaker"));
    Objective->CompletionFacts = { Definition->CompletionFact };
    Player->Objectives->Definitions = { Objective };
    ADialogueNPC* NPC = World->SpawnActor<ADialogueNPC>();
    NPC->Dialogue = Definition;
    NPC->SaveComponent->PersistentKey = TEXT("DialogueTestNPC");
    World->GetWorldSettings()->NotifyBeginPlay();
    Definition->RequiredFacts = { TEXT("CaretakerAccess") };
    TestFalse(TEXT("Required fact gates conversation"), Player->DialogueGameplay->CanStart(NPC));
    Player->Objectives->RecordFact(TEXT("CaretakerAccess"));
    TestTrue(TEXT("Required fact unlocks conversation"), Player->DialogueGameplay->CanStart(NPC));
    Definition->BlockingFacts = { TEXT("CaretakerUnavailable") };
    Player->Objectives->RecordFact(TEXT("CaretakerUnavailable"));
    TestFalse(TEXT("Blocking fact hides conversation"), Player->DialogueGameplay->CanStart(NPC));
    Definition->BlockingFacts.Reset();
    Player->DialogueGameplay->bEnableDialogue = false;
    TestFalse(TEXT("Disabled integration rejects conversation"), Player->DialogueGameplay->CanStart(NPC));
    Player->DialogueGameplay->bEnableDialogue = true;
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    Save->ActorRegistry->RegisterActor(Player);
    FString Error;
    USystemSaveGame* Before = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture before dialogue"), Save->CaptureWorld(Before, Error));
    Player->Dialogue->Start(Definition);
    Player->Dialogue->Cancel();
    TestFalse(TEXT("Cancellation grants no completion fact"), Player->Objectives->HasFact(Definition->CompletionFact));
    Player->Dialogue->Start(Definition);
    Player->Dialogue->Advance();
    Player->Dialogue->Advance();
    TestTrue(TEXT("Completion updates objective journal"), Player->Objectives->HasFact(Definition->CompletionFact));
    TestTrue(TEXT("Objective completes"), Player->Objectives->GetRecords()[0].State == EObjectiveState::Completed);
    USystemSaveGame* Complete = NewObject<USystemSaveGame>();
    TestTrue(TEXT("Capture completed conversation"), Save->CaptureWorld(Complete, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Serialize snapshot"), UGameplayStatics::SaveGameToMemory(Complete, Bytes));
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    Definition->CompletionFact = TEXT("UnexpectedDialogueReplay");
    Player->Dialogue->Start(Definition);
    Player->Dialogue->Advance();
    Player->Dialogue->Advance();
    TestFalse(TEXT("Repeat does not reemit completion fact"), Player->Objectives->HasFact(Definition->CompletionFact));
    TestTrue(TEXT("Rewind before completion"), Save->RestoreWorld(Before, Error));
    TestFalse(TEXT("Completion history rewound"), Player->Dialogue->HasCompleted(Definition->DialogueId));
    Player->Dialogue->Start(Definition);
    TestTrue(TEXT("Restore while conversation is active"), Save->RestoreWorld(Decoded, Error));
    TestFalse(TEXT("Load cancels incomplete session"), Player->Dialogue->IsConversationActive());
    TestTrue(TEXT("History restored"), Player->Dialogue->HasCompleted(Definition->DialogueId));
    TestTrue(TEXT("Original objective fact restored"), Player->Objectives->HasFact(TEXT("Dialogue.Caretaker.Completed")));
    TestFalse(TEXT("Load does not replay completion"), Player->Objectives->HasFact(Definition->CompletionFact));
    Definition->bRepeatable = false;
    TestFalse(TEXT("Restored one-time dialogue remains unavailable"), Player->Dialogue->Start(Definition));
    return true;
}
#endif
