#pragma once

#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Processor.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Processor.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    struct CKPROCEDURALANIMATION_API FProceduralRig_WorldTarget
    {
        CK_GENERATED_BODY(FProceduralRig_WorldTarget);

    private:
        FName _Id = NAME_None;
        FTransform _EndpointWorld = FTransform::Identity;
        TOptional<FVector> _PoleWorld;

    public:
        CK_PROPERTY_GET(_Id);
        CK_PROPERTY_GET(_EndpointWorld);
        CK_PROPERTY_GET(_PoleWorld);
        CK_DEFINE_CONSTRUCTORS(FProceduralRig_WorldTarget, _Id, _EndpointWorld, _PoleWorld);
    };

    struct CKPROCEDURALANIMATION_API FProceduralRig_SelectedChain
    {
        CK_GENERATED_BODY(FProceduralRig_SelectedChain);

    private:
        FName _Id = NAME_None;
        FCk_Handle_ProceduralLeg _Leg;
        FVector _RequestedEndpoint = FVector::ZeroVector;
        FVector _AcceptedEndpoint = FVector::ZeroVector;
        FVector _Pole = FVector::ZeroVector;
        TArray<FVector> _Joints;
        TArray<FCk_Handle_Transform> _Parts;
        TArray<FTransform> _PartPoses;
        ECk_ProceduralRig_ChainState _ChainState = ECk_ProceduralRig_ChainState::Clear;
        int32 _CrossingLinks = 0;
        int32 _SiblingCrossingLinks = 0;

    public:
        CK_PROPERTY_GET(_Id);
        CK_PROPERTY_GET(_Leg);
        CK_PROPERTY_GET(_RequestedEndpoint);
        CK_PROPERTY_GET(_AcceptedEndpoint);
        CK_PROPERTY_GET(_Pole);
        CK_PROPERTY_GET(_Joints);
        CK_PROPERTY_GET(_Parts);
        CK_PROPERTY_GET(_PartPoses);
        CK_PROPERTY_GET(_ChainState);
        CK_PROPERTY_GET(_CrossingLinks);
        CK_PROPERTY_GET(_SiblingCrossingLinks);
        FProceduralRig_SelectedChain() = default;

        FProceduralRig_SelectedChain(
            FName InId,
            FCk_Handle_ProceduralLeg InLeg,
            FVector InRequestedEndpoint,
            FVector InAcceptedEndpoint,
            FVector InPole,
            TArray<FVector> InJoints,
            TArray<FCk_Handle_Transform> InParts,
            TArray<FTransform> InPartPoses,
            ECk_ProceduralRig_ChainState InChainState,
            int32 InCrossingLinks,
            int32 InSiblingCrossingLinks)
            : _Id{InId}, _Leg{InLeg}, _RequestedEndpoint{InRequestedEndpoint},
              _AcceptedEndpoint{InAcceptedEndpoint}, _Pole{InPole},
              _Joints{MoveTemp(InJoints)}, _Parts{MoveTemp(InParts)},
              _PartPoses{MoveTemp(InPartPoses)}, _ChainState{InChainState},
              _CrossingLinks{InCrossingLinks}, _SiblingCrossingLinks{InSiblingCrossingLinks}
        {}
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralRig_Setup : public ck_exp::TProcessor<
        FProcessor_ProceduralRig_Setup,
        FCk_Handle_ProceduralRig,
        TReadOnly<FFragment_ProceduralRig_Params>,
        TReadWrite<FFragment_ProceduralRig>,
        FTag_ProceduralRig_NeedsSetup,
        TExclude<FTag_DestroyEntity_Initiate>,
        TExclude<FTag_ProceduralLeg_Detached>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralGait_Update>;
        using MarkedDirtyBy = FTag_ProceduralRig_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralRig_Params& InParams,
            FFragment_ProceduralRig& InRigComp)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // One serial batch per ready gait body: Jolt queries are game-thread-only and sibling choices precede publication.
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralRig_Update : public ck_exp::TProcessor<
        FProcessor_ProceduralRig_Update,
        FCk_Handle_ProceduralGait,
        TReadOnly<FFragment_ProceduralGait>,
        TReadWrite<FFragment_ProceduralGait_Debug>,
        TExclude<FTag_ProceduralGait_NeedsSetup>,
        TExclude<FFragment_ProceduralGait_Failure>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralGait_Update, FProcessor_ProceduralRig_Setup, FProcessor_ProceduralBodyPose_Update>;

    public:
        using TProcessor::TProcessor;

        // Read-only preview through the same Jolt, swivel and sibling selector used by the live rig.
        static auto
        EvaluateSubjectCandidate(
            const FCk_Handle_ProceduralGait& InSubject,
            TArrayView<const FProceduralRig_WorldTarget> InTargets,
            const TOptional<FTransform>& InPosedBodyOverride,
            TArray<FProceduralRig_SelectedChain>& OutChains)
            -> bool;

        // Dedicated fixed topology uses the same chain solver, Jolt swivel and sibling selector.
        static auto
        EvaluateDedicatedCandidate(
            const FCk_Handle_ProceduralArticulation& InSubject,
            const FTransform& InPosedBody,
            TArrayView<const FProceduralRig_WorldTarget> InTargets,
            TArray<FProceduralRig_SelectedChain>& OutChains)
            -> bool;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp)
            -> void;

    private:
        static auto
        DoComputeSubject(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp,
            TArrayView<const FProceduralRig_WorldTarget> InTargets,
            const TOptional<FTransform>& InPosedBodyOverride,
            TArray<FProceduralRig_SelectedChain>* OutPreview)
            -> bool;
    };
}

// --------------------------------------------------------------------------------------------------------------------
