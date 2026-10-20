#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"
#include "HorrorAudioTypes.h"
#include "HorrorAudioSubsystem.generated.h"

class UHorrorSoundDefinition;
class UAudioComponent;
class USceneComponent;
class USoundClass;
class AActor;
struct FStreamableHandle;

USTRUCT()
struct FHorrorAudioPlayback
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UHorrorSoundDefinition> Definition;
    UPROPERTY() TObjectPtr<UAudioComponent> Component;
    UPROPERTY() TWeakObjectPtr<UWorld> World;
    UPROPERTY() TWeakObjectPtr<USceneComponent> Attachment;
    UPROPERTY() TWeakObjectPtr<AActor> AttachmentOwner;
    FVector Location = FVector::ZeroVector;
    bool bAttached = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHorrorAudioVolumesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHorrorAudioPlaybackFailed, FGuid, Handle, FText, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHorrorAudioPlaybackStarted, FGuid, Handle);

UCLASS()
class HORRORAUDIO_API UHorrorAudioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // The returned handle identifies a pending or playing request. Failure is also logged/emitted asynchronously.
    UFUNCTION(BlueprintCallable, Category = "Horror Audio", meta = (WorldContext = "WorldContextObject"))
    FGuid PlaySound(UObject* WorldContextObject, UHorrorSoundDefinition* Definition, FVector Location,
        USceneComponent* AttachTo = nullptr);
    UFUNCTION(BlueprintCallable, Category = "Horror Audio") void StopSound(FGuid Handle, bool bFadeOut = true);
    UFUNCTION(BlueprintCallable, Category = "Horror Audio") void StopAllSounds();
    UFUNCTION(BlueprintPure, Category = "Horror Audio") bool HasPlayback(FGuid Handle) const;

    UFUNCTION(BlueprintPure, Category = "Horror Audio|Volumes") FHorrorAudioVolumes GetVolumes() const { return CurrentVolumes; }
    UFUNCTION(BlueprintPure, Category = "Horror Audio|Volumes") FHorrorAudioVolumes GetAppliedVolumes() const { return AppliedVolumes; }
    UFUNCTION(BlueprintCallable, Category = "Horror Audio|Volumes") bool BeginVolumeEdit(UObject* Owner);
    UFUNCTION(BlueprintCallable, Category = "Horror Audio|Volumes") bool PreviewVolumes(UObject* Owner, const FHorrorAudioVolumes& Volumes);
    UFUNCTION(BlueprintCallable, Category = "Horror Audio|Volumes") bool ApplyVolumes(UObject* Owner, FText& Error);
    UFUNCTION(BlueprintCallable, Category = "Horror Audio|Volumes") void CancelVolumeEdit(UObject* Owner);
    UFUNCTION(BlueprintPure, Category = "Horror Audio|Volumes") bool HasUnappliedVolumes() const { return !CurrentVolumes.Equals(AppliedVolumes); }

    UPROPERTY(BlueprintAssignable, Category = "Horror Audio") FHorrorAudioPlaybackStarted OnPlaybackStarted;
    UPROPERTY(BlueprintAssignable, Category = "Horror Audio") FHorrorAudioPlaybackFailed OnPlaybackFailed;
    UPROPERTY(BlueprintAssignable, Category = "Horror Audio|Volumes") FHorrorAudioVolumesChanged OnVolumesChanged;
    static FString GetPreferencesPath();

private:
    friend class FHorrorAudioPreferencesTest;
    friend class FHorrorAudioPlaybackTest;
    void FinishLoading(FGuid Handle);
    void FailPlayback(FGuid Handle, const FText& Error);
    void RemovePlayback(FGuid Handle);
    void HandleAudioFinished(UAudioComponent* Component);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    UFUNCTION() void HandleAttachmentEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    void UpdateVolumes();
    void DiscardAbandonedEdit();

    UPROPERTY(Transient) TMap<FGuid, FHorrorAudioPlayback> Playbacks;
    UPROPERTY(Transient) TMap<EHorrorAudioChannel, TObjectPtr<USoundClass>> ChannelClasses;
    TMap<FGuid, TSharedPtr<FStreamableHandle>> PendingLoads;
    TWeakObjectPtr<UObject> EditOwner;
    bool bEditingVolumes = false;
    bool bShuttingDown = false;
    FString PreferencesPath;
    FHorrorAudioVolumes CurrentVolumes;
    FHorrorAudioVolumes AppliedVolumes;
    FDelegateHandle WorldCleanupHandle;
};
