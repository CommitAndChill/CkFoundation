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

    /** Acknowledges an Explicit action's records through InRequest's stream sequence. Acknowledging the terminal record
     *  releases the action on the player's next pass. An Auto action's stream is the player's to acknowledge: consuming
     *  it ensures and completes Failed_NotEnqueued. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionPlayer",
        DisplayName = "[Ck][ProceduralActionPlayer] Request Consume Receipts",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralActionPlayer
    Request_ConsumeReceipts(
        UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_Request_ProceduralActionPlayer_ConsumeReceipts& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);
};
