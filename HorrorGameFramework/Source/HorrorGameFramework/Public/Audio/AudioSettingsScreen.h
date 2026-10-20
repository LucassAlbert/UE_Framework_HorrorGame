#pragma once

#include "CoreMinimal.h"
#include "UIScreenBase.h"
#include "HorrorAudioTypes.h"
#include "AudioSettingsScreen.generated.h"

class UHorrorAudioSubsystem;
class UHorrorSoundDefinition;
class UAudioSettingsScreen;
class USlider;
class UTextBlock;

UCLASS()
class HORRORGAMEFRAMEWORK_API UAudioVolumeRow : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UAudioSettingsScreen* Screen, EHorrorAudioChannel InChannel);
    void Refresh();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void Decrease();
    UFUNCTION() void Increase();
    UFUNCTION() void Change(float Value);
    UFUNCTION() void Preview();
    TWeakObjectPtr<UAudioSettingsScreen> OwnerScreen;
    EHorrorAudioChannel Channel = EHorrorAudioChannel::Master;
    UPROPERTY(Transient) TObjectPtr<USlider> Slider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
    bool bRefreshing = false;
};

UCLASS()
class HORRORGAMEFRAMEWORK_API UAudioSettingsScreen : public UUIScreenBase
{
    GENERATED_BODY()
public:
    UAudioSettingsScreen(const FObjectInitializer& Initializer);
    virtual void OnScreenEnter() override;
    virtual void OnScreenExit() override;
    UPROPERTY(EditDefaultsOnly, Category = "Audio|Preview")
    TMap<EHorrorAudioChannel, TObjectPtr<UHorrorSoundDefinition>> PreviewSounds;
    float GetVolume(EHorrorAudioChannel Channel) const;
    void SetVolume(EHorrorAudioChannel Channel, float Value);
    void PreviewChannel(EHorrorAudioChannel Channel);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void RefreshRows();
    UFUNCTION() void Apply();
    UFUNCTION() void Back();
    UFUNCTION() void StopPreview();
    UFUNCTION() void HandlePreviewFailure(FGuid Handle, FText Error);
    void EndEditing();
    TWeakObjectPtr<UHorrorAudioSubsystem> Audio;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioVolumeRow>> Rows;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    FGuid PreviewHandle;
    bool bEditing = false;
};
