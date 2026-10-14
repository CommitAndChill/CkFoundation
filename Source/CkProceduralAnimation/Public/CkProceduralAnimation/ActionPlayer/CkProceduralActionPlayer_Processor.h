#pragma once

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Processor.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Processor.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Processor.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Processor.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

namespace ck
{
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Physics;
        using RunBefore = TDepList<FProcessor_ProceduralLimbControl_Reconcile>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer& InPlayer)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_HandleRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_HandleRequests,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer>,
        TReadWrite<FFragment_ProceduralActionPlayer_Requests>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using MarkedDirtyBy = FFragment_ProceduralActionPlayer_Requests;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer& InPlayer,
            FFragment_ProceduralActionPlayer_Requests& InRequests)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_CancelPendingRequests,
        FCk_Handle_ProceduralActionPlayer,
        TReadOnly<FFragment_ProceduralActionPlayer_Requests>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralActionPlayer_Requests& InRequests)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_Service : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_Service,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralBodyPose_Update,
            FProcessor_ProceduralLimbControl_UpdateRecovery,
            FProcessor_ProceduralAction_HandleRequests,
            FProcessor_ProceduralActionPlayer_HandleRequests>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer& InPlayer)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_Observe : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_Observe,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Camera;
        using RunAfter = TDepList<FProcessor_ProceduralArticulation_Observe,
            FProcessor_ProceduralLimbControl_ObserveRecovery>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer& InPlayer)
            -> void;
    };

    // An Auto action has no consumer left to release it once its player is gone.
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_EndPlay : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_EndPlay,
        FCk_Handle_ProceduralActionPlayer,
        TReadOnly<FFragment_ProceduralActionPlayer>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralActionPlayer& InPlayer)
            -> void;
    };
}
