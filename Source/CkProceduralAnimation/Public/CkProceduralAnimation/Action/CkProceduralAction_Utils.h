#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"

#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Signal/CkSignal_Fragment.h"
#include "CkEcsExt/CkEcsExt_Utils.h"

#include "CkProceduralAction_Utils.generated.h"

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_ProceduralAction"))
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralAction_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_ProceduralAction_UE);

public:
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_ProceduralAction);
    friend class UCk_Utils_Ecs_Base_UE;

public:
    /** Plays a clip on the player's subject, as a child entity of InScope. A clip that is already loaded is compiled and
     *  bound before this returns; a clip that fails to compile or bind still returns its action, already Finished with
     *  the rejection as its result. An unloaded clip is loaded first (PendingLoad). Returns an invalid handle, with an
     *  ensure, when the scope, the player or the spec is invalid or the player has no receipt capacity left. The action
     *  releases itself once its terminal record is acknowledged: by the caller's consume for Explicit delivery, by the
     *  player for Auto. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Create From Clip")
    static FCk_Handle_ProceduralAction
    Create_FromClip(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Validate Data")
    static FCk_ProceduralAction_ValidationReport
    Validate_Data(
        UCk_ProceduralAction_Data* InData);

    static auto
        Has(
            const FCk_Handle& InHandle)
        -> bool;

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Cast", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_ProceduralAction
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Handle -> ProceduralAction Handle",
        meta = (CompactNodeTitle = "<AsProceduralAction>", BlueprintAutocast))
    static FCk_Handle_ProceduralAction
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck] Get Invalid ProceduralAction Handle",
        meta = (CompactNodeTitle = "INVALID_ProceduralActionHandle", Keywords = "make"))
    static FCk_Handle_ProceduralAction
    Get_InvalidHandle()
    {
        return {};
    }

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Get State")
    static ECk_ProceduralAction_State
    Get_State(
        const FCk_Handle_ProceduralAction& InAction);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Get Result")
    static FCk_ProceduralAction_Result
    Get_Result(
        const FCk_Handle_ProceduralAction& InAction);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Get Identity")
    static FCk_ProceduralAction_Identity
    Get_Identity(
        const FCk_Handle_ProceduralAction& InAction);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Get Last Receipt")
    static FCk_ProceduralAction_Receipt
    Get_LastReceipt(
        const FCk_Handle_ProceduralAction& InAction);

    /** Renders the outcome, recovery and reason, then only the context fields that were captured. Reasons render as
     *  their identifier, including reasons this build does not know; an unknown enum value renders as its number.
     *  Formats on demand: call it when a person reads the result, not every frame. */
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Get Result Text")
    static FText
    Get_ResultText(
        const FCk_ProceduralAction_Result& InResult,
        const FCk_ProceduralAction_FailureContext& InContext);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Bind To OnStateChanged")
    static FCk_Handle_ProceduralAction
    BindTo_OnStateChanged(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnStateChanged& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Unbind From OnStateChanged")
    static FCk_Handle_ProceduralAction
    UnbindFrom_OnStateChanged(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnStateChanged& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Bind To OnFinished")
    static FCk_Handle_ProceduralAction
    BindTo_OnFinished(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnFinished& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Unbind From OnFinished")
    static FCk_Handle_ProceduralAction
    UnbindFrom_OnFinished(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnFinished& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Request Cancel",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralAction
    Request_Cancel(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_Cancel& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    // Ends an action early: a running action returns its limbs first. A finished action is destroyed.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Request Release",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralAction
    Request_Release(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_Release& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

private:
    static auto
        DoCreate(
            FCk_Handle& InScope,
            const FCk_Handle_ProceduralActionPlayer& InPlayer,
            const FCk_ProceduralAction_PlaySpec& InPlay)
        -> FCk_Handle_ProceduralAction;

    static auto
        DoAdmit_LoadedClip(
            FCk_Handle_ProceduralAction& InAction,
            const UCk_ProceduralAction_Data& InClip)
        -> void;
};
