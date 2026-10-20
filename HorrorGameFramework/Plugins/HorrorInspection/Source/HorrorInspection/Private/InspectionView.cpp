#include "InspectionView.h"

void FInspectionView::Reset(const FRotator& Initial)
{
    InitialRotation = Initial.Quaternion().GetNormalized();
    Rotation = InitialRotation;
    Zoom = 1.f;
}

bool FInspectionView::Adjust(const FVector2D& Degrees, float ZoomSteps)
{
    if (Degrees.ContainsNaN() || !FMath::IsFinite(ZoomSteps)) return false;
    const FQuat Previous = Rotation;
    const float PreviousZoom = Zoom;
    // Axes are fixed in preview-camera space rather than accumulating Euler angles.
    Rotation = (FQuat(FVector::UpVector, FMath::DegreesToRadians(Degrees.X))
        * FQuat(FVector::RightVector, FMath::DegreesToRadians(Degrees.Y)) * Rotation).GetNormalized();
    Zoom = FMath::Clamp(Zoom * FMath::Exp(FMath::Clamp(ZoomSteps, -10.f, 10.f) * 0.15f), 0.5f, 2.f);
    return !Rotation.Equals(Previous, 0.00001f) || !FMath::IsNearlyEqual(Zoom, PreviousZoom);
}

bool FInspectionView::NormalizeBounds(const FBoxSphereBounds& Bounds, FVector& Offset, float& Scale)
{
    if (Bounds.Origin.ContainsNaN() || !FMath::IsFinite(Bounds.SphereRadius) || Bounds.SphereRadius <= UE_SMALL_NUMBER)
        return false;
    Scale = 100.f / Bounds.SphereRadius;
    Offset = -Bounds.Origin * Scale;
    return !Offset.ContainsNaN() && FMath::IsFinite(Scale);
}
