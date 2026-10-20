#include "HorrorEnemyDefinition.h"

bool FHorrorEnemyMemory::Validate() const
{
    return State <= EHorrorEnemyState::Disabled && PatrolIndex >= 0 && !LastKnownLocation.ContainsNaN() && !WitnessedEntry.ContainsNaN()
        && FMath::IsFinite(RemainingTime) && RemainingTime >= 0.f;
}
bool UHorrorEnemyDefinition::Validate(FString& Error) const
{
    const float Positive[] = { PatrolSpeed, InvestigateSpeed, ChaseSpeed, SightRadius, LoseSightRadius,
        HearingRange, SearchSeconds, MoveTimeoutSeconds, ArrivalRadius, CaptureDistance };
    for (float Value : Positive)
        if (!FMath::IsFinite(Value) || Value <= 0.f) { Error = TEXT("Enemy speeds, ranges and durations must be finite and positive."); return false; }
    if (LoseSightRadius < SightRadius || !FMath::IsFinite(SightHalfAngle) || SightHalfAngle < 1.f || SightHalfAngle > 179.f
        || !FMath::IsFinite(UpdateInterval) || UpdateInterval < .05f || UpdateInterval > 1.f
        || !FMath::IsFinite(PatrolWaitSeconds) || PatrolWaitSeconds < 0.f || !FMath::IsFinite(SearchRadius) || SearchRadius < 0.f)
    { Error = TEXT("Invalid enemy perception, update interval or search settings."); return false; }
    return true;
}
