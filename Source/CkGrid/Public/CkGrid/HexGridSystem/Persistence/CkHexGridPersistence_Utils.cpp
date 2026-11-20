#include "CkHexGridPersistence_Utils.h"

#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGrid_Projection.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridObjectBinding_Utils.h"

namespace ck::hex_grid::persistence
{
    namespace
    {
        using EFailure = ECk_HexGridFailure;
        using EStatus = ECk_Persistence_ApplyResult;
        using EOccupant = ECk_HexGridOccupantState;

        auto Before(const FCk_HexCoord& InLeft, const FCk_HexCoord& InRight) -> bool
        {
            return InLeft.Get_R() == InRight.Get_R() ? InLeft.Get_Q() < InRight.Get_Q() : InLeft.Get_R() < InRight.Get_R();
        }

        auto SortedCells(TArray<FCk_HexCoord> InCells) -> TArray<FCk_HexCoord>
        {
            InCells.Sort(Before);
            return InCells;
        }

        auto SortedTags(const FGameplayTagContainer& InTags) -> FGameplayTagContainer
        {
            auto Tags = InTags.GetGameplayTagArray();
            Tags.Sort([](const FGameplayTag& InLeft, const FGameplayTag& InRight)
            { return InLeft.GetTagName().LexicalLess(InRight.GetTagName()); });
            return FGameplayTagContainer::CreateFromArray(Tags);
        }

        auto HasValidTags(const FGameplayTagContainer& InTags) -> bool
        {
            TSet<FGameplayTag> Seen;
            for (const auto& Tag : InTags)
            {
                if (NOT Tag.IsValid() || Seen.Contains(Tag))
                { return false; }
                Seen.Add(Tag);
            }
            return true;
        }

        auto CanonicalCells(const TArray<FCk_HexCoord>& InCells) -> bool
        {
            for (int32 Index = 1; Index < InCells.Num(); ++Index)
            {
                if (NOT Before(InCells[Index-1], InCells[Index]))
                { return false; }
            }
            return true;
        }

        auto SameRecipe(const FCk_HexGridRecipeIdentity& InExpected, const FCk_HexGridRecipeIdentity& InActual) -> bool
        {
            return NOT InExpected.Get_AssetPath().IsEmpty() && InExpected.Get_RecipeRevision() > 0 &&
                InExpected.Get_Fingerprint().Len() == 40 && InExpected.Get_RecipeRevision() == InActual.Get_RecipeRevision() &&
                InExpected.Get_AssetPath().Equals(InActual.Get_AssetPath(), ESearchCase::CaseSensitive) &&
                InExpected.Get_Fingerprint().Equals(InActual.Get_Fingerprint(), ESearchCase::CaseSensitive);
        }

        auto Encode(const FCk_Handle_HexGrid& InGrid, const FCk_HexGridCommittedProjection& InState)
            -> FCk_RepData_HexGridState
        {
            const auto& Params = InGrid.Get<FFragment_HexGrid_Params>();
            FCk_RepData_HexGridState Data;
            Data.Set_Recipe(Params.Get_RecipeIdentity());
            for (const auto& View : InState._Placements)
            {
                const auto Bound = View.Get_OccupantState() == EOccupant::Bound;
                const auto Reference = Bound ? View.Get_Occupant() : View.Get_ExpectedOccupant();
                Data.Get_Placements().AddDefaulted_GetRef()
                    .Set_PlacementId(View.Get_PlacementId()).Set_HasOccupantReference(View.Get_OccupantState() != EOccupant::Anonymous)
                    .Set_OccupantBindingId(View.Get_OccupantBindingId()).Set_Occupant(hex_rules::IsLive(Reference) ? Reference : FCk_Handle{})
                    .Set_FootprintOffsets(SortedCells(View.Get_FootprintOffsets())).Set_Anchor(View.Get_Anchor())
                    .Set_Rotation(View.Get_Rotation()).Set_Connectivity(View.Get_Connectivity()).Set_Cells(View.Get_Cells())
                    .Set_RequiredCellTags(SortedTags(View.Get_RequiredCellTags())).Set_ForbiddenCellTags(SortedTags(View.Get_ForbiddenCellTags()));
            }
            Data.Get_Placements().Sort([](const auto& InLeft, const auto& InRight)
            { return InLeft.Get_PlacementId() < InRight.Get_PlacementId(); });
            for (const auto& View : InState._Blockers)
            {
                Data.Get_Blockers().AddDefaulted_GetRef().Set_BlockerId(View._BlockerId).Set_Name(View._Name)
                    .Set_Cells(SortedCells(View._Cells)).Set_Active(View._Active);
            }
            Data.Get_Blockers().Sort([](const auto& InLeft, const auto& InRight)
            { return InLeft.Get_BlockerId() < InRight.Get_BlockerId(); });

            auto Changed = InState._ManualBlocks;
            for (const auto& Pair : InState._LocalTags)
            { Changed.Add(Pair.Key); }
            for (const auto& Coord : Changed)
            {
                const auto* Override = InState._LocalTags.Find(Coord);
                const auto Tags = Override ? *Override : Params.Get_AuthoredLocalTags().FindRef(Coord);
                Data.Get_CellRules().AddDefaulted_GetRef().Set_Coordinate(Coord).Set_LocalTags(SortedTags(Tags))
                    .Set_ManualBlocked(InState._ManualBlocks.Contains(Coord));
            }
            Data.Get_CellRules().Sort([](const auto& InLeft, const auto& InRight)
            { return Before(InLeft.Get_Coordinate(), InRight.Get_Coordinate()); });
            return Data;
        }

