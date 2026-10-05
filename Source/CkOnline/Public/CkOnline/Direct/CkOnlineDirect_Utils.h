#pragma once

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcs/Signal/CkSignal_Fragment_Data.h"
#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"
#include "CkOnline/Direct/CkOnlineDirect_ReadyLobbyBinding.h"
#include "CkOnline/Direct/CkOnlineDirect_ListenTarget.h"

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

    /** Prepare local listen intent from a canonical map object path and loaded mode. Rejection resets OutTarget. */
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    TryBuildListenTarget(
        const TSoftObjectPtr<UWorld>& InMap,
        TSubclassOf<AGameModeBase> InGameMode,
        const FString& InHostAddress,
        int32 InPort,
        FCk_OnlineDirect_ListenTarget& OutTarget);

    /** Require hosting correlation before preparing listen intent; current ownership remains the consumer's check. */
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    TryBuildReadyLobbyListenTarget(
        const FCk_OnlineDirect_ReadyLobbyBinding& InBinding,
        const TSoftObjectPtr<UWorld>& InMap,
        TSubclassOf<AGameModeBase> InGameMode,
        int32 InPort,
        FCk_OnlineDirect_ListenTarget& OutTarget);

    /** Capture all ready members using the caller's explicit roster-count policy. Rejection resets OutBinding. */
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    TryCaptureReadyLobby(
        const FCk_OnlineDirect_Snapshot& InSnapshot,
        bool InHosting,
        int32 InExpectedMembers,
        FCk_OnlineDirect_ReadyLobbyBinding& OutBinding);

    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    MatchesReadyLobby(
        const FCk_OnlineDirect_ReadyLobbyBinding& InBinding,
        const FCk_OnlineDirect_Snapshot& InSnapshot);

    /** Normalize a literal IPv4:port endpoint; rejection clears OutCanonical. */
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    TryParseGameplayEndpoint(
        const FString& InEndpoint,
        FString& OutCanonical);

    /** Validate binding correlation and require its host address; this does not authorize gameplay admission. */
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    static bool
    TryParseReadyLobbyEndpoint(
        const FString& InEndpoint,
        const FCk_OnlineDirect_ReadyLobbyBinding& InBinding,
        FString& OutCanonical);

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
