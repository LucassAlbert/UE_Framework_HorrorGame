#include "Documents/DocumentGameplayComponent.h"
#include "Documents/DocumentPickup.h"
#include "Documents/DocumentScreens.h"
#include "DocumentCollectionComponent.h"
#include "DocumentDefinition.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "Inventory/InventoryScreen.h"
#include "Progression/ProgressionGameplayComponent.h"
#include "ObjectiveJournalComponent.h"
#include "InteractableComponent.h"
#include "UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogDocumentGameplay, Log, All);

UDocumentGameplayComponent::UDocumentGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    CollectionScreenClass = UDocumentCollectionScreen::StaticClass();
    ReaderScreenClass = UDocumentReaderScreen::StaticClass();
}
UDocumentCollectionComponent* UDocumentGameplayComponent::GetCollection() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UDocumentCollectionComponent>() : nullptr;
}
UUIManagerSubsystem* UDocumentGameplayComponent::GetUI() const
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    return PC && PC->GetLocalPlayer() && PC->GetGameInstance()
        ? PC->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
}

void UDocumentGameplayComponent::BeginPlay()
{
    Super::BeginPlay();
    UDocumentCollectionComponent* Collection = GetCollection();
    if (!bEnableDocuments || !Collection) return;
    if (UInventoryGameplayComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryGameplayComponent>())
        Inventory->OnDocumentsRequested.AddUniqueDynamic(this, &UDocumentGameplayComponent::OpenCollection);
    Collection->OnDocumentCollected.AddUniqueDynamic(this, &UDocumentGameplayComponent::HandleCollected);
    Collection->OnDocumentRead.AddUniqueDynamic(this, &UDocumentGameplayComponent::HandleRead);
}

bool UDocumentGameplayComponent::ValidateTarget(ADocumentPickup* Pickup) const
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !IsValid(Pickup) || !Pickup->CanInteract_Implementation()) return false;
    FVector Origin;
    FRotator Rotation;
    PC->GetPlayerViewPoint(Origin, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DocumentPickup), false, GetOwner());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin,
        Origin + Rotation.Vector() * Pickup->Interactable->InteractionDistance, ECC_GameTraceChannel1, Query)
        || Hit.GetActor() != Pickup) return false;
    FHitResult Obstruction;
    return !GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, Hit.ImpactPoint, ECC_Visibility, Query)
        || Obstruction.GetActor() == Pickup;
}

bool UDocumentGameplayComponent::CollectDocument(ADocumentPickup* Pickup)
{
    UDocumentCollectionComponent* Collection = GetCollection();
    if (!bEnableDocuments || bCollecting || !Collection || !ValidateTarget(Pickup)) return false;
    UUIManagerSubsystem* UI = GetUI();
    if (UI && UI->HasModalScreen()) return false;
    UDocumentDefinition* Definition = Pickup->Document.Get();
    TGuardValue<bool> Collecting(bCollecting, true);
    // No save may capture the world pickup and the collection halfway through acquisition.
    Pickup->SetCollected(true);
    FText Error;
    if (!Collection->Contains(Definition) && !Collection->Collect(Definition, Error))
    {
        Pickup->SetCollected(false);
        UE_LOG(LogDocumentGameplay, Warning, TEXT("Cannot collect %s: %s"), *GetNameSafe(Definition), *Error.ToString());
        return false;
    }
    // A headless player can collect without a viewport; opening requires a local UI owner.
    if (UI && !OpenReader(Definition))
        UE_LOG(LogDocumentGameplay, Error, TEXT("Collected %s, but cannot open its reader. It remains in the collection."), *GetNameSafe(Definition));
    return true;
}

