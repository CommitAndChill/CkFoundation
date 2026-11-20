#pragma once

#include "CkHexGridPlacement_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Macros.h"

namespace ck
{
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGrid_OccupantBindingFailed,
        FCk_Delegate_HexGrid_OccupantBindingFailed, FCk_Handle_HexGrid, FGuid, ECk_HexGridFailure);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGridPlacement_BindingConflict,
        FCk_Delegate_HexGridPlacement_BindingConflict, FCk_Handle_HexGrid, FCk_HexGridPlacement_Result);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGridPlacement_ObjectPlaced,
        FCk_Delegate_HexGridPlacement_ObjectPlaced, FCk_Handle_HexGrid, FCk_HexGridPlacement_View);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGridPlacement_ObjectMoved,
        FCk_Delegate_HexGridPlacement_ObjectMoved, FCk_Handle_HexGrid, FCk_HexGridPlacement_View);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGridPlacement_ObjectRemoved,
        FCk_Delegate_HexGridPlacement_ObjectRemoved, FCk_Handle_HexGrid, FCk_HexGridPlacement_View);
}
