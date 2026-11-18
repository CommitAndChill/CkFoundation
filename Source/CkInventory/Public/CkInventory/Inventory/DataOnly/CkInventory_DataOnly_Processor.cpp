#include "CkInventory_DataOnly_Processor.h"

#include "CkInventory/CkInventory_Log.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"
#include "CkInventory/Item/CkItem_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkRecord/Record/CkRecord_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_DataOnly_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_DataOnly_CancelOnEndPlay);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_DataOnly_Replicate);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_DataOnly_SyncReplication);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    using FInventoryItemRecord = UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils;

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Inventory_DataOnly_Replicate::
        ForEachEntity(
            TimeType,
            HandleType& InHandle,
            const FFragment_Inventory_Params&) -> void
    {
        auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);

        // Rebuild the complete named owner projection, including empty siblings.
        // PersistContents affects only the unchanged durable producer/hydrator.
        auto Data = FCk_RepData_Inventory_DataOnly_NamedItems{};
        for (const auto& Inventory : UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(LifetimeOwner))
        {
            if (NOT Inventory.Has<FTag_Inventory_Replicates>() || NOT UCk_Utils_Inventory_UE::Get_IsDataOnly(Inventory))
            { continue; }
            auto Snapshot = FCk_Inventory_DataOnly_ReplicatedSnapshot{};
            Snapshot.InventoryName = Inventory.Get<FFragment_Inventory_Params>().Get_Name();
            for (const auto& Item : UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Get_ValidEntries(Inventory))
            { Snapshot.Items.Emplace(Item); }
            Data.Inventories.Add(MoveTemp(Snapshot));
        }
        UCk_Utils_Net_UE::TryUpdateContainerFragment<FCk_RepData_Inventory_DataOnly_NamedItems>(LifetimeOwner, Data);

        InHandle.Remove<MarkedDirtyBy>();
    }
}

// --------------------------------------------------------------------------------------------------------------------
