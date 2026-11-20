#include "CkHexCell_Utils.h"

#include "CkHexCell_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"

auto
    UCk_Utils_HexCell_UE::
    Has(
        const FCk_Handle& InHandle)
    -> bool
{
    return ck::IsValid(InHandle) && InHandle.Has_All<ck::FFragment_HexCell_Identity, ck::FFragment_HexCell_Tags>();
}

auto
    UCk_Utils_HexCell_UE::
    DoCast(
        FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult)
    -> FCk_Handle_HexCell
{
    const auto Handle = Cast(InHandle);
    OutResult = ck::IsValid(Handle) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Handle;
}

auto
    UCk_Utils_HexCell_UE::
    DoCastChecked(
        FCk_Handle InHandle)
    -> FCk_Handle_HexCell
{
    return CastChecked(InHandle);
}

auto
    UCk_Utils_HexCell_UE::
    TryGet_Grid(
        const FCk_Handle_HexCell& InCell,
        FCk_Handle_HexGrid& OutGrid)
    -> bool
{
    OutGrid = {};
    if (NOT Has(InCell))
    { return false; }

    const auto& Identity = InCell.Get<ck::FFragment_HexCell_Identity>();
    auto Grid = UCk_Utils_HexGrid_UE::Cast(Identity.Get_Grid());
    if (ck::Is_NOT_Valid(Grid))
    { return false; }

    const auto* Member = Grid.Get<ck::FFragment_HexGrid>().Get_Index().Find(Identity.Get_Coordinate());
    if (Member == nullptr || *Member != InCell)
    { return false; }

    OutGrid = Grid;
    return true;
}

auto
    UCk_Utils_HexCell_UE::
    TryGet_Coordinate(
        const FCk_Handle_HexCell& InCell,
        FCk_HexCoord& OutCoordinate)
    -> bool
{
    OutCoordinate = {};
    auto Grid = FCk_Handle_HexGrid{};
    if (NOT TryGet_Grid(InCell, Grid))
    { return false; }
    OutCoordinate = InCell.Get<ck::FFragment_HexCell_Identity>().Get_Coordinate();
    return true;
}

auto
    UCk_Utils_HexCell_UE::
    TryGet_Tags(
        const FCk_Handle_HexCell& InCell,
        FGameplayTagContainer& OutTags)
    -> bool
{
    OutTags.Reset();
    auto Grid = FCk_Handle_HexGrid{};
    if (NOT TryGet_Grid(InCell, Grid))
    { return false; }
    if (!UCk_Utils_HexGrid_UE::Get_IsReady(Grid)) { return false; }
    OutTags = Grid.Get<ck::FFragment_HexGrid_Params>().Get_DefaultCellTags();
    OutTags.AppendTags(InCell.Get<ck::FFragment_HexCell_Tags>().Get_Tags());
    return true;
}

auto
    UCk_Utils_HexCell_UE::
    TryGet_IsDisabled(
        const FCk_Handle_HexCell& InCell,
        bool& OutIsDisabled)
    -> bool
{
    OutIsDisabled = false;
    auto Grid = FCk_Handle_HexGrid{};
    if (NOT TryGet_Grid(InCell, Grid))
    { return false; }
    if (!UCk_Utils_HexGrid_UE::Get_IsReady(Grid)) { return false; }
    OutIsDisabled = InCell.Has<ck::FTag_HexCell_Disabled>();
    return true;
}

auto
    UCk_Utils_HexCell_UE::
    TryGet_LocalTags(
        const FCk_Handle_HexCell& InCell,
        FGameplayTagContainer& OutTags)
    -> bool
{
    OutTags.Reset();
    auto Board = FCk_Handle_HexGrid{};
    if (!TryGet_Grid(InCell, Board) || !UCk_Utils_HexGrid_UE::Get_IsReady(Board)) { return false; }
    OutTags = InCell.Get<ck::FFragment_HexCell_Tags>().Get_Tags();
    return true;
}

auto
    UCk_Utils_HexCell_UE::
    Request_SetBlocked(
        FCk_Handle_HexCell& InCell,
        bool InBlocked,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_HexCell
{
    auto Request = FCk_Request_HexCell_SetBlocked{InBlocked};
    Request.Set_CompletionDelegate(InDelegate);
    auto Board = FCk_Handle_HexGrid{};
    if (!TryGet_Grid(InCell, Board))
    {
        Request.TryFireCompletion(InCell, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InCell;
    }
    ck::hex_rules::Enqueue(Board, InCell, MoveTemp(Request));
    return InCell;
}

auto
    UCk_Utils_HexCell_UE::
    Request_AddTag(
        FCk_Handle_HexCell& InCell,
        FGameplayTag InTag,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_HexCell
{
    auto Request = FCk_Request_HexCell_AddTag{InTag};
    Request.Set_CompletionDelegate(InDelegate);
    auto Board = FCk_Handle_HexGrid{};
    if (!InTag.IsValid() || !TryGet_Grid(InCell, Board))
    {
        Request.TryFireCompletion(InCell, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InCell;
    }
    ck::hex_rules::Enqueue(Board, InCell, MoveTemp(Request));
    return InCell;
}

auto
    UCk_Utils_HexCell_UE::
    Request_RemoveTag(
        FCk_Handle_HexCell& InCell,
        FGameplayTag InTag,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_HexCell
{
    auto Request = FCk_Request_HexCell_RemoveTag{InTag};
    Request.Set_CompletionDelegate(InDelegate);
    auto Board = FCk_Handle_HexGrid{};
    if (!InTag.IsValid() || !TryGet_Grid(InCell, Board))
    {
        Request.TryFireCompletion(InCell, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InCell;
    }
    ck::hex_rules::Enqueue(Board, InCell, MoveTemp(Request));
    return InCell;
}
