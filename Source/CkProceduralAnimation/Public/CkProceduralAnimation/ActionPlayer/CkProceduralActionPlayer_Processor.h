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
        TReadWrite<FFragment_ProceduralActionPlayer_Tunables>,
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
            FFragment_ProceduralActionPlayer_Tunables& InTunables,
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

    // Delivers managed streams after Observe so a frame's accepted poses and the terminals written earlier in the
    // frame are seen together. A terminal written by an action's end of play is delivered one frame later.
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_ManagedDispatch : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_ManagedDispatch,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer>,
        TReadWrite<FFragment_ProceduralActionPlayer_Managed>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Camera;
        using RunAfter = TDepList<FProcessor_ProceduralActionPlayer_Observe>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer& InPlayer,
            FFragment_ProceduralActionPlayer_Managed& InManaged)
            -> void;

    private:
        struct FDispatchResult
        {
            int32 Delivered = 0;
            bool Serviced = false;
            bool Completed = false;
        };

        static auto
        DoDispatch(
            HandleType InHandle,
            FCk_ProceduralAction_Identity InIdentity,
            int32 InMaxRecords)
            -> FDispatchResult;

        static auto
        DoComplete(
            HandleType InHandle,
            const FCk_ProceduralAction_ManagedCompletion& InCompletion)
            -> void;

        static auto
        Get_IsDispatchable(
            const HandleType& InHandle)
            -> bool;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralActionPlayer_ManagedEndPlay : public ck_exp::TProcessor<
        FProcessor_ProceduralActionPlayer_ManagedEndPlay,
        FCk_Handle_ProceduralActionPlayer,
        TReadWrite<FFragment_ProceduralActionPlayer_Managed>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralActionPlayer_Managed& InManaged)
            -> void;
    };
}
