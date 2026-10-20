#include "HorrorAudioSubsystem.h"
#include "HorrorSoundDefinition.h"
#include "AudioPreferenceStore.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "AudioDeviceManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorAudio, Log, All);

FString UHorrorAudioSubsystem::GetPreferencesPath()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("HorrorAudioPreferences.json"));
}
void UHorrorAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PreferencesPath = GetPreferencesPath();
    FString Error;
    if (IFileManager::Get().FileExists(*PreferencesPath)
        && !HorrorAudioPreferences::Load(PreferencesPath, AppliedVolumes, Error))
        UE_LOG(LogHorrorAudio, Warning, TEXT("%s File retained: %s"), *Error, *PreferencesPath);
    CurrentVolumes = AppliedVolumes;
    for (uint8 Index = 1; Index <= static_cast<uint8>(EHorrorAudioChannel::Music); ++Index)
        ChannelClasses.Add(static_cast<EHorrorAudioChannel>(Index), NewObject<USoundClass>(this));
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UHorrorAudioSubsystem::HandleWorldCleanup);
}
void UHorrorAudioSubsystem::Deinitialize()
{
    bShuttingDown = true;
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    StopAllSounds();
    EditOwner.Reset();
    bEditingVolumes = false;
    if (FAudioDeviceManager* Devices = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
        for (const auto& Entry : ChannelClasses) Devices->UnregisterSoundClass(Entry.Value);
    ChannelClasses.Empty();
    Super::Deinitialize();
}
void UHorrorAudioSubsystem::DiscardAbandonedEdit()
{
    if (bEditingVolumes && !EditOwner.IsValid())
    {
        bEditingVolumes = false;
        CurrentVolumes = AppliedVolumes;
        UpdateVolumes();
    }
}
bool UHorrorAudioSubsystem::BeginVolumeEdit(UObject* Owner)
{
    DiscardAbandonedEdit();
    if (!IsValid(Owner) || bShuttingDown || (bEditingVolumes && EditOwner.Get() != Owner)) return false;
    EditOwner = Owner;
    bEditingVolumes = true;
    return true;
}
bool UHorrorAudioSubsystem::PreviewVolumes(UObject* Owner, const FHorrorAudioVolumes& Volumes)
{
    if (!bEditingVolumes || EditOwner.Get() != Owner || !IsValid(Owner) || !Volumes.Validate()) return false;
    CurrentVolumes = Volumes;
    UpdateVolumes();
    return true;
}
bool UHorrorAudioSubsystem::ApplyVolumes(UObject* Owner, FText& Error)
{
    if (!bEditingVolumes || !IsValid(Owner) || EditOwner.Get() != Owner)
    { Error = NSLOCTEXT("HorrorAudio", "NoEdit", "Esta tela não possui uma edição de áudio ativa."); return false; }
    FString FileError;
    if (!HorrorAudioPreferences::Save(PreferencesPath, CurrentVolumes, FileError))
    {
        UE_LOG(LogHorrorAudio, Warning, TEXT("%s Path: %s"), *FileError, *PreferencesPath);
        Error = NSLOCTEXT("HorrorAudio", "SaveFailed", "Não foi possível salvar os volumes. Tente novamente.");
        return false;
    }
    AppliedVolumes = CurrentVolumes;
    OnVolumesChanged.Broadcast();
    return true;
}
void UHorrorAudioSubsystem::CancelVolumeEdit(UObject* Owner)
{
    if (!bEditingVolumes || EditOwner.Get() != Owner) return;
    EditOwner.Reset();
    bEditingVolumes = false;
    CurrentVolumes = AppliedVolumes;
    UpdateVolumes();
}
void UHorrorAudioSubsystem::UpdateVolumes()
{
    for (auto& Entry : Playbacks)
        if (IsValid(Entry.Value.Component))
            Entry.Value.Component->SetVolumeMultiplier(Entry.Value.Definition->Volume * CurrentVolumes.GetPlaybackGain(Entry.Value.Definition->Channel));
    OnVolumesChanged.Broadcast();
}
bool UHorrorAudioSubsystem::HasPlayback(FGuid Handle) const { return Playbacks.Contains(Handle); }

FGuid UHorrorAudioSubsystem::PlaySound(UObject* WorldContextObject, UHorrorSoundDefinition* Definition, FVector Location, USceneComponent* AttachTo)
{
    FText Error;
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (bShuttingDown || !World || World->bIsTearingDown || World->GetGameInstance() != GetGameInstance()) return FGuid();
    if (!IsValid(Definition) || !Definition->Validate(Error) || Location.ContainsNaN()
        || (AttachTo && (!IsValid(AttachTo) || AttachTo->GetWorld() != World || !IsValid(AttachTo->GetOwner()))))
    {
        UE_LOG(LogHorrorAudio, Warning, TEXT("Playback request rejected (%s): %s"), *GetNameSafe(Definition), *Error.ToString());
        return FGuid();
    }
    DiscardAbandonedEdit();
    const FGuid Handle = FGuid::NewGuid();
    FHorrorAudioPlayback Playback;
    Playback.Definition = Definition;
    Playback.World = World;
    Playback.Location = Location;
    Playback.bAttached = AttachTo != nullptr;
    Playback.Attachment = AttachTo;
    Playback.AttachmentOwner = AttachTo ? AttachTo->GetOwner() : nullptr;
    Playbacks.Add(Handle, Playback);
    if (AActor* Owner = Playback.AttachmentOwner.Get()) Owner->OnEndPlay.AddUniqueDynamic(this, &UHorrorAudioSubsystem::HandleAttachmentEndPlay);
    TArray<FSoftObjectPath> Paths{ Definition->Sound.ToSoftObjectPath() };
    if (Definition->bSpatialized && Definition->bUseAttenuationAsset) Paths.AddUnique(Definition->Attenuation.ToSoftObjectPath());
    TSharedPtr<FStreamableHandle> Load = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateUObject(this, &UHorrorAudioSubsystem::FinishLoading, Handle));
    if (Playbacks.Contains(Handle))
    {
        if (Load) PendingLoads.Add(Handle, Load);
        else FailPlayback(Handle, NSLOCTEXT("HorrorAudio", "LoadRequestFailed", "Não foi possível carregar o som."));
    }
    return Handle;
}
void UHorrorAudioSubsystem::FinishLoading(FGuid Handle)
{
    FHorrorAudioPlayback* Playback = Playbacks.Find(Handle);
    if (!Playback) return;
    UWorld* World = Playback->World.Get();
    if (bShuttingDown || !World || World->bIsTearingDown
        || (Playback->bAttached && (!Playback->Attachment.IsValid() || !Playback->AttachmentOwner.IsValid())))
    { RemovePlayback(Handle); return; }
    UHorrorSoundDefinition* Definition = Playback->Definition;
    FText Error;
    if (!Definition->Validate(Error) || !Definition->Sound.IsValid()
        || (Definition->bSpatialized && Definition->bUseAttenuationAsset && !Definition->Attenuation.IsValid()))
    { FailPlayback(Handle, NSLOCTEXT("HorrorAudio", "MissingAsset", "O som ou sua atenuação não foram carregados.")); return; }
    FAudioDeviceHandle Device = World->GetAudioDevice();
    if (!Device.IsValid())
    { FailPlayback(Handle, NSLOCTEXT("HorrorAudio", "MissingDevice", "Não há dispositivo de áudio disponível neste mundo.")); return; }
    UAudioComponent* Component = NewObject<UAudioComponent>(World);
    Component->bAutoActivate = false;
    Component->bAutoDestroy = false;
    Component->bStopWhenOwnerDestroyed = true;
    Component->bShouldRemainActiveIfDropped = true;
    Component->SoundClassOverride = ChannelClasses.FindRef(Definition->Channel);
    Device->RegisterSoundClass(Component->SoundClassOverride);
    Component->SetUISound(Definition->PausePolicy == EHorrorAudioPausePolicy::ContinueWhilePaused);
    Component->bAllowSpatialization = Definition->bSpatialized;
    if (Definition->bSpatialized)
    {
        if (Definition->bUseAttenuationAsset) Component->AttenuationSettings = Definition->Attenuation.Get();
        else Component->AdjustAttenuation(Definition->SpatialSettings);
    }
    Component->SetSound(Definition->Sound.Get());
    Component->SetVolumeMultiplier(Definition->Volume * CurrentVolumes.GetPlaybackGain(Definition->Channel));
    Component->SetPitchMultiplier(Definition->Pitch);
    if (Playback->bAttached) Component->SetupAttachment(Playback->Attachment.Get());
    else Component->SetWorldLocation(Playback->Location);
    Component->RegisterComponentWithWorld(World);
    Component->OnAudioFinishedNative.AddUObject(this, &UHorrorAudioSubsystem::HandleAudioFinished);
    Playback->Component = Component;
    Component->FadeIn(Definition->FadeInSeconds, 1.f);
    PendingLoads.Remove(Handle);
    OnPlaybackStarted.Broadcast(Handle);
}
void UHorrorAudioSubsystem::FailPlayback(FGuid Handle, const FText& Error)
{
    UE_LOG(LogHorrorAudio, Warning, TEXT("Playback %s failed: %s"), *Handle.ToString(), *Error.ToString());
    RemovePlayback(Handle);
    OnPlaybackFailed.Broadcast(Handle, Error);
}
void UHorrorAudioSubsystem::RemovePlayback(FGuid Handle)
{
    FHorrorAudioPlayback Playback;
    if (!Playbacks.RemoveAndCopyValue(Handle, Playback)) return;
    TSharedPtr<FStreamableHandle> Load;
    if (PendingLoads.RemoveAndCopyValue(Handle, Load) && Load) Load->CancelHandle();
    if (UAudioComponent* Component = Playback.Component.Get(); IsValid(Component))
    {
        Component->OnAudioFinishedNative.RemoveAll(this);
        Component->Stop();
        Component->DestroyComponent();
    }
    if (AActor* Owner = Playback.AttachmentOwner.Get())
    {
        bool bStillUsed = false;
        for (const auto& Entry : Playbacks) if (Entry.Value.AttachmentOwner.Get() == Owner) { bStillUsed = true; break; }
        if (!bStillUsed) Owner->OnEndPlay.RemoveDynamic(this, &UHorrorAudioSubsystem::HandleAttachmentEndPlay);
    }
}
void UHorrorAudioSubsystem::StopSound(FGuid Handle, bool bFadeOut)
{
    const FHorrorAudioPlayback* Playback = Playbacks.Find(Handle);
    if (!Playback) return;
    if (bFadeOut && IsValid(Playback->Component) && Playback->Component->IsPlaying() && Playback->Definition->FadeOutSeconds > 0.f)
        Playback->Component->FadeOut(Playback->Definition->FadeOutSeconds, 0.f);
    else RemovePlayback(Handle);
}
void UHorrorAudioSubsystem::StopAllSounds()
{
    TArray<FGuid> Handles;
    Playbacks.GetKeys(Handles);
    for (const FGuid& Handle : Handles) RemovePlayback(Handle);
}
void UHorrorAudioSubsystem::HandleAudioFinished(UAudioComponent* Component)
{
    FGuid Handle;
    for (const auto& Entry : Playbacks) if (Entry.Value.Component == Component) { Handle = Entry.Key; break; }
    if (Handle.IsValid()) RemovePlayback(Handle);
}
void UHorrorAudioSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    TArray<FGuid> Handles;
    for (const auto& Entry : Playbacks) if (Entry.Value.World.Get() == World) Handles.Add(Entry.Key);
    for (const FGuid& Handle : Handles) RemovePlayback(Handle);
}
void UHorrorAudioSubsystem::HandleAttachmentEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
    TArray<FGuid> Handles;
    for (const auto& Entry : Playbacks) if (Entry.Value.AttachmentOwner.Get() == Actor) Handles.Add(Entry.Key);
    for (const FGuid& Handle : Handles) RemovePlayback(Handle);
}
