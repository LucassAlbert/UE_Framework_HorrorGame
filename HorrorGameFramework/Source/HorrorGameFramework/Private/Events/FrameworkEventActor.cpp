#include "Events/FrameworkEventActor.h"
#include "HorrorEventComponent.h"
#include "HorrorAudioSubsystem.h"
#include "HorrorNoiseEmitterComponent.h"
#include "ObjectiveJournalComponent.h"
#include "InventoryComponent.h"
#include "InteractableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SaveComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogFrameworkEvents, Log, All);

AFrameworkEventActor::AFrameworkEventActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
    SetRootComponent(Volume);
    Volume->SetBoxExtent(FVector(100, 300, 160));
    Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
    Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Volume->SetGenerateOverlapEvents(true);
    Event = CreateDefaultSubobject<UHorrorEventComponent>(TEXT("Event"));
    Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    Noise = CreateDefaultSubobject<UHorrorNoiseEmitterComponent>(TEXT("Noise"));
    Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
    InteractionText = NSLOCTEXT("Events", "Interact", "Investigar");
}
void AFrameworkEventActor::BeginPlay()
{
    Super::BeginPlay();
    Volume->SetCollisionResponseToChannel(ECC_GameTraceChannel1, Trigger == EFrameworkEventTrigger::Interaction ? ECR_Block : ECR_Ignore);
    Volume->OnComponentBeginOverlap.AddUniqueDynamic(this, &AFrameworkEventActor::HandleEntry);
    Event->OnReady.AddUniqueDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    Event->OnInterrupted.AddUniqueDynamic(this, &AFrameworkEventActor::StopEventSounds);
    auto* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    Saves->OnWorldReady.AddUniqueDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    Saves->OnLoadCompleted.AddUniqueDynamic(this, &AFrameworkEventActor::HandleLoad);
    ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AFrameworkEventActor::HandleActorSpawned));
    BindController(UGameplayStatics::GetPlayerController(this, 0));
    EvaluateAutomatic();
}
void AFrameworkEventActor::BindController(APlayerController* PlayerController)
{
    if (!PlayerController || Controller.Get() == PlayerController) return;
    if (Controller.IsValid()) Controller->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
    Controller = PlayerController;
    PawnChangedHandle = PlayerController->GetOnNewPawnNotifier().AddUObject(this, &AFrameworkEventActor::BindPlayer);
    BindPlayer(PlayerController->GetPawn());
}
void AFrameworkEventActor::HandleActorSpawned(AActor* Actor)
{
    if (auto* PC = Cast<APlayerController>(Actor); PC && PC->IsLocalController()) BindController(PC);
}
void AFrameworkEventActor::BindPlayer(APawn* Pawn)
{
    if (Journal.IsValid()) Journal->OnJournalChanged.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    PlayerPawn = Pawn;
    Journal = Pawn ? Pawn->FindComponentByClass<UObjectiveJournalComponent>() : nullptr;
    Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (Journal.IsValid()) Journal->OnJournalChanged.AddUniqueDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    if (Inventory.IsValid()) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    EvaluateAutomatic();
}
bool AFrameworkEventActor::IsOperational() const
{
    const auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    return Saves && Saves->IsWorldReady() && !Saves->IsBusy() && !Saves->IsRestoring() && GetWorld() && !GetWorld()->IsPaused();
}
bool AFrameworkEventActor::TryTrigger(AActor* InstigatorActor)
{
    const APawn* Pawn = Cast<APawn>(InstigatorActor);
    if (!IsOperational() || !Pawn || !Pawn->IsPlayerControlled()) return false;
    FText Error;
    return Event->TryStart(InstigatorActor, Error);
}
void AFrameworkEventActor::EvaluateAutomatic()
{
    if (!PlayerPawn.IsValid() || !IsOperational()) return;
    if (Trigger == EFrameworkEventTrigger::Progression || (Trigger == EFrameworkEventTrigger::Volume && Volume->IsOverlappingActor(PlayerPawn.Get())))
        TryTrigger(PlayerPawn.Get());
}
void AFrameworkEventActor::HandleEntry(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* Other, int32 Body, bool bSweep, const FHitResult& Hit)
{
    if (Trigger == EFrameworkEventTrigger::Volume) TryTrigger(Actor);
}
void AFrameworkEventActor::HandleLoad(int32 Slot, bool bSuccess, FText Message)
{
    if (bSuccess)
    {
        BindController(UGameplayStatics::GetPlayerController(this, 0));
        EvaluateAutomatic();
    }
}
bool AFrameworkEventActor::CanInteract_Implementation() const
{
    if (Trigger != EFrameworkEventTrigger::Interaction || !IsOperational()) return false;
    FText Error;
    return Event->CanStart(UGameplayStatics::GetPlayerPawn(this, 0), Error);
}
void AFrameworkEventActor::Interact_Implementation(AActor* Interactor) { if (Trigger == EFrameworkEventTrigger::Interaction) TryTrigger(Interactor); }
bool AFrameworkEventActor::ValidateSaveState(FString& Error) const
{
    FText Detail;
    bool bValid = Event->ValidateState(Detail);
    if (bValid && Event->Progress.State == EHorrorEventState::Running)
        bValid = Event->ValidateActions(UGameplayStatics::GetPlayerPawn(this, 0), Detail);
    Error = Detail.ToString();
    return bValid;
}
void AFrameworkEventActor::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    Event->GatherAssets(Paths);
}
void AFrameworkEventActor::OnSaveStateRestored()
{
    FText Error;
    if (!Event->NotifyStateRestored(UGameplayStatics::GetPlayerPawn(this, 0), Error))
        UE_LOG(LogFrameworkEvents, Error, TEXT("Unable to restore %s: %s"), *GetName(), *Error.ToString());
}
void AFrameworkEventActor::TrackPlayback(FGuid Handle)
{
    auto* Audio = GetGameInstance()->GetSubsystem<UHorrorAudioSubsystem>();
    Playbacks.RemoveAll([Audio](FGuid Existing) { return !Audio->HasPlayback(Existing); });
    Playbacks.Add(Handle);
}
void AFrameworkEventActor::StopEventSounds()
{
    if (GetGameInstance())
    {
        auto* Audio = GetGameInstance()->GetSubsystem<UHorrorAudioSubsystem>();
        for (FGuid Handle : Playbacks) Audio->StopSound(Handle, false);
    }
    Playbacks.Reset();
}
void AFrameworkEventActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Journal.IsValid()) Journal->OnJournalChanged.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    if (Controller.IsValid()) Controller->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
    GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    if (GetGameInstance())
    {
        auto* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
        Saves->OnWorldReady.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
        Saves->OnLoadCompleted.RemoveDynamic(this, &AFrameworkEventActor::HandleLoad);
    }
    Event->OnReady.RemoveDynamic(this, &AFrameworkEventActor::EvaluateAutomatic);
    Event->OnInterrupted.RemoveDynamic(this, &AFrameworkEventActor::StopEventSounds);
    Volume->OnComponentBeginOverlap.RemoveDynamic(this, &AFrameworkEventActor::HandleEntry);
    StopEventSounds();
    Super::EndPlay(Reason);
}

