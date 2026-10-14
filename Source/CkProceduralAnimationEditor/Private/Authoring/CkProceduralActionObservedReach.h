#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

namespace ck::procedural_action_editor
{
    // Observation segments are centered on the rigid solver bone. Their scale is visual only.
    inline auto
        TryGetObservedHip(
            const FCk_ProceduralArticulation_LimbPose& InLimb,
            double InFirstSegmentLengthCm,
            FVector& OutHip)
        -> bool
    {
        if (InLimb.Get_Segments().IsEmpty() || NOT FMath::IsFinite(InFirstSegmentLengthCm)
            || InFirstSegmentLengthCm <= 0.0)
        { return false; }
        const auto& FirstSegment = InLimb.Get_Segments()[0];
        const auto Rotation = FirstSegment.GetRotation();
        if (FirstSegment.GetTranslation().ContainsNaN() || NOT Rotation.IsNormalized())
        { return false; }
        OutHip = FirstSegment.GetTranslation()
            - Rotation.RotateVector(FVector::XAxisVector) * (InFirstSegmentLengthCm * 0.5);
        return NOT OutHip.ContainsNaN();
    }
}
