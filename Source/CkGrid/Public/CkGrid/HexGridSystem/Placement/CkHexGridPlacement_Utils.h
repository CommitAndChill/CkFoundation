#pragma once

#include "CkHexGridPlacement_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkHexGridPlacement_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_HexGrid"))
class CKGRID_API UCk_Utils_HexGridPlacement_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_HexGridPlacement_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_HexGridPlacement);

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridPlacement")
    static bool Has(const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridPlacement")
    static FCk_HexGridPlacement_Result Get_CanPlace(const FCk_Handle_HexGrid& InGrid, const FCk_HexGridPlacement_Spec& InSpec);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridPlacement")
    static FCk_HexGridPlacement_Result Get_CanMove(const FCk_Handle_HexGrid& InGrid, FGuid InPlacementId,
        const FCk_HexCoord& InAnchor, ECk_HexRotation InRotation);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid Request_Place(UPARAM(ref) FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Place& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid Request_Move(UPARAM(ref) FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Move& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid Request_Remove(UPARAM(ref) FCk_Handle_HexGrid& InGrid, const FCk_Request_HexGridPlacement_Remove& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnObjectPlaced(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectPlaced& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnObjectPlaced(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectPlaced& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnObjectMoved(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectMoved& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnObjectMoved(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectMoved& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnObjectRemoved(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectRemoved& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnObjectRemoved(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_ObjectRemoved& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnOccupantBindingConflict(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_BindingConflict& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnOccupantBindingConflict(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGridPlacement_BindingConflict& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid BindTo_OnOccupantBindingFailed(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_OccupantBindingFailed& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_HexGrid UnbindFrom_OnOccupantBindingFailed(UPARAM(ref) FCk_Handle_HexGrid& InGrid,
        const FCk_Delegate_HexGrid_OccupantBindingFailed& InDelegate);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|HexGridPlacement", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_HexGridPlacement DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridPlacement", meta = (CompactNodeTitle = "<AsHexGridPlacement>", BlueprintAutocast))
    static FCk_Handle_HexGridPlacement DoCastChecked(FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|HexGridPlacement")
    static FCk_Handle_HexGridPlacement Get_InvalidHandle() { return {}; }
};
