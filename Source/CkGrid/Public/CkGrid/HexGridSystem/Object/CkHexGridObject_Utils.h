#pragma once

#include "CkHexGridObject_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGridInitialization_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexGridObject_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexGridObject"))
class CKGRID_API UCk_Utils_HexGridObject_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexGridObject_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_HexGridObject);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Try Add")
    static bool TryAdd(UPARAM(ref) FCk_Handle& InHandle, const FCk_HexGridObject_Spec& InSpec,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Try Create")
    static bool TryCreate(UPARAM(ref) FCk_Handle& InOwner, const FCk_HexGridObject_Spec& InSpec,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Try Add With Initialization")
    static bool TryAddWithInitialization(UPARAM(ref) FCk_Handle& InHandle, const FCk_HexGridObject_Spec& InSpec,
        const FCk_HexGridInitialization& InInitialization,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Try Create With Initialization")
    static bool TryCreateWithInitialization(UPARAM(ref) FCk_Handle& InOwner, const FCk_HexGridObject_Spec& InSpec,
        const FCk_HexGridInitialization& InInitialization,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Has Feature")
    static bool Has(const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Resolve Cells")
    static FCk_HexGridObject_ResolveResult TryResolve_Cells(const FCk_Handle_HexGridObject& InObject,
        const FCk_HexCoord& InAnchor, ECk_HexRotation InRotation, ECk_HexGridConnectivity InConnectivity);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Get Spec")
    static FCk_HexGridObject_Spec Get_Spec(const FCk_Handle_HexGridObject& InObject);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Cast",
        meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_HexGridObject DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck][HexGridObject] Cast Checked",
        meta = (CompactNodeTitle = "<AsHexGridObject>", BlueprintAutocast))
    static FCk_Handle_HexGridObject DoCastChecked(FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridObject", DisplayName = "[Ck] Get Invalid HexGridObject Handle")
    static FCk_Handle_HexGridObject Get_InvalidHandle() { return {}; }
};
