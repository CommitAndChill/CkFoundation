#include "CkHexGrid_Projection.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"

#include "CkHexGridOccupancy_Fragment.h"
#include "CkHexGridOccupancy_Transaction.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkGrid/HexGridSystem/Blocker/CkHexGridBlocker_Fragment.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkRecord/Record/CkRecord_Utils.h"

namespace ck::hex_grid
{
    namespace
    {
        using Life = UCk_Utils_EntityLifetime_UE;
        using Failure = ECk_HexGridFailure;
        using Result = FCk_HexGridPlacement_Result;

        auto GetContributors(const FCk_Handle_HexGrid& Board) -> TArray<FCk_Handle_HexGridBlocker>
        {
            if (!FHexBlockerRecord::Has(Board)) { return {}; }
            auto Entries = FHexBlockerRecord::Get_ValidEntries(Board);
            Entries.RemoveAll([&](const auto& Entry)
            {
                return !ck::IsValid(Entry) || !Entry.template Has<FFragment_HexGridBlocker>()
                    || Entry.template Get<FFragment_HexGridBlocker>()._Board != Board;
            });
            return Entries;
        }

        struct FPreparedBlocker
        {
            FCk_Handle_HexGridBlocker Existing;
            FCk_Handle NewEntity;
            TArray<FCk_Handle_HexCell> Cells;
        };

        struct FPreparedRules
        {
            TArray<FPreparedBlocker> Blockers;
            TArray<FCk_Handle_HexGridBlocker> Removed;
        };

        auto PrepareRules(const FCk_Handle_HexGrid& Board, const FCk_HexGridCommittedProjection& Projection,
            FPreparedRules& Plan) -> bool
        {
            const auto& Index = Board.Get<FFragment_HexGrid>().Get_Index();
            // Replacing local state touches every cell, including cells with no override.
            for (const auto& Pair : Index)
            { if (!hex_rules::IsLive(Pair.Value)) { return false; } }
            for (const auto& Coord : Projection._ManualBlocks)
            { if (!Index.Contains(Coord)) { return false; } }
            for (const auto& Pair : Projection._LocalTags)
            { if (!Index.Contains(Pair.Key)) { return false; } }

            const auto Existing = GetContributors(Board);
            TMap<FGuid, FCk_Handle_HexGridBlocker> Old;
            for (const auto& Handle : Existing)
            {
                if (!hex_rules::IsLive(Handle)) { return false; }
                Old.Add(Handle.Get<FFragment_HexGridBlocker>()._BlockerId, Handle);
            }
            TSet<FGuid> Ids;
            TSet<FGameplayTag> Names;
            for (const auto& View : Projection._Blockers)
            {
                if (!View._BlockerId.IsValid() || Ids.Contains(View._BlockerId) || View._Cells.IsEmpty()
                    || View._Cells.Num() > Index.Num() || (View._Name.IsValid() && Names.Contains(View._Name)))
                { return false; }
                Ids.Add(View._BlockerId);
                if (View._Name.IsValid()) { Names.Add(View._Name); }
                auto& Prepared = Plan.Blockers.AddDefaulted_GetRef();
                TSet<FCk_HexCoord> Seen;
                for (const auto& Coord : View._Cells)
                {
                    const auto* Cell = Index.Find(Coord);
                    if (!Cell || Seen.Contains(Coord)) { return false; }
                    Prepared.Cells.Add(*Cell);
                    Seen.Add(Coord);
                }
                if (Old.RemoveAndCopyValue(View._BlockerId, Prepared.Existing))
                {
                    const auto& Previous = Prepared.Existing.Get<FFragment_HexGridBlocker>();
                    // Contributor identity owns an immutable name and shape. Only its vote can change.
                    if (View._Name != Previous._Name || Previous._Coordinates.Num() != Seen.Num()) { return false; }
                    for (const auto& Coord : Previous._Coordinates)
                    { if (!Seen.Contains(Coord)) { return false; } }
                }
            }
            Old.GenerateValueArray(Plan.Removed);
            return true;
        }

