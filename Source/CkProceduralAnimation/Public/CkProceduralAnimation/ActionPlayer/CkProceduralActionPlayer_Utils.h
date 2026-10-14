#pragma once

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"

#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcsExt/CkEcsExt_Utils.h"

#include "CkProceduralActionPlayer_Utils.generated.h"

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_ProceduralActionPlayer"))
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralActionPlayer_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_ProceduralActionPlayer_UE);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Get Admission Report")
    static FCk_ProceduralAction_ValidationReport
    Get_AdmissionReport(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec);

    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_ProceduralActionPlayer);
    friend class UCk_Utils_Ecs_Base_UE;

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Add")
    static FCk_Handle_ProceduralActionPlayer
    Add(
        UPARAM(ref) FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ProceduralActionPlayer_Spec& InSpec);

    static auto
    Has(
        const FCk_Handle& InHandle)
        -> bool;

public:
    // A player for dedicated appendages only: walking-limb borrowing stays disabled.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Make Dedicated Player Spec")
    static FCk_ProceduralActionPlayer_Spec
    Make_DedicatedPlayerSpec(
        const FCk_ProceduralAction_Tuning& InTuning);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Make Borrowing Player Spec")
    static FCk_ProceduralActionPlayer_Spec
    Make_BorrowingPlayerSpec(
        const FCk_ProceduralActionPlayer_BorrowingSpec& InSpec);

    /** Counts trusted planted contacts, not visually standing legs. A minimum outside 1..64 ensures and is returned
     *  unchanged, so the player's Add rejects it rather than borrowing being silently disabled. */
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Make Count Only Support")
    static FCk_ProceduralLimbControl_Support
    Make_CountOnlySupport(
        int32 InMinimumTrustedContacts);

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Cast", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_ProceduralActionPlayer
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Handle -> ProceduralActionPlayer Handle",
        meta = (CompactNodeTitle = "<AsProceduralActionPlayer>", BlueprintAutocast))
    static FCk_Handle_ProceduralActionPlayer
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck] Get Invalid ProceduralActionPlayer Handle",
        meta = (CompactNodeTitle = "INVALID_ProceduralActionPlayerHandle", Keywords = "make"))
    static FCk_Handle_ProceduralActionPlayer
    Get_InvalidHandle()
    {
        return {};
    }

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Read Receipts")
    static FCk_ProceduralAction_ReadPage
    Read_Receipts(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        int64 InAfterStreamSequence,
        int32 InMaxRecords);

    // A bounded, sorted snapshot of streams still retained by this player, including terminal streams.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Get Retained Receipt Identities")
    static TArray<FCk_ProceduralAction_Identity>
    Get_RetainedReceiptIdentities(
        const FCk_Handle_ProceduralActionPlayer& InPlayer);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Request Consume Receipts",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralActionPlayer
    Request_ConsumeReceipts(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_Request_ProceduralActionPlayer_ConsumeReceipts& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    /** Adds a listener to a managed identity's copied pages. A listener bound after playback started receives only
     *  the pages delivered from then on; nothing is replayed. At most 8 listeners per identity. Binding an identity
     *  that is not an active managed delivery ensures. Unbinding takes effect from the next page. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Bind To OnManagedRecords")
    static FCk_Handle_ProceduralActionPlayer
    BindTo_OnManagedRecords(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate);

    // A no-op once the identity's delivery has completed.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Unbind From OnManagedRecords")
    static FCk_Handle_ProceduralActionPlayer
    UnbindFrom_OnManagedRecords(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate);

    /** Passive notification of each accepted receipt of a managed identity, in stream order. Ephemeral: no replay
     *  and no promise that gameplay sees every pose exactly once; consume explicit receipts for that. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Bind To OnManagedAcceptedPose")
    static FCk_Handle_ProceduralActionPlayer
    BindTo_OnManagedAcceptedPose(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedAcceptedPose& InDelegate);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Unbind From OnManagedAcceptedPose")
    static FCk_Handle_ProceduralActionPlayer
    UnbindFrom_OnManagedAcceptedPose(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedAcceptedPose& InDelegate);

    // True from Play's return for as long as the identity's stream is retained, which outlasts its completion.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Get Is Managed Delivery")
    static bool
    Get_IsManagedDelivery(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Request Update Tuning",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralActionPlayer
    Request_UpdateTuning(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_Request_ProceduralActionPlayer_UpdateTuning& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);
};
