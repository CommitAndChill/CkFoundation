#include "CkHexGridOccupancy_Transaction.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"

#include "CkHexGridOccupancy_Fragment.h"
#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Fragment.h"
#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Utils.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Geometry.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Utils.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridObjectBinding_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

namespace ck::hex_grid
{
    namespace
    {
        using FResult = FCk_HexGridPlacement_Result;
        using FView = FCk_HexGridPlacement_View;
        using EFailure = ECk_HexGridFailure;
        using FLife = UCk_Utils_EntityLifetime_UE;
        using FCells = UCk_Utils_HexCell_UE;

        auto Success(const FView& View) -> FResult
        {
            return FResult{}.Set_Succeeded(true).Set_Failure(EFailure::None)
                .Set_PlacementId(View.Get_PlacementId()).Set_Placement(View);
        }

        auto Reject(EFailure Reason, const FView* Old = nullptr, const TArray<FCk_HexCoord>& Cells = {}) -> FResult
        {
            auto Result = FResult{}.Set_Failure(Reason).Set_FailedCells(Cells);
            if (Old) { Result.Set_Placement(*Old).Set_PlacementId(Old->Get_PlacementId()); }
            return Result;
        }

        auto Lookup(const FCk_Handle_HexGrid& Grid, FGuid Id) -> const FView*
        {
            if (!Grid.Has<FFragment_HexGridOccupancy>()) { return nullptr; }
            return Grid.Get<FFragment_HexGridOccupancy>()._Placements.Find(Id);
        }

        auto ReadinessFailure(const FCk_Handle_HexGrid& Grid) -> EFailure
        {
            const auto State = UCk_Utils_HexGrid_UE::Get_Readiness(Grid);
            return State.Get_State() == ECk_HexGridReadiness::Ready ? EFailure::None : State.Get_Failure();
        }

        // Resolve into scratch. Validation accumulates every cell for the first failing reason,
        // preserving the canonical ordering supplied by the geometry layer.
        auto Validate(const FCk_Handle_HexGrid& Grid, FView Candidate, const FView* Old,
            const TMap<FCk_HexCoord, FGuid>* Claims, bool Admission) -> FResult
        {
            const auto& Offsets = Candidate.Get_FootprintOffsets();
            if (ValidateOffsets(Offsets) != ECk_HexGridObject_Failure::None ||
                (Candidate.Get_Connectivity() != ECk_HexGridConnectivity::Ignore &&
                 Candidate.Get_Connectivity() != ECk_HexGridConnectivity::RequireConnected))
            { return Reject(EFailure::InvalidFootprint, Old); }

            const auto Shape = ResolveOffsets(Offsets, Candidate.Get_Anchor(), Candidate.Get_Rotation(),
                ECk_HexGridConnectivity::Ignore);
            if (!Shape.Get_Succeeded())
            {
                return Reject(Shape.Get_Failure() == ECk_HexGridObject_Failure::InvalidRotation ?
                    EFailure::InvalidRotation : EFailure::Overflow, Old);
            }
            const auto& Index = Grid.Get<FFragment_HexGrid>().Get_Index();
            const auto* AnchorCell = Index.Find(Candidate.Get_Anchor());
            if (!AnchorCell || !hex_rules::IsLive(*AnchorCell))
            { return Reject(EFailure::AnchorOutsideDomain, Old, {Candidate.Get_Anchor()}); }

            TArray<FCk_HexCoord> Outside, Blocked, WrongTags, Occupied;
            for (const auto& Coord : Shape.Get_Cells())
            {
                const auto* Cell = Index.Find(Coord);
                if (!Cell || !hex_rules::IsLive(*Cell))
                { Outside.Add(Coord); continue; }
                if (Admission)
                {
                    bool Disabled = false;
                    FGameplayTagContainer Tags;
                    if (!FCells::TryGet_IsDisabled(*Cell, Disabled) || !FCells::TryGet_Tags(*Cell, Tags))
                    { return Reject(EFailure::NotReady, Old); }
                    if (Disabled) { Blocked.Add(Coord); }
                    if (!Tags.HasAll(Candidate.Get_RequiredCellTags()) || Tags.HasAny(Candidate.Get_ForbiddenCellTags()))
                    { WrongTags.Add(Coord); }
                }
                if (const auto* Claim = Claims ? Claims->Find(Coord) : nullptr;
                    Claim && (!Old || *Claim != Old->Get_PlacementId()))
                { Occupied.Add(Coord); }
            }
            if (!Outside.IsEmpty()) { return Reject(EFailure::OutsideDomain, Old, Outside); }
            if (!Blocked.IsEmpty()) { return Reject(EFailure::Blocked, Old, Blocked); }
            if (!WrongTags.IsEmpty()) { return Reject(EFailure::TagMismatch, Old, WrongTags); }
            if (Candidate.Get_Connectivity() == ECk_HexGridConnectivity::RequireConnected && !IsConnected(Shape.Get_Cells()))
            { return Reject(EFailure::Disconnected, Old); }
            if (!Occupied.IsEmpty()) { return Reject(EFailure::Occupied, Old, Occupied); }
            Candidate.Set_Cells(Shape.Get_Cells());
            return Success(Candidate);
        }