AFrameworkEventProp::AFrameworkEventProp()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(GetRootComponent());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
    Light->SetupAttachment(GetRootComponent());
    Light->SetIntensity(350.f);
    Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
    States.Add(TEXT("Default"), FFrameworkEventPropState());
}
void AFrameworkEventProp::BeginPlay()
{
    Super::BeginPlay();
    ApplyEventState_Implementation(CurrentState, true);
}
bool AFrameworkEventProp::CanApplyEventState_Implementation(FName State, FText& Error) const
{
    const auto* Value = States.Find(State);
    if (Value && !Value->MeshOffset.ContainsNaN() && !Value->MeshRotation.ContainsNaN()) return true;
    Error = FText::Format(NSLOCTEXT("Events", "PropStateMissing", "O objeto não possui um estado válido chamado {0}."), FText::FromName(State));
    return false;
}
bool AFrameworkEventProp::ApplyEventState_Implementation(FName State, bool bRestoring)
{
    FText Error;
    if (!CanApplyEventState_Implementation(State, Error)) return false;
    CurrentState = State;
    const auto& Value = States.FindChecked(State);
    Mesh->SetVisibility(Value.bMeshVisible, true);
    Mesh->SetRelativeLocationAndRotation(Value.MeshOffset, Value.MeshRotation);
    Light->SetVisibility(Value.bLightEnabled, true);
    return true;
}
bool AFrameworkEventProp::ValidateSaveState(FString& Error) const
{
    FText Detail;
    const bool bValid = CanApplyEventState_Implementation(CurrentState, Detail);
    Error = Detail.ToString();
    return bValid;
}
void AFrameworkEventProp::OnSaveStateRestored() { ApplyEventState_Implementation(CurrentState, true); }
