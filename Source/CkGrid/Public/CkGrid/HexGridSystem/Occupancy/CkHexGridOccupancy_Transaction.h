#pragma once

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Fragment_Data.h"

namespace ck { struct FHexPlacementDeath; }

namespace ck::hex_grid
{

    auto PreviewPlace(const FCk_Handle_HexGrid& InGrid, const FCk_HexGridPlacement_Spec& InSpec) -> FCk_HexGridPlacement_Result;
    auto PreviewMove(const FCk_Handle_HexGrid& InGrid, FGuid InId, const FCk_HexCoord& InAnchor, ECk_HexRotation InRotation) -> FCk_HexGridPlacement_Result;
    auto ApplyPlacement(FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Place& InRequest) -> FCk_HexGridPlacement_Result;
    auto ApplyPlacement(FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Move& InRequest) -> FCk_HexGridPlacement_Result;
    auto ApplyPlacement(FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Remove& InRequest) -> FCk_HexGridPlacement_Result;
    auto ApplyDeath(FCk_Handle_HexGrid& InGrid, const FHexPlacementDeath& InDeath) -> void;
    auto ClearPlacements(FCk_Handle InBoard) -> void;
    auto ReadPlacements(const FCk_Handle_HexGrid& InGrid) -> TArray<FCk_HexGridPlacement_View>;
    auto TryReplacePlacements(FCk_Handle_HexGrid& InGrid, const TArray<FCk_HexGridPlacement_View>& InPlacements,
        TArray<FCk_Handle>& OutRetired) -> FCk_HexGridPlacement_Result;
}