        template <typename TLink>
        auto OnBeginDestroy(FCk_Handle Entity) -> void
        {
            if (!Entity.Has<TLink>()) { return; }
            const auto& Link = Entity.Get<TLink>();
            hex_rules::EnqueueDeath(Link._Grid, Link._PlacementId, Entity);
        }

        template <typename TLink>
        auto LinkEntity(FCk_Handle Entity, const FCk_Handle_HexGrid& Grid, FGuid Id) -> void
        {
            if (!Entity.Has<TLink>())
            {
                Entity.Add<TLink>(TLink{Grid, Id});
                (void)UUtils_Signal_OnEntityBeginDestroy::Bind<&OnBeginDestroy<TLink>,
                    ECk_Signal_BindingPolicy::IgnorePayloadInFlight, ECk_Signal_PostFireBehavior::DoNothing>(Entity);
            }
            else { Entity.Get<TLink>() = TLink{Grid, Id}; }
        }

        template <typename TLink>
        auto UnlinkEntity(FCk_Handle Entity, const FCk_Handle_HexGrid& Grid, FGuid Id) -> void
        {
            if (!ck::IsValid(Entity) || !Entity.Has<TLink>()) { return; }
            const auto& Link = Entity.Get<TLink>();
            if (Link._Grid != Grid || Link._PlacementId != Id) { return; }
            UUtils_Signal_OnEntityBeginDestroy::Unbind<&OnBeginDestroy<TLink>>(Entity);
            Entity.Remove<TLink>();
        }

        auto CreateHandle(const FCk_Handle_HexGrid& Grid, FGuid Id) -> FCk_Handle_HexGridPlacement
        {
            if (!FLife::Get_CanCreateEntity(Grid)) { return {}; }
            auto Entity = FLife::Request_CreateEntity(Grid);
            if (!hex_rules::IsLive(Entity)) { return {}; }
            LinkEntity<FFragment_HexGridPlacement>(Entity, Grid, Id);
            return UCk_Utils_HexGridPlacement_UE::Cast(Entity);
        }

        auto Unstamp(const FView& Old, FFragment_HexGridOccupancy& State) -> void
        {
            const auto& Index = Old.Get_Grid().Get<FFragment_HexGrid>().Get_Index();
            for (const auto& Coord : Old.Get_Cells())
            {
                const auto* Claim = State._Cells.Find(Coord);
                if (!Claim || *Claim != Old.Get_PlacementId()) { continue; }
                State._Cells.Remove(Coord);
                if (const auto* Found = Index.Find(Coord); Found && ck::IsValid(*Found))
                {
                    auto Cell = *Found;
                    Cell.Try_Remove<FFragment_HexCell_Placement>();
                }
            }
        }

