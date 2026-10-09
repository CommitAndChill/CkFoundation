#pragma once

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKRUNTIMEMESH_API FProcessor_RuntimeMesh_Setup : public ck_exp::TProcessor<
        FProcessor_RuntimeMesh_Setup,
        FCk_Handle_RuntimeMesh,
        TReadWrite<FFragment_RuntimeMesh>,
        TReadWrite<FFragment_RuntimeMesh_PendingImport>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMesh& InMesh,
            FFragment_RuntimeMesh_PendingImport& InPendingImport) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The queue is world-level (one fragment on the transient entity), so this drains it directly instead of
    // iterating a view; it runs only in the main pass and executes at most DrainBudgetPerTick slices.
    class CKRUNTIMEMESH_API FProcessor_RuntimeMesh_Drain : public ck_exp::TProcessor<
        FProcessor_RuntimeMesh_Drain,
        FCk_Handle,
        TReadWrite<FFragment_RuntimeMesh_Queue>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay;
        using RunAfter = TDepList<FProcessor_RuntimeMesh_Setup>;
        static constexpr auto PumpPolicy = ECk_ProcessorPumpPolicy::SkipPump;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMesh_Queue& InQueue) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKRUNTIMEMESH_API FProcessor_RuntimeMesh_SourceEndPlay : public ck_exp::TProcessor<
        FProcessor_RuntimeMesh_SourceEndPlay,
        FCk_Handle_RuntimeMesh,
        TReadOnly<FFragment_RuntimeMesh>,
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
            const FFragment_RuntimeMesh& InMesh) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKRUNTIMEMESH_API FProcessor_RuntimeMesh_WorldEndPlay : public ck_exp::TProcessor<
        FProcessor_RuntimeMesh_WorldEndPlay,
        FCk_Handle,
        TReadWrite<FFragment_RuntimeMesh_Queue>,
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
            FFragment_RuntimeMesh_Queue& InQueue) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
