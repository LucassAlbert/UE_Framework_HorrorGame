#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HorrorEventDefinition.h"
#include "Interfaces/InteractionInterface.h"
#include "Interfaces/SaveStateListener.h"
#include "FrameworkEventActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UHorrorEventComponent;
class USaveComponent;
class UInteractableComponent;
class UHorrorNoiseEmitterComponent;
class UObjectiveJournalComponent;
class UInventoryComponent;
class APlayerController;
class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EFrameworkEventTrigger : uint8 { Volume, Interaction, Progression };

UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkEventActor : public AActor, public IInteractionInterface, public ISaveStateListener
{
    GENERATED_BODY()
public:
    AFrameworkEventActor();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") EFrameworkEventTrigger Trigger = EFrameworkEventTrigger::Volume;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FText InteractionText;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UHorrorEventComponent> Event;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UBoxComponent> Volume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<USaveComponent> Save;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UHorrorNoiseEmitterComponent> Noise;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override { return InteractionText; }
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override;
    virtual void OnSaveStateRestored() override;
    void TrackPlayback(FGuid Handle);
    bool TryTrigger(AActor* InstigatorActor);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool IsOperational() const;
    void BindPlayer(APawn* Pawn);
    void BindController(APlayerController* PlayerController);
    void HandleActorSpawned(AActor* Actor);
    UFUNCTION() void EvaluateAutomatic();
    UFUNCTION() void HandleLoad(int32 Slot, bool bSuccess, FText Message);
    UFUNCTION() void HandleEntry(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* Other, int32 Body, bool bSweep, const FHitResult& Hit);
    UFUNCTION() void StopEventSounds();
    TWeakObjectPtr<APawn> PlayerPawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UObjectiveJournalComponent> Journal;
    TWeakObjectPtr<UInventoryComponent> Inventory;
    TArray<FGuid> Playbacks;
    FDelegateHandle PawnChangedHandle;
    FDelegateHandle ActorSpawnedHandle;
};

USTRUCT(BlueprintType)
struct FFrameworkEventPropState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bMeshVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bLightEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator MeshRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector MeshOffset = FVector::ZeroVector;
};

// A save participant for event-controlled lights and moving presentation props.
UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkEventProp : public AActor, public IHorrorEventTarget, public ISaveStateListener
{
    GENERATED_BODY()
public:
    AFrameworkEventProp();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TMap<FName, FFrameworkEventPropState> States;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Event") FName CurrentState = TEXT("Default");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<UPointLightComponent> Light;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> Save;
    virtual bool CanApplyEventState_Implementation(FName State, FText& Error) const override;
    virtual bool ApplyEventState_Implementation(FName State, bool bRestoring) override;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
};
