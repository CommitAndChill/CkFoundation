#pragma once

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkOnline/Connection/CkOnlineConnection_Fragment_Data.h"

#include "CkOnlineConnection_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_OnlineConnection"))
class CKONLINE_API UCk_Utils_OnlineConnection_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_Utils_OnlineConnection_UE);
    friend class UCk_Utils_Ecs_Base_UE;
    template <typename T>
    static auto Cast(T&& InHandle) -> FCk_Handle_OnlineConnection
    {
        if (NOT Has(InHandle)) { return {}; }
        return ck::StaticCast<FCk_Handle_OnlineConnection>(std::forward<T>(InHandle));
    }
    template <typename T>
    static auto CastChecked(T&& InHandle) -> FCk_Handle_OnlineConnection { return Cast(std::forward<T>(InHandle)); }
    CK_REGISTER_ANGELSCRIPT_HANDLE_CONVERSION(FCk_Handle_OnlineConnection)
    static auto Has(const FCk_Handle& InHandle) -> bool;
private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_OnlineConnection DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection", meta = (CompactNodeTitle = "<AsOnlineConnection>", BlueprintAutocast))
    static FCk_Handle_OnlineConnection DoCastChecked(FCk_Handle InHandle);
public:
    /** Admission is atomic and deferred. OutOperation is cleared on rejection; readiness requires its complete key. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    static bool TryRequest(UPARAM(ref) FCk_Handle_OnlineConnection& InService,
        const FCk_Request_OnlineConnection& InRequest, FCk_OnlineConnection_Operation& OutOperation);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    static bool Get_IsIdle(const FCk_Handle_OnlineConnection& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    static FCk_OnlineConnection_Snapshot Get_Snapshot(const FCk_Handle_OnlineConnection& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    static FCk_OnlineConnection_Completion Get_LastCompletion(const FCk_Handle_OnlineConnection& InService);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    static int32 Get_CompletedCount(const FCk_Handle_OnlineConnection& InService);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    static void BindTo_OnCompleted(UPARAM(ref) FCk_Handle_OnlineConnection& InService,
        const FCk_Delegate_OnlineConnection_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    static void BindTo_OnChanged(UPARAM(ref) FCk_Handle_OnlineConnection& InService,
        const FCk_Delegate_OnlineConnection_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    static void UnbindFrom_OnCompleted(UPARAM(ref) FCk_Handle_OnlineConnection& InService,
        const FCk_Delegate_OnlineConnection_Completed& InDelegate);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    static void UnbindFrom_OnChanged(UPARAM(ref) FCk_Handle_OnlineConnection& InService,
        const FCk_Delegate_OnlineConnection_Changed& InDelegate);
};
