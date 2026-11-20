#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexCell_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexCell"))
class CKGRID_API UCk_Utils_HexCell_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexCell_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_HexCell);

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Has Feature")
    static bool
    Has(
        const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Cast",
              meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_HexCell
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Cast Checked",
              meta = (CompactNodeTitle = "<AsHexCell>", BlueprintAutocast))
    static FCk_Handle_HexCell
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexCell", DisplayName = "[Ck] Get Invalid HexCell Handle")
    static FCk_Handle_HexCell
    Get_InvalidHandle() { return {}; }

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Try Get Coordinate")
    static bool
    TryGet_Coordinate(
        const FCk_Handle_HexCell& InCell,
        FCk_HexCoord& OutCoordinate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Try Get Grid")
    static bool
    TryGet_Grid(
        const FCk_Handle_HexCell& InCell,
        FCk_Handle_HexGrid& OutGrid);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Try Get Tags")
    static bool
    TryGet_Tags(
        const FCk_Handle_HexCell& InCell,
        FGameplayTagContainer& OutTags);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Try Get Is Disabled")
    static bool
    TryGet_IsDisabled(
        const FCk_Handle_HexCell& InCell,
        bool& OutIsDisabled);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] TryGet LocalTags")
    static bool
    TryGet_LocalTags(
        const FCk_Handle_HexCell& InCell,
        FGameplayTagContainer& OutTags);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Request SetBlocked", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexCell
    Request_SetBlocked(
        UPARAM(ref) FCk_Handle_HexCell& InCell,
        bool InBlocked,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Request AddTag", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexCell
    Request_AddTag(
        UPARAM(ref) FCk_Handle_HexCell& InCell,
        FGameplayTag InTag,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexCell", DisplayName = "[Ck][HexCell] Request RemoveTag", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexCell
    Request_RemoveTag(
        UPARAM(ref) FCk_Handle_HexCell& InCell,
        FGameplayTag InTag,
        const FCk_Delegate_Request_OnCompleted& InDelegate);
};