        auto Stamp(const FView& View, FFragment_HexGridOccupancy& State) -> void
        {
            const auto& Index = View.Get_Grid().Get<FFragment_HexGrid>().Get_Index();
            for (const auto& Coord : View.Get_Cells())
            {
                State._Cells.Add(Coord, View.Get_PlacementId());
                auto Cell = Index.FindChecked(Coord);
                Cell.AddOrGet<FFragment_HexCell_Placement>()._PlacementId = View.Get_PlacementId();
            }
            if (hex_rules::IsLive(View.Get_Occupant()))
            { LinkEntity<FFragment_HexGridOccupant_PlacementRef>(View.Get_Occupant(), View.Get_Grid(), View.Get_PlacementId()); }
            State._Placements.Add(View.Get_PlacementId(), View);
            PublishPlacementClaim(View);
        }

        auto Commit(FCk_Handle_HexGrid& Grid, FResult Result) -> FResult
        {
            if (!Result.Get_Succeeded()) { return Result; }
            auto View = Result.Get_Placement();
            const bool Fresh = !View.Get_PlacementId().IsValid();
            const auto* Prior = Fresh ? nullptr : Lookup(Grid, View.Get_PlacementId());
            if (const auto Failure = PreparePlacementAssociation(Grid, View); Failure != EFailure::None)
            { return Reject(Failure, Prior); }
            if (View.Get_OccupantBindingId().IsValid() &&
                !CanClaimPlacement(Grid, View.Get_OccupantBindingId(), View.Get_PlacementId(), false))
            { return Reject(EFailure::OccupantAlreadyPlaced, Prior); }
            if (Fresh)
            {
                auto Id = FGuid::NewGuid();
                while (Lookup(Grid, Id)) { Id = FGuid::NewGuid(); }
                const auto Handle = CreateHandle(Grid, Id);
                if (!hex_rules::IsLive(Handle)) { return Reject(EFailure::InternalFailure); }
                View.Set_PlacementId(Id).Set_Handle(Handle);
            }
            auto& State = Grid.AddOrGet<FFragment_HexGridOccupancy>();
            if (!Fresh)
            {
                const auto& Old = State._Placements.FindChecked(View.Get_PlacementId());
                if (Old.Get_Anchor() == View.Get_Anchor() && Old.Get_Rotation() == View.Get_Rotation() &&
                    Old.Get_Connectivity() == View.Get_Connectivity() &&
                    Old.Get_FootprintOffsets() == View.Get_FootprintOffsets() &&
                    Old.Get_RequiredCellTags() == View.Get_RequiredCellTags() &&
                    Old.Get_ForbiddenCellTags() == View.Get_ForbiddenCellTags())
                { return Result; }
                Unstamp(Old, State);
            }
            Stamp(View, State);
            ++State._Revision;
            ArmBoardReplication(Grid);
            if (Fresh)
            { UUtils_Signal_HexGridPlacement_ObjectPlaced::Broadcast(Grid, ck::MakePayload(Grid, View)); }
            else
            { UUtils_Signal_HexGridPlacement_ObjectMoved::Broadcast(Grid, ck::MakePayload(Grid, View)); }
            return Success(View);
        }

        auto Remove(FCk_Handle_HexGrid& Grid, FGuid Id) -> FResult
        {
            const auto* Found = Lookup(Grid, Id);
            if (!Found) { return Reject(EFailure::InvalidPlacementId).Set_PlacementId(Id); }
            const auto Old = *Found;
            auto& State = Grid.Get<FFragment_HexGridOccupancy>();
            Unstamp(Old, State);
            UnlinkEntity<FFragment_HexGridOccupant_PlacementRef>(Old.Get_Occupant(), Grid, Id);
            UnlinkEntity<FFragment_HexGridPlacement>(Old.Get_Handle(), Grid, Id);
            State._Placements.Remove(Id);
            State._PendingBindingConflicts.Remove(Id);
            ReleasePlacementClaim(Old);
            ++State._Revision;
            ArmBoardReplication(Grid);
            UUtils_Signal_HexGridPlacement_ObjectRemoved::Broadcast(Grid, ck::MakePayload(Grid, Old));
            auto Entity = FCk_Handle{Old.Get_Handle()};
            if (hex_rules::IsLive(Entity)) { FLife::Request_DestroyEntity(Entity); }
            return Success(Old);
        }
    }

