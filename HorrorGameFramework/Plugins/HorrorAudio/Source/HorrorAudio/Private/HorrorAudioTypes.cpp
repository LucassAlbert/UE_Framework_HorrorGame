#include "HorrorAudioTypes.h"

bool FHorrorAudioVolumes::Validate() const
{
    for (float Value : { Master, Ambience, Effects, Interface, Music })
        if (!FMath::IsFinite(Value) || Value < 0.f || Value > 1.f) return false;
    return true;
}
float FHorrorAudioVolumes::Get(EHorrorAudioChannel Channel) const
{
    switch (Channel)
    {
        case EHorrorAudioChannel::Master: return Master;
        case EHorrorAudioChannel::Ambience: return Ambience;
        case EHorrorAudioChannel::Effects: return Effects;
        case EHorrorAudioChannel::Interface: return Interface;
        case EHorrorAudioChannel::Music: return Music;
    }
    return 0.f;
}
bool FHorrorAudioVolumes::Set(EHorrorAudioChannel Channel, float Value)
{
    if (!FMath::IsFinite(Value) || Value < 0.f || Value > 1.f) return false;
    switch (Channel)
    {
        case EHorrorAudioChannel::Master: Master = Value; break;
        case EHorrorAudioChannel::Ambience: Ambience = Value; break;
        case EHorrorAudioChannel::Effects: Effects = Value; break;
        case EHorrorAudioChannel::Interface: Interface = Value; break;
        case EHorrorAudioChannel::Music: Music = Value; break;
        default: return false;
    }
    return true;
}
bool FHorrorAudioVolumes::Equals(const FHorrorAudioVolumes& Other) const
{
    return FMath::IsNearlyEqual(Master, Other.Master) && FMath::IsNearlyEqual(Ambience, Other.Ambience)
        && FMath::IsNearlyEqual(Effects, Other.Effects) && FMath::IsNearlyEqual(Interface, Other.Interface)
        && FMath::IsNearlyEqual(Music, Other.Music);
}
float FHorrorAudioVolumes::GetPlaybackGain(EHorrorAudioChannel Channel) const
{
    return Channel == EHorrorAudioChannel::Master ? Master : Master * Get(Channel);
}
