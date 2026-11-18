#include "CkInventory_Spatial_Fragment.h"

#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Inventory/CkInventory_NetSnapshot.h"
#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"
#include "CkInventory/Inventory/Spatial/CkInventory_Spatial_Utils.h" // UCk_Utils_Inventory_Spatial_UE (Has/Cast/Get_ItemPlacement*) — Produce
#include "CkCore/Algorithms/CkAlgorithms.h"                          // ck::algo::ForEachIsValid — Produce

#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.h"
#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.inl.h" // Register_* entry-point bodies
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"              // Request_TransferLifetimeOwner — load hydration

// --------------------------------------------------------------------------------------------------------------------

[[maybe_unused]] static struct FInventory_Spatial_RepHandlerRegistrar
{
    FInventory_Spatial_RepHandlerRegistrar()
    {
        // The saved payload remains keyed on the inventory entity.
        const auto DoHydrateSpatialItems = [](FCk_Handle& Entity, const TArray<FCk_InventoryItem_Spatial_ReplicatedEntry>& NewItems) -> ECk_Persistence_ApplyResult
        {
            if (NOT UCk_Utils_Inventory_Spatial_UE::Has(Entity))
            { return ECk_Persistence_ApplyResult::NotReady; }

            if (ck::algo::AnyOf(NewItems, [](const FCk_InventoryItem_Spatial_ReplicatedEntry& InEntry)
                { return ck::Is_NOT_Valid(InEntry.Get_ItemHandle()); }))
            { return ECk_Persistence_ApplyResult::NotReady; }

            auto SpatialInventory = UCk_Utils_Inventory_Spatial_UE::Cast(Entity);
            auto InventoryHandle = UCk_Utils_Inventory_UE::Cast(Entity);
            for (const auto& NewEntry : NewItems)
            {
                auto ItemHandle = NewEntry.Get_ItemHandle();
                UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Request_Connect(
                    Entity, ItemHandle, ECk_Record_LabelRequirementPolicy::Optional);
                ck::TUtils_Item_ParentInventory::AddOrReplace(ItemHandle, InventoryHandle);
                UCk_Utils_EntityLifetime_UE::Request_TransferLifetimeOwner(ItemHandle, Entity);
                if (NewEntry.Get_Coordinate().X >= 0 && NewEntry.Get_Coordinate().Y >= 0)
                {
                    UCk_Utils_Inventory_Spatial_UE::Request_PlaceItemOnGrid(
                        SpatialInventory, ItemHandle, NewEntry.Get_Coordinate(), NewEntry.Get_Rotation());
                }
            }

            // The connect/place above is server-local; without re-arming, a fresh post-travel client's
            // container stays at its empty Construct-time value forever.
            UCk_Utils_Inventory_UE::Request_TryReplicateInventory(InventoryHandle);

            // Same contract as the DataOnly hydrate: the connects/placements above mutate the item record
            // OUTSIDE the request pipeline, so without this flag FProcessor_Inventory_FireSignals never runs
            // post-hydration and its FFragment_Inventory_PreviousItems baseline stays EMPTY. The FIRST real
            // mutation after a load would then diff empty-against-empty and broadcast no OnItemsChanged.
            UCk_Utils_Inventory_UE::Request_MarkInventory_AsMayHaveChanged(InventoryHandle);

            return ECk_Persistence_ApplyResult::Applied;
        };

        FCk_PersistenceHandlerRegistry::Register_SaveOnly<FCk_RepData_Inventory_Spatial_Items>({
                .Posture = ECk_Snapshot_Posture::Durable,
                // Keyed on the INVENTORY entity — emits THAT inventory's items. Capture-only, no mutation.
                .Produce = [](FCk_Handle& Entity) -> TOptional<FInstancedStruct>
                {
                    if (NOT UCk_Utils_Inventory_Spatial_UE::Has(Entity))
                    { return {}; }

                    auto Inventory = UCk_Utils_Inventory_Spatial_UE::Cast(Entity);
                    const auto& Items = UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Get_ValidEntries(Inventory);

                    auto Entries = TArray<FCk_InventoryItem_Spatial_ReplicatedEntry>{};
                    Entries.Reserve(Items.Num());

                    ck::algo::ForEachIsValid(Items, [&](const FCk_Handle_Item& InItemHandle)
                    {
                        const auto Coordinate = UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementCoordinate(Inventory, InItemHandle);
                        const auto Rotation   = UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementRotation(InItemHandle);

                        Entries.Emplace(FCk_InventoryItem_Spatial_ReplicatedEntry(InItemHandle)
                            .Set_Coordinate(Coordinate)
                            .Set_Rotation(Rotation));
                    });

                    auto Data = FCk_RepData_Inventory_Spatial_Items{};
                    Data.Items = MoveTemp(Entries);
                    return FInstancedStruct::Make(Data);
                },
                .HydrationApply = [DoHydrateSpatialItems](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& /*Old*/) -> ECk_Persistence_ApplyResult
                {
                    return DoHydrateSpatialItems(Entity, New.Get<FCk_RepData_Inventory_Spatial_Items>().Items);
                }});

        FCk_PersistenceHandlerRegistry::Register_NetOnly<FCk_RepData_Inventory_Spatial_NamedItems>({
            .Posture = ECk_Snapshot_Posture::Session,
            .NetApply = [](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& /*Old*/) -> ECk_Persistence_ApplyResult
            {
                return ck::inventory_net::ApplyNamedSnapshots<FCk_InventoryItem_Spatial_ReplicatedEntry,
                    ck::FFragment_Inventory_Spatial_SyncReplication>(Entity,
                    New.Get<FCk_RepData_Inventory_Spatial_NamedItems>().Inventories,
                    [](const FCk_Handle_Inventory& Inventory)
                    { return UCk_Utils_Inventory_UE::Get_IsSpatial(Inventory); },
                    [](const FCk_Handle_Inventory& Inventory, const FCk_Handle_Item& Item)
                    {
                        auto Entry = FCk_InventoryItem_Spatial_ReplicatedEntry{Item};
                        const auto Spatial = UCk_Utils_Inventory_Spatial_UE::CastChecked(Inventory);
                        Entry.Set_Coordinate(UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementCoordinate(Spatial, Item));
                        Entry.Set_Rotation(UCk_Utils_Inventory_Spatial_UE::Get_ItemPlacementRotation(Item));
                        return Entry;
                    },
                    [](const FCk_Handle_Inventory& Inventory, const FCk_InventoryItem_Spatial_ReplicatedEntry& Entry)
                    {
                        return UCk_Utils_2dGridSystem_UE::Has(Inventory)
                            && UCk_Utils_2dGridSystem_UE::Has(Entry.Get_ItemHandle());
                    });
            }});
    }
} GInventory_Spatial_RepHandlerRegistrar;
