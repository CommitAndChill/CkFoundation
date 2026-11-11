#pragma once

#include "CkPmg_Fragment_Polyline.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKPMG_API FProcessor_Pmg_Polyline_Setup : public ck_exp::TProcessor<
            FProcessor_Pmg_Polyline_Setup,
            FCk_Handle_Pmg_Polyline,
            ck::TReadOnly<FFragment_Pmg_Polyline_Params>,
            ck::TReadWrite<FFragment_Pmg_Polyline>,
            FTag_Pmg_Polyline_NeedsSetup,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using MarkedDirtyBy = FTag_Pmg_Polyline_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Pmg_Polyline_Params& InParams,
            FFragment_Pmg_Polyline& InPolyline)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Drains in FGroup_PostTransform: a consumer that computes its points from final poses does so in
    // FGroup_Gameplay_Camera; draining after that group puts the new vertices on screen the same frame.
    class CKPMG_API FProcessor_Pmg_Polyline_HandleRequests : public ck_exp::TProcessor<
            FProcessor_Pmg_Polyline_HandleRequests,
            FCk_Handle_Pmg_Polyline,
            ck::TReadOnly<FFragment_Pmg_Polyline_Params>,
            ck::TReadWrite<FFragment_Pmg_Polyline>,
            ck::TReadWrite<FFragment_Pmg_Polyline_Requests>,
            TExclude<FTag_Pmg_Polyline_NeedsSetup>,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using MarkedDirtyBy = FFragment_Pmg_Polyline_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Pmg_Polyline_Params& InParams,
            FFragment_Pmg_Polyline& InPolyline,
            FFragment_Pmg_Polyline_Requests& InRequests)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed polyline's still-queued
    // requests fire here with Failed_Cancelled instead of never completing.
    class CKPMG_API FProcessor_Pmg_Polyline_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_Pmg_Polyline_CancelPendingRequests,
        FCk_Handle_Pmg_Polyline,
        ck::TReadOnly<FFragment_Pmg_Polyline_Requests>,
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
            const FFragment_Pmg_Polyline_Requests& InRequests)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKPMG_API FProcessor_Pmg_Polyline_EndPlay : public ck_exp::TProcessor<
            FProcessor_Pmg_Polyline_EndPlay,
            FCk_Handle_Pmg_Polyline,
            ck::TReadWrite<FFragment_Pmg_Polyline>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_Pmg_Polyline& InPolyline)
            -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
