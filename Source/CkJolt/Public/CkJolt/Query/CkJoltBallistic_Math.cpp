#include "CkJoltBallistic_Math.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt::ballistic
{
    auto
        Advance_OneStep(
            FVector3f& InOutVelocity,
            FVector& InOutLocation,
            const FCk_Jolt_BallisticFlight& InFlight,
            const FVector3f& InGravity,
            float InDeltaT)
        -> void
    {
        // Same operand grouping as MotionProperties::ApplyForceTorqueAndDragInternal and ClampLinearVelocity, so a
        // replayed step rounds the way the simulated one does.
        InOutVelocity = InOutVelocity + InDeltaT * (InFlight.Get_GravityFactor() * InGravity);
        InOutVelocity *= FMath::Max(0.0f, 1.0f - InFlight.Get_LinearDamping() * InDeltaT);

        const auto MaxLinearVelocity = InFlight.Get_MaxLinearVelocity();
        const auto SpeedSquared = InOutVelocity.SizeSquared();
        if (SpeedSquared > FMath::Square(MaxLinearVelocity))
        { InOutVelocity *= MaxLinearVelocity / FMath::Sqrt(SpeedSquared); }

        InOutLocation += FVector{InOutVelocity} * InDeltaT;
    }
}

// --------------------------------------------------------------------------------------------------------------------
