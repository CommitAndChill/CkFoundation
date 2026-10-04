#pragma once

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkOnline/CkOnline_Fragment_Data.h"

#include "CkOnline_Utils.generated.h"

UCLASS(NotBlueprintable, meta = (ScriptMixin = "FCk_Handle_OnlineService"))
class CKONLINE_API UCk_Utils_OnlineService_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_OnlineService_UE);
    friend class UCk_Utils_Ecs_Base_UE;

    template <typename T>
    static auto Cast(T&& InHandle) -> FCk_Handle_OnlineService
    {
        if (ck::Is_NOT_Valid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) || NOT Has(InHandle))
        { return {}; }
        return ck::StaticCast<FCk_Handle_OnlineService>(std::forward<T>(InHandle));
    }

    template <typename T>
    static auto CastChecked(T&& InHandle) -> FCk_Handle_OnlineService
    {
        if (ck::Is_NOT_Valid(InHandle, ck::IsValid_Policy_IncludePendingKill{}))
        { return {}; }
        const auto HasService = Has(InHandle);
        CK_ENSURE_IF_NOT(HasService, TEXT("Handle does not contain an online service.")) {}
        if (NOT HasService)
        { return {}; }
        return ck::StaticCast<FCk_Handle_OnlineService>(std::forward<T>(InHandle));
    }

    CK_REGISTER_ANGELSCRIPT_HANDLE_CONVERSION(FCk_Handle_OnlineService)

public:
    static bool Has(const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Online", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_OnlineService DoCast(UPARAM(ref) FCk_Handle& InHandle, ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Online", meta = (CompactNodeTitle = "<AsOnlineService>", BlueprintAutocast))
    static FCk_Handle_OnlineService DoCastChecked(FCk_Handle InHandle);

public:

    UFUNCTION(BlueprintCallable, Category = "Ck|Online", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_OnlineService Request_Execute(UPARAM(ref) FCk_Handle_OnlineService& InService,
        const FCk_Request_OnlineService_Execute& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Online", meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_OnlineService Request_Cancel(UPARAM(ref) FCk_Handle_OnlineService& InService,
        const FCk_Request_OnlineService_Cancel& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintPure, Category = "Ck|Online")
    static FCk_OnlineService_Diagnostics Get_Diagnostics(const FCk_Handle_OnlineService& InService);

    UFUNCTION(BlueprintCallable, Category = "Ck|Online")
    static void BindTo_OnCompleted(UPARAM(ref) FCk_Handle_OnlineService& InService,
        const FCk_Delegate_OnlineService_Completed& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy, ECk_Signal_PostFireBehavior InPostFireBehavior);

    UFUNCTION(BlueprintCallable, Category = "Ck|Online")
    static void UnbindFrom_OnCompleted(UPARAM(ref) FCk_Handle_OnlineService& InService,
        const FCk_Delegate_OnlineService_Completed& InDelegate);
};