        // A released placement or dedicated blocker can own another composed feature.
        // A retained contributor, placement handle or bound occupant must survive publication.
        auto RetainsRetiringEntity(const FCk_Handle_HexGrid& Board, const FCk_HexGridCommittedProjection& Projection,
            const FPreparedRules& Plan) -> bool
        {
            TSet<FGuid> IncomingIds;
            TSet<FCk_Handle> Retained;
            for (const auto& Entry : Plan.Blockers)
            { if (ck::IsValid(Entry.Existing)) { Retained.Add(Entry.Existing); } }
            for (const auto& View : Projection._Placements)
            {
                IncomingIds.Add(View.Get_PlacementId());
                if (ck::IsValid(View.Get_Occupant())) { Retained.Add(View.Get_Occupant()); }
                if (ck::IsValid(View.Get_ExpectedOccupant())) { Retained.Add(View.Get_ExpectedOccupant()); }
            }
            TArray<FCk_Handle> Retiring;
            if (Board.Has<FFragment_HexGridOccupancy>())
            {
                for (const auto& Pair : Board.Get<FFragment_HexGridOccupancy>()._Placements)
                {
                    if (IncomingIds.Contains(Pair.Key)) { Retained.Add(Pair.Value.Get_Handle()); }
                    else { Retiring.Add(Pair.Value.Get_Handle()); }
                }
            }
            for (const auto& Removed : Plan.Removed)
            {
                if (Removed.Get<FFragment_HexGridBlocker>()._OwnsEntity) { Retiring.Add(Removed); }
            }
            if (Retiring.IsEmpty()) { return false; }
            // Either entity can be transferred beneath a contributor after board construction.
            // Lifetime cascades follow same-registry dependents. The private cell registry is
            // retired by the board itself, so preserving the board also preserves that world.
            Retained.Add(Board);
            Retained.Add(Board.Get<FFragment_HexGrid>().Get_Pivot());
            TSet<FCk_Handle> Visited;
            while (!Retiring.IsEmpty())
            {
                const auto Entity = Retiring.Pop(EAllowShrinking::No);
                if (!ck::IsValid(Entity) || Visited.Contains(Entity)) { continue; }
                if (Retained.Contains(Entity)) { return true; }
                Visited.Add(Entity);
                Retiring.Append(Life::Get_LifetimeDependents(Entity));
            }
            return false;
        }

        auto ApplyRules(FCk_Handle_HexGrid& Board, const FCk_HexGridCommittedProjection& Projection,
            FPreparedRules& Plan, TArray<FCk_Handle>& Retired) -> void
        {
            for (auto Blocker : Plan.Removed)
            {
                const bool Owned = Blocker.Get<FFragment_HexGridBlocker>()._OwnsEntity;
                hex_rules::SetActive(Blocker, false);
                FHexBlockerRecord::Request_Disconnect(Board, Blocker);
                Blocker.Remove<FFragment_HexGridBlocker>();
                if (Owned) { Retired.Add(Blocker); }
            }
            for (int32 I = 0; I < Plan.Blockers.Num(); ++I)
            {
                auto& Prepared = Plan.Blockers[I];
                const auto& View = Projection._Blockers[I];
                auto Blocker = Prepared.Existing;
                if (!ck::IsValid(Blocker))
                {
                    Life::Request_SetupEntityWithLifetimeOwner(Prepared.NewEntity, Board);
                    Blocker = InstallBlocker(Prepared.NewEntity, Board, View._BlockerId, View._Name, View._Cells,
                        MoveTemp(Prepared.Cells), true);
                }
                hex_rules::SetActive(Blocker,View._Active);
            }
            const auto& Initial = Board.Get<FFragment_HexGrid_Params>().Get_AuthoredLocalTags();
            for (const auto& Pair : Board.Get<FFragment_HexGrid>().Get_Index())
            {
                auto Cell = Pair.Value;
                const auto* Override = Projection._LocalTags.Find(Pair.Key);
                Cell.Get<FFragment_HexCell_Tags>().Get_Tags() = Override ? *Override : Initial.FindRef(Pair.Key);
                const bool Manual = Projection._ManualBlocks.Contains(Pair.Key);
                if (Manual == Cell.Has<FTag_HexCell_ManualBlock>()) { continue; }
                if (Manual) { Cell.Add<FTag_HexCell_ManualBlock>(); Cell.Add<FTag_HexCell_Disabled>(); }
                else { Cell.Remove<FTag_HexCell_ManualBlock>(); Cell.Remove<FTag_HexCell_Disabled>(); }
            }
        }
    }

    auto ReadCommittedProjection(const FCk_Handle_HexGrid& InBoard) -> FCk_HexGridCommittedProjection
    {
        FCk_HexGridCommittedProjection Projection;
        if (!UCk_Utils_HexGrid_UE::Has(InBoard)) { return Projection; }
        Projection._Placements = ReadPlacements(InBoard);
        if (InBoard.Has<FFragment_HexGridOccupancy>())
        { Projection._Revision = InBoard.Get<FFragment_HexGridOccupancy>()._Revision; }

        for (const auto& Blocker : GetContributors(InBoard))
        {
            const auto& State = Blocker.Get<FFragment_HexGridBlocker>();
            Projection._Blockers.Add(FCk_HexGridBlockerView{State._BlockerId, State._Name, State._Coordinates, State._Active});
        }
        Projection._Blockers.Sort([](const auto& A, const auto& B){ return A._BlockerId < B._BlockerId; });
        const auto& Initial = InBoard.Get<FFragment_HexGrid_Params>().Get_AuthoredLocalTags();
        for (const auto& Pair : InBoard.Get<FFragment_HexGrid>().Get_Index())
        {
            if (!hex_rules::IsLive(Pair.Value)) { continue; }
            const auto& Local = Pair.Value.Get<FFragment_HexCell_Tags>().Get_Tags();
            if (Local != Initial.FindRef(Pair.Key)) { Projection._LocalTags.Add(Pair.Key, Local); }
            if (Pair.Value.Has<FTag_HexCell_ManualBlock>()) { Projection._ManualBlocks.Add(Pair.Key); }
        }
        return Projection;
    }

