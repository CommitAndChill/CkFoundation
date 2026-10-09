#pragma once

#include "CkGait/Gait/CkGait_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::gait
{
    inline constexpr auto kTwoPi = 2.0f * UE_PI;

    struct CKGAIT_API FClockState
    {
        float _Phase = 0.0f;        // radians [0, 2pi)
        float _Amount = 0.0f;       // 0 at rest, 1 at reference speed, up to MaxAmountScale
        float _SpeedRatio = 0.0f;   // ground speed / reference speed, unsmoothed
        float _BreathPhase = 0.0f;  // radians [0, 2pi)
    };

    struct CKGAIT_API FFootfalls
    {
        int32 _Count = 0;
        ECk_Gait_Side _LastSide = ECk_Gait_Side::Left;  // meaningful only when _Count > 0
    };

    /** All fields finite; ReferenceSpeed > 0; StridesPerSecond >= 0; MaxAmountScale >= 0; AmountInterpSpeed >= 0; FootfallMinSpeedRatio >= 0; CrouchScale and MinCadenceScale in [0,1]. */
    CKGAIT_API auto Get_IsStrideValid(const FCk_Gait_StrideParams& InStride) -> bool;

    /** Every tunable, not the movement component: Get_IsStrideValid and a finite BreathPeriodSeconds > 0. */
    CKGAIT_API auto Get_AreTunablesValid(const FCk_Gait_Spec& InSpec) -> bool;

    /** Get_AreTunablesValid and a live movement component. */
    CKGAIT_API auto Get_IsSpecValid(const FCk_Gait_Spec& InSpec) -> bool;

    /** Zero velocity, Grounded, Standing. */
    CKGAIT_API auto Get_RestMotion() -> FCk_Gait_Motion;

    /** Horizontal speed (Size2D) while Grounded; 0 while Airborne. */
    CKGAIT_API auto Compute_GroundSpeed(const FCk_Gait_Motion& InMotion) -> float;

    /** Wraps into [0, 2pi). */
    CKGAIT_API auto Wrap_Phase(float InPhase) -> float;

    /** Advances the clock by one sample: SpeedRatio, Amount toward min(ratio, max) x crouch, Phase at 2pi x strides x max(ratio, minCadence), Breath. A non-finite ground speed counts as 0. No-op when InDeltaSeconds <= 0 or non-finite. Returns the UNWRAPPED phase advance (radians, 0 on a no-op), so a caller can count every crossing a large step makes. */
    CKGAIT_API auto Step_Clock(FClockState& InOutState, const FCk_Gait_Spec& InSpec, const FCk_Gait_Motion& InMotion, float InDeltaSeconds) -> float;

    /** The footfalls (dip bottoms at pi/2 + k pi: even k = Left, odd k = Right) in the arc (InPhaseBefore, InPhaseBefore + InPhaseAdvance]. Every crossing counts, however large the advance; none for a non-positive or non-finite advance. */
    CKGAIT_API auto Count_Footfalls(float InPhaseBefore, float InPhaseAdvance) -> FFootfalls;

    /** Footfalls count only while Grounded and InSpeedRatio >= _FootfallMinSpeedRatio. Crossings outside the gate are dropped, not deferred. */
    CKGAIT_API auto Get_CanFootfall(const FCk_Gait_StrideParams& InStride, const FCk_Gait_Motion& InMotion, float InSpeedRatio) -> bool;

    /** Set, to the impact speed max(-InPrev.Velocity.Z, 0), exactly when InPrev is Airborne and InCurr is Grounded; unset otherwise. */
    CKGAIT_API auto Detect_Landing(const FCk_Gait_Motion& InPrev, const FCk_Gait_Motion& InCurr) -> TOptional<float>;
}

// --------------------------------------------------------------------------------------------------------------------
