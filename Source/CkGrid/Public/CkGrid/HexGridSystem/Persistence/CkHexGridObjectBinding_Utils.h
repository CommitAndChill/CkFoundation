#pragma once

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Result_Data.h"

namespace ck::hex_grid
{
    CKGRID_API auto PreparePlacementAssociation(const FCk_Handle_HexGrid& InGrid,
        FCk_HexGridPlacement_View& InOutPlacement) -> ECk_HexGridFailure;
    auto RegisterObjectBinding(FCk_Handle InObject, FGuid InBindingId) -> bool;
    auto ResolveObjectBinding(FCk_Handle InObject) -> void;
    auto SuspendReplicaOccupant(FCk_Handle_HexGrid InGrid, FGuid InId, FCk_Handle InObject, FGuid InKey) -> void;
    auto PublishPlacementClaim(const FCk_HexGridPlacement_View& InPlacement) -> void;
    auto ReleasePlacementClaim(const FCk_HexGridPlacement_View& InPlacement) -> void;
    auto CanClaimPlacement(const FCk_Handle_HexGrid& InGrid, FGuid InBindingId, FGuid InPlacementId,
        bool InReplacingBoard) -> bool;
    auto ResolvePlacementOccupant(FCk_Handle_HexGrid InGrid, FGuid InPlacementId, bool InIdentityReleased) -> void;
    auto FlushBindingConflicts(FCk_Handle_HexGrid InGrid) -> void;
}
