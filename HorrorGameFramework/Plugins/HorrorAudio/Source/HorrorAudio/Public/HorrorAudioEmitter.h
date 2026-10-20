#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "HorrorAudioEmitter.generated.h"

class UHorrorSoundDefinition;
class UHorrorAudioSubsystem;

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class HORRORAUDIO_API UHorrorAudioEmitterComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UHorrorAudioEmitterComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio") TObjectPtr<UHorrorSoundDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio") bool bAutoPlay = true;
    UFUNCTION(BlueprintCallable, Category = "Audio") void Play();
    UFUNCTION(BlueprintCallable, Category = "Audio") void Stop();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FGuid PlaybackHandle;
    TWeakObjectPtr<UHorrorAudioSubsystem> Audio;
};

UCLASS()
class HORRORAUDIO_API AHorrorAudioEmitter : public AActor
{
    GENERATED_BODY()
public:
    AHorrorAudioEmitter();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio") TObjectPtr<UHorrorAudioEmitterComponent> Emitter;
};
