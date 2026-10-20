#include "Progression/ProgressionArea.h"
#include "ObjectiveJournalComponent.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SaveComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SaveSubsystem.h"

AProgressionArea::AProgressionArea()
{
    PrimaryActorTick.bCanEverTick = false;
    Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
    SetRootComponent(Volume);
    Volume->SetBoxExtent(FVector(100.f));
    Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
    Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Volume->SetGenerateOverlapEvents(true);
    Volume->OnComponentBeginOverlap.AddDynamic(this, &AProgressionArea::HandleEntry);
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
}

void AProgressionArea::HandleEntry(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (bVisited || EntryFact.IsNone() || !Pawn || !Pawn->IsPlayerControlled()) return;
    const USaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    // Actor transforms are restored before callbacks; their overlaps must not create new progress.
    if (Save && Save->IsRestoring()) return;
    const UProgressionGameplayComponent* Gameplay = OtherActor->FindComponentByClass<UProgressionGameplayComponent>();
    if (Gameplay && !Gameplay->bEnableObjectives) return;
    UObjectiveJournalComponent* Journal = OtherActor->FindComponentByClass<UObjectiveJournalComponent>();
    if (!Journal) return;
    // Mark the area before observers run; a snapshot taken by them sees the final state.
    bVisited = true;
    if (!Journal->HasFact(EntryFact) && !Journal->RecordFact(EntryFact)) bVisited = false;
}

void AProgressionArea::EndPlay(const EEndPlayReason::Type Reason)
{
    Volume->OnComponentBeginOverlap.RemoveDynamic(this, &AProgressionArea::HandleEntry);
    Super::EndPlay(Reason);
}
