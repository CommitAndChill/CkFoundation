#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Readiness_Data.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexGrid_Utils.generated.h"

struct FCk_HexGridInitialization;
struct FCk_HexGridRecipeIdentity;

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexGrid"))
class CKGRID_API UCk_Utils_HexGrid_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexGrid_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_HexGrid);

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Has Feature")
    static bool
    Has(
        const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Cast",
              meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_HexGrid
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Cast Checked",
              meta = (CompactNodeTitle = "<AsHexGrid>", BlueprintAutocast))
    static FCk_Handle_HexGrid
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck] Get Invalid HexGrid Handle")
    static FCk_Handle_HexGrid
    Get_InvalidHandle() { return {}; }

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid")
    static FCk_HexGridReadiness_Result Get_Readiness(const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid")
    static bool Get_IsReady(const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGrid")
    static bool Get_IsPending(const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnReadinessChanged(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_ReadinessChanged& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnReadinessChanged(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_ReadinessChanged& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnReplicationBaselineRejected(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_ReplicationBaselineRejected& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnReplicationBaselineRejected(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_ReplicationBaselineRejected& InDelegate);

    static bool
    TryValidateSpec(
        const FCk_HexGrid_Spec& InSpec,
        ECk_HexBoardFailure& OutFailure);

    static bool
    TryAddForAuthoring(
        FCk_Handle& InOwner,
        const FTransform& InSpawnTransform,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure,
        const FCk_HexGridInitialization* InInitialization = nullptr,
        const FCk_HexGridRecipeIdentity* InRecipe = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Add")
    static bool
    TryAdd(
        UPARAM(ref) FCk_Handle_Transform& InOwner,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Create")
    static bool
    TryCreate(
        UPARAM(ref) FCk_Handle& InOwner,
        const FTransform& InInitialTransform,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell At")
    static bool
    TryGet_CellAt(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InCoordinate,
        FCk_Handle_HexCell& OutCell,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Neighbor Cell")
    static bool
    TryGet_NeighborCell(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InFrom,
        ECk_HexDirection InDirection,
        FCk_Handle_HexCell& OutCell,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Get Cells")
    static TArray<FCk_Handle_HexCell>
    Get_Cells(
        const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell From World Point")
    static bool
    TryGet_CellFromWorldPoint(
        const FCk_Handle_HexGrid& InGrid,
        const FVector& InPoint,
        FCk_Handle_HexCell& OutCell,
        ECk_HexGeometryFailure& OutSpatialFailure,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell From World Ray")
    static bool
    TryGet_CellFromWorldRay(
        const FCk_Handle_HexGrid& InGrid,
        const FVector& InOrigin,
        const FVector& InDirection,
        FCk_Handle_HexCell& OutCell,
        FVector& OutWorldHit,
        ECk_HexGeometryFailure& OutSpatialFailure,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell World Center")
    static bool
    TryGet_CellWorldCenter(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InCoordinate,
        FVector& OutCenter,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell World Corners")
    static bool
    TryGet_CellWorldCorners(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InCoordinate,
        TArray<FVector>& OutCorners,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Try Get Cell World Bounds")
    static bool
    TryGet_CellWorldBounds(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InCoordinate,
        FBox& OutBounds,
        ECk_HexBoardFailure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGrid", DisplayName = "[Ck][HexGrid] Debug Draw Grid")
    static void
    DebugDraw_Grid(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexGrid_DebugDraw_Options& InOptions);
};
