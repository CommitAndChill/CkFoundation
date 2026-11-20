#pragma once

#include "CkHexGridBlocker_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexGridBlocker_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexGridBlocker"))
class CKGRID_API UCk_Utils_HexGridBlocker_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_Utils_HexGridBlocker_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_HexGridBlocker);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] TryAdd")
    static bool
    TryAdd(
        UPARAM(ref) FCk_Handle& InHandle,
        const FCk_HexGridBlocker_Spec& InSpec,
        FCk_Handle_HexGridBlocker& OutBlocker,
        ECk_HexGridBlocker_Failure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] TryCreate")
    static bool
    TryCreate(
        UPARAM(ref) FCk_Handle& InOwner,
        const FCk_HexGridBlocker_Spec& InSpec,
        FCk_Handle_HexGridBlocker& OutBlocker,
        ECk_HexGridBlocker_Failure& OutFailure);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Has")
    static bool
    Has(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Get Grid")
    static FCk_Handle_HexGrid
    Get_Grid(
        const FCk_Handle_HexGridBlocker& InBlocker);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Get IsActive")
    static bool
    Get_IsActive(
        const FCk_Handle_HexGridBlocker& InBlocker);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Get BlockedCells")
    static TArray<FCk_HexCoord>
    Get_BlockedCells(
        const FCk_Handle_HexGridBlocker& InBlocker);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Get Blockers")
    static TArray<FCk_Handle_HexGridBlocker>
    Get_Blockers(
        const FCk_Handle_HexGrid& InGrid);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Get BlockerWithTag")
    static FCk_Handle_HexGridBlocker
    Get_BlockerWithTag(
        const FCk_Handle_HexGrid& InGrid,
        FGameplayTag InTag);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridBlocker", DisplayName = "[Ck][HexGridBlocker] Request SetActive", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGridBlocker
    Request_SetActive(
        UPARAM(ref) FCk_Handle_HexGridBlocker& InBlocker,
        bool InActive,
        const FCk_Delegate_Request_OnCompleted& InDelegate);
    static bool TryInstallAuthored(FCk_Handle& InBoard, const TMap<FCk_HexCoord,FCk_Handle_HexCell>& InIndex,
        const TArray<FCk_HexGridAuthoredBlocker>& InDeclarations);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridBlocker", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_HexGridBlocker DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridBlocker", meta = (BlueprintAutocast))
    static FCk_Handle_HexGridBlocker DoCastChecked(FCk_Handle InHandle);
};
