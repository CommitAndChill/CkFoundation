#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReport.h"
#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "UObject/StrongObjectPtr.h"

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_ProceduralActionPlayer_UE,
    FCk_Handle_ProceduralActionPlayer, ck::FFragment_ProceduralActionPlayer);

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Add(
        FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ProceduralActionPlayer_Spec& InSpec)
    -> FCk_Handle_ProceduralActionPlayer
{
    auto InvalidField = FName{};
    const auto Valid = ck::IsValid(InSubject)
        && NOT InSubject.Has<ck::FTag_DestroyEntity_Initiate>()
        && NOT Has(InSubject)
        && ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField)
        && ck::procedural_action::ValidateCapacity(InSpec.Get_Capacity(), InvalidField);
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralActionPlayer Add rejected subject [{}], invalid field [{}]."), InSubject, InvalidField)
    { return {}; }
    if (NOT FCk_ProceduralLimbControl_Control::Attach(InSubject, InSpec.Get_Locomotion())) { return {}; }

    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Params>(InSpec.Get_Capacity(), InSpec.Get_Tuning());
    auto& Player = InSubject.Add<ck::FFragment_ProceduralActionPlayer>();
    if (NOT Player._Receipts.Configure(InSpec.Get_Capacity()))
    {
        InSubject.Remove<ck::FFragment_ProceduralActionPlayer>();
        InSubject.Remove<ck::FFragment_ProceduralActionPlayer_Params>();
        return {};
    }
    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Requests>();
    return CastChecked(InSubject);
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Read_Receipts(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        int64 InAfterStreamSequence,
        int32 InMaxRecords)
    -> FCk_ProceduralAction_ReadPage
{
    if (ck::Is_NOT_Valid(InPlayer) || NOT Has(InPlayer))
    { return FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::InvalidPlayer,
        {}, InAfterStreamSequence, false, false}; }
    return InPlayer.Get<ck::FFragment_ProceduralActionPlayer>().Get_Receipts().Read(
        InIdentity, InAfterStreamSequence, InMaxRecords);
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Get_RetainedReceiptIdentities(
        const FCk_Handle_ProceduralActionPlayer& InPlayer)
    -> TArray<FCk_ProceduralAction_Identity>
{
    if (ck::Is_NOT_Valid(InPlayer) || NOT Has(InPlayer)) { return {}; }
    return InPlayer.Get<ck::FFragment_ProceduralActionPlayer>().Get_Receipts()
        .Get_RetainedIdentities();
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Request_ConsumeReceipts(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_Request_ProceduralActionPlayer_ConsumeReceipts& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InPlayer) && Has(InPlayer)
        && NOT InPlayer.Has<ck::FTag_DestroyEntity_Initiate>()
        && InRequest.Get_ActionIdentity().Get_Value() > 0
        && InRequest.Get_ThroughStreamSequence() >= 0;
    CK_ENSURE_IF_NOT(Valid, TEXT("ProceduralActionPlayer ConsumeReceipts rejected player [{}]."), InPlayer)
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }

    const auto IsExplicit = NOT InPlayer.Get<ck::FFragment_ProceduralActionPlayer>().Get_Receipts()
        .Get_IsAutoDelivered(InRequest.Get_ActionIdentity());
    CK_ENSURE_IF_NOT(IsExplicit,
        TEXT("ProceduralActionPlayer ConsumeReceipts rejected Auto identity [{}]: the player acknowledges it."),
        InRequest.Get_ActionIdentity().Get_Value())
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }

    InPlayer.AddOrGet<ck::FFragment_ProceduralActionPlayer_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InPlayer;
}

