#pragma once

#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Data.h"

#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexGrid_Geometry_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class CKGRID_API UCk_Utils_HexGeometry_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexGeometry_UE);

public:
    static auto
    TryValidateWorldLayout(
        const FCk_HexLayout& InLayout,
        const FTransform& InPivot,
        ECk_HexGeometryFailure& OutFailure) -> bool;

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Rotate",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryRotate(
        const FCk_HexCoord& InOffset,
        ECk_HexRotation InRotation,
        FCk_HexCoord& OutOffset,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Neighbor",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_Neighbor(
        const FCk_HexCoord& InCoordinate,
        ECk_HexDirection InDirection,
        FCk_HexCoord& OutNeighbor,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Translate",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryTranslate(
        const FCk_HexCoord& InAnchor,
        const FCk_HexCoord& InOffset,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Distance",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_Distance(
        const FCk_HexCoord& InA,
        const FCk_HexCoord& InB,
        int64& OutDistance,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Resolve Domain",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryResolve_Domain(
        const FCk_HexDomain& InDomain,
        TArray<FCk_HexCoord>& OutCoordinates,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Local Center",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_LocalCenter(
        const FCk_HexCoord& InCoordinate,
        const FCk_HexLayout& InLayout,
        FVector2D& OutCenter,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Local Corners",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_LocalCorners(
        const FCk_HexCoord& InCoordinate,
        const FCk_HexLayout& InLayout,
        TArray<FVector2D>& OutCorners,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Coordinate From Local",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_CoordFromLocal(
        const FVector2D& InPoint,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Coordinate From World Point",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_CoordFromWorldPoint(
        const FVector& InPoint,
        const FTransform& InPivotWorld,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][HexGeometry] Try Get Coordinate From World Ray",
              Category = "Ck|Utils|HexGeometry")
    static bool
    TryGet_CoordFromWorldRay(
        const FVector& InOrigin,
        const FVector& InDirection,
        const FTransform& InPivotWorld,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        FVector& OutWorldHit,
        ECk_HexGeometryFailure& OutFailure);
};

// --------------------------------------------------------------------------------------------------------------------
