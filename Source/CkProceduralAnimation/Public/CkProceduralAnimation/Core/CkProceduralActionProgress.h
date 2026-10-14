#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    struct FCompiledPhase;

    // The one progress evaluator: the runtime evaluator, the compiler's inverse and editor curve drawing all use it.
    CKPROCEDURALANIMATION_API auto
        EvaluateProgress(
            const FCompiledPhase& InPhase,
            double InRaw)
        -> double;

    // Smallest raw phase time whose progress reaches InProgress.
    CKPROCEDURALANIMATION_API auto
        InvertProgress(
            const FCompiledPhase& InPhase,
            double InProgress)
        -> double;
}

// --------------------------------------------------------------------------------------------------------------------