    void ClearReplicaProjection(FCk_Handle_HexGrid& InBoard, TArray<FCk_Handle>& OutRetired)
    {
        if (InBoard.Has<FFragment_HexGridOccupancy>())
        {
            for (const auto& Pair : InBoard.Get<FFragment_HexGridOccupancy>()._Placements)
            { OutRetired.Add(Pair.Value.Get_Handle()); }
            ClearPlacements(InBoard);
        }
        for (auto Blocker : GetContributors(InBoard))
        {
            const bool Owned = Blocker.Get<FFragment_HexGridBlocker>()._OwnsEntity;
            hex_rules::SetActive(Blocker, false);
            FHexBlockerRecord::Request_Disconnect(InBoard, Blocker);
            Blocker.Remove<FFragment_HexGridBlocker>();
            if (Owned) { OutRetired.Add(Blocker); }
        }
        const auto& Initial = InBoard.Get<FFragment_HexGrid_Params>().Get_AuthoredLocalTags();
        for (const auto& Pair : InBoard.Get<FFragment_HexGrid>().Get_Index())
        {
            auto Cell = Pair.Value;
            if (!hex_rules::IsLive(Cell)) { continue; }
            Cell.Get<FFragment_HexCell_Tags>().Get_Tags() = Initial.FindRef(Pair.Key);
            if (Cell.Has<FTag_HexCell_ManualBlock>())
            { Cell.Remove<FTag_HexCell_ManualBlock>(); Cell.Remove<FTag_HexCell_Disabled>(); }
        }
    }

    void RetireReplicaProjectionEntities(const TArray<FCk_Handle>& InRetired)
    {
        for (auto Entity : InRetired)
        {
            // User code can adopt a dependent beneath a projection handle. Retire its feature while
            // preserving that lifetime tree and letting its owner perform final entity destruction.
            if (hex_rules::IsLive(Entity) && Life::Get_LifetimeDependents(Entity).IsEmpty())
            { Life::Request_DestroyEntity(Entity); }
        }
    }

    auto TryCommitProjection(FCk_Handle_HexGrid& InBoard, const FCk_HexGridCommittedProjection& InProjection) -> Result
    {
        if (!IsInGameThread() || !hex_rules::IsLive(InBoard) || !UCk_Utils_HexGrid_UE::Has(InBoard))
        { return Result{}.Set_Failure(Failure::InvalidGrid); }
        FPreparedRules Plan;
        if (!PrepareRules(InBoard, InProjection, Plan) || RetainsRetiringEntity(InBoard, InProjection, Plan))
        { return Result{}.Set_Failure(Failure::InvalidProjection); }

        // Temporary entities have no owner, label, record, feature or external callbacks.
        // Adopt them only after both halves of the transaction have succeeded.
        if (Plan.Blockers.ContainsByPredicate([](const FPreparedBlocker& Entry){ return !ck::IsValid(Entry.Existing); })
            && !Life::Get_CanCreateEntity(InBoard))
        { return Result{}.Set_Failure(Failure::InternalFailure); }
        auto Registry = InBoard.Get_RegistryView();
        TArray<FCk_Handle> Staged;
        for (auto& Entry : Plan.Blockers)
        {
            if (ck::IsValid(Entry.Existing)) { continue; }

            Entry.NewEntity = Life::Request_CreateEntity(Registry);
            if (!ck::IsValid(Entry.NewEntity))
            {
                for (auto& Temp : Staged) { Life::Request_DestroyEntity(Temp); }
                return Result{}.Set_Failure(Failure::InternalFailure);
            }
            Staged.Add(Entry.NewEntity);
        }

        TArray<FCk_Handle> Retired;
        const auto Outcome = TryReplacePlacements(InBoard, InProjection._Placements, Retired);
        if (!Outcome.Get_Succeeded())
        {
            for (auto& Temp : Staged) { Life::Request_DestroyEntity(Temp); }
            return Outcome;
        }
        ApplyRules(InBoard, InProjection, Plan, Retired);
        ArmBoardReplication(InBoard);
        // Destroy can broadcast synchronously. The entire replacement must be observable by then.
        for (auto& Old : Retired) { Life::Request_DestroyEntity(Old); }
        return Outcome;
    }
}
