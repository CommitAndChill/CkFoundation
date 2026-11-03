#pragma once

#include "CkProceduralAnimation/Core/CkProceduralFoothold.h"
#include "CkProceduralAnimation/Core/CkProceduralContactStaging.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceContactStaging.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class FProcessor_ProceduralGait_Update;

    // Native down-probe terrain observation, never an admitted foothold or a plant.
    struct CKPROCEDURALANIMATION_API FProceduralGaitHeightCandidate
    {
        CK_GENERATED_BODY(FProceduralGaitHeightCandidate);

    private:
        FCk_Handle_ProceduralLeg _Leg;
        FVector _HipLocal = FVector::ZeroVector;
        FVector _FootWorld = FVector::ZeroVector;
        FVector _PointWorld = FVector::ZeroVector;
        FVector _NormalWorld = FVector::UpVector;
        float _Reach = 0.0f;
        float _AdmissionReach = 0.0f;
        bool _RequiresRelief = false;

    public:
        CK_PROPERTY_GET(_Leg);
        CK_PROPERTY_GET(_HipLocal);
        CK_PROPERTY_GET(_FootWorld);
        CK_PROPERTY_GET(_PointWorld);
        CK_PROPERTY_GET(_NormalWorld);
        CK_PROPERTY_GET(_Reach);
        CK_PROPERTY_GET(_AdmissionReach);
        CK_PROPERTY_GET(_RequiresRelief);
        CK_DEFINE_CONSTRUCTORS(FProceduralGaitHeightCandidate, _Leg, _HipLocal, _FootWorld, _PointWorld, _NormalWorld,
            _Reach, _AdmissionReach, _RequiresRelief);
    };

    struct CKPROCEDURALANIMATION_API FProceduralGaitHeightPlanning
    {
        CK_GENERATED_BODY(FProceduralGaitHeightPlanning);

        friend class FProcessor_ProceduralGait_Update;

    private:
        FTransform _BodyAtSolve = FTransform::Identity;
        uint64 _SolveSequence = 0;
        uint64 _Frame = 0;
        FVector _Direction = FVector::ZeroVector;
        float _Speed = 0.0f;
        uint64 _EnabledMask = 0;
        uint64 _SupportingMask = 0;
        TArray<FProceduralGaitHeightCandidate, TInlineAllocator<8>> _Candidates;
        bool _HasSample = false;

    public:
        CK_PROPERTY_GET(_BodyAtSolve);
        CK_PROPERTY_GET(_SolveSequence);
        CK_PROPERTY_GET(_Frame);
        CK_PROPERTY_GET(_Direction);
        CK_PROPERTY_GET(_Speed);
        CK_PROPERTY_GET(_EnabledMask);
        CK_PROPERTY_GET(_SupportingMask);
        CK_PROPERTY_GET(_Candidates);
        CK_PROPERTY_GET(_HasSample);
    };

    // A same-call observation after a native seed fits but retained-anchor visibility rejects the height goal.
    // It places one further proposal only: never a landing, support grant, replacement sample or cross-frame cache.
    struct CKPROCEDURALANIMATION_API FProceduralGaitHeightRetryWitness
    {
        CK_GENERATED_BODY(FProceduralGaitHeightRetryWitness);

    private:
        FProceduralGaitHeightCandidate _Candidate;
        FTransform _BodyAtSolve = FTransform::Identity;
        FTransform _RejectedBody = FTransform::Identity;
        TOptional<FTransform> _PoseOffset;
        FVector _SupportUp = FVector::ZeroVector;
        FVector _PointWorld = FVector::ZeroVector;
        FVector _NormalWorld = FVector::ZeroVector;
        EProceduralFootholdCast _Cast = EProceduralFootholdCast::Down;
        uint64 _SolveSequence = 0;
        uint64 _Frame = 0;

    public:
        CK_PROPERTY_GET(_Candidate);
        CK_PROPERTY_GET(_BodyAtSolve);
        CK_PROPERTY_GET(_RejectedBody);
        CK_PROPERTY_GET(_PoseOffset);
        CK_PROPERTY_GET(_SupportUp);
        CK_PROPERTY_GET(_PointWorld);
        CK_PROPERTY_GET(_NormalWorld);
        CK_PROPERTY_GET(_Cast);
        CK_PROPERTY_GET(_SolveSequence);
        CK_PROPERTY_GET(_Frame);
        FProceduralGaitHeightRetryWitness() = default;

        FProceduralGaitHeightRetryWitness(
            const FProceduralGaitHeightCandidate& InCandidate,
            const FTransform& InBodyAtSolve,
            const FTransform& InRejectedBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InSupportUp,
            const FVector& InPointWorld,
            const FVector& InNormalWorld,
            EProceduralFootholdCast InCast,
            uint64 InSolveSequence,
            uint64 InFrame)
            : _Candidate(InCandidate)
            , _BodyAtSolve(InBodyAtSolve)
            , _RejectedBody(InRejectedBody)
            , _PoseOffset(InPoseOffset)
            , _SupportUp(InSupportUp)
            , _PointWorld(InPointWorld)
            , _NormalWorld(InNormalWorld)
            , _Cast(InCast)
            , _SolveSequence(InSolveSequence)
            , _Frame(InFrame)
        {
        }
    };

    // Exact native seed failure of this Landing admission call; never a retained contact or generic rejection.
    enum class EProceduralHeightSeedFailure : uint8
    {
        None,
        TrustedInitialDownClampReprobeMiss,
        TrustedDownFinalForceReject
    };

    enum class EProceduralHeightPlanStage : uint8
    {
        Landing,
        Transit,
        Recovery,
        PreparatoryBody
    };

    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralHeightPlanAdmitted(
            FCk_Handle_ProceduralGait InGait,
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FProceduralGaitHeightCandidate& InCandidate,
            int32& InOutRays,
            int32 InRayLimit,
            EProceduralHeightPlanStage InStage = EProceduralHeightPlanStage::Landing,
            TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry = nullptr,
            TArray<FProceduralContactStagingBoundary>* OutStagingBoundaries = nullptr,
            EProceduralHeightSeedFailure* OutSeedFailure = nullptr)
        -> bool;

    CKPROCEDURALANIMATION_API auto
        Get_ProceduralContactStagingLegs(
            FCk_Handle_ProceduralGait InGait,
            const TArray<FProceduralContactStagingBoundary>& InBoundaries)
        -> TArray<FProceduralContactStagingLeg>;

    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralContactStagingBodyAdmitted(
            FCk_Handle_ProceduralGait InGait,
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            int32& InOutRays,
            int32 InRayLimit)
        -> bool;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-attached, copy-only receipts from actual Surface planning calls; never gameplay authority.
    struct CKPROCEDURALANIMATION_API FProceduralHeightPlanningTestAttempt
    {
        CK_GENERATED_BODY(FProceduralHeightPlanningTestAttempt);

        FProceduralGaitHeightCandidate Candidate;
        double Delta = 0.0;
        uint8 Route = 0; // Ordinary=0, Deferred=1, Interior=2, Cooperative=3.
        EProceduralHeightPlanStage Stage = EProceduralHeightPlanStage::Landing;
        EProceduralHeightSeedFailure SeedFailure = EProceduralHeightSeedFailure::None;
        bool Accepted = false;
        bool Completed = false;
        bool WitnessRequested = false;
        TOptional<FProceduralGaitHeightRetryWitness> Witness;
        int32 Rays = 0;
        int32 Envelopes = 0;
        int32 Trials = 0;
        FTransform BodyBefore = FTransform::Identity;
        FTransform BodyAfter = FTransform::Identity;
        bool Preparing = false;
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralHeightPlanning_Test
    {
        CK_GENERATED_BODY(FFragment_ProceduralHeightPlanning_Test);

        FProceduralHeightPlanningTestAttempt Attempts[4]{};
        int32 Count = 0;
        bool Overflow = false;
        uint64 Frame = 0;
        uint64 Solve = 0;
        bool CooperativeThirdOffered = false;
        bool InteriorOffered = false;
        int32 ValidatedWitnessTrial = 0; // One-based charged trial, zero when no witness validated.
        bool PlanningConsumed = false;
    };
#endif
}
