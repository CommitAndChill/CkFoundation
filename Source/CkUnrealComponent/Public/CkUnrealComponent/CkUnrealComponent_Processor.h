#pragma once

#include "CkUnrealComponent_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_Setup : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_Setup,
            FCk_Handle_UnrealComponent,
            ck::TReadOnly<FFragment_UnrealComponent_Params>,
            ck::TReadWrite<FFragment_UnrealComponent>,
            FTag_UnrealComponent_NeedsSetup,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using MarkedDirtyBy = FTag_UnrealComponent_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_UnrealComponent_Params& InParams,
            FFragment_UnrealComponent& InUnrealComponent) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_PushTransform : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_PushTransform,
            FCk_Handle_Transform,
            ck::TReadOnly<FFragment_Transform>,
            ck::TReadOnly<FFragment_RecordOfUnrealComponents>,
            FTag_Transform_Updated,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using RunAfter = TDepList<FProcessor_UnrealComponent_Setup>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfUnrealComponents& InComponents) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_Tick : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_Tick,
            FCk_Handle_UnrealComponent,
            ck::TReadOnly<FFragment_UnrealComponent>,
            FTag_UnrealComponent_TickViaProcessor,
            TExclude<FTag_UnrealComponent_NeedsSetup>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using RunAfter = TDepList<FProcessor_UnrealComponent_PushTransform>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_UnrealComponent& InUnrealComponent) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_HandleRequests : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_HandleRequests,
            FCk_Handle_UnrealComponent,
            ck::TReadOnly<FFragment_UnrealComponent_Params>,
            ck::TReadOnly<FFragment_UnrealComponent>,
            ck::TReadWrite<FFragment_UnrealComponent_Requests>,
            TExclude<FTag_UnrealComponent_NeedsSetup>,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using RunAfter = TDepList<FProcessor_UnrealComponent_PushTransform>;
        using MarkedDirtyBy = FFragment_UnrealComponent_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_UnrealComponent_Params& InParams,
            const FFragment_UnrealComponent& InUnrealComponent,
            FFragment_UnrealComponent_Requests& InRequestsComp) -> void;

    private:
        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_UnrealComponent_Params& InParams,
            const FFragment_UnrealComponent& InUnrealComponent,
            const FCk_Request_UnrealComponent_SetCustomPrimitiveData& InRequest) -> ECk_Request_OperationResult;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_EndPlay : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_EndPlay,
            FCk_Handle_UnrealComponent,
            ck::TReadWrite<FFragment_UnrealComponent>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using RunAfter = TDepList<FProcessor_UnrealComponent_Setup>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_UnrealComponent& InUnrealComponent) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed component's still-queued
    // requests are never drained. This completes each of them with Failed_Cancelled so a caller awaiting
    // completion terminates instead of hanging. Request_SetCustomPrimitiveData refuses to enqueue once that tag
    // is present, so nothing can be queued behind this pass.
    class CKUNREALCOMPONENT_API FProcessor_UnrealComponent_CancelPendingRequests : public ck_exp::TProcessor<
            FProcessor_UnrealComponent_CancelPendingRequests,
            FCk_Handle_UnrealComponent,
            ck::TReadOnly<FFragment_UnrealComponent_Requests>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using RunAfter = TDepList<FProcessor_UnrealComponent_EndPlay>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_UnrealComponent_Requests& InRequestsComp) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
