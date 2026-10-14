#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    struct FCompiledPhase;

    // Monotone cubic Hermite through custom progress keys. Empty means the phase uses its preset easing.
    struct CKPROCEDURALANIMATION_API FCompiledProgressCurve
    {
        CK_GENERATED_BODY(FCompiledProgressCurve);
    private:
        TArray<double> _Times;
        TArray<double> _Values;
        TArray<double> _Tangents;
    public:
        CK_PROPERTY(_Times);
        CK_PROPERTY(_Values);
        CK_PROPERTY(_Tangents);
    };

    // Fritsch-Carlson tangents. Rejects (and resets OutCurve) unless the keys already satisfy the authored contract:
    // 2..64 finite keys, strictly ascending times, nondecreasing values, exact (0,0) and (1,1) endpoints.
    CKPROCEDURALANIMATION_API auto
        BuildMonotoneProgress(
            const TArray<FCk_ProceduralAction_ProgressKey>& InKeys,
            FCompiledProgressCurve& OutCurve)
        -> bool;

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
