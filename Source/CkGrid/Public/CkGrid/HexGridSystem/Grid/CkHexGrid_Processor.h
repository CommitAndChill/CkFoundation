#pragma once

#include "CkHexGrid_Rules.h"
#include "CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGridOccupancy_Fragment.h"
#include "CkGrid/HexGridSystem/Blocker/CkHexGridBlocker_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

namespace ck
{
    class FProcessor_HexGrid_Rules : public ck_exp::TProcessor<FProcessor_HexGrid_Rules, FCk_Handle,
        TReadWrite<FFragment_HexGrid_RuleRequests>, TExclude<FTag_DestroyEntity_Initiate>, CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay;
        using MarkedDirtyBy = FFragment_HexGrid_RuleRequests;
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGrid_RuleRequests& InRequests) const -> void;
        auto DoTick(TimeType InTime) -> void;

    private:
        struct FBatch { FCk_Handle _Board; TArray<FHexRuleCommand> _Commands; };
        mutable TArray<FBatch> _Batches;
    };

    class FProcessor_HexGrid_CancelRules : public ck_exp::TProcessor<FProcessor_HexGrid_CancelRules, FCk_Handle,
        TReadWrite<FFragment_HexGrid_RuleRequests>, CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;
        static auto ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGrid_RuleRequests& InRequests) -> void;
    };

    class FProcessor_HexGrid_BaselineDeadline : public ck_exp::TProcessor<FProcessor_HexGrid_BaselineDeadline,
        FCk_Handle, TReadOnly<FFragment_HexGridBaselineDeadline>, TExclude<FTag_DestroyEntity_Initiate>, CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Replication;
        using TProcessor::TProcessor;
        static auto ForEachEntity(TimeType, HandleType InBoard, const FFragment_HexGridBaselineDeadline& InDeadline) -> void;
    };

    class FProcessor_HexGrid_EndPlay : public ck_exp::TProcessor<FProcessor_HexGrid_EndPlay,
        FCk_Handle, TReadWrite<FFragment_HexGridReadiness>, CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;
        static auto ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGridReadiness& InState) -> void;
    };

    class FProcessor_HexGridBlocker_EndPlay : public ck_exp::TProcessor<FProcessor_HexGridBlocker_EndPlay,
        FCk_Handle, TReadWrite<FFragment_HexGridBlocker>, CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;
        static auto ForEachEntity(TimeType, HandleType InBlocker, FFragment_HexGridBlocker& InState) -> void;
    };
}
