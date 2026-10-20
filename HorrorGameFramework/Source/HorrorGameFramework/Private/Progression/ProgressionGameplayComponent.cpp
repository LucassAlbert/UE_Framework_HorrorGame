#include "Progression/ProgressionGameplayComponent.h"
#include "Progression/ProgressionScreens.h"
#include "Progression/PuzzleSocket.h"
#include "Progression/PuzzleDoor.h"
#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"
#include "ObjectiveDefinition.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "Inventory/InventoryScreen.h"
#include "InteractableComponent.h"
#include "UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Templates/UnrealTemplate.h"

UProgressionGameplayComponent::UProgressionGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ItemSelectionClass = UPuzzleItemSelectionScreen::StaticClass();
    ObjectivesClass = UObjectivesScreen::StaticClass();
}

UInventoryComponent* UProgressionGameplayComponent::GetInventory() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}
UObjectiveJournalComponent* UProgressionGameplayComponent::GetJournal() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UObjectiveJournalComponent>() : nullptr;
}

void UProgressionGameplayComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bEnableObjectives)
    {
        if (UInventoryGameplayComponent* InventoryGameplay = GetOwner()->FindComponentByClass<UInventoryGameplayComponent>())
            InventoryGameplay->OnObjectivesRequested.AddUniqueDynamic(this, &UProgressionGameplayComponent::OpenObjectives);
        if (UObjectiveJournalComponent* Journal = GetJournal())
        {
            Journal->OnObjectiveChanged.AddUniqueDynamic(this, &UProgressionGameplayComponent::HandleObjectiveChanged);
            if (Journal->IsReady())
                for (const FObjectiveRecord& Record : Journal->GetRecords())
                    if (Record.State == EObjectiveState::Active) HandleObjectiveChanged(Record.Definition.Get(), Record.State);
        }
    }
}

void UProgressionGameplayComponent::OpenObjectives()
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    UUIManagerSubsystem* UI = PC ? PC->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    if (!bEnableObjectives || !UI || !GetJournal() || !ObjectivesClass) return;
    UInventoryScreen* Parent = Cast<UInventoryScreen>(UI->GetTopScreen());
    UObjectivesScreen* Screen = Cast<UObjectivesScreen>(UI->PushScreen(ObjectivesClass));
    if (Screen) { ObjectivesScreen = Screen; if (Parent) ObjectivesParentScreen = Parent; Screen->SetSource(GetJournal()); }
}

bool UProgressionGameplayComponent::ValidateTarget(APuzzleSocket* Socket) const
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !IsValid(Socket) || !Socket->CanInteract_Implementation()) return false;
    FVector Origin;
    FRotator Rotation;
    PC->GetPlayerViewPoint(Origin, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PuzzleSelection), false, GetOwner());
    FHitResult Hit;
    const float Distance = Socket->Interactable->InteractionDistance;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Rotation.Vector() * Distance,
        ECC_GameTraceChannel1, Query) || Hit.GetActor() != Socket) return false;
    FHitResult Obstruction;
    return !GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, Hit.ImpactPoint, ECC_Visibility, Query)
        || Obstruction.GetActor() == Socket;
}

void UProgressionGameplayComponent::OpenItemSelection(APuzzleSocket* Socket)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    UUIManagerSubsystem* UI = PC ? PC->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    if (!bEnablePuzzles || !UI || UI->HasModalScreen() || !ItemSelectionClass || !GetInventory() || !ValidateTarget(Socket)) return;
    SelectedSocket = Socket;
    UPuzzleItemSelectionScreen* Screen = Cast<UPuzzleItemSelectionScreen>(UI->PushScreen(ItemSelectionClass));
    if (Screen) { SelectionScreen = Screen; Screen->SetSource(this); }
    else SelectedSocket.Reset();
}

