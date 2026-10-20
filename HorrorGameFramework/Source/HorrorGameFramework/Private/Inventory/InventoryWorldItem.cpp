#include "Inventory/InventoryWorldItem.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryWorldItem, Log, All);

AInventoryWorldItem::AInventoryWorldItem()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}

void AInventoryWorldItem::BeginPlay()
{
    Super::BeginPlay();
    RefreshPresentation();
}

void AInventoryWorldItem::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    Super::EndPlay(Reason);
}

void AInventoryWorldItem::RefreshPresentation()
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedItem = Item.Get();
    SetActorHiddenInGame(bCollected);
    SetActorEnableCollision(!bCollected);
    if (bCollected || Item.IsNull()) return;
    if (!LoadedItem)
    {
        LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Item.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this]()
            {
                if (Item.Get()) RefreshPresentation();
                else UE_LOG(LogInventoryWorldItem, Error, TEXT("Missing item definition: %s"), *Item.ToString());
            }));
        return;
    }
    if (LoadedItem->Mesh.IsNull()) return;
    if (UStaticMesh* LoadedMesh = LoadedItem->Mesh.Get())
    {
        Mesh->SetStaticMesh(LoadedMesh);
        Mesh->SetWorldScale3D(FVector(LoadedItem->WorldScale));
        OnPresentationReady();
    }
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(LoadedItem->Mesh.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [this]()
        {
            if (LoadedItem && LoadedItem->Mesh.Get()) RefreshPresentation();
            else UE_LOG(LogInventoryWorldItem, Error, TEXT("Missing mesh for %s."), *GetNameSafe(LoadedItem));
        }));
}

bool AInventoryWorldItem::CanInteract_Implementation() const
{
    return !bCollected && IsValid(LoadedItem) && Quantity > 0;
}

void AInventoryWorldItem::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation() || !IsValid(Interactor)) return;
    AController* Controller = Cast<AController>(Interactor);
    AActor* User = Controller ? Controller->GetPawn() : Interactor;
    UInventoryComponent* Inventory = User ? User->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) return;
    const FInventoryResult Result = Inventory->AddItem(LoadedItem, Quantity);
    if (UInventoryGameplayComponent* Gameplay = User->FindComponentByClass<UInventoryGameplayComponent>())
        Gameplay->ReportResult(Result);
    if (!Result.bSuccess) return;
    bCollected = true;
    RefreshPresentation();
    // Authored pickups remain available for rewind; runtime drops are reconstructed from their snapshot.
    if (SaveComponent->bRecreateOnLoad) Destroy();
}

FText AInventoryWorldItem::GetInteractionText_Implementation() const
{
    return LoadedItem ? FText::Format(NSLOCTEXT("Inventory", "Pickup", "Pegar {0}"), LoadedItem->DisplayName) : FText::GetEmpty();
}

void AInventoryWorldItem::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (!Item.IsNull()) Paths.AddUnique(Item.ToSoftObjectPath());
    if (LoadedItem && !LoadedItem->Mesh.IsNull()) Paths.AddUnique(LoadedItem->Mesh.ToSoftObjectPath());
}

void AInventoryWorldItem::OnSaveStateRestored() { RefreshPresentation(); }

bool AInventoryWorldItem::ValidateSaveState(FString& Error) const
{
    const UInventoryItemDefinition* Definition = Item.Get();
    if (!Definition || Definition->ItemId.IsNone() || Quantity < 1)
    { Error = TEXT("World item has missing definition or invalid quantity."); return false; }
    return true;
}
