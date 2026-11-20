#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Data.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_HexCoord::
    Get_S() const
    -> int64
{
    return -static_cast<int64>(Get_Q()) - static_cast<int64>(Get_R());
}

auto
    FCk_HexCoord::
    operator==(
        const FCk_HexCoord& InOther) const
    -> bool
{
    return Get_Q() == InOther.Get_Q() && Get_R() == InOther.Get_R();
}

uint32
    GetTypeHash(
        const FCk_HexCoord& InCoordinate)
{
    return HashCombine(::GetTypeHash(InCoordinate.Get_Q()), ::GetTypeHash(InCoordinate.Get_R()));
}

// --------------------------------------------------------------------------------------------------------------------
