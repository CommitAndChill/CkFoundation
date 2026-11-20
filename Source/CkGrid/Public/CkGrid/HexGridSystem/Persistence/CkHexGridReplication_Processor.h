#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Fragment.h"

namespace ck::hex_grid
{
    auto IsLinkedReplicaAwaitingRecipe(const FCk_Handle& InEntity) -> bool;
    auto ArmBoardReplication(FCk_Handle InBoard) -> void;
    auto ArmObjectReplication(FCk_Handle InObject) -> void;
}

namespace ck
{
    struct FFragment_HexGrid_PendingPublish
    {
        bool _Board = false;
        bool _Object = false;
    };

    // A feature composed before driver attachment enters this view as soon as its own driver exists.
    class FProcessor_HexGrid_Publish : public ck_exp::TProcessor<FProcessor_HexGrid_Publish, FCk_Handle,
        TReadWrite<FFragment_HexGrid_PendingPublish>, TReadOnly<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Replication;
        using MarkedDirtyBy = FFragment_HexGrid_PendingPublish;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType, HandleType InEntity, FFragment_HexGrid_PendingPublish& InPending,
            const TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>& InDriver) const -> void;
    };
}
