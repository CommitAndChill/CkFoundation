#pragma once

#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Processor.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Processor.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Processor.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Processor.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralLimbControl_HandleRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralLimbControl_HandleRequests,
        FCk_Handle_ProceduralArticulation,
        TReadWrite<FFragment_ProceduralLimbControl>,
        TReadWrite<FFragment_ProceduralLimbControl_Requests>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralGait_Update, FProcessor_ProceduralBodyPose_Update>;
        using MarkedDirtyBy = FFragment_ProceduralLimbControl_Requests;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(TimeType InDeltaT, HandleType InHandle, FFragment_ProceduralLimbControl& InControl,
            FFragment_ProceduralLimbControl_Requests& InRequests) -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralLimbControl_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralLimbControl_CancelPendingRequests,
        FCk_Handle_ProceduralArticulation,
        TReadOnly<FFragment_ProceduralLimbControl_Requests>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(TimeType InDeltaT, HandleType InHandle,
            const FFragment_ProceduralLimbControl_Requests& InRequests) -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralLimbControl_Reconcile : public ck_exp::TProcessor<
        FProcessor_ProceduralLimbControl_Reconcile,
        FCk_Handle_ProceduralArticulation,
        TReadWrite<FFragment_ProceduralLimbControl>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Physics;
        using RunBefore = TDepList<FProcessor_SurfaceMotion_Update>;
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralLimbControl& InControl)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralLimbControl_UpdateRecovery : public ck_exp::TProcessor<
        FProcessor_ProceduralLimbControl_UpdateRecovery,
        FCk_Handle_ProceduralArticulation,
        TReadWrite<FFragment_ProceduralLimbControl>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralGait_Update, FProcessor_ProceduralBodyPose_Update,
            FProcessor_ProceduralLimbControl_HandleRequests>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(TimeType InDeltaT, HandleType InHandle, FFragment_ProceduralLimbControl& InControl) -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralLimbControl_ObserveRecovery : public ck_exp::TProcessor<
        FProcessor_ProceduralLimbControl_ObserveRecovery,
        FCk_Handle_ProceduralArticulation,
        TReadWrite<FFragment_ProceduralLimbControl>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Camera;
        using RunAfter = TDepList<FProcessor_ProceduralArticulation_Observe>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(TimeType InDeltaT, HandleType InHandle, FFragment_ProceduralLimbControl& InControl) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
