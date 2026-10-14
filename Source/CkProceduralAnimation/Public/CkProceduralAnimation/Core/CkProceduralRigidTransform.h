#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_animation
{
    // Authored and receipt frames are held to a tight unit rotation; frames the runtime composes (solved,
    // observed, blended) accumulate rounding and are held to the engine's normalization threshold instead.
    inline constexpr auto AuthoredRotationTolerance = 1.0e-4;
    inline constexpr auto ComposedRotationTolerance = static_cast<double>(UE_THRESH_QUAT_NORMALIZED);
    inline constexpr auto UnitScaleTolerance = 1.0e-6;

    // Finite, unit rotation and unit scale: the frames chains, bodies and action tracks are expressed in.
    inline auto
        IsRigid(
            const FTransform& InValue,
            double InRotationTolerance,
            double InScaleTolerance = UnitScaleTolerance)
        -> bool
    {
        return NOT InValue.ContainsNaN()
            && FMath::Abs(InValue.GetRotation().SizeSquared() - 1.0) <= InRotationTolerance
            && InValue.GetScale3D().Equals(FVector::OneVector, InScaleTolerance);
    }
}

// --------------------------------------------------------------------------------------------------------------------
