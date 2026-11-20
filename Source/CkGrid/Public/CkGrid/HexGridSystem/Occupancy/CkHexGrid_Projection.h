#pragma once

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Result_Data.h"

struct FCk_HexGridBlockerView
{
    FGuid _BlockerId;
    FGameplayTag _Name;
    TArray<FCk_HexCoord> _Cells;
    bool _Active = false;
};

// Native transaction state. Local tags are sparse replacements relative to the board recipe.
// An explicit empty local tag container is therefore meaningful.
struct FCk_HexGridCommittedProjection
{
    TArray<FCk_HexGridPlacement_View> _Placements;
    TArray<FCk_HexGridBlockerView> _Blockers;
    TSet<FCk_HexCoord> _ManualBlocks;
    TMap<FCk_HexCoord, FGameplayTagContainer> _LocalTags;
    uint64 _Revision = 0;
};

namespace ck::hex_grid
{

    CKGRID_API auto SetReadiness(FCk_Handle_HexGrid& InBoard, const FCk_HexGridReadiness_Result& InState) -> bool;
    CKGRID_API auto ReadCommittedProjection(const FCk_Handle_HexGrid& InBoard) -> FCk_HexGridCommittedProjection;
    auto ClearReplicaProjection(FCk_Handle_HexGrid& InBoard, TArray<FCk_Handle>& OutRetired) -> void;
    auto RetireReplicaProjectionEntities(const TArray<FCk_Handle>& InRetired) -> void;
    CKGRID_API auto TryCommitProjection(FCk_Handle_HexGrid& InBoard, const FCk_HexGridCommittedProjection& InProjection)
        -> FCk_HexGridPlacement_Result;
}
