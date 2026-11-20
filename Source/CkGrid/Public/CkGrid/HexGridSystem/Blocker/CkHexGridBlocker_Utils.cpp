#include "CkHexGridBlocker_Utils.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"

#include "CkHexGridBlocker_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkRecord/Record/CkRecord_Utils.h"

namespace
{
    using Utils = UCk_Utils_HexGridBlocker_UE;
    using Lifetime = UCk_Utils_EntityLifetime_UE;
    using Record = ck::FHexBlockerRecord;

}
namespace ck::hex_grid
{
    auto InstallBlocker(FCk_Handle Entity, FCk_Handle Board, FGuid Id, FGameplayTag Name, TArray<FCk_HexCoord> Coordinates,
        TArray<FCk_Handle_HexCell> Cells, bool OwnsEntity) -> FCk_Handle_HexGridBlocker
    {
        Entity.Add<ck::FFragment_HexGridBlocker>(ck::FFragment_HexGridBlocker{
            Board, Name, MoveTemp(Coordinates), MoveTemp(Cells), false, Id, OwnsEntity});
        auto Blocker = Utils::Cast(Entity);
        if (Name.IsValid()) { UCk_Utils_GameplayLabel_UE::Add(Entity, Name); }
        Record::AddIfMissing(Board);
        Record::Request_Connect(Board, Blocker, ECk_Record_LabelRequirementPolicy::Optional);
        return Blocker;
    }

}

namespace
{
    auto Compose(FCk_Handle Owner, const FCk_HexGridBlocker_Spec& Spec, bool Create,
        FCk_Handle_HexGridBlocker& OutBlocker) -> ECk_HexGridBlocker_Failure
    {
        OutBlocker = {};
        const auto Board = Spec.Get_Grid();
        if (!IsInGameThread()) { return ECk_HexGridBlocker_Failure::Unavailable; }
        if (!ck::hex_rules::IsLive(Board) || !UCk_Utils_HexGrid_UE::Has(Board)) { return ECk_HexGridBlocker_Failure::InvalidGrid; }
        if (!ck::hex_rules::IsLive(Owner) || Owner.Get_RegistryView().Get_RegistryHandle() != Board.Get_RegistryView().Get_RegistryHandle())
        { return ECk_HexGridBlocker_Failure::InvalidOwner; }
        if (!Create && Utils::Has(Owner)) { return ECk_HexGridBlocker_Failure::AlreadyComposed; }
        if (!UCk_Utils_Net_UE::Get_HasAuthority(Board)) { return ECk_HexGridBlocker_Failure::NotAuthority; }
        if (!ck::hex_rules::CanWrite(Board) || !Lifetime::Get_CanCreateEntity(Owner)) { return ECk_HexGridBlocker_Failure::Unavailable; }
        const auto Name = Spec.Get_Name();
        if (Name.IsValid() && ck::IsValid(Utils::Get_BlockerWithTag(Board, Name))) { return ECk_HexGridBlocker_Failure::DuplicateName; }
        if (!Create && Name.IsValid() && UCk_Utils_GameplayLabel_UE::Has(Owner)
            && UCk_Utils_GameplayLabel_UE::Get_Label(Owner) != Name) { return ECk_HexGridBlocker_Failure::InvalidOwner; }
        if (Spec.Get_Cells().IsEmpty()) { return ECk_HexGridBlocker_Failure::EmptyCells; }
        TSet<FCk_HexCoord> Seen;
        TArray<FCk_Handle_HexCell> Cells;
        for (const auto& Coordinate : Spec.Get_Cells())
        {
            if (Seen.Contains(Coordinate)) { return ECk_HexGridBlocker_Failure::DuplicateCell; }
            FCk_Handle_HexCell Cell;
            ECk_HexBoardFailure Reason;
            if (!UCk_Utils_HexGrid_UE::TryGet_CellAt(Board, Coordinate, Cell, Reason) || !ck::hex_rules::IsLive(Cell))
            { return ECk_HexGridBlocker_Failure::OutsideDomain; }
            Cells.Add(Cell);
            Seen.Add(Coordinate);
        }
        auto Entity = Create ? Lifetime::Request_CreateEntity(Owner) : Owner;
        if (!ck::hex_rules::IsLive(Entity)) { return ECk_HexGridBlocker_Failure::CompositionFailed; }
        // No external callback between validation and enqueue. Queue initial activation with
        // the same ownership as a subsequent public toggle.
        OutBlocker = ck::hex_grid::InstallBlocker(Entity, Board, FGuid::NewGuid(), Name, Spec.Get_Cells(), MoveTemp(Cells), Create);
        ck::hex_grid::ArmBoardReplication(Board);
        if (Spec.Get_InitiallyActive())
        {
            auto Host = FCk_Handle{Board};
            Host.AddOrGet<ck::FFragment_HexGrid_RuleRequests>()._Commands.Add(
                ck::FHexRuleCommand{OutBlocker, FCk_Request_HexGridBlocker_SetActive{true}});
        }
        return ECk_HexGridBlocker_Failure::None;
    }
}

