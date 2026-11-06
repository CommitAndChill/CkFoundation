#include "CkSnapshot/Snapshot/CkSnapshot_RestoreInvariants.h"

#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h" // ck::FFragment_LifetimeOwner / FFragment_LifetimeDependents
#include "CkEcs/Handle/CkHandle.h"                          // ck::FFragment_ContextOwner, FCk_Handle

// --------------------------------------------------------------------------------------------------------------------

namespace ck::snapshot
{
    namespace ck_snapshot_restore_invariants
    {
        auto
            DoCheck_Handle(
                ck::SnapshotRegistryType& InRegistry,
                const FCk_Handle& InHandle,
                const TCHAR* InFragmentName,
                ck::SnapshotEntityType InOwnerEntity,
                TArray<FString>& OutDangling) -> void
        {
            const auto Entity = InHandle.Get_Entity();

            if (Entity.Get_IsTombstone())
            { return; }

            const auto Id = Entity.Get_ID();
            if (Id == entt::null)
            { return; }

            if (InRegistry.valid(Id))
            { return; }

            OutDangling.Add(FString::Printf(
                TEXT("entity [%u] fragment [%s] -> dangling target entity [%u] (not present in restored registry)"),
                static_cast<uint32>(entt::to_integral(InOwnerEntity)),
                InFragmentName,
                static_cast<uint32>(entt::to_integral(Id))));
        }

        // The dependents list is intrusive: a linked dependent unlinks itself when it is destroyed, so every link
        // must name a live entity whose LifetimeOwner points back at this owner. Returns the next link so the
        // caller can keep walking, or a tombstone handle where the chain is broken.
        auto
            DoCheck_Dependent(
                ck::SnapshotRegistryType& InRegistry,
                const FCk_Handle& InDependent,
                ck::SnapshotEntityType InOwnerEntity,
                TArray<FString>& OutDangling) -> FCk_Handle
        {
            const auto DependentEntity = InDependent.Get_Entity();
            const auto DependentId = DependentEntity.Get_ID();

            if (DependentEntity.Get_IsTombstone() || DependentId == entt::null || NOT InRegistry.valid(DependentId))
            {
                OutDangling.Add(FString::Printf(
                    TEXT("entity [%u] fragment [LifetimeDependents] -> dangling linked dependent [%u] (not present in ")
                    TEXT("restored registry: the dependent was destroyed without unlinking)"),
                    static_cast<uint32>(entt::to_integral(InOwnerEntity)),
                    static_cast<uint32>(entt::to_integral(DependentId))));
                return {};
            }

            const auto* BackOwner = InRegistry.try_get<ck::FFragment_LifetimeOwner>(DependentId);
            if (BackOwner == nullptr || NOT BackOwner->Get_IsReverseLinked())
            {
                OutDangling.Add(FString::Printf(
                    TEXT("entity [%u] fragment [LifetimeDependents] -> linked dependent [%u] carries no reverse link")
                    TEXT(" (sibling-link corruption)"),
                    static_cast<uint32>(entt::to_integral(InOwnerEntity)),
                    static_cast<uint32>(entt::to_integral(DependentId))));
                return {};
            }

            const auto BackOwnerId = BackOwner->Get_Entity().Get_Entity().Get_ID();
            if (BackOwnerId != InOwnerEntity)
            {
                OutDangling.Add(FString::Printf(
                    TEXT("entity [%u] fragment [LifetimeDependents] -> live dependent [%u] whose LifetimeOwner names a ")
                    TEXT("different entity [%u] (aliasing / registry-rehome corruption)"),
                    static_cast<uint32>(entt::to_integral(InOwnerEntity)),
                    static_cast<uint32>(entt::to_integral(DependentId)),
                    static_cast<uint32>(entt::to_integral(BackOwnerId))));
            }

            return BackOwner->Get_NextSibling();
        }
    }

    auto
        Verify_AllStoredHandlesResolve(
            ck::SnapshotRegistryType& InRegistry) -> TArray<FString>
    {
        auto Dangling = TArray<FString>{};

        for (const auto Entity : InRegistry.view<ck::FFragment_LifetimeOwner>())
        {
            const auto& Fragment = InRegistry.get<ck::FFragment_LifetimeOwner>(Entity);
            ck_snapshot_restore_invariants::DoCheck_Handle(InRegistry, Fragment.Get_Entity(), TEXT("LifetimeOwner"), Entity, Dangling);
        }

        for (const auto Entity : InRegistry.view<ck::FFragment_ContextOwner>())
        {
            const auto& Fragment = InRegistry.get<ck::FFragment_ContextOwner>(Entity);
            ck_snapshot_restore_invariants::DoCheck_Handle(InRegistry, Fragment.Get_Entity(), TEXT("ContextOwner"), Entity, Dangling);
        }

        for (const auto Entity : InRegistry.view<ck::FFragment_LifetimeDependents>())
        {
            const auto& Fragment = InRegistry.get<ck::FFragment_LifetimeDependents>(Entity);
            auto Dependent = Fragment.Get_FirstDependent();
            auto Walked = int32{0};

            while (NOT Dependent.Get_Entity().Get_IsTombstone())
            {
                // A corrupted chain could loop; the count is the bound a well-formed list can never exceed.
                if (Walked++ >= Fragment.Get_LinkedCount())
                {
                    Dangling.Add(FString::Printf(
                        TEXT("entity [%u] fragment [LifetimeDependents] -> more linked dependents than its count [%d] ")
                        TEXT("(sibling-link cycle)"),
                        static_cast<uint32>(entt::to_integral(Entity)), Fragment.Get_LinkedCount()));
                    break;
                }

                Dependent = ck_snapshot_restore_invariants::DoCheck_Dependent(InRegistry, Dependent, Entity, Dangling);
            }
        }

        return Dangling;
    }
}
