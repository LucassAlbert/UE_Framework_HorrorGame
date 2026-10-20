#include "Dialogue/DialogueNPC.h"
#include "Dialogue/DialogueGameplayComponent.h"
#include "DialogueDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SaveComponent.h"
#include "InteractableComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogueNPC, Log, All);

ADialogueNPC::ADialogueNPC()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NPCMesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    LookTarget = CreateDefaultSubobject<USceneComponent>(TEXT("LookTarget"));
    LookTarget->SetupAttachment(RootComponent);
    LookTarget->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
    SaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
}
void ADialogueNPC::BeginPlay() { Super::BeginPlay(); LoadDefinition(); }
void ADialogueNPC::LoadDefinition()
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedDefinition = Dialogue.Get();
    if (Dialogue.IsNull()) return;
    if (LoadedDefinition) FinishLoading();
    else LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Dialogue.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &ADialogueNPC::FinishLoading));
}
void ADialogueNPC::FinishLoading()
{
    LoadedDefinition = Dialogue.Get();
    FText Error;
    if (!LoadedDefinition || !LoadedDefinition->Validate(Error))
        UE_LOG(LogDialogueNPC, Error, TEXT("Configure dialogue on %s (%s): %s"), *GetName(), *Dialogue.ToString(), *Error.ToString());
}
bool ADialogueNPC::CanInteract_Implementation() const
{
    FText Error;
    if (!LoadedDefinition || LoadedDefinition != Dialogue.Get() || !LoadedDefinition->Validate(Error)) return false;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    const UDialogueGameplayComponent* Gameplay = Pawn ? Pawn->FindComponentByClass<UDialogueGameplayComponent>() : nullptr;
    return Gameplay && Gameplay->CanStart(this);
}
void ADialogueNPC::Interact_Implementation(AActor* Interactor)
{
    AActor* User = Interactor;
    if (AController* Controller = Cast<AController>(Interactor)) User = Controller->GetPawn();
    if (IsValid(User))
        if (UDialogueGameplayComponent* Gameplay = User->FindComponentByClass<UDialogueGameplayComponent>()) Gameplay->StartDialogue(this);
}
FText ADialogueNPC::GetInteractionText_Implementation() const
{
    return LoadedDefinition ? FText::Format(NSLOCTEXT("Dialogue", "Talk", "Conversar com {0}"), LoadedDefinition->SpeakerName) : FText::GetEmpty();
}
void ADialogueNPC::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    if (!Dialogue.IsNull()) Paths.AddUnique(Dialogue.ToSoftObjectPath());
}
bool ADialogueNPC::ValidateSaveState(FString& Error) const
{
    FText Detail;
    if (!Dialogue.Get() || !Dialogue.Get()->Validate(Detail))
    {
        Error = FString::Printf(TEXT("Invalid dialogue on %s: %s"), *GetName(), *Detail.ToString());
        return false;
    }
    return true;
}
void ADialogueNPC::OnSaveStateRestored() { LoadDefinition(); }
void ADialogueNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    LoadedDefinition = nullptr;
    Super::EndPlay(Reason);
}
