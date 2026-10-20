#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Inventory/InventoryCharacter.h"
#include "Documents/DocumentPickup.h"
#include "DocumentDefinition.h"
#include "DocumentCollectionComponent.h"
#include "ObjectiveJournalComponent.h"
#include "Components/SaveComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SystemSaveGame.h"
#include "Subsystems/SaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorDocumentColdRestartTest, "HorrorFramework.Documents.ColdRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorDocumentColdRestartTest::RunTest(const FString& Parameters)
{
    const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("DocumentTestWrite"));
    const bool bRead = FParse::Param(FCommandLine::Get(), TEXT("DocumentTestRead"));
    if (!bWrite && !bRead)
    {
        AddInfo(TEXT("Run in separate processes with DocumentTestWrite/DocumentTestRead and an isolated DocumentTestSlot."));
        return true;
    }
    FString Slot;
    FParse::Value(FCommandLine::Get(), TEXT("DocumentTestSlot="), Slot);
    if (bWrite == bRead || !Slot.StartsWith(TEXT("HorrorFramework_Documents_Test_")))
    {
        AddError(TEXT("Use exactly one test phase and a dedicated HorrorFramework_Documents_Test_ slot."));
        return false;
    }
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("DocumentColdRestartAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    AInventoryCharacter* Player = World->SpawnActor<AInventoryCharacter>();
    ADocumentPickup* Pickup = World->SpawnActor<ADocumentPickup>();
    Pickup->SaveComponent->PersistentKey = TEXT("ColdRestart.Document");
    const FSoftObjectPath Path(TEXT("/Game/_Project/Documents/DA_PowerMemo.DA_PowerMemo"));
    if (bWrite) Pickup->Document = LoadObject<UDocumentDefinition>(nullptr, *Path.ToString());
    World->GetWorldSettings()->NotifyBeginPlay();
    Save->ActorRegistry->RegisterActor(Player);
    Save->ActorRegistry->RegisterActor(Pickup);
    FString Error;
    if (bWrite)
    {
        if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        {
            AddError(TEXT("Refusing to overwrite an existing test file. Use a new isolated slot name."));
            return false;
        }
        UDocumentDefinition* Document = Pickup->Document.Get();
        if (!TestNotNull(TEXT("Demo document asset loaded"), Document)) return false;
        FText Detail;
        TestTrue(TEXT("Collect authored document"), Player->Documents->Collect(Document, Detail));
        TestTrue(TEXT("Mark authored document read"), Player->Documents->MarkRead(Document));
        Pickup->SetCollected(true);
        USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
        if (!TestTrue(TEXT("Capture document state"), Save->CaptureWorld(Snapshot, Error))) return false;
        TestTrue(TEXT("Write real isolated save file"), UGameplayStatics::SaveGameToSlot(Snapshot, Slot, 0));
        return !HasAnyErrors();
    }
    TestEqual(TEXT("Fresh process has no acquired documents"), Player->Documents->GetDocuments().Num(), 0);
    TestTrue(TEXT("Document definition starts unloaded in the reading process"), Path.ResolveObject() == nullptr);
    USystemSaveGame* Snapshot = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!TestNotNull(TEXT("Read real isolated save file"), Snapshot)) return false;
    TArray<FSoftObjectPath> Paths;
    for (const FActorSaveData& Actor : Snapshot->SavedActors)
        for (const FSoftObjectPath& Dependency : Actor.Dependencies) Paths.AddUnique(Dependency);
    TSharedPtr<FStreamableHandle> Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    if (Loading) Loading->WaitUntilComplete(5.f);
    if (!TestTrue(TEXT("Snapshot dependencies load asynchronously"), Loading && Loading->HasLoadCompleted())) return false;
    if (!TestTrue(FString(TEXT("Restore restarted world: ")) + Error, Save->RestoreWorld(Snapshot, Error))) return false;
    UDocumentDefinition* Document = Pickup->Document.Get();
    if (!TestNotNull(TEXT("Definition restored"), Document)) return false;
    TestTrue(TEXT("Ownership restored after process restart"), Player->Documents->Contains(Document));
    TestTrue(TEXT("Read state restored after process restart"), Player->Documents->IsRead(Document));
    TestTrue(TEXT("Authored pickup remains hidden"), Pickup->bCollected && Pickup->IsHidden());
    TestTrue(TEXT("Collection fact persists"), Player->Objectives->HasFact(Document->CollectionFact));
    TestTrue(TEXT("Read fact persists"), Player->Objectives->HasFact(Document->ReadFact));
    if (!HasAnyErrors()) TestTrue(TEXT("Remove only the dedicated test file"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
    return !HasAnyErrors();
}
#endif
