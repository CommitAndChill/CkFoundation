#pragma once

#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkItem_Utils.h"
#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.h"

namespace ck::inventory_net
{
    // Admission is owner-wide: unresolved targets or items never stage a successful subset.
    template <typename TEntry, typename TSync, typename TSnapshot, typename TShapePredicate, typename TMakeEntry, typename TEntryReady>
    auto ApplyNamedSnapshots(
        FCk_Handle& InOwner,
        const TArray<TSnapshot>& InSnapshots,
        TShapePredicate InHasShape,
        TMakeEntry InMakeEntry,
        TEntryReady InEntryReady) -> ECk_Persistence_ApplyResult
    {
        struct FStagedInventory
        {
            FCk_Handle_Inventory Inventory;
            TArray<TEntry> Current;
            TArray<TEntry> Previous;
        };
        using FApplied = TFragment_Inventory_AppliedReplication<TEntry>;
        const auto Inventories = UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(InOwner);
        auto Names = TSet<FGameplayTag>{};
        auto ItemIdentities = TSet<FCk_Handle_Item>{};
        auto Staged = TArray<FStagedInventory>{};

        const auto Stage = [&](FCk_Handle_Inventory Inventory, const TArray<TEntry>& Current) -> bool
        {
            auto Previous = TArray<TEntry>{};
            if (Inventory.template Has<TSync>())
            { Previous = Inventory.template Get<TSync>().Get_ItemsToReplicate_Previous(); }
            else if (Inventory.template Has<FApplied>())
            { Previous = Inventory.template Get<FApplied>().Items; }
            else
            {
                for (const auto& Item : UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Get_Entries(Inventory))
                {
                    CK_ENSURE_IF_NOT(ck::IsValid(Item),
                        TEXT("Inventory named snapshot found an invalid connected baseline item [{}]"), Item)
                    { return false; }
                    Previous.Add(InMakeEntry(Inventory, Item));
                }
            }
            Staged.Add(FStagedInventory{Inventory, Current, MoveTemp(Previous)});
            return true;
        };

        for (const auto& Snapshot : InSnapshots)
        {
            CK_ENSURE_IF_NOT(Snapshot.InventoryName.IsValid() && NOT Names.Contains(Snapshot.InventoryName),
                TEXT("Inventory named snapshot has an invalid or repeated inventory name [{}]"), Snapshot.InventoryName)
            { return ECk_Persistence_ApplyResult::Rejected; }
            Names.Add(Snapshot.InventoryName);
            auto Target = FCk_Handle_Inventory{};
            auto MatchingTargets = int32{0};
            for (const auto& Inventory : Inventories)
            {
                if (Inventory.Get<FFragment_Inventory_Params>().Get_Name() != Snapshot.InventoryName)
                { continue; }
                ++MatchingTargets;
                Target = Inventory;
            }
            if (MatchingTargets == 0)
            { return ECk_Persistence_ApplyResult::NotReady; }
            CK_ENSURE_IF_NOT(MatchingTargets == 1 && InHasShape(Target),
                TEXT("Inventory named snapshot target [{}] is ambiguous or has the wrong shape"), Snapshot.InventoryName)
            { return ECk_Persistence_ApplyResult::Rejected; }

            for (const auto& Entry : Snapshot.Items)
            {
                const auto Item = Entry.Get_ItemHandle();
                if (ck::Is_NOT_Valid(Item))
                { return ECk_Persistence_ApplyResult::NotReady; }
                if (NOT UCk_Utils_Item_UE::Has(Item))
                { return ECk_Persistence_ApplyResult::NotReady; }
                if (NOT InEntryReady(Target, Entry))
                { return ECk_Persistence_ApplyResult::NotReady; }
                CK_ENSURE_IF_NOT(NOT ItemIdentities.Contains(Item),
                    TEXT("Inventory named snapshot repeats an item identity [{}] across its inventories"), Item)
                { return ECk_Persistence_ApplyResult::Rejected; }
                ItemIdentities.Add(Item);
            }
            if (NOT Stage(Target, Snapshot.Items))
            { return ECk_Persistence_ApplyResult::Rejected; }
        }

        // A removed named inventory still composed locally must clear its old
        // membership. Unrelated local-only inventories have no applied baseline.
        for (const auto& Inventory : Inventories)
        {
            if (NOT InHasShape(Inventory) || Names.Contains(Inventory.Get<FFragment_Inventory_Params>().Get_Name()))
            { continue; }
            if (NOT Inventory.template Has<FApplied>() && NOT Inventory.template Has<TSync>())
            { continue; }
            if (NOT Stage(Inventory, {}))
            { return ECk_Persistence_ApplyResult::Rejected; }
        }

        for (auto& Pending : Staged)
        {
            // AddOrGet alone keeps an older queued value. Replace Current while
            // preserving the earliest unconsumed Previous captured above.
            auto& Sync = Pending.Inventory.template AddOrGet<TSync>(Pending.Current, Pending.Previous);
            Sync = TSync{MoveTemp(Pending.Current), MoveTemp(Pending.Previous)};
        }
        return ECk_Persistence_ApplyResult::Applied;
    }
}
