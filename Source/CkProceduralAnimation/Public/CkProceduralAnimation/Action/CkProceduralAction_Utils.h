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
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Create Manual")
    static FCk_Handle_ProceduralAction
    Create_Manual(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_ControlSpec& InSpec);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Create From Clip")
    static FCk_Handle_ProceduralAction
    Create_FromClip(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Create From Loaded Clip")
    static FCk_Handle_ProceduralAction
    Create_FromLoadedClip(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_LoadedPlaySpec& InSpec);

    /** Plays a soft clip and lets the player deliver its receipt stream: records go to listeners bound with
     *  BindTo_OnManagedRecords, the stream is acknowledged and the finished action released without consumer code.
     *  The completion fires once, on a later tick than this call, after the terminal record was delivered. Public
     *  consumes of the identity are refused; Read_Receipts stays a non-consuming observation. A start that cannot be
     *  created reports Invalid and never completes. Use Create_FromClip when gameplay must consume every record. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Play",
        meta = (AutoCreateRefTerm = "InOnCompleted"))
    static FCk_ProceduralAction_ManagedStart
    Play(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted);

    /** Play for resident content. A clip that fails to compile or bind still returns its action; the rejection
     *  arrives as the completion's producer result on a later tick, never inside this call. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Play Loaded",
        meta = (AutoCreateRefTerm = "InOnCompleted"))
    static FCk_ProceduralAction_ManagedStart
    Play_Loaded(
        UPARAM(ref) FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_LoadedPlaySpec& InSpec,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted);

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
        DisplayName = "[Ck][ProceduralAction] Request Submit Pose Batch",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralAction
    Request_SubmitPoseBatch(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_SubmitPoseBatch& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Request End",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralAction
    Request_End(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_End& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction",
        DisplayName = "[Ck][ProceduralAction] Request Cancel",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralAction
    Request_Cancel(
        UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_Cancel& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

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
            const FCk_ProceduralAction_PlaySpec& InPlay,
            const FCk_ProceduralAction_ControlSpec& InControl,
            bool InIsClip,
            ck::EProceduralActionDelivery InDelivery,
            const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
        -> FCk_Handle_ProceduralAction;

    static auto
        DoCreate_FromClip(
            FCk_Handle& InScope,
            const FCk_Handle_ProceduralActionPlayer& InPlayer,
            const FCk_ProceduralAction_PlaySpec& InSpec,
            ck::EProceduralActionDelivery InDelivery,
            const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
        -> FCk_Handle_ProceduralAction;

    static auto
        DoCreate_Loaded(
            FCk_Handle& InScope,
            const FCk_Handle_ProceduralActionPlayer& InPlayer,
            const FCk_ProceduralAction_LoadedPlaySpec& InSpec,
            ck::EProceduralActionDelivery InDelivery,
            const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
        -> FCk_Handle_ProceduralAction;

    static auto
        DoMake_ManagedStart(
            const FCk_Handle_ProceduralAction& InAction,
            const FString& InAssetPath)
        -> FCk_ProceduralAction_ManagedStart;
};
