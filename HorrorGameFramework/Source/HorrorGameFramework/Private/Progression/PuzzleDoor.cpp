#include "Progression/PuzzleDoor.h"
#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"

APuzzleDoor::APuzzleDoor() { Puzzle = CreateDefaultSubobject<UItemPuzzleComponent>(TEXT("Puzzle")); }

void APuzzleDoor::BeginPlay()
{
    Super::BeginPlay();
    Puzzle->OnPuzzleChanged.AddUniqueDynamic(this, &APuzzleDoor::HandlePuzzleChanged);
    HandlePuzzleChanged(Puzzle->IsSolved(), false);
}

void APuzzleDoor::HandlePuzzleChanged(bool bSolved, bool bRestored)
{
    bLocked = !bSolved;
    if (bLocked) bOpen = false;
    // Solving releases the lock, but opening remains an explicit interaction.
    ApplyPresentation();
}

void APuzzleDoor::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (!Puzzle->Definition.IsNull()) Paths.AddUnique(Puzzle->Definition.ToSoftObjectPath());
}

bool APuzzleDoor::ValidateSaveState(FString& Error) const
{
    FText Detail;
    if (!Puzzle->ValidateState(Detail)) { Error = Detail.ToString(); return false; }
    if (bLocked == Puzzle->IsSolved() || (bLocked && bOpen))
    {
        Error = TEXT("Puzzle door state does not match its puzzle progress.");
        return false;
    }
    return true;
}

void APuzzleDoor::OnSaveStateRestored()
{
    FText Error;
    Puzzle->NotifyStateRestored(Error);
    HandlePuzzleChanged(Puzzle->IsSolved(), true);
}

void APuzzleDoor::EndPlay(const EEndPlayReason::Type Reason)
{
    Puzzle->OnPuzzleChanged.RemoveDynamic(this, &APuzzleDoor::HandlePuzzleChanged);
    Super::EndPlay(Reason);
}