        auto Decode(const FCk_Handle_HexGrid& InGrid, const FCk_RepData_HexGridState& InData,
            FCk_HexGridCommittedProjection& Out) -> FCk_HexGridApply_Result
        {
            const auto& Domain = InGrid.Get<FFragment_HexGrid>().Get_Index();
            // Blockers may all vote on one cell. Their count is bounded by available entity indices,
            // independently of the cell count. Placements each need at least one exclusive cell.
            constexpr auto MaxContributors = entt::entt_traits<FCk_Entity::IdType>::entity_mask;
            if (InData.Get_Placements().Num() > Domain.Num() || InData.Get_CellRules().Num() > Domain.Num() ||
                static_cast<uint32>(InData.Get_Blockers().Num()) > MaxContributors)
            { return {EStatus::Rejected, EFailure::InvalidProjection, {}}; }

            const auto& Authored = InGrid.Get<FFragment_HexGrid_Params>().Get_AuthoredLocalTags();
            TSet<FCk_HexCoord> SeenRules;
            for (const auto& Rule : InData.Get_CellRules())
            {
                const auto Coord = Rule.Get_Coordinate();
                if (NOT Domain.Contains(Coord) || SeenRules.Contains(Coord) || NOT HasValidTags(Rule.Get_LocalTags()))
                { return {EStatus::Rejected, EFailure::InvalidProjection, {}}; }
                SeenRules.Add(Coord);
                const auto ChangedTags = Rule.Get_LocalTags() != Authored.FindRef(Coord);
                if (ChangedTags)
                { Out._LocalTags.Add(Coord, Rule.Get_LocalTags()); }
                if (Rule.Get_ManualBlocked())
                { Out._ManualBlocks.Add(Coord); }
                else if (NOT ChangedTags)
                { return {EStatus::Rejected, EFailure::InvalidProjection, {}}; }
            }
            for (const auto& Blocker : InData.Get_Blockers())
            {
                if (Blocker.Get_Cells().IsEmpty() || Blocker.Get_Cells().Num() > Domain.Num() || NOT CanonicalCells(Blocker.Get_Cells()))
                { return {EStatus::Rejected, EFailure::InvalidProjection, {}}; }
                Out._Blockers.Add({Blocker.Get_BlockerId(), Blocker.Get_Name(), Blocker.Get_Cells(), Blocker.Get_Active()});
            }
            TSet<FGuid> BindingIds;
            TSet<FCk_Handle> ReferencedEntities;
            auto AvailableCells = Domain.Num();
            for (const auto& Entry : InData.Get_Placements())
            {
                const auto Id = Entry.Get_PlacementId();
                if (Entry.Get_Cells().IsEmpty() || Entry.Get_Cells().Num() > AvailableCells ||
                    Entry.Get_FootprintOffsets().Num() != Entry.Get_Cells().Num() || NOT CanonicalCells(Entry.Get_Cells()) ||
                    NOT HasValidTags(Entry.Get_RequiredCellTags()) || NOT HasValidTags(Entry.Get_ForbiddenCellTags()))
                { return {EStatus::Rejected, EFailure::InvalidProjection, Id}; }
                AvailableCells -= Entry.Get_Cells().Num();
                const auto Key = Entry.Get_OccupantBindingId();
                if (Entry.Get_HasOccupantReference() != Key.IsValid() ||
                    (NOT Key.IsValid() && Entry.Get_Occupant() != FCk_Handle{}))
                { return {EStatus::Rejected, EFailure::InvalidProjection, Id}; }
                if (Key.IsValid() && BindingIds.Contains(Key))
                { return {EStatus::Rejected, EFailure::OccupantAlreadyPlaced, Id}; }
                if (Key.IsValid())
                { BindingIds.Add(Key); }
                const auto Hint = Entry.Get_Occupant();
                if (ck::IsValid(Hint))
                {
                    if (ReferencedEntities.Contains(Hint))
                    { return {EStatus::Rejected, EFailure::OccupantAlreadyPlaced, Id}; }
                    ReferencedEntities.Add(Hint);
                }
                auto& View = Out._Placements.AddDefaulted_GetRef();
                View.Set_PlacementId(Id).Set_Anchor(Entry.Get_Anchor()).Set_Rotation(Entry.Get_Rotation())
                    .Set_Connectivity(Entry.Get_Connectivity()).Set_Cells(Entry.Get_Cells())
                    .Set_FootprintOffsets(Entry.Get_FootprintOffsets()).Set_RequiredCellTags(Entry.Get_RequiredCellTags())
                    .Set_ForbiddenCellTags(Entry.Get_ForbiddenCellTags()).Set_OccupantBindingId(Key)
                    .Set_ExpectedOccupant(ck::IsValid(Hint) ? Hint : FCk_Handle{})
                    .Set_OccupantState(Key.IsValid() ? EOccupant::AwaitingBinding : EOccupant::Anonymous);
                const auto Failure = PreparePlacementAssociation(InGrid, View);
                if (Failure != EFailure::None)
                { return {EStatus::Rejected, Failure, Id}; }
            }
            Out._Placements.Sort([](const auto& InLeft, const auto& InRight)
            { return InLeft.Get_PlacementId() < InRight.Get_PlacementId(); });
            return {EStatus::Applied, EFailure::None, {}};
        }

