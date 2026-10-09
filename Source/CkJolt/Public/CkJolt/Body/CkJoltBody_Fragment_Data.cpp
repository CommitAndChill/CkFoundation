#include "CkJoltBody_Fragment_Data.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------
// Most JoltBody data types are header-only USTRUCTs; this TU holds the out-of-line spec validation.
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_JoltBody_RuntimeConvexSpec::
    Get_IsValid() const
    -> bool
{
    const auto HasPointCountInRange = _PointsCm.Num() >= ck::jolt_body::MinRuntimeConvexPoints
        && _PointsCm.Num() <= ck::jolt_body::MaxRuntimeConvexPoints;
    const auto HasValidHullSettings = FMath::IsFinite(_HullToleranceCm) && _HullToleranceCm > 0.0f
        && FMath::IsFinite(_MaxConvexRadiusCm) && _MaxConvexRadiusCm >= 0.0f;
    const auto HasRepresentablePoints = ck::algo::AllOf(_PointsCm, [](const FVector& InPoint)
    {
        return NOT InPoint.ContainsNaN() && InPoint.GetAbsMax() <= ck::jolt_body::MaxRuntimeConvexPointMagnitudeCm;
    });

    return HasPointCountInRange && HasValidHullSettings && HasRepresentablePoints;
}
