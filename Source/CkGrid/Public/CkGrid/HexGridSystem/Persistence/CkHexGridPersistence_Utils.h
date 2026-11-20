#pragma once

#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Readiness_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"

enum class ECk_HexGridApplySource : uint8
{
    Hydration,
    Replication
};

struct CKGRID_API FCk_HexGridApply_Result
{
    CK_GENERATED_BODY(FCk_HexGridApply_Result);

private:
    ECk_Persistence_ApplyResult _Status = ECk_Persistence_ApplyResult::Rejected;
    ECk_HexGridFailure _Failure = ECk_HexGridFailure::InvalidProjection;
    FGuid _PlacementId;

public:
    CK_PROPERTY_GET(_Status);
    CK_PROPERTY_GET(_Failure);
    CK_PROPERTY_GET(_PlacementId);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridApply_Result, _Status, _Failure, _PlacementId);
};

namespace ck::hex_grid::persistence
{
    CKGRID_API auto ReadCommittedProjection(const FCk_Handle_HexGrid& InGrid) -> FCk_RepData_HexGridState;
    CKGRID_API auto TryReplaceCommittedProjection(FCk_Handle_HexGrid& InGrid, const FCk_RepData_HexGridState& InData,
        ECk_HexGridApplySource InSource) -> FCk_HexGridApply_Result;
}