    auto PreviewPlace(const FCk_Handle_HexGrid& Grid, const FCk_HexGridPlacement_Spec& Spec) -> FResult
    {
        if (const auto Reason = ReadinessFailure(Grid); Reason != EFailure::None) { return Reject(Reason); }
        const auto Object = Spec.Get_Object();
        if (!hex_rules::IsLive(Object) || !UCk_Utils_HexGridObject_UE::Has(Object))
        { return Reject(EFailure::InvalidObject); }
        const FView* Old = nullptr;
        if (Object.Has<FFragment_HexGridOccupant_PlacementRef>())
        {
            const auto& Link = Object.Get<FFragment_HexGridOccupant_PlacementRef>();
            if (Link._Grid != Grid) { return Reject(EFailure::CrossGridTransfer); }
            Old = Lookup(Grid, Link._PlacementId);
            if (!Old || !hex_rules::IsLive(Old->Get_Handle())) { return Reject(EFailure::InvalidPlacementId, Old); }
        }
        if (Grid.Get_RegistryView().Get_RegistryHandle() != Object.Get_RegistryView().Get_RegistryHandle())
        { return Reject(EFailure::InvalidObject, Old); }
        const auto ObjectSpec = UCk_Utils_HexGridObject_UE::Get_Spec(Object);
        auto Candidate = Old ? *Old : FView{};
        Candidate.Set_Grid(Grid).Set_Occupant(Object).Set_OccupantState(ECk_HexGridOccupantState::Bound)
            .Set_FootprintOffsets(ObjectSpec.Get_FootprintOffsets()).Set_RequiredCellTags(ObjectSpec.Get_RequiredCellTags())
            .Set_ForbiddenCellTags(ObjectSpec.Get_ForbiddenCellTags()).Set_Anchor(Spec.Get_Anchor())
            .Set_Rotation(Spec.Get_Rotation()).Set_Connectivity(Spec.Get_Connectivity());
        return Validate(Grid, Candidate, Old, Grid.Has<FFragment_HexGridOccupancy>() ?
            &Grid.Get<FFragment_HexGridOccupancy>()._Cells : nullptr, true);
    }

    auto PreviewMove(const FCk_Handle_HexGrid& Grid, FGuid Id, const FCk_HexCoord& Anchor, ECk_HexRotation Rotation) -> FResult
    {
        if (const auto Reason = ReadinessFailure(Grid); Reason != EFailure::None) { return Reject(Reason); }
        const auto* Old = Lookup(Grid, Id);
        if (!Old || !hex_rules::IsLive(Old->Get_Handle()))
        { return Reject(EFailure::InvalidPlacementId, Old).Set_PlacementId(Id); }
        if (Old->Get_OccupantState() == ECk_HexGridOccupantState::Bound && !hex_rules::IsLive(Old->Get_Occupant()))
        { return Reject(EFailure::InvalidObject, Old); }
        auto Candidate = *Old;
        Candidate.Set_Anchor(Anchor).Set_Rotation(Rotation);
        return Validate(Grid, Candidate, Old, &Grid.Get<FFragment_HexGridOccupancy>()._Cells, true);
    }

    auto ApplyPlacement(FCk_Handle_HexGrid& Grid, const FCk_Request_HexGridPlacement_Place& Request) -> FResult
    { return Commit(Grid, PreviewPlace(Grid, Request.Get_Spec())); }

    auto ApplyPlacement(FCk_Handle_HexGrid& Grid, const FCk_Request_HexGridPlacement_Move& Request) -> FResult
    { return Commit(Grid, PreviewMove(Grid, Request.Get_PlacementId(), Request.Get_Anchor(), Request.Get_Rotation())); }

    auto ApplyPlacement(FCk_Handle_HexGrid& Grid, const FCk_Request_HexGridPlacement_Remove& Request) -> FResult
    { return Remove(Grid, Request.Get_PlacementId()); }

