#pragma once

#include "CkProceduralAnimation/Core/CkProceduralSurfaceMotion.h"

namespace ck
{
    struct CKPROCEDURALANIMATION_API FProceduralHeightRecoveryLegGeometry
    {
        CK_GENERATED_BODY(FProceduralHeightRecoveryLegGeometry);

    private:
        FVector _HipLocal = FVector::ZeroVector;
        FVector _FirstPoint = FVector::ZeroVector;
        TOptional<FVector> _SecondPoint;
        float _AdmissionReach = 0.0f;

    public:
        CK_PROPERTY_GET(_HipLocal);
        CK_PROPERTY_GET(_FirstPoint);
        CK_PROPERTY_GET(_SecondPoint);
        CK_PROPERTY_GET(_AdmissionReach);

    public:
        CK_DEFINE_CONSTRUCTORS(FProceduralHeightRecoveryLegGeometry, _HipLocal, _FirstPoint, _SecondPoint, _AdmissionReach);
    };

    /** One smaller positive Recovery proposal from alternative observed points within each leg.
     *  Native all-leg admission remains required; this scalar grants no permission to move. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightRecoveryRetryDelta(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
        TArrayView<const FProceduralHeightRecoveryLegGeometry> InLegs,
        double InMaxTranslation,
        double InFailedDelta)
        -> TOptional<double>;

    /** Existing stance bounds only; native future landing admission is a separate required proof. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightStanceInterval(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
        double InMaxTranslation)
        -> TOptional<FVector2D>;

    /** Lowest support-normal translation that keeps every retained hip on or above its foot plane.
     *  Proposal geometry only; no visibility, contact admission or permission to move is implied. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightRetainedPlaneLower(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors)
        -> TOptional<double>;

    /** Intersection of the stance and prospective landing reach spheres along a support-normal translation.
     *  The result describes a goal, not permission to move or a native foothold admission. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightPlanningInterval(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
        const FVector& InCandidateHipLocal,
        const FVector& InCandidatePoint,
        float InCandidateReach,
        double InMaxTranslation)
        -> TOptional<FVector2D>;

    /** A bounded proposal for fresh native reprojection after fixed-seed planning fails.
     *  Preserves every current stance sphere; grants neither a landing nor permission to move. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightReprojectionDelta(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
        const FVector& InCandidateHipLocal,
        const FVector& InCandidatePoint,
        float InCandidateReach,
        double InMaxTranslation)
        -> TOptional<double>;

    /** One strict downward interior proposal after a failed goal produced a fresh native seed.
     *  Retained-foot planes and the returned point place a query; neither grants native admission. */
    CKPROCEDURALANIMATION_API auto
    Get_ProceduralHeightInteriorRetryDelta(
        const FTransform& InBody,
        const TOptional<FTransform>& InPoseOffset,
        const FVector& InUp,
        TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
        const FVector& InCandidateHipLocal,
        const FVector& InNativePoint,
        float InCandidateReach,
        double InMaxTranslation,
        double InFailedDelta)
        -> TOptional<double>;
}
