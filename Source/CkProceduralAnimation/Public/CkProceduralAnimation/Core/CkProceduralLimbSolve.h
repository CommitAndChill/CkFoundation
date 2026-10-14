#pragma once

#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"

namespace ck
{
    /** Solves a rigid chain without a gait, world query or transform publication. */
    CKPROCEDURALANIMATION_API auto
    SolveProceduralLimb(
        ECk_ProceduralRig_ChainSolver InSolver,
        TArrayView<const float> InLengths,
        const FVector& InHip,
        const FVector& InTarget,
        const FVector& InPole,
        const FQuat& InBodyRotation,
        TArrayView<FVector> OutJoints)
        -> bool;

    CKPROCEDURALANIMATION_API auto
    Get_ProceduralLimbSegmentRotation(
        const FVector& InFrom,
        const FVector& InTo,
        const FVector& InPole)
        -> FQuat;
}
