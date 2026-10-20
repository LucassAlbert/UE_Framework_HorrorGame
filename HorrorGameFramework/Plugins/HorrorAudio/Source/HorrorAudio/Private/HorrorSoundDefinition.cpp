#include "HorrorSoundDefinition.h"

bool UHorrorSoundDefinition::Validate(FText& Error) const
{
    if (Sound.IsNull()) Error = NSLOCTEXT("HorrorAudio", "MissingSound", "Defina um som no Data Asset.");
    else if (Channel < EHorrorAudioChannel::Ambience || Channel > EHorrorAudioChannel::Music)
        Error = NSLOCTEXT("HorrorAudio", "InvalidChannel", "Escolha ambiente, efeitos, interface ou música.");
    else if (PausePolicy != EHorrorAudioPausePolicy::PauseWithWorld && PausePolicy != EHorrorAudioPausePolicy::ContinueWhilePaused)
        Error = NSLOCTEXT("HorrorAudio", "InvalidPause", "A política de pausa é inválida.");
    else if (!FMath::IsFinite(Volume) || Volume < 0.f || Volume > 4.f || !FMath::IsFinite(Pitch) || Pitch < .125f || Pitch > 4.f
        || !FMath::IsFinite(FadeInSeconds) || FadeInSeconds < 0.f || FadeInSeconds > 10.f
        || !FMath::IsFinite(FadeOutSeconds) || FadeOutSeconds < 0.f || FadeOutSeconds > 10.f)
        Error = NSLOCTEXT("HorrorAudio", "InvalidParameters", "Volume, pitch ou duração do fade são inválidos.");
    else if (bSpatialized && bUseAttenuationAsset && Attenuation.IsNull())
        Error = NSLOCTEXT("HorrorAudio", "MissingAttenuation", "Um som espacial precisa de uma configuração de atenuação.");
    else if (bSpatialized && !bUseAttenuationAsset && (SpatialSettings.AttenuationShapeExtents.ContainsNaN()
        || SpatialSettings.AttenuationShapeExtents.GetMin() < 0.f || !FMath::IsFinite(SpatialSettings.FalloffDistance) || SpatialSettings.FalloffDistance < 0.f))
        Error = NSLOCTEXT("HorrorAudio", "InvalidSpatialSettings", "As distâncias de atenuação são inválidas.");
    else return true;
    return false;
}
