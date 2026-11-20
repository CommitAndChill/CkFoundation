#pragma once

#include "CkGrid/HexGridSystem/Placement/CkHexGridPlacement_Result_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>
#include "CkHexGridOccupancy_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexGrid"))
class CKGRID_API UCk_Utils_HexGridOccupancy_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexGridOccupancy_UE);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridOccupancy")
    static FCk_HexGridPlacements_Result Get_Placements(const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridOccupancy")
    static FCk_HexGridPlacement_Result TryGet_Placement(const FCk_Handle_HexGrid& InGrid, FGuid InPlacementId);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridOccupancy")
    static FCk_HexGridPlacement_Result TryGet_PlacementAt(const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InCoordinate);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridOccupancy")
    static FCk_HexGridPlacement_Result TryGet_PlacementForOccupant(const FCk_Handle_HexGridObject& InObject);
};
