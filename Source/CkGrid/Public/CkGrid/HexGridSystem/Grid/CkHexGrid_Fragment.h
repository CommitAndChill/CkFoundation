#pragma once

#include "CkHexGrid_Fragment_Data.h"
#include "CkHexGrid_Readiness_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Macros.h"
#include "CkEcs/Tag/CkTag.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment_Data.h"

namespace ck::persistence { struct FInitializationStatus; }
namespace ck::hex_grid
{
    auto ReadinessFor(const ck::persistence::FInitializationStatus& InStatus) -> FCk_HexGridReadiness_Result;
    CKGRID_API auto OnNetBaselineAccepted(FCk_Handle_HexGrid& InBoard) -> void;
    CKGRID_API auto OnNetBaselineRejected(FCk_Handle_HexGrid& InBoard, ECk_HexGridFailure InFailure,
        FGuid InPlacementId = {}) -> void;
    CKGRID_API auto OnNetBaselineRemoved(FCk_Handle_HexGrid& InBoard) -> void;
    CKGRID_API auto CanReceiveNetBaseline(const FCk_Handle_HexGrid& InBoard) -> bool;
    auto StartNetBaselineDeadline(FCk_Handle& InBoard) -> void;
    CKGRID_API auto RecordRestoreFailure(FCk_Handle_HexGrid& InBoard, ECk_HexGridFailure InFailure,
        FGuid InPlacementId = {}) -> void;
}

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_HexGrid_RecipeRejected);

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGrid_ReadinessChanged,
        FCk_Delegate_HexGrid_ReadinessChanged, FCk_Handle_HexGrid, FCk_HexGridReadiness_Result);

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKGRID_API, HexGrid_ReplicationBaselineRejected,
        FCk_Delegate_HexGrid_ReplicationBaselineRejected, FCk_Handle_HexGrid, FCk_HexGridReadiness_Result);

    struct FFragment_HexGridBaselineDeadline
    {
        double _ExpiresAt = 0.0;
    };

    struct FFragment_HexGridReadiness
    {
        FCk_HexGridReadiness_Result _Current{ECk_HexGridReadiness::Ready, ECk_HexGridFailure::None, FGuid{}};
        bool _ObservesRestore = false;
        bool _ObservesReplica = false;
        bool _NetFeatureRemoved = false;
        FCk_HexGridReadiness_Result _RestoreFailure;
        uint64 _FailureEpoch = 0;
    };

    struct CKGRID_API FFragment_HexGrid_Params
    {
        CK_GENERATED_BODY(FFragment_HexGrid_Params);

    private:
        FCk_HexLayout _Layout;
        FGameplayTagContainer _DefaultCellTags;
        TMap<FCk_HexCoord, FGameplayTagContainer> _AuthoredLocalTags;
        FCk_HexGridRecipeIdentity _RecipeIdentity;


    public:
        CK_PROPERTY_GET(_Layout);
        CK_PROPERTY_GET(_DefaultCellTags);
        CK_PROPERTY_GET(_AuthoredLocalTags);
        CK_PROPERTY_GET(_RecipeIdentity);
        CK_DEFINE_CONSTRUCTORS(FFragment_HexGrid_Params, _Layout, _DefaultCellTags, _AuthoredLocalTags, _RecipeIdentity);
    };

    struct CKGRID_API FFragment_HexGrid
    {
        CK_GENERATED_BODY(FFragment_HexGrid);

    private:
        TUniquePtr<FEcsWorld> _CellWorld;
        TMap<FCk_HexCoord, FCk_Handle_HexCell> _Index;
        TArray<FCk_Handle_HexCell> _Cells;
        FCk_Handle_SceneNode _Pivot;

    public:
        CK_PROPERTY_GET(_Index);
        CK_PROPERTY_GET(_Cells);
        CK_PROPERTY_GET(_Pivot);
        FFragment_HexGrid() = default;
        FFragment_HexGrid(FFragment_HexGrid&&) = default;
        FFragment_HexGrid& operator=(FFragment_HexGrid&&) = default;
        FFragment_HexGrid(const FFragment_HexGrid&) = delete;
        FFragment_HexGrid& operator=(const FFragment_HexGrid&) = delete;
        FFragment_HexGrid(
            TUniquePtr<FEcsWorld> InWorld,
            TMap<FCk_HexCoord, FCk_Handle_HexCell> InIndex,
            TArray<FCk_Handle_HexCell> InCells,
            FCk_Handle_SceneNode InPivot)
            : _CellWorld(MoveTemp(InWorld))
            , _Index(MoveTemp(InIndex))
            , _Cells(MoveTemp(InCells))
            , _Pivot(InPivot)
        {
        }
    };
}
