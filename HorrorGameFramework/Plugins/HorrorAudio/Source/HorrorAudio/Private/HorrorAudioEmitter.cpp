#include "HorrorAudioEmitter.h"
#include "HorrorAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UHorrorAudioEmitterComponent::UHorrorAudioEmitterComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UHorrorAudioEmitterComponent::BeginPlay()
{
    Super::BeginPlay();
    Audio = GetWorld()->GetGameInstance()->GetSubsystem<UHorrorAudioSubsystem>();
    if (bAutoPlay && Definition) Play();
}
void UHorrorAudioEmitterComponent::Play()
{
    if (!Audio.IsValid() || !Definition) return;
    Stop();
    PlaybackHandle = Audio->PlaySound(this, Definition, GetComponentLocation(), this);
}
void UHorrorAudioEmitterComponent::Stop()
{
    if (Audio.IsValid()) Audio->StopSound(PlaybackHandle, false);
    PlaybackHandle.Invalidate();
}
void UHorrorAudioEmitterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop();
    Audio.Reset();
    Super::EndPlay(Reason);
}
AHorrorAudioEmitter::AHorrorAudioEmitter()
{
    PrimaryActorTick.bCanEverTick = false;
    Emitter = CreateDefaultSubobject<UHorrorAudioEmitterComponent>(TEXT("AudioEmitter"));
    RootComponent = Emitter;
}
