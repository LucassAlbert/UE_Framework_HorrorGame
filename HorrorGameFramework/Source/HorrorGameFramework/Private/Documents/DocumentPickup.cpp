#include "Documents/DocumentPickup.h"
#include "Documents/DocumentGameplayComponent.h"
#include "DocumentDefinition.h"
#include "InteractableComponent.h"
#include "Components/SaveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogDocumentPickup, Log, All);

ADocumentPickup::ADocumentPickup()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DocumentMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}

void ADocumentPickup::BeginPlay() { Super::BeginPlay(); RefreshPresentation(); }
void ADocumentPickup::SetCollected(bool bValue) { bCollected = bValue; RefreshPresentation(); }

void ADocumentPickup::RefreshPresentation()
{
    SetActorHiddenInGame(bCollected);
    SetActorEnableCollision(!bCollected);
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedDocument = Document.Get();
    if (Document.IsNull()) return;
    if (LoadedDocument) FinishLoading();
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Document.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &ADocumentPickup::FinishLoading));
}

void ADocumentPickup::FinishLoading()
{
    LoadedDocument = Document.Get();
    FText Error;
    if (!LoadedDocument || !LoadedDocument->Validate(Error))
        UE_LOG(LogDocumentPickup, Error, TEXT("Invalid document on %s (%s): %s"),
            *GetName(), *Document.ToString(), *Error.ToString());
}

bool ADocumentPickup::CanInteract_Implementation() const
{
    FText Error;
    return !bCollected && LoadedDocument && LoadedDocument == Document.Get() && LoadedDocument->Validate(Error);
}

void ADocumentPickup::Interact_Implementation(AActor* Interactor)
{
    if (!IsValid(Interactor) || !CanInteract_Implementation()) return;
    AActor* User = Interactor;
    if (AController* Controller = Cast<AController>(Interactor)) User = Controller->GetPawn();
    if (User)
        if (UDocumentGameplayComponent* Gameplay = User->FindComponentByClass<UDocumentGameplayComponent>())
            Gameplay->CollectDocument(this);
}

FText ADocumentPickup::GetInteractionText_Implementation() const
{
    return LoadedDocument ? FText::Format(NSLOCTEXT("Documents", "Pickup", "Ler e coletar: {0}"), LoadedDocument->Title) : FText::GetEmpty();
}

void ADocumentPickup::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (!Document.IsNull()) Paths.AddUnique(Document.ToSoftObjectPath());
}
bool ADocumentPickup::ValidateSaveState(FString& Error) const
{
    FText Detail;
    if (!Document.Get() || !Document.Get()->Validate(Detail))
    {
        Error = FString::Printf(TEXT("Invalid document pickup %s: %s"), *GetName(), *Detail.ToString());
        return false;
    }
    return true;
}
void ADocumentPickup::OnSaveStateRestored() { RefreshPresentation(); }
void ADocumentPickup::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedDocument = nullptr;
    Super::EndPlay(Reason);
}
