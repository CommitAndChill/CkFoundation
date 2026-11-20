#pragma once

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Fragment_Data.h"

namespace ck
{
    struct FFragment_HexGridOccupancy
    {
        TMap<FGuid, FCk_HexGridPlacement_View> _Placements;
        TMap<FCk_HexCoord, FGuid> _Cells;
        TSet<FGuid> _PendingBindingConflicts;
        uint64 _Revision = 0;
    };

    struct FFragment_HexGridPlacement
    {
        FCk_Handle_HexGrid _Grid;
        FGuid _PlacementId;
    };

    struct FFragment_HexGridOccupant_PlacementRef
    {
        FCk_Handle_HexGrid _Grid;
        FGuid _PlacementId;
    };

    struct FFragment_HexCell_Placement
    {
        FGuid _PlacementId;
    };
}