    auto ApplyDeath(FCk_Handle_HexGrid& Grid, const FHexPlacementDeath& Death) -> void
    {
        const auto* Old = Lookup(Grid, Death._PlacementId);
        if (Old && (Old->Get_Occupant() == Death._ExpectedHandle || Old->Get_Handle() == Death._ExpectedHandle))
        { Remove(Grid, Death._PlacementId); }
    }

    auto ClearPlacements(FCk_Handle InBoard) -> void
    {
        if (!ck::IsValid(InBoard) || !InBoard.Has<FFragment_HexGridOccupancy>()) { return; }
        auto Grid = UCk_Utils_HexGrid_UE::Cast(InBoard);
        auto& State = InBoard.Get<FFragment_HexGridOccupancy>();
        for (const auto& Pair : State._Placements)
        {
            const auto& Old = Pair.Value;
            ReleasePlacementClaim(Old);
            Unstamp(Old, State);
            UnlinkEntity<FFragment_HexGridOccupant_PlacementRef>(Old.Get_Occupant(), Grid, Pair.Key);
            UnlinkEntity<FFragment_HexGridPlacement>(Old.Get_Handle(), Grid, Pair.Key);
        }
        State._Placements.Reset();
        State._PendingBindingConflicts.Reset();
    }

    auto TryReplacePlacements(FCk_Handle_HexGrid& Grid, const TArray<FView>& Placements,
        TArray<FCk_Handle>& OutRetired) -> FResult
    {
        OutRetired.Reset();
        if (!hex_rules::IsLive(Grid) || !UCk_Utils_HexGrid_UE::Has(Grid))
        { return Reject(EFailure::InvalidGrid); }
        TArray<FView> Staged;
        TSet<FGuid> Ids;
        TSet<FCk_Handle> Occupants;
        TSet<FGuid> BindingIds;
        TMap<FCk_HexCoord, FGuid> Claims;
        Staged.Reserve(Placements.Num());
        for (auto Candidate : Placements)
        {
            const auto Id = Candidate.Get_PlacementId();
            if (!Id.IsValid()) { return Reject(EFailure::InvalidPlacementId); }
            if (Ids.Contains(Id)) { return Reject(EFailure::DuplicatePlacementId).Set_PlacementId(Id); }
            Ids.Add(Id);
            if (Candidate.Get_Grid() != FCk_Handle{} && Candidate.Get_Grid() != Grid)
            { return Reject(EFailure::InvalidProjection).Set_PlacementId(Id); }
            const auto* Old = Lookup(Grid, Id);
            if (Candidate.Get_Handle() != FCk_Handle{} && (!Old || Candidate.Get_Handle() != Old->Get_Handle()))
            { return Reject(EFailure::InvalidProjection).Set_PlacementId(Id); }
            Candidate.Set_Grid(Grid).Set_Handle(Old ? Old->Get_Handle() : FCk_Handle_HexGridPlacement{});
            if (Old && !hex_rules::IsLive(Old->Get_Handle()))
            { return Reject(EFailure::InvalidPlacementId, Old); }

            if (const auto Failure = PreparePlacementAssociation(Grid, Candidate); Failure != EFailure::None)
            { return Reject(Failure).Set_PlacementId(Id); }
            const auto Key = Candidate.Get_OccupantBindingId();
            if (Key.IsValid())
            {
                if (BindingIds.Contains(Key) || !CanClaimPlacement(Grid, Key, Id, true))
                { return Reject(EFailure::OccupantAlreadyPlaced).Set_PlacementId(Id); }
                BindingIds.Add(Key);
            }
            const auto Occupant = Candidate.Get_Occupant();
            switch (Candidate.Get_OccupantState())
            {
            case ECk_HexGridOccupantState::Bound:
                if (!hex_rules::IsLive(Occupant) || !UCk_Utils_HexGridObject_UE::Has(Occupant) ||
                    Occupant.Get_RegistryView().Get_RegistryHandle() != Grid.Get_RegistryView().Get_RegistryHandle())
                { return Reject(EFailure::InvalidObject).Set_PlacementId(Id); }
                if (Occupant.Has<FFragment_HexGridOccupant_PlacementRef>() &&
                    Occupant.Get<FFragment_HexGridOccupant_PlacementRef>()._Grid != Grid)
                { return Reject(EFailure::CrossGridTransfer).Set_PlacementId(Id); }
                if (Occupants.Contains(Occupant)) { return Reject(EFailure::OccupantAlreadyPlaced).Set_PlacementId(Id); }
                Occupants.Add(Occupant);
                break;
            case ECk_HexGridOccupantState::Anonymous:
            case ECk_HexGridOccupantState::AwaitingBinding:
            case ECk_HexGridOccupantState::BindingConflict:
                if (Occupant != FCk_Handle{}) { return Reject(EFailure::InvalidProjection).Set_PlacementId(Id); }
                break;
            default:
                return Reject(EFailure::InvalidProjection).Set_PlacementId(Id);
            }

            // Rules may change after admission without evicting placements. Restore structural
            // invariants against the staged claims independently of the current blocked/tag state.
            const auto Valid = Validate(Grid, Candidate, nullptr, &Claims, false);
            if (!Valid.Get_Succeeded()) { return FResult{Valid}.Set_PlacementId(Id); }
            if (Valid.Get_Placement().Get_Cells() != Candidate.Get_Cells())
            { return Reject(EFailure::InvalidProjection).Set_PlacementId(Id); }
            for (const auto& Coord : Candidate.Get_Cells()) { Claims.Add(Coord, Id); }
            Staged.Add(MoveTemp(Candidate));
        }

        // Allocate outside the board's lifetime tree. A failed later allocation cannot expose
        // earlier staging entities through a board dependent query or a lifetime callback.
        if (!FLife::Get_CanCreateEntity(Grid)) { return Reject(EFailure::InternalFailure); }
        auto Registry = Grid.Get_RegistryView();
        TMap<FGuid, FCk_Handle> Provisional;
        for (const auto& Candidate : Staged)
        {
            if (ck::IsValid(Candidate.Get_Handle())) { continue; }
            auto Entity = FLife::Request_CreateEntity(Registry);
            if (!hex_rules::IsLive(Entity))
            {
                for (auto& Pair : Provisional) { FLife::Request_DestroyEntity(Pair.Value); }
                return Reject(EFailure::InternalFailure).Set_PlacementId(Candidate.Get_PlacementId());
            }
            Provisional.Add(Candidate.Get_PlacementId(), Entity);
        }
        for (auto& Candidate : Staged)
        {
            if (auto* Entity = Provisional.Find(Candidate.Get_PlacementId()))
            {
                FLife::Request_SetupEntityWithLifetimeOwner(*Entity, Grid);
                LinkEntity<FFragment_HexGridPlacement>(*Entity, Grid, Candidate.Get_PlacementId());
                Candidate.Set_Handle(UCk_Utils_HexGridPlacement_UE::Cast(*Entity));
            }
        }

        // No more fallible operations. Remove obsolete watches and projections first, so two
        // occupants can exchange placements without seeing each other's old backreferences.
        auto& State = Grid.AddOrGet<FFragment_HexGridOccupancy>();
        for (const auto& Pair : State._Placements)
        {
            const auto& Old = Pair.Value;
            ReleasePlacementClaim(Old);
            Unstamp(Old, State);
            UnlinkEntity<FFragment_HexGridOccupant_PlacementRef>(Old.Get_Occupant(), Grid, Pair.Key);
            if (!Ids.Contains(Pair.Key))
            {
                UnlinkEntity<FFragment_HexGridPlacement>(Old.Get_Handle(), Grid, Pair.Key);
                OutRetired.Add(Old.Get_Handle());
            }
        }
        State._Placements.Reset();
        for (const auto& Candidate : Staged) { Stamp(Candidate, State); }
        for (auto It = State._PendingBindingConflicts.CreateIterator(); It; ++It)
        {
            const auto* Current = State._Placements.Find(*It);
            if (!Current || Current->Get_OccupantState() != ECk_HexGridOccupantState::BindingConflict)
            { It.RemoveCurrent(); }
        }
        ++State._Revision;
        return FResult{}.Set_Succeeded(true).Set_Failure(EFailure::None);
    }

