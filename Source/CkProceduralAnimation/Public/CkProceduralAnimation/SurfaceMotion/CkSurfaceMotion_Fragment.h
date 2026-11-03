#pragma once

#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Fragment_Data.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceContactStaging.h"

#include "CkProceduralAnimation/Core/CkProceduralSurfaceMotion.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"

#include "CkEcs/Tag/CkTag.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CkJolt/Query/CkJoltQuery_Data.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_SurfaceMotion_UE;
class UCk_Utils_ProceduralAnimation_Debug_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class FProcessor_SurfaceMotion_Setup;
    class FProcessor_SurfaceMotion_HandleRequests;
    class FProcessor_SurfaceMotion_Update;
    class FProcessor_ProceduralGait_Update;

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_ECS_TAG(FTag_SurfaceMotion_NeedsSetup);

    // --------------------------------------------------------------------------------------------------------------------

    using FFragment_SurfaceMotion_Params = FCk_SurfaceMotion_Spec;

    // --------------------------------------------------------------------------------------------------------------------

    // Steering intent. The membership anchor: Has/Cast key on it.
    struct CKPROCEDURALANIMATION_API FFragment_SurfaceMotion
    {
    public:
        CK_GENERATED_BODY(FFragment_SurfaceMotion);

    public:
        friend class FProcessor_SurfaceMotion_Setup;
        friend class FProcessor_SurfaceMotion_HandleRequests;
        friend class FProcessor_SurfaceMotion_Update;
        friend class FProcessor_ProceduralGait_Update;
        friend class ::UCk_Utils_SurfaceMotion_UE;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        FVector _Direction = FVector::ForwardVector;
        float _Speed = 0.0f;

    public:
        CK_PROPERTY_GET(_Direction);
        CK_PROPERTY_GET(_Speed);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // A live trusted plant and its hips in the final integration substep's rejected full-motion trial. The support's
    // accepted-body and frame stamps bound consumption; the gait also checks ownership and the unchanged plant.
    struct FProceduralSurfaceReachPaceFeedback
    {
        CK_GENERATED_BODY(FProceduralSurfaceReachPaceFeedback);

    private:
        FCk_Handle_ProceduralLeg _Leg;
        FVector _FootWorld = FVector::ZeroVector;
        FVector _TrialHipWorld = FVector::ZeroVector;
        TOptional<FVector> _TrialPosedHipWorld;

    public:
        CK_PROPERTY_GET(_Leg);
        CK_PROPERTY_GET(_FootWorld);
        CK_PROPERTY_GET(_TrialHipWorld);
        CK_PROPERTY_GET(_TrialPosedHipWorld);

    public:
        CK_DEFINE_CONSTRUCTORS(FProceduralSurfaceReachPaceFeedback, _Leg, _FootWorld, _TrialHipWorld, _TrialPosedHipWorld);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // A current native top ray proposes ordinary gait search work only; it grants no support or landing admission.
    struct FProceduralSurfaceAdaptiveTopReceipt
    {
        CK_GENERATED_BODY(FProceduralSurfaceAdaptiveTopReceipt);

    private:
        FVector _Point = FVector::ZeroVector;
        FVector _Normal = FVector::UpVector;
        FTransform _Body = FTransform::Identity;
        FVector _Direction = FVector::ZeroVector;
        FVector _Up = FVector::UpVector;
        float _Speed = 0.0f;
        uint64 _Frame = 0;

    public:
        CK_PROPERTY_GET(_Point);
        CK_PROPERTY_GET(_Normal);
        CK_PROPERTY_GET(_Body);
        CK_PROPERTY_GET(_Direction);
        CK_PROPERTY_GET(_Up);
        CK_PROPERTY_GET(_Speed);
        CK_PROPERTY_GET(_Frame);

    public:
        CK_DEFINE_CONSTRUCTORS(FProceduralSurfaceAdaptiveTopReceipt, _Point, _Normal, _Body, _Direction, _Up, _Speed, _Frame);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FProceduralSupportReturnObservation
    {
        CK_GENERATED_BODY(FProceduralSupportReturnObservation);
        FCk_Handle Root;
        FCk_Handle Collider;
        TWeakObjectPtr<UWorld> World;
        TArray<FCk_Handle> Roster;
        TArray<FCk_Handle> Parts;
        TOptional<uint64> ClaimsRevision;
        FTransform Body = FTransform::Identity;
        TOptional<FTransform> Pose;
        FVector Up = FVector::ZeroVector;
        FVector Direction = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
        float Speed = 0.0f;
        uint64 EnabledMask = 0;
        uint64 SupportingMask = 0;
        uint64 Frame = 0;
        uint64 Solve = 0;
        double WorldSeconds = 0.0;
        double SeenSeconds = 0.0;
    };

#if WITH_DEV_AUTOMATION_TESTS
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPreparation_Test
    {
        CK_GENERATED_BODY(FFragment_ProceduralBodyPreparation_Test);
        uint64 Frame = 0;
        bool ClipConsumed = false;
        bool ClipAccepted = false;
        int32 ClipTrial = 0;
        FTransform ClipFullGoal = FTransform::Identity;
        FCk_Jolt_HitResult ClipFullHit;
        FCk_Jolt_HitResult ClipPartialHit;
        bool ClipEntryDormant = false;
        bool ClipStagingDormant = false;
        FTransform ClipBefore = FTransform::Identity;
        FTransform ClipRejected = FTransform::Identity;
        FTransform ClipCandidate = FTransform::Identity;
        int32 ClipRaysBefore = 0;
        int32 ClipRaysAfter = 0;
        int32 ClipEnvelopesBefore = 0;
        int32 ClipEnvelopesAfter = 0;
        bool ObstacleContinuationIssued = false;
        bool SupportEligible = false;
        bool SupportPending = false;
        bool SupportRoom = false;
        int32 SupportRoomCalls = 0;
        bool SupportBudgetDeclined = false;
        bool SupportPendingRetained = false;
        int32 SupportBudgetEnvelopesBefore = 0;
        int32 SupportRemainingSubsteps = 0;
        int32 SupportProofQueries = 0;
        int32 SupportAcceptedSubsteps = 0;
        bool SupportPhysical = false;
        bool SupportLandings = false;
        bool SupportEnvelope = false;
        bool SupportCommitted = false;
        bool SupportZeroAccepted = false;
        FCk_Jolt_HitResult SupportSweep;
        FProceduralSurfaceMotionState SupportStateBefore;
        FProceduralSurfaceMotionState SupportStateAfter;
        ECk_SurfaceMotion_ContactSource SupportPublicSource = ECk_SurfaceMotion_ContactSource::None;
        FTransform SupportBefore = FTransform::Identity;
        FTransform SupportAfter = FTransform::Identity;
        FVector SupportNormal = FVector::ZeroVector;
        int32 SupportTrials = 0;
        int32 SupportRays = 0;
        int32 SupportEnvelopes = 0;
    };
#endif

    // The accepted support frame, the pending contact that would replace it and the body's integrated motion, rewritten
    // every substep.
    struct CKPROCEDURALANIMATION_API FFragment_SurfaceMotion_Support
    {
    public:
        CK_GENERATED_BODY(FFragment_SurfaceMotion_Support);

    public:
        friend class FProcessor_SurfaceMotion_Setup;
        friend class FProcessor_SurfaceMotion_Update;
        friend class FProcessor_ProceduralGait_Update;
        friend class ::UCk_Utils_SurfaceMotion_UE;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        FProceduralSurfaceMotionState _State;
        uint64 _EvaluatedFrame = 0;
        FTransform _EvaluatedBody = FTransform::Identity;
        float _ReachPaceScale = 1.0f;
        ECk_SurfaceMotion_ReachPaceState _ReachPaceState = ECk_SurfaceMotion_ReachPaceState::Free;
        int32 _ReachPaceTrials = 0;
        int32 _ReachPaceRays = 0;
        float _AttemptedStanceSpeed = 0.0f;
        TArray<FProceduralSurfaceReachPaceFeedback, TInlineAllocator<8>> _ReachPaceFeedback;
        bool _AdaptiveHeightActive = false;
        bool _AdaptiveHeightPreparing = false;
        FVector _AdaptiveHeightUp = FVector::UpVector;
        FVector _AdaptiveHeightGoal = FVector::ZeroVector;
        int32 _HeightPlanningTrials = 0;
        int32 _HeightPlanningRays = 0;
        int32 _HeightEnvelopeQueries = 0;
        FProceduralContactStagingState _ContactStaging;
        TOptional<FVector> _HeightObstructionPoint;
        FVector _HeightObstructionNormal = FVector::ZeroVector;
        FVector _HeightObstructionDirection = FVector::ZeroVector;
        FVector _HeightObstructionUp = FVector::UpVector;
        FTransform _HeightObstructionBody = FTransform::Identity;
        uint64 _HeightObstructionFrame = 0;
        TOptional<FProceduralSurfaceAdaptiveTopReceipt> _AdaptiveTopAcquisition;
        TOptional<FProceduralSupportReturnObservation> _SupportReturnObservation;

    public:
        CK_PROPERTY_GET(_EvaluatedFrame);
        CK_PROPERTY_GET(_EvaluatedBody);
        CK_PROPERTY_GET(_ReachPaceScale);
        CK_PROPERTY_GET(_ReachPaceState);
        CK_PROPERTY_GET(_ReachPaceTrials);
        CK_PROPERTY_GET(_ReachPaceRays);
        CK_PROPERTY_GET(_AttemptedStanceSpeed);
        CK_PROPERTY_GET(_ReachPaceFeedback);
        CK_PROPERTY_GET(_AdaptiveHeightActive);
        CK_PROPERTY_GET(_AdaptiveHeightPreparing);
        CK_PROPERTY_GET(_AdaptiveHeightGoal);
        CK_PROPERTY_GET(_HeightPlanningTrials);
        CK_PROPERTY_GET(_HeightPlanningRays);
        CK_PROPERTY_GET(_HeightEnvelopeQueries);
        CK_PROPERTY_GET(_ContactStaging);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_SurfaceMotion_Failure
    {
    public:
        CK_GENERATED_BODY(FFragment_SurfaceMotion_Failure);

    private:
        ECk_SurfaceMotion_Failure _Reason = ECk_SurfaceMotion_Failure::None;

    public:
        CK_PROPERTY_GET(_Reason);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_SurfaceMotion_Failure, _Reason);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_SurfaceMotion_Requests
    {
    public:
        CK_GENERATED_BODY(FFragment_SurfaceMotion_Requests);

    public:
        friend class FProcessor_SurfaceMotion_HandleRequests;
        friend class ::UCk_Utils_SurfaceMotion_UE;

    private:
        TArray<FCk_Request_SurfaceMotion_Steering> _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };
}

// --------------------------------------------------------------------------------------------------------------------
