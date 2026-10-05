#pragma once

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkOnline/Steam/CkOnlineSteam_Fragment_Data.h"

#include "CkOnlineSteam_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_OnlineSteam"))
class CKONLINE_API UCk_Utils_OnlineSteam_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_Utils_OnlineSteam_UE);
    friend class UCk_Utils_Ecs_Base_UE;
    template <typename T>
    static auto Cast(T&& InHandle) -> FCk_Handle_OnlineSteam
    {
        if (NOT Has(InHandle)) { return {}; }
        return ck::StaticCast<FCk_Handle_OnlineSteam>(std::forward<T>(InHandle));
    }
    template <typename T>
    static auto CastChecked(T&& InHandle) -> FCk_Handle_OnlineSteam { return Cast(std::forward<T>(InHandle)); }
    CK_REGISTER_ANGELSCRIPT_HANDLE_CONVERSION(FCk_Handle_OnlineSteam)
    static auto Has(const FCk_Handle& InHandle) -> bool;
private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_OnlineSteam DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam", meta = (CompactNodeTitle = "<AsOnlineSteam>", BlueprintAutocast))
    static FCk_Handle_OnlineSteam DoCastChecked(FCk_Handle InHandle);
public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    static bool TryRequest(UPARAM(ref) FCk_Handle_OnlineSteam& InService, const FCk_Request_OnlineSteam& InRequest);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    static FCk_OnlineSteam_Snapshot Get_Snapshot(const FCk_Handle_OnlineSteam& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    static FCk_OnlineSteam_Completion Get_LastCompletion(const FCk_Handle_OnlineSteam& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    static int32 Get_CompletedCount(const FCk_Handle_OnlineSteam& InService);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    static void BindTo_OnCompleted(UPARAM(ref) FCk_Handle_OnlineSteam& InService,
        const FCk_Delegate_OnlineSteam_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    static void BindTo_OnChanged(UPARAM(ref) FCk_Handle_OnlineSteam& InService,
        const FCk_Delegate_OnlineSteam_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    static void UnbindFrom_OnCompleted(UPARAM(ref) FCk_Handle_OnlineSteam& InService,
        const FCk_Delegate_OnlineSteam_Completed& InDelegate);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    static void UnbindFrom_OnChanged(UPARAM(ref) FCk_Handle_OnlineSteam& InService,
        const FCk_Delegate_OnlineSteam_Changed& InDelegate);
};
