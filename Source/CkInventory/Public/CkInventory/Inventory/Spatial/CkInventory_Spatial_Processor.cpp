#include "CkInventory_Spatial_Processor.h"

#include "CkInventory/CkInventory_Log.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Inventory/Spatial/CkInventory_Spatial_RequestTraits.h"
#include "CkInventory/Inventory/Spatial/CkInventory_Spatial_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"
#include "CkInventory/Item/CkItem_Fragment.h"
#include "CkInventory/Item/CkItem_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Fragment.h"
#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"

#include "CkRecord/Record/CkRecord_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_Spatial_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_Spatial_CancelOnEndPlay);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_Spatial_Replicate);
CK_REGISTER_PROCESSOR(ck::FProcessor_Inventory_Spatial_SyncReplication);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    using FInventoryItemRecord = UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils;

    auto
        FProcessor_Inventory_Spatial_SyncReplication::
        OnEntryAdded(
            FCk_Handle_Inventory_Spatial& InHandle,
            const FCk_InventoryItem_Spatial_ReplicatedEntry& InEntry,
            FCk_Handle_Item& InItem) -> void
    {
        if (InEntry.Get_Coordinate().X >= 0 && InEntry.Get_Coordinate().Y >= 0)
        {
            UCk_Utils_Inventory_Spatial_UE::Request_PlaceItemOnGrid(
                InHandle, InItem, InEntry.Get_Coordinate(), InEntry.Get_Rotation());
        }
    }

    auto
        FProcessor_Inventory_Spatial_SyncReplication::
        OnEntryRemoved(
            FCk_Handle_Inventory_Spatial& InHandle,
            FCk_Handle_Item& InItem) -> void
    {
        UCk_Utils_Inventory_Spatial_UE::Request_RemoveItemFromGrid(InHandle, InItem);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Inventory_Spatial_Replicate::
        ForEachEntity(
            TimeType,
            HandleType& InHandle,
            const FFragment_Inventory_Params&) -> void
    {
        auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);

        // Rebuild the complete named owner projection, including empty siblings.
        // PersistContents affects only the unchanged durable producer/hydrator.
        auto Data = FCk_RepData_Inventory_Spatial_NamedItems{};
        for (const auto& Inventory : UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(LifetimeOwner))
        {
            if (NOT Inventory.Has<FTag_Inventory_Replicates>() || NOT UCk_Utils_Inventory_UE::Get_IsSpatial(Inventory))
            { continue; }
            auto Snapshot = FCk_Inventory_Spatial_ReplicatedSnapshot{};
            Snapshot.InventoryName = Inventory.Get<FFragment_Inventory_Params>().Get_Name();
            const auto Spatial = UCk_Utils_Inventory_Spatial_UE::CastChecked(Inventory);
            for (const auto& Item : UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Get_ValidEntries(Inventory))
            {
                auto Entry = FCk_InventoryItem_Spatial_ReplicatedEntry{Item};
                Entry.Set_Coordinate(UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementCoordinate(Spatial, Item));
                Entry.Set_Rotation(UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementRotation(Item));
                Snapshot.Items.Add(Entry);
            }
            Data.Inventories.Add(MoveTemp(Snapshot));
        }
        UCk_Utils_Net_UE::TryUpdateContainerFragment<FCk_RepData_Inventory_Spatial_NamedItems>(LifetimeOwner, Data);

        InHandle.Remove<MarkedDirtyBy>();
    }
}

// --------------------------------------------------------------------------------------------------------------------