        auto SameState(const FCk_Handle_HexGrid& InGrid, const FCk_HexGridCommittedProjection& InCandidate) -> bool
        {
            const auto Current = ck::hex_grid::ReadCommittedProjection(InGrid);
            if (Current._Placements.Num() != InCandidate._Placements.Num() || Current._Blockers.Num() != InCandidate._Blockers.Num())
            { return false; }
            for (const auto& Pair : InGrid.Get<FFragment_HexGrid>().Get_Index())
            {
                if (NOT hex_rules::IsLive(Pair.Value)) { return false; }
            }
            for (int32 Index = 0; Index < Current._Placements.Num(); ++Index)
            {
                const auto& Existing = Current._Placements[Index];
                const auto& Next = InCandidate._Placements[Index];
                if (NOT hex_rules::IsLive(Existing.Get_Handle()) ||
                    Existing.Get_OccupantState() != Next.Get_OccupantState() || Existing.Get_Occupant() != Next.Get_Occupant() ||
                    (Next.Get_OccupantState() == EOccupant::AwaitingBinding && Existing.Get_ExpectedOccupant() != Next.Get_ExpectedOccupant()))
                { return false; }
            }
            const auto Old = Encode(InGrid, Current);
            const auto New = Encode(InGrid, InCandidate);
            return FCk_RepData_HexGridState::StaticStruct()->CompareScriptStruct(&Old, &New, 0);
        }
    }

    auto ReadCommittedProjection(const FCk_Handle_HexGrid& InGrid) -> FCk_RepData_HexGridState
    {
        if (NOT UCk_Utils_HexGrid_UE::Has(InGrid)) { return {}; }
        return Encode(InGrid, ck::hex_grid::ReadCommittedProjection(InGrid));
    }

    auto TryReplaceCommittedProjection(FCk_Handle_HexGrid& InGrid, const FCk_RepData_HexGridState& InData,
        ECk_HexGridApplySource InSource) -> FCk_HexGridApply_Result
    {
        if (NOT IsInGameThread() || NOT hex_rules::IsLive(InGrid))
        { return {EStatus::Rejected, EFailure::InvalidGrid, {}}; }
        if (NOT UCk_Utils_HexGrid_UE::Has(InGrid))
        { return {EStatus::NotReady, EFailure::NotReady, {}}; }
        switch (InSource)
        {
            case ECk_HexGridApplySource::Hydration:
                if (UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InGrid) || !UCk_Utils_Net_UE::Get_HasAuthority(InGrid))
                { return {EStatus::Rejected, EFailure::NotAuthority, {}}; }
                break;
            case ECk_HexGridApplySource::Replication:
                if (!ck::hex_grid::CanReceiveNetBaseline(InGrid))
                { return {EStatus::Rejected, EFailure::NotAuthority, {}}; }
                break;
            default: return {EStatus::Rejected, EFailure::InvalidProjection, {}};
        }
        if (InData.Get_SchemaVersion() != 1)
        { return {EStatus::Rejected, EFailure::InvalidSchema, {}}; }
        if (NOT SameRecipe(InGrid.Get<FFragment_HexGrid_Params>().Get_RecipeIdentity(), InData.Get_Recipe()))
        { return {EStatus::Rejected, EFailure::RecipeMismatch, {}}; }

        FCk_HexGridCommittedProjection Candidate;
        const auto Decoded = Decode(InGrid, InData, Candidate);
        if (Decoded.Get_Status() != EStatus::Applied || SameState(InGrid, Candidate))
        { return Decoded; }

        const auto Committed = TryCommitProjection(InGrid, Candidate);
        return {Committed.Get_Succeeded() ? EStatus::Applied : EStatus::Rejected, Committed.Get_Failure(), Committed.Get_PlacementId()};
    }
}