FInventoryResult UProgressionGameplayComponent::InsertItem(int32 SlotIndex)
{
    UInventoryComponent* Inventory = GetInventory();
    APuzzleSocket* Socket = SelectedSocket.Get();
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
    if (!bEnablePuzzles || bInserting || !Item || !Inventory->ValidateState().bSuccess || !ValidateTarget(Socket))
        return FInventoryResult::Failure(NSLOCTEXT("Puzzles", "InvalidTarget", "O item ou encaixe não está mais disponível ao alcance."));
    APuzzleDoor* Door = Socket->Door.Get();
    if (!Door) return FInventoryResult::Failure(NSLOCTEXT("Puzzles", "MissingPuzzle", "O puzzle não está disponível."));
    TGuardValue<bool> Transaction(bInserting, true);
    FText Error;
    if (!Door->Puzzle->BeginInsertion(Socket->SlotId, Item->ItemId, Error))
        return FInventoryResult::Failure(Error);
    const FInventoryResult Consumption = Inventory->RemoveFromSlot(SlotIndex, 1);
    if (!Consumption.bSuccess) { Door->Puzzle->CancelInsertion(); return Consumption; }
    Socket->SetInsertedItem(Item);
    Door->Puzzle->CommitInsertion();
    const UItemPuzzleDefinition* Definition = Door->Puzzle->GetDefinition();
    if (bEnableObjectives && Door->Puzzle->IsSolved() && Definition && !Definition->CompletionFact.IsNone())
        if (UObjectiveJournalComponent* Journal = GetJournal()) Journal->RecordFact(Definition->CompletionFact);
    if (SelectionScreen.IsValid()) SelectionScreen->CloseScreen();
    SelectedSocket.Reset();
    return FInventoryResult::Success();
}

void UProgressionGameplayComponent::HandleObjectiveChanged(UObjectiveDefinition* Definition, EObjectiveState State)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!bEnableObjectives || !PC || !PC->GetLocalPlayer() || !Definition || State == EObjectiveState::Inactive) return;
    if (!Notice.IsValid() || !Notice->IsInViewport())
    {
        UObjectiveNotice* Widget = CreateWidget<UObjectiveNotice>(PC);
        if (!Widget) return;
        Notice = Widget;
        Widget->AddToViewport(200);
    }
    const FText Message = State == EObjectiveState::Completed
        ? FText::Format(NSLOCTEXT("Objectives", "CompletedNotice", "Objetivo concluído: {0}"), Definition->Title)
        : FText::Format(NSLOCTEXT("Objectives", "ActivatedNotice", "Novo objetivo: {0}"), Definition->Title);
    Notice->Enqueue(Message);
}

void UProgressionGameplayComponent::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (UObjectiveJournalComponent* Journal = GetJournal())
        for (const auto& Definition : Journal->Definitions)
            if (!Definition.IsNull()) Paths.AddUnique(Definition.ToSoftObjectPath());
}

bool UProgressionGameplayComponent::ValidateSaveState(FString& Error) const
{
    if (bInserting) { Error = TEXT("Cannot save during a puzzle insertion transaction."); return false; }
    if (UObjectiveJournalComponent* Journal = GetJournal())
    {
        FText Detail;
        if (!Journal->ValidateState(Detail)) { Error = Detail.ToString(); return false; }
    }
    return true;
}

void UProgressionGameplayComponent::CloseScreens()
{
    if (SelectionScreen.IsValid()) SelectionScreen->CloseScreen();
    if (ObjectivesScreen.IsValid()) ObjectivesScreen->CloseScreen();
    if (ObjectivesParentScreen.IsValid()) ObjectivesParentScreen->CloseScreen();
    if (Notice.IsValid()) Notice->RemoveFromParent();
    SelectionScreen.Reset();
    ObjectivesScreen.Reset();
    ObjectivesParentScreen.Reset();
    Notice.Reset();
    SelectedSocket.Reset();
}

void UProgressionGameplayComponent::OnSaveStateRestored()
{
    CloseScreens();
    if (UObjectiveJournalComponent* Journal = GetJournal())
    {
        FText Error;
        Journal->NotifyStateRestored(Error);
    }
}

void UProgressionGameplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UInventoryGameplayComponent* InventoryGameplay = GetOwner()->FindComponentByClass<UInventoryGameplayComponent>())
        InventoryGameplay->OnObjectivesRequested.RemoveDynamic(this, &UProgressionGameplayComponent::OpenObjectives);
    if (UObjectiveJournalComponent* Journal = GetJournal())
        Journal->OnObjectiveChanged.RemoveDynamic(this, &UProgressionGameplayComponent::HandleObjectiveChanged);
    CloseScreens();
    Super::EndPlay(Reason);
}
