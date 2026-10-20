#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Threat/FrameworkThreatCharacter.h"
#include "Threat/FrameworkThreatTarget.h"
#include "Threat/FrameworkHidingSpot.h"
#include "HideSpotComponent.h"
#include "HidingParticipantComponent.h"
#include "Components/SaveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Core/HorrorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorThreatWorldSaveTest, "HorrorFramework.Threat.HidingWorldSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorThreatWorldSaveTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone(TEXT("ThreatWorldSaveAutomation"));
    UWorld* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    AFrameworkThreatCharacter* Player = World->SpawnActor<AFrameworkThreatCharacter>(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
    AHorrorPlayerController* PC = World->SpawnActor<AHorrorPlayerController>();
    PC->PlayerState = World->SpawnActor<APlayerState>();
    PC->Possess(Player);
    AFrameworkHidingSpot* Spot = World->SpawnActor<AFrameworkHidingSpot>(FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator);
    Spot->Save->PersistentKey = TEXT("ThreatTestHideSpot");
    Spot->HideSpot->HiddenOffset = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 5.f));
    World->GetWorldSettings()->NotifyBeginPlay();
    USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
    Save->ActorRegistry->RegisterActor(Player);
    Save->ActorRegistry->RegisterActor(Spot);
    TestTrue(TEXT("Player enters an available nearby spot"), Player->Hiding->Enter(Spot->HideSpot));
    TestTrue(TEXT("Hiding claims occupancy and concealment"), Spot->HideSpot->IsOccupied() && Player->ThreatTarget->IsConcealed());
    TestTrue(TEXT("Hiding disables capsule collision"), Player->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    Player->Hiding->Look(FVector2D(9999.f, -9999.f));
    TestTrue(TEXT("Hidden yaw is clamped"), FMath::Abs(FMath::UnwindDegrees(PC->GetControlRotation().Yaw)) <= Spot->HideSpot->LookYawLimit + .01f);
    TestTrue(TEXT("Hidden pitch is clamped"), FMath::Abs(FMath::UnwindDegrees(PC->GetControlRotation().Pitch)) <= Spot->HideSpot->LookPitchLimit + .01f);
    USystemSaveGame* Snapshot = NewObject<USystemSaveGame>();
    FString Error;
    TestTrue(TEXT("Capture a hidden player"), Save->CaptureWorld(Snapshot, Error));
    TArray<uint8> Bytes;
    UGameplayStatics::SaveGameToMemory(Snapshot, Bytes);
    USystemSaveGame* Decoded = Cast<USystemSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    TestTrue(TEXT("Exit restores normal collision"), Player->Hiding->Exit() && Player->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Restore serialized concealment"), Save->RestoreWorld(Decoded, Error));
    TestTrue(TEXT("Restore reacquires the same spot"), Player->Hiding->GetSpot() == Spot->HideSpot && Player->ThreatTarget->IsConcealed() && Spot->HideSpot->IsOccupied());
    TestTrue(TEXT("Restored player can leave the spot"), Player->Hiding->Exit());
    TestTrue(TEXT("Exit after load returns to the original entry"), Player->GetActorLocation().Equals(FVector(0.f, 0.f, 100.f), .1f));
    return !HasAnyErrors();
}
#endif
