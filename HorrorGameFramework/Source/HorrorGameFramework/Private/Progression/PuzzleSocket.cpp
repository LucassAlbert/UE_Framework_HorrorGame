#include "Progression/PuzzleSocket.h"
#include "Progression/PuzzleDoor.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "ItemPuzzleComponent.h"
#include "ItemPuzzleDefinition.h"
#include "InventoryItemDefinition.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogPuzzleSocket, Log, All);

APuzzleSocket::APuzzleSocket()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
    Frame->SetupAttachment(GetRootComponent());
    Frame->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Frame->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InsertedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InsertedMesh"));
    InsertedMesh->SetupAttachment(GetRootComponent());
    InsertedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}

void APuzzleSocket::BeginPlay()
{
    Super::BeginPlay();
    BoundDoor = Door.Get();
    if (BoundDoor.IsValid()) BoundDoor->Puzzle->OnPuzzleChanged.AddUniqueDynamic(this, &APuzzleSocket::HandlePuzzleChanged);
    RefreshPresentation();
}

bool APuzzleSocket::CanInteract_Implementation() const
{
    const APuzzleDoor* Target = Door.Get();
    return Target && Target->Puzzle->IsReady() && Target->Puzzle->GetDefinition()->FindSlot(SlotId)
        && !Target->Puzzle->IsSlotFilled(SlotId) && !Target->Puzzle->IsSolved();
}

void APuzzleSocket::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation() || !IsValid(Interactor)) return;
    AActor* User = Interactor;
    if (const AController* Controller = Cast<AController>(Interactor)) User = Controller->GetPawn();
    if (User)
        if (UProgressionGameplayComponent* Gameplay = User->FindComponentByClass<UProgressionGameplayComponent>())
            Gameplay->OpenItemSelection(this);
}

FText APuzzleSocket::GetSlotName() const
{
    const APuzzleDoor* Target = Door.Get();
    const UItemPuzzleDefinition* Data = Target ? Target->Puzzle->GetDefinition() : nullptr;
    const FItemPuzzleSlot* Slot = Data ? Data->FindSlot(SlotId) : nullptr;
    return Slot && !Slot->DisplayName.IsEmpty() ? Slot->DisplayName : FText::FromName(SlotId);
}

FText APuzzleSocket::GetInteractionText_Implementation() const
{
    return FText::Format(NSLOCTEXT("Puzzles", "SelectItem", "Inserir item: {0}"), GetSlotName());
}

void APuzzleSocket::SetInsertedItem(UInventoryItemDefinition* Item)
{
    InsertedItem = Item;
    RefreshPresentation();
}

void APuzzleSocket::HandlePuzzleChanged(bool bSolved, bool bRestored) { RefreshPresentation(); }

void APuzzleSocket::RefreshPresentation()
{
    const uint64 Generation = ++PresentationGeneration;
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedItem = InsertedItem.Get();
    InsertedMesh->SetVisibility(false);
    if (InsertedItem.IsNull()) { InsertedMesh->SetStaticMesh(nullptr); return; }
    TArray<FSoftObjectPath> Paths;
    if (!LoadedItem) Paths.Add(InsertedItem.ToSoftObjectPath());
    else if (!LoadedItem->Mesh.Get() && !LoadedItem->Mesh.IsNull()) Paths.Add(LoadedItem->Mesh.ToSoftObjectPath());
    if (!Paths.IsEmpty())
    {
        LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
            FStreamableDelegate::CreateWeakLambda(this, [this, Generation]()
            {
                if (Generation != PresentationGeneration) return;
                if (InsertedItem.Get() && (InsertedItem.Get()->Mesh.Get() || !LoadedItem)) RefreshPresentation();
                else UE_LOG(LogPuzzleSocket, Error, TEXT("Cannot load inserted item presentation on %s."), *GetName());
            }));
        return;
    }
    if (LoadedItem && LoadedItem->Mesh.Get())
    {
        InsertedMesh->SetStaticMesh(LoadedItem->Mesh.Get());
        InsertedMesh->SetRelativeScale3D(FVector(LoadedItem->WorldScale));
        InsertedMesh->SetVisibility(true);
    }
}

void APuzzleSocket::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (!InsertedItem.IsNull()) Paths.AddUnique(InsertedItem.ToSoftObjectPath());
    if (UInventoryItemDefinition* Item = InsertedItem.Get(); Item && !Item->Mesh.IsNull()) Paths.AddUnique(Item->Mesh.ToSoftObjectPath());
}

bool APuzzleSocket::ValidateSaveState(FString& Error) const
{
    const APuzzleDoor* Target = Door.Get();
    const UItemPuzzleDefinition* Definition = Target ? Target->Puzzle->Definition.Get() : nullptr;
    const FItemPuzzleSlot* Slot = Definition ? Definition->FindSlot(SlotId) : nullptr;
    const UInventoryItemDefinition* Item = InsertedItem.Get();
    if (!Slot || Target->Puzzle->IsSlotFilled(SlotId) != !InsertedItem.IsNull()
        || (!InsertedItem.IsNull() && (!Item || Item->ItemId != Slot->RequiredItemId)))
    {
        Error = TEXT("Puzzle socket item does not match the saved puzzle state.");
        return false;
    }
    return true;
}

void APuzzleSocket::OnSaveStateRestored() { RefreshPresentation(); }

void APuzzleSocket::EndPlay(const EEndPlayReason::Type Reason)
{
    ++PresentationGeneration;
    if (BoundDoor.IsValid()) BoundDoor->Puzzle->OnPuzzleChanged.RemoveDynamic(this, &APuzzleSocket::HandlePuzzleChanged);
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedItem = nullptr;
    Super::EndPlay(Reason);
}