    auto ResolvePlacementOccupant(FCk_Handle_HexGrid Grid, FGuid Id, bool InIdentityReleased) -> void
    {
        if (!hex_rules::IsLive(Grid)) { return; }
        const auto* Old = Lookup(Grid, Id);
        if (!Old || !hex_rules::IsLive(Old->Get_Handle()) ||
            Old->Get_OccupantState() != ECk_HexGridOccupantState::AwaitingBinding) { return; }
        auto Updated = *Old;
        auto Failure = PreparePlacementAssociation(Grid, Updated);
        const auto Object = Updated.Get_Occupant();
        if (hex_rules::IsLive(Object) && Object.Has<FFragment_HexGridOccupant_PlacementRef>())
        {
            const auto& Ref = Object.Get<FFragment_HexGridOccupant_PlacementRef>();
            if (Ref._Grid != Grid || Ref._PlacementId != Id) { Failure = EFailure::LateOccupantConflict; }
        }
        if (Failure != EFailure::None && !InIdentityReleased) { return; }
        if (Failure == EFailure::None && Updated.Get_OccupantState() != ECk_HexGridOccupantState::Bound) { return; }
        auto& State = Grid.Get<FFragment_HexGridOccupancy>();
        if (Failure != EFailure::None)
        {
            Updated.Set_Occupant({}).Set_OccupantState(ECk_HexGridOccupantState::BindingConflict);
            State._PendingBindingConflicts.Add(Id);
        }
        Stamp(Updated, State);
        ++State._Revision;
        ArmBoardReplication(Grid);
        FlushBindingConflicts(Grid);
    }

