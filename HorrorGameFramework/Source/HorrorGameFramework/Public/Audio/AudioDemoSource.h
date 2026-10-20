#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractionInterface.h"
#include "AudioDemoSource.generated.h"

class UStaticMeshComponent;
class UHorrorAudioEmitterComponent;
class UInteractableComponent;

// Project-only sound test prop; reusable playback remains in HorrorAudio.
UCLASS()
class HORRORGAMEFRAMEWORK_API AAudioDemoSource : public AActor, public IInteractionInterface
{
    GENERATED_BODY()
public:
    AAudioDemoSource();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio") TObjectPtr<UHorrorAudioEmitterComponent> Emitter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction") TObjectPtr<UInteractableComponent> Interactable;
    virtual bool CanInteract_Implementation() const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
private:
    double NextUseTime = 0.0;
};
