#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkJolt/Query/CkJoltQuery_Data.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt::ballistic
{
    // One fixed simulation step of free flight, in the order the Jolt integrator applies it: gravity, linear damping,
    // the speed clamp, then the move. Single precision, as the simulation is.
    CKJOLT_API auto
    Advance_OneStep(
        FVector3f& InOutVelocity,
        FVector& InOutLocation,
        const FCk_Jolt_BallisticFlight& InFlight,
        const FVector3f& InGravity,
        float InDeltaT) -> void;
}

// --------------------------------------------------------------------------------------------------------------------