void UDocumentGameplayComponent::OpenCollection()
{
    UUIManagerSubsystem* UI = GetUI();
    FText Error;
    UDocumentCollectionComponent* Collection = GetCollection();
    if (!bEnableDocuments || !UI || !Collection || !Collection->ValidateState(Error) || !CollectionScreenClass) return;
    if (UInventoryScreen* Inventory = Cast<UInventoryScreen>(UI->GetTopScreen())) ParentScreen = Inventory;
    UDocumentCollectionScreen* Screen = Cast<UDocumentCollectionScreen>(UI->PushScreen(CollectionScreenClass));
    if (!Screen) { UE_LOG(LogDocumentGameplay, Error, TEXT("Cannot create document collection screen.")); return; }
    CollectionScreen = Screen;
    Screen->SetSource(this);
}

bool UDocumentGameplayComponent::OpenReader(UDocumentDefinition* Document)
{
    UUIManagerSubsystem* UI = GetUI();
    UDocumentCollectionComponent* Collection = GetCollection();
    FText Error;
    if (!bEnableDocuments || !UI || !Collection || !Collection->Contains(Document) || !Document->Validate(Error) || !ReaderScreenClass) return false;
    UDocumentReaderScreen* Screen = Cast<UDocumentReaderScreen>(UI->PushScreen(ReaderScreenClass));
    if (!Screen) return false;
    ReaderScreen = Screen;
    Screen->SetDocument(Document);
    // Text is already presented; optional image loading does not delay reading progress.
    Collection->MarkRead(Document);
    return true;
}

void UDocumentGameplayComponent::RecordFact(FName Fact)
{
    if (Fact.IsNone()) return;
    const UProgressionGameplayComponent* Progression = GetOwner()->FindComponentByClass<UProgressionGameplayComponent>();
    if (Progression && !Progression->bEnableObjectives) return;
    if (UObjectiveJournalComponent* Journal = GetOwner()->FindComponentByClass<UObjectiveJournalComponent>())
        Journal->RecordFact(Fact);
}
void UDocumentGameplayComponent::HandleCollected(UDocumentDefinition* Document) { if (Document) RecordFact(Document->CollectionFact); }
void UDocumentGameplayComponent::HandleRead(UDocumentDefinition* Document) { if (Document) RecordFact(Document->ReadFact); }

void UDocumentGameplayComponent::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (UDocumentCollectionComponent* Collection = GetCollection())
        for (const FCollectedDocument& Entry : Collection->GetDocuments())
            if (!Entry.Definition.IsNull()) Paths.AddUnique(Entry.Definition.ToSoftObjectPath());
}
bool UDocumentGameplayComponent::ValidateSaveState(FString& Error) const
{
    if (bCollecting) { Error = TEXT("Cannot save during document acquisition."); return false; }
    if (UDocumentCollectionComponent* Collection = GetCollection())
    {
        FText Detail;
        if (!Collection->ValidateState(Detail)) { Error = Detail.ToString(); return false; }
    }
    return true;
}
void UDocumentGameplayComponent::CloseScreens()
{
    if (ReaderScreen.IsValid()) ReaderScreen->CloseScreen();
    if (CollectionScreen.IsValid()) CollectionScreen->CloseScreen();
    if (ParentScreen.IsValid()) ParentScreen->CloseScreen();
    ReaderScreen.Reset();
    CollectionScreen.Reset();
    ParentScreen.Reset();
}
void UDocumentGameplayComponent::OnSaveStateRestored()
{
    CloseScreens();
    if (UDocumentCollectionComponent* Collection = GetCollection())
    {
        FText Error;
        if (!Collection->NotifyStateRestored(Error)) UE_LOG(LogDocumentGameplay, Error, TEXT("Document restore failed: %s"), *Error.ToString());
    }
}
void UDocumentGameplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UInventoryGameplayComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryGameplayComponent>())
        Inventory->OnDocumentsRequested.RemoveDynamic(this, &UDocumentGameplayComponent::OpenCollection);
    if (UDocumentCollectionComponent* Collection = GetCollection())
    {
        Collection->OnDocumentCollected.RemoveDynamic(this, &UDocumentGameplayComponent::HandleCollected);
        Collection->OnDocumentRead.RemoveDynamic(this, &UDocumentGameplayComponent::HandleRead);
    }
    CloseScreens();
    Super::EndPlay(Reason);
}
