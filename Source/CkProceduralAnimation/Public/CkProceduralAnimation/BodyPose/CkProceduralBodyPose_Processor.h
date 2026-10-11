#pragma once

#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Processor.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralBodyPose_HandleRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralBodyPose_HandleRequests,
        FCk_Handle_ProceduralBodyPose,
        TReadWrite<FFragment_ProceduralBodyPose_Action>,
        TReadWrite<FFragment_ProceduralBodyPose_Requests>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using MarkedDirtyBy = FFragment_ProceduralBodyPose_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralBodyPose_Action& InActionComp,
            FFragment_ProceduralBodyPose_Requests& InRequestsComp)
            -> void;

    private:
        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_ProceduralBodyPose_Action& InActionComp,
            const FCk_Request_ProceduralBodyPose_SetActionOffset& InRequest)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralBodyPose_Update : public ck_exp::TProcessor<
        FProcessor_ProceduralBodyPose_Update,
        FCk_Handle_ProceduralBodyPose,
        TReadOnly<FFragment_ProceduralBodyPose_Params>,
        TReadOnly<FFragment_ProceduralBodyPose_SupportLayout>,
        TReadWrite<FFragment_ProceduralBodyPose>,
        TReadWrite<FFragment_ProceduralBodyPose_Action>,
        TReadOnly<FFragment_ProceduralGait>,
        TReadOnly<FFragment_Transform>,
        TExclude<FFragment_ProceduralBodyPose_Failure>,
        TExclude<FTag_ProceduralGait_NeedsSetup>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralGait_Update, FProcessor_ProceduralBodyPose_HandleRequests>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralBodyPose_Params& InParams,
            const FFragment_ProceduralBodyPose_SupportLayout& InSupportLayout,
            FFragment_ProceduralBodyPose& InPoseComp,
            FFragment_ProceduralBodyPose_Action& InActionComp,
            const FFragment_ProceduralGait& InGaitComp,
            const FFragment_Transform& InTransform)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralBodyPose_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralBodyPose_CancelPendingRequests,
        FCk_Handle_ProceduralBodyPose,
        TReadOnly<FFragment_ProceduralBodyPose_Requests>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralBodyPose_Requests& InRequestsComp)
            -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
