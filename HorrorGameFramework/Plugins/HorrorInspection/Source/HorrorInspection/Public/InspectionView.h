#pragma once

#include "CoreMinimal.h"

// Input-independent view state. Quaternion composition permits examining every side.
struct HORRORINSPECTION_API FInspectionView
{
    FQuat Rotation = FQuat::Identity;
    FQuat InitialRotation = FQuat::Identity;
    float Zoom = 1.f;

    void Reset(const FRotator& Initial);
    bool Adjust(const FVector2D& Degrees, float ZoomSteps);
    static bool NormalizeBounds(const FBoxSphereBounds& Bounds, FVector& Offset, float& Scale);
};
