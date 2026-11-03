#pragma once

#include "CkProceduralAnimation/Core/CkProceduralSurfaceMotion.h"

#include <array>

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    struct FProceduralContactStagingPlane
    {
        FVector _Point = FVector::ZeroVector;
        FVector _Normal = FVector::ZeroVector;
    };

    // Borrowed current-input geometry only. Anchor reach is the raw chain radius; the selector applies the existing
    // physical 1e-3 tolerance in double precision, never by widening a stored float. Native radii remain unchanged.
    // Freshness, attribution and native admission belong to the caller.
    struct FProceduralContactStagingLeg
    {
        int32 _RosterIndex = INDEX_NONE;
        FProceduralSurfaceReachPaceAnchor _Anchor;
        FVector _OutboardWorld = FVector::ZeroVector;
        float _ForceReach = 0.0f;
        float _SearchRadius = 0.0f;
        float _OwnSideTolerance = 0.0f;
        TOptional<FProceduralContactStagingPlane> _NativeTop;
    };

    struct FProceduralContactStagingInput
    {
        FTransform _Body = FTransform::Identity;
        // Fixed world presentation transform: finite, normalized and unit scale; its rotation need not be cardinal.
        TOptional<FTransform> _PosedBody;
        FVector _Up = FVector::ZeroVector;
        FVector _Forward = FVector::ZeroVector;
        FVector _BodyHalfExtents = FVector::ZeroVector;
        float _CollisionMargin = 0.0f;
        FProceduralContactStagingPlane _Face;
        TArrayView<const FProceduralContactStagingLeg> _Legs;
        TArrayView<const FProceduralSurfaceLandingConstraint> _Landings;
    };

    struct FProceduralContactStagingRequiredLeg
    {
        int32 _RosterIndex = INDEX_NONE;
        FVector _OriginalPlant = FVector::ZeroVector;
        // Final query remains in the original field for single-hop callers.
        FVector _ProposedQuery = FVector::ZeroVector;
        TOptional<FVector> _IntermediateQuery;
    };

    struct FProceduralContactStagingPlan
    {
        FTransform _RetreatBody = FTransform::Identity;
        FTransform _ExitBody = FTransform::Identity;
        std::array<FProceduralContactStagingRequiredLeg, 2> _Required;
        int32 _RequiredCount = 0;
        bool _TwoPlacement = false;
    };

    // One cardinal, unit-scale geometry proposal, at most 64 legs/landings and two required replacements.
    // No native query, contact admission, hypothetical stance publication or numerical widening occurs here.
    CKPROCEDURALANIMATION_API auto
        Get_ProceduralContactStagingPlan(
            const FProceduralContactStagingInput& InInput)
        -> TOptional<FProceduralContactStagingPlan>;

    // Old single-hop success wins unchanged. Only its direct search-sphere failure permits a midpoint fallback.
    CKPROCEDURALANIMATION_API auto
        Get_ProceduralContactStagingTwoPlacementPlan(
            const FProceduralContactStagingInput& InInput)
        -> TOptional<FProceduralContactStagingPlan>;

    // Pure line-and-two-spheres check for an actual selected intermediate, without native contact authority.
    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralContactStagingIntermediateTargetValid(
            const FVector& InPoint,
            const FVector& InActualPlant,
            const FVector& InFinalQuery,
            const FVector& InForward,
            float InSearchRadius)
        -> bool;

    // Rechecks the locked hold using fresh actual contacts and immutable milestones; no new top hit is needed.
    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralContactStagingHoldValid(
            const FProceduralContactStagingInput& InInput,
            const FProceduralContactStagingPlan& InPlan)
        -> bool;

    // Future planted-contact certificate only, for every pending landing after native completion. Current body
    // goals still use Get_IsProceduralSurfaceLandingReachValid while the contact remains an active landing.
    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralContactStagingFuturePlantReachValid(
            const FTransform& InExit,
            const TOptional<FTransform>& InPoseOffset,
            const FProceduralSurfaceLandingConstraint& InConstraint)
        -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
