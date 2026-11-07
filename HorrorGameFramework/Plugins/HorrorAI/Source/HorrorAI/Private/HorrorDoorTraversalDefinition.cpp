#include "HorrorDoorTraversalDefinition.h"
bool UHorrorDoorTraversalDefinition::Validate() const
{
    return FMath::IsFinite(OpenDelaySeconds) && OpenDelaySeconds >= 0
        && FMath::IsFinite(InteractionDistance) && InteractionDistance > 0
        && FMath::IsFinite(CrossingTimeoutSeconds) && CrossingTimeoutSeconds > 0;
}
