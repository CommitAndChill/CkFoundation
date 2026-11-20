#include "CkHexGridOccupancy_Utils.h"

#include "CkHexGridOccupancy_Fragment.h"
#include "CkHexGridOccupancy_Transaction.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"

namespace ck::hex_grid
{
    auto ReadPlacements(const FCk_Handle_HexGrid& InGrid) -> TArray<FCk_HexGridPlacement_View>
    {
        TArray<FCk_HexGridPlacement_View> Views;
        if (ck::IsValid(InGrid) && InGrid.Has<FFragment_HexGridOccupancy>())
        {
            InGrid.Get<FFragment_HexGridOccupancy>()._Placements.GenerateValueArray(Views);
            Views.Sort([](const auto& A, const auto& B) { return A.Get_PlacementId() < B.Get_PlacementId(); });
        }
        return Views;
    }
}

FCk_HexGridPlacements_Result UCk_Utils_HexGridOccupancy_UE::Get_Placements(const FCk_Handle_HexGrid& InGrid)
{
    const auto State = UCk_Utils_HexGrid_UE::Get_Readiness(InGrid);
    if (State.Get_State() != ECk_HexGridReadiness::Ready)
    { return FCk_HexGridPlacements_Result{}.Set_Failure(State.Get_Failure()); }
    return FCk_HexGridPlacements_Result{}.Set_Succeeded(true).Set_Failure(ECk_HexGridFailure::None)
        .Set_Placements(ck::hex_grid::ReadPlacements(InGrid));
}

FCk_HexGridPlacement_Result UCk_Utils_HexGridOccupancy_UE::TryGet_Placement(const FCk_Handle_HexGrid& InGrid, FGuid InPlacementId)
{
    auto Result = FCk_HexGridPlacement_Result{}.Set_PlacementId(InPlacementId);
    const auto State = UCk_Utils_HexGrid_UE::Get_Readiness(InGrid);
    if (State.Get_State() != ECk_HexGridReadiness::Ready)
    { return Result.Set_Failure(State.Get_Failure()); }
    if (InGrid.Has<ck::FFragment_HexGridOccupancy>())
    {
        if (const auto* View = InGrid.Get<ck::FFragment_HexGridOccupancy>()._Placements.Find(InPlacementId))
        { return Result.Set_Placement(*View).Set_Failure(ECk_HexGridFailure::None).Set_Succeeded(true); }
    }
    return Result.Set_Failure(ECk_HexGridFailure::InvalidPlacementId);
}

FCk_HexGridPlacement_Result UCk_Utils_HexGridOccupancy_UE::TryGet_PlacementAt(const FCk_Handle_HexGrid& InGrid,
    const FCk_HexCoord& InCoordinate)
{
    const auto State = UCk_Utils_HexGrid_UE::Get_Readiness(InGrid);
    if (State.Get_State() != ECk_HexGridReadiness::Ready)
    { return FCk_HexGridPlacement_Result{}.Set_Failure(State.Get_Failure()); }
    FGuid Id;
    if (InGrid.Has<ck::FFragment_HexGridOccupancy>())
    {
        if (const auto* Found = InGrid.Get<ck::FFragment_HexGridOccupancy>()._Cells.Find(InCoordinate))
        { Id = *Found; }
    }
    return TryGet_Placement(InGrid, Id);
}

FCk_HexGridPlacement_Result UCk_Utils_HexGridOccupancy_UE::TryGet_PlacementForOccupant(const FCk_Handle_HexGridObject& InObject)
{
    if (!ck::hex_rules::IsLive(InObject) || !InObject.Has<ck::FFragment_HexGridOccupant_PlacementRef>())
    { return FCk_HexGridPlacement_Result{}.Set_Failure(ECk_HexGridFailure::InvalidPlacementId); }
    const auto& Link = InObject.Get<ck::FFragment_HexGridOccupant_PlacementRef>();
    const auto State = UCk_Utils_HexGrid_UE::Get_Readiness(Link._Grid);
    if (State.Get_State() != ECk_HexGridReadiness::Ready)
    { return FCk_HexGridPlacement_Result{}.Set_Failure(State.Get_Failure()); }
    return TryGet_Placement(Link._Grid, Link._PlacementId);
}
