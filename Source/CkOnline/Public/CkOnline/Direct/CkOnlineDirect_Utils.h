#pragma once

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"

#include "CkOnlineDirect_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_OnlineDirect"))
class CKONLINE_API UCk_Utils_OnlineDirect_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_Utils_OnlineDirect_UE);
    friend class UCk_Utils_Ecs_Base_UE;

    template <typename T>
    static auto Cast(T&& InHandle) -> FCk_Handle_OnlineDirect
    {
        if (!Has(InHandle)) { return {}; }
        return ck::StaticCast<FCk_Handle_OnlineDirect>(std::forward<T>(InHandle));
    }
    template <typename T>
    static auto CastChecked(T&& InHandle) -> FCk_Handle_OnlineDirect { return Cast(std::forward<T>(InHandle)); }
    CK_REGISTER_ANGELSCRIPT_HANDLE_CONVERSION(FCk_Handle_OnlineDirect)
public:
    static bool Has(const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_OnlineDirect DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct", meta = (CompactNodeTitle = "<AsOnlineDirect>", BlueprintAutocast))
    static FCk_Handle_OnlineDirect DoCastChecked(FCk_Handle InHandle);

public:

    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    static bool TryRequest(UPARAM(ref) FCk_Handle_OnlineDirect& InService, const FCk_Request_OnlineDirect& InRequest);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static FCk_OnlineDirect_Snapshot Get_Snapshot(const FCk_Handle_OnlineDirect& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static FCk_OnlineDirect_Completion Get_LastCompletion(const FCk_Handle_OnlineDirect& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static int32 Get_CompletedCount(const FCk_Handle_OnlineDirect& InService);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    static void BindTo_OnChanged(UPARAM(ref) FCk_Handle_OnlineDirect& InService,
        const FCk_Delegate_OnlineDirect_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    static void BindTo_OnCompleted(UPARAM(ref) FCk_Handle_OnlineDirect& InService,
        const FCk_Delegate_OnlineDirect_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    static void UnbindFrom_OnChanged(UPARAM(ref) FCk_Handle_OnlineDirect& InService,
        const FCk_Delegate_OnlineDirect_Changed& InDelegate);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    static void UnbindFrom_OnCompleted(UPARAM(ref) FCk_Handle_OnlineDirect& InService,
        const FCk_Delegate_OnlineDirect_Completed& InDelegate);
};
