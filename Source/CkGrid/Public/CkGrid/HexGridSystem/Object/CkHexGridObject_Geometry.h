#pragma once

#include "CkHexGridObject_Fragment_Data.h"

namespace ck::hex_grid
{
    auto ValidateOffsets(const TArray<FCk_HexCoord>& InOffsets) -> ECk_HexGridObject_Failure;
    auto IsConnected(const TArray<FCk_HexCoord>& InCells) -> bool;
    auto ResolveOffsets(const TArray<FCk_HexCoord>& InOffsets, const FCk_HexCoord& InAnchor,
        ECk_HexRotation InRotation, ECk_HexGridConnectivity InConnectivity) -> FCk_HexGridObject_ResolveResult;
}