    void SuspendReplicaOccupant(FCk_Handle_HexGrid Grid, FGuid Id, FCk_Handle Object, FGuid Key)
    {
        if (!hex_rules::IsLive(Grid)) { return; }
        const auto* Old = Lookup(Grid, Id);
        if (!Old || Old->Get_OccupantState() != ECk_HexGridOccupantState::Bound ||
            Old->Get_OccupantBindingId() != Key || Old->Get_Occupant() != Object || !hex_rules::IsLive(Old->Get_Handle())) { return; }
        auto Reservation = *Old;
        Reservation.Set_Occupant({}).Set_ExpectedOccupant({}).Set_OccupantState(ECk_HexGridOccupantState::AwaitingBinding);
        UnlinkEntity<FFragment_HexGridOccupant_PlacementRef>(Object, Grid, Id);
        auto& State = Grid.Get<FFragment_HexGridOccupancy>();
        State._Placements.FindChecked(Id) = Reservation;
        PublishPlacementClaim(Reservation);
        ++State._Revision;
    }

    auto FlushBindingConflicts(FCk_Handle_HexGrid Grid) -> void
    {
        if (!hex_rules::IsLive(Grid) || !Grid.Has<FFragment_HexGridOccupancy>() ||
            !UCk_Utils_HexGrid_UE::Get_IsReady(Grid)) { return; }
        auto Pending = MoveTemp(Grid.Get<FFragment_HexGridOccupancy>()._PendingBindingConflicts);
        for (const auto Id : Pending)
        {
            if (!hex_rules::IsLive(Grid)) { return; }
            const auto* View = Lookup(Grid, Id);
            if (!View || View->Get_OccupantState() != ECk_HexGridOccupantState::BindingConflict) { continue; }
            const auto BindingId = View->Get_OccupantBindingId();
            UUtils_Signal_HexGridPlacement_BindingConflict::Broadcast(Grid,
                ck::MakePayload(Grid, Reject(EFailure::LateOccupantConflict, View)));
            if (!hex_rules::IsLive(Grid)) { return; }
            const auto* Current = Lookup(Grid, Id);
            if (Current && Current->Get_OccupantBindingId() == BindingId &&
                Current->Get_OccupantState() == ECk_HexGridOccupantState::BindingConflict)
            {
                UUtils_Signal_HexGrid_OccupantBindingFailed::Broadcast(Grid,
                    ck::MakePayload(Grid, Id, EFailure::LateOccupantConflict));
            }
        }
    }
}
