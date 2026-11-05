#include "HidingTransitionDefinition.h"
#include "Curves/CurveFloat.h"

bool UHidingTransitionDefinition::Validate() const
{
    if (!FMath::IsFinite(EnterSeconds) || EnterSeconds < 0 || !FMath::IsFinite(ExitSeconds) || ExitSeconds < 0
        || !FMath::IsFinite(DoorPhaseFraction) || DoorPhaseFraction < .01f || DoorPhaseFraction > .45f || DoorOpenRotation.ContainsNaN()) return false;
    if (MovementCurve)
        for (const FRichCurveKey& Key : MovementCurve->FloatCurve.GetConstRefOfKeys())
            if (!FMath::IsFinite(Key.Time) || !FMath::IsFinite(Key.Value) || !FMath::IsFinite(Key.ArriveTangent) || !FMath::IsFinite(Key.LeaveTangent)) return false;
    return true;
}
float UHidingTransitionDefinition::GetMotionAlpha(float Progress) const
{
    const float Alpha = FMath::Clamp((Progress - DoorPhaseFraction) / (1.f - 2.f * DoorPhaseFraction), 0.f, 1.f);
    if (Alpha == 0 || Alpha == 1) return Alpha;
    return MovementCurve ? FMath::Clamp(MovementCurve->GetFloatValue(Alpha), 0.f, 1.f) : FMath::SmoothStep(0.f, 1.f, Alpha);
}
float UHidingTransitionDefinition::GetDoorAlpha(float Progress) const
{
    return FMath::Clamp(FMath::Min(Progress, 1.f - Progress) / DoorPhaseFraction, 0.f, 1.f);
}