auto
    UCk_Utils_HexGridBlocker_UE::
    TryAdd(
        FCk_Handle& InHandle,
        const FCk_HexGridBlocker_Spec& InSpec,
        FCk_Handle_HexGridBlocker& OutBlocker,
        ECk_HexGridBlocker_Failure& OutFailure)
    -> bool
{
    OutFailure = Compose(InHandle, InSpec, false, OutBlocker);
    return OutFailure == ECk_HexGridBlocker_Failure::None;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    TryCreate(
        FCk_Handle& InOwner,
        const FCk_HexGridBlocker_Spec& InSpec,
        FCk_Handle_HexGridBlocker& OutBlocker,
        ECk_HexGridBlocker_Failure& OutFailure)
    -> bool
{
    OutFailure = Compose(InOwner, InSpec, true, OutBlocker);
    return OutFailure == ECk_HexGridBlocker_Failure::None;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Has(
        const FCk_Handle& InHandle)
    -> bool
{
    return ck::IsValid(InHandle) && InHandle.Has<ck::FFragment_HexGridBlocker>();
}

auto
    UCk_Utils_HexGridBlocker_UE::
    DoCast(
        FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult)
    -> FCk_Handle_HexGridBlocker
{
    const auto Blocker = Cast(InHandle);
    OutResult = ck::IsValid(Blocker) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Blocker;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    DoCastChecked(
        FCk_Handle InHandle)
    -> FCk_Handle_HexGridBlocker
{ return CastChecked(InHandle); }

auto
    UCk_Utils_HexGridBlocker_UE::
    Get_Grid(
        const FCk_Handle_HexGridBlocker& InBlocker)
    -> FCk_Handle_HexGrid
{
    if (!ck::hex_rules::IsLive(InBlocker) || !Has(InBlocker)) { return {}; }
    const auto& Board = InBlocker.Get<ck::FFragment_HexGridBlocker>()._Board;
    return ck::hex_rules::IsLive(Board) ? UCk_Utils_HexGrid_UE::Cast(Board) : FCk_Handle_HexGrid{};
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Get_BlockedCells(
        const FCk_Handle_HexGridBlocker& InBlocker)
    -> TArray<FCk_HexCoord>
{
    if (!UCk_Utils_HexGrid_UE::Get_IsReady(Get_Grid(InBlocker))) { return {}; }
    return InBlocker.Get<ck::FFragment_HexGridBlocker>()._Coordinates;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Get_IsActive(
        const FCk_Handle_HexGridBlocker& InBlocker)
    -> bool
{
    return UCk_Utils_HexGrid_UE::Get_IsReady(Get_Grid(InBlocker)) && InBlocker.Get<ck::FFragment_HexGridBlocker>()._Active;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Get_Blockers(
        const FCk_Handle_HexGrid& InGrid)
    -> TArray<FCk_Handle_HexGridBlocker>
{
    if (!UCk_Utils_HexGrid_UE::Get_IsReady(InGrid) || !Record::Has(InGrid)) { return {}; }
    auto Entries = Record::Get_ValidEntries(InGrid);
    Entries.RemoveAll([&](const auto& Entry){ return !ck::hex_rules::IsLive(Entry) || Get_Grid(Entry) != InGrid; });
    return Entries;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Get_BlockerWithTag(
        const FCk_Handle_HexGrid& InGrid,
        FGameplayTag InTag)
    -> FCk_Handle_HexGridBlocker
{
    if (!InTag.IsValid()) { return {}; }
    for (const auto& Entry : Get_Blockers(InGrid))
    {
        if (Entry.Get<ck::FFragment_HexGridBlocker>()._Name == InTag) { return Entry; }
    }
    return {};
}

auto
    UCk_Utils_HexGridBlocker_UE::
    Request_SetActive(
        FCk_Handle_HexGridBlocker& InBlocker,
        bool InActive,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_HexGridBlocker
{
    auto Request = FCk_Request_HexGridBlocker_SetActive{InActive};
    Request.Set_CompletionDelegate(InDelegate);
    ck::hex_rules::Enqueue(Get_Grid(InBlocker), InBlocker, MoveTemp(Request));
    return InBlocker;
}

auto
    UCk_Utils_HexGridBlocker_UE::
    TryInstallAuthored(
        FCk_Handle& InBoard,
        const TMap<FCk_HexCoord,FCk_Handle_HexCell>& InIndex,
        const TArray<FCk_HexGridAuthoredBlocker>& InDeclarations)
    -> bool
{
    TArray<TArray<FCk_Handle_HexCell>> Resolved;
    TArray<FGuid> Ids;
    TSet<FGuid> SeenIds;
    for (const auto& Declaration : InDeclarations)
    {
        const auto Name = Declaration.Get_Name();
        const auto Id = Name.IsValid() ? FGuid::NewDeterministicGuid(Name.ToString()) : FGuid::NewGuid();
        if (!Id.IsValid() || SeenIds.Contains(Id)) { return false; }
        SeenIds.Add(Id);
        Ids.Add(Id);
        auto& Cells = Resolved.AddDefaulted_GetRef();
        for (const auto& Coord : Declaration.Get_Cells())
        {
            const auto* Cell = InIndex.Find(Coord);
            if (!Cell || !ck::hex_rules::IsLive(*Cell)) { return false; }
            Cells.Add(*Cell);
        }
    }
    TArray<FCk_Handle> Children;
    for (int32 Idx = 0; Idx < InDeclarations.Num(); ++Idx)
    {
        auto Child = Lifetime::Request_CreateEntity(InBoard);
        if (!ck::hex_rules::IsLive(Child))
        {
            for (auto& Prior : Children) { Lifetime::Request_DestroyEntity(Prior); }
            return false;
        }
        Children.Add(Child);
    }
    for (int32 Idx = 0; Idx < InDeclarations.Num(); ++Idx)
    {
        const auto& Declaration = InDeclarations[Idx];
        const auto Name = Declaration.Get_Name();
        auto Blocker = ck::hex_grid::InstallBlocker(Children[Idx], InBoard, Ids[Idx], Name,
            Declaration.Get_Cells(), MoveTemp(Resolved[Idx]), true);
        if (Declaration.Get_InitiallyActive()) { ck::hex_rules::SetActive(Blocker, true); }
    }
    return true;
}
