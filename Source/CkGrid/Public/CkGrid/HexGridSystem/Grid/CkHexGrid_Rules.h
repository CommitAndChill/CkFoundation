#pragma once

#include "CkGrid/HexGridSystem/Blocker/CkHexGridBlocker_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Fragment_Data.h"

#include <variant>

namespace ck
{
    struct FHexPlacementDeath
    {
        FGuid _PlacementId;
        FCk_Handle _ExpectedHandle;
    };

    using FHexRule = std::variant<FCk_Request_HexGridBlocker_SetActive, FCk_Request_HexCell_SetBlocked,
        FCk_Request_HexCell_AddTag, FCk_Request_HexCell_RemoveTag, FCk_Request_HexGridPlacement_Place,
        FCk_Request_HexGridPlacement_Move, FCk_Request_HexGridPlacement_Remove, FHexPlacementDeath>;

    struct FHexRuleCommand
    {
        FCk_Handle _Target;
        FHexRule _Rule;
    };

    struct FFragment_HexGrid_RuleRequests
    {
        TArray<FHexRuleCommand> _Commands;
    };

    namespace hex_rules
    {
        auto IsLive(const FCk_Handle& InHandle) -> bool;
        auto CanWrite(const FCk_Handle& InBoard) -> bool;
        auto Enqueue(FCk_Handle InBoard, FCk_Handle InTarget, FHexRule InRequest) -> void;
        auto EnqueueDeath(FCk_Handle_HexGrid InBoard, FGuid InId, FCk_Handle InDying) -> void;
        auto SetActive(FCk_Handle InBlocker, bool InActive) -> bool;
        auto Finish(FHexRuleCommand& InCommand, ECk_Request_OperationResult InResult,
            ECk_HexGridFailure InFailure = ECk_HexGridFailure::Cancelled) -> void;
    }
}
