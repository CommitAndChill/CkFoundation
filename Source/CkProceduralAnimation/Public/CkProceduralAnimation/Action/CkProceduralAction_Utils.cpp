#include "CkProceduralAnimation/Action/CkProceduralAction_Utils.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReport.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "UObject/StrongObjectPtr.h"

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_ProceduralAction_UE,
    FCk_Handle_ProceduralAction, ck::FFragment_ProceduralAction);

namespace ck_procedural_action_utils
{
    template <typename T_Enum>
    auto
    Get_EnumIdentifier(
        T_Enum InValue)
        -> FString
    {
        const auto Identifier = StaticEnum<T_Enum>()->GetNameStringByValue(static_cast<int64>(InValue));
        return Identifier.IsEmpty() ? FString::FromInt(static_cast<int32>(InValue)) : Identifier;
    }
}

auto
    UCk_Utils_ProceduralAction_UE::
    DoCreate(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InPlay)
    -> FCk_Handle_ProceduralAction
{
    const auto Valid = ck::IsValid(InScope)
        && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InScope)
        && ck::IsValid(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer>()
        && NOT InPlayer.Has<ck::FTag_DestroyEntity_Initiate>()
        && InScope.Get_RegistryView().Get_RegistryHandle()
            == InPlayer.Get_RegistryView().Get_RegistryHandle();
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralAction Create rejected scope [{}] or player [{}]."), InScope, InPlayer)
    { return {}; }

    auto Player = InPlayer;
    auto& Store = Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts;
    const auto& Tuning = Player.Get<ck::FFragment_ProceduralActionPlayer_Params>().Get_Tuning();
    auto Identity = FCk_ProceduralAction_Identity{};
    if (Store.ReserveUnbound(Identity) != ck::procedural_action::EReceiptStoreWrite::Accepted) { return {}; }

    // Sealed before the entity exists, so a terminal the loaded path commits synchronously is already the player's to
    // acknowledge.
    if (InPlay.Get_Delivery() == ECk_ProceduralAction_Delivery::Auto)
    {
        const auto Sealed = Store.SealAuto(Identity);
        CK_ENSURE_IF_NOT(Sealed,
            TEXT("ProceduralAction Create could not seal the freshly reserved stream [{}]."), Identity.Get_Value())
        {
            Store.AbortUnpublished(Identity);
            return {};
        }
    }

    const auto ClipParams = ck::FProceduralActionClipParams{InPlay.Get_Clip(), InPlay.Get_SlotBindings()};
    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InScope,
        [&](FCk_Handle InNewEntity)
        {
            InNewEntity.Add<ck::FFragment_ProceduralAction_Params>(InPlayer, Identity, ClipParams, Tuning);
            auto& Action = InNewEntity.Add<ck::FFragment_ProceduralAction>();
            Action._State = ECk_ProceduralAction_State::PendingLoad;
            InNewEntity.Add<ck::FFragment_ProceduralAction_Loading>(Action._Generation);
            InNewEntity.Add<ck::FFragment_ProceduralAction_Requests>();
        });
    if (ck::Is_NOT_Valid(NewEntity))
    {
        Store.AbortUnpublished(Identity);
        return {};
    }
    auto Action = CastChecked(NewEntity);
    Player.Get<ck::FFragment_ProceduralActionPlayer>()._Actions.Add(Action);
    return Action;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Create_FromClip(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec)
    -> FCk_Handle_ProceduralAction
{
    const auto Valid = InSpec.Get_SlotBindings().Num() <= ck::procedural_action::receipt_limits::MaxClaimedLimbs
        && (InSpec.Get_Delivery() == ECk_ProceduralAction_Delivery::Explicit
            || InSpec.Get_Delivery() == ECk_ProceduralAction_Delivery::Auto)
        && NOT InSpec.Get_Clip().IsNull()
        && ck::IsValid(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Params>();
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralAction Create_FromClip rejected player [{}] or spec: clip [{}], [{}] slot bindings, delivery [{}]."),
        InPlayer, InSpec.Get_Clip().ToSoftObjectPath().ToString(), InSpec.Get_SlotBindings().Num(), InSpec.Get_Delivery())
    { return {}; }

    // Pinned across the synchronous compile; playback owns detached compiled values afterwards.
    const auto Loaded = TStrongObjectPtr<UCk_ProceduralAction_Data>{InSpec.Get_Clip().Get()};
    auto Action = DoCreate(InScope, InPlayer, InSpec);
    if (ck::IsValid(Action) && Loaded.IsValid())
    { DoAdmit_LoadedClip(Action, *Loaded); }
    return Action;
}

auto
    UCk_Utils_ProceduralAction_UE::
    DoAdmit_LoadedClip(
        FCk_Handle_ProceduralAction& InAction,
        const UCk_ProceduralAction_Data& InClip)
    -> void
{
    auto Compiled = ck::procedural_action::FCompiledClip{};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    const auto Compiles = ck::procedural_action::Compile(InClip, Compiled, Diagnostics);
    const auto& Params = InAction.Get<ck::FFragment_ProceduralAction_Params>();
    auto Player = Params.Get_Player();
    const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(Player);
    auto BoundIds = TArray<FName>{};
    const auto Binds = Compiles
        && ck_procedural_action_player_service::BindCompiledClip(Subject, Params.Get_Clip(), Compiled, BoundIds)
        && Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts.BindChannels(
            Params.Get_Identity(), BoundIds, Compiled.Get_HasBodyChannel())
            == ck::procedural_action::EReceiptStoreWrite::Accepted;
    InAction.Remove<ck::FFragment_ProceduralAction_Loading>();
    auto& Action = InAction.Get<ck::FFragment_ProceduralAction>();
    if (NOT Binds)
    {
        Action._Result = FCk_ProceduralAction_Result{
            Compiles ? ECk_ProceduralAction_Outcome::Rejected : ECk_ProceduralAction_Outcome::InvalidContent,
            ECk_ProceduralAction_Recovery::NotRequired,
            Compiles ? FName{TEXT("ClipBindingsRejected")} : FName{TEXT("InvalidClipContent")}};
        Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts.Finish(Params.Get_Identity(), Action._Result);
        Action.CommitState(InAction, Params, ECk_ProceduralAction_State::Finished);
        return;
    }

    TSharedPtr<const ck::procedural_action::FCompiledClip> SharedClip =
        MakeShared<ck::procedural_action::FCompiledClip>(MoveTemp(Compiled));
    InAction.Add<ck::FFragment_ProceduralAction_Compiled>(MoveTemp(SharedClip), MoveTemp(BoundIds));
    Action.CommitState(InAction, Params, ECk_ProceduralAction_State::PendingAdmission);
}

auto
    UCk_Utils_ProceduralAction_UE::
    Validate_Data(
        UCk_ProceduralAction_Data* InData)
    -> FCk_ProceduralAction_ValidationReport
{
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InData};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    if (NOT Pinned.IsValid())
    {
        Diagnostics.Add(ck::procedural_action::MakeReportDiagnostic(TEXT("NullData"), {},
            TEXT("Data"), {}, TEXT("valid loaded clip object"), TEXT("null or invalid")));
        return ck::procedural_action::MakePublicReport(
            ECk_ProceduralAction_ReportStatus::Invalid, false, Diagnostics);
    }
    auto Compiled = ck::procedural_action::FCompiledClip{};
    const auto Valid = ck::procedural_action::Compile(*Pinned, Compiled, Diagnostics);
    return ck::procedural_action::MakePublicReport(
        Valid ? ECk_ProceduralAction_ReportStatus::Valid
            : ECk_ProceduralAction_ReportStatus::Invalid,
        false, Diagnostics);
}

auto
    UCk_Utils_ProceduralAction_UE::
    Get_State(
        const FCk_Handle_ProceduralAction& InAction)
    -> ECk_ProceduralAction_State
{
    return ck::IsValid(InAction) && Has(InAction)
        ? InAction.Get<ck::FFragment_ProceduralAction>().Get_State()
        : ECk_ProceduralAction_State::Finished;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Get_Result(
        const FCk_Handle_ProceduralAction& InAction)
    -> FCk_ProceduralAction_Result
{
    return ck::IsValid(InAction) && Has(InAction)
        ? InAction.Get<ck::FFragment_ProceduralAction>().Get_Result()
        : FCk_ProceduralAction_Result{};
}

auto
    UCk_Utils_ProceduralAction_UE::
    Get_Identity(
        const FCk_Handle_ProceduralAction& InAction)
    -> FCk_ProceduralAction_Identity
{
    return ck::IsValid(InAction) && Has(InAction)
        ? InAction.Get<ck::FFragment_ProceduralAction_Params>().Get_Identity()
        : FCk_ProceduralAction_Identity{};
}

auto
    UCk_Utils_ProceduralAction_UE::
    Get_LastReceipt(
        const FCk_Handle_ProceduralAction& InAction)
    -> FCk_ProceduralAction_Receipt
{
    if (ck::Is_NOT_Valid(InAction) || NOT Has(InAction)) { return {}; }
    const auto& Params = InAction.Get<ck::FFragment_ProceduralAction_Params>();
    const auto Player = Params.Get_Player();
    return ck::IsValid(Player) && Player.Has<ck::FFragment_ProceduralActionPlayer>()
        ? Player.Get<ck::FFragment_ProceduralActionPlayer>().Get_Receipts()
            .Get_LastAccepted(Params.Get_Identity())
        : FCk_ProceduralAction_Receipt{};
}

auto
    UCk_Utils_ProceduralAction_UE::
    Get_ResultText(
        const FCk_ProceduralAction_Result& InResult,
        const FCk_ProceduralAction_FailureContext& InContext)
    -> FText
{
    auto Text = ck::Format_UE(TEXT("Outcome: {}; Recovery: {}"),
        ck_procedural_action_utils::Get_EnumIdentifier(InResult.Get_Outcome()),
        ck_procedural_action_utils::Get_EnumIdentifier(InResult.Get_Recovery()));
    if (NOT InResult.Get_Reason().IsNone())
    { Text += ck::Format_UE(TEXT("; Reason: {}"), InResult.Get_Reason()); }
    if (NOT InContext.Get_PhaseId().IsNone())
    { Text += ck::Format_UE(TEXT("; Phase: {}"), InContext.Get_PhaseId()); }
    if (NOT InContext.Get_LimbIds().IsEmpty())
    {
        auto LimbIds = TArray<FString>{};
        for (const auto Id : InContext.Get_LimbIds())
        { LimbIds.Add(Id.ToString()); }
        Text += ck::Format_UE(TEXT("; Limbs: {}"), FString::Join(LimbIds, TEXT(", ")));
    }
    if (InContext.Get_Elapsed() != FCk_Time{})
    { Text += ck::Format_UE(TEXT("; Elapsed: {:.3f} s"), InContext.Get_Elapsed().Get_Seconds()); }
    if (InContext.Get_Limit() != FCk_Time{})
    { Text += ck::Format_UE(TEXT("; Limit: {:.3f} s"), InContext.Get_Limit().Get_Seconds()); }
    if (InContext.Get_Observed() != 0)
    { Text += ck::Format_UE(TEXT("; Observed: {} {}"), InContext.Get_Observed(), InContext.Get_BudgetUnit()); }
    if (InContext.Get_Budget() != 0)
    { Text += ck::Format_UE(TEXT("; Budget: {} {}"), InContext.Get_Budget(), InContext.Get_BudgetUnit()); }
    return FText::FromString(Text);
}

auto
    UCk_Utils_ProceduralAction_UE::
    BindTo_OnStateChanged(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnStateChanged& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_ProceduralAction
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnProceduralAction_StateChanged, InAction,
        InDelegate, InBindingPolicy, InPostFireBehavior);
    return InAction;
}

auto
    UCk_Utils_ProceduralAction_UE::
    UnbindFrom_OnStateChanged(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnStateChanged& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnProceduralAction_StateChanged, InAction, InDelegate);
    return InAction;
}

auto
    UCk_Utils_ProceduralAction_UE::
    BindTo_OnFinished(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnFinished& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_ProceduralAction
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnProceduralAction_Finished, InAction,
        InDelegate, InBindingPolicy, InPostFireBehavior);
    return InAction;
}

auto
    UCk_Utils_ProceduralAction_UE::
    UnbindFrom_OnFinished(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Delegate_ProceduralAction_OnFinished& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnProceduralAction_Finished, InAction, InDelegate);
    return InAction;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Request_Cancel(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_Cancel& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InAction) && Has(InAction)
        && NOT InAction.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(Valid, TEXT("ProceduralAction Cancel rejected action [{}]."), InAction)
    {
        Request.TryFireCompletion(InAction, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InAction;
    }
    if (NOT Valid)
    {
        Request.TryFireCompletion(InAction, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InAction;
    }
    InAction.AddOrGet<ck::FFragment_ProceduralAction_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InAction;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Request_Release(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_Release& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InAction) && Has(InAction)
        && NOT InAction.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(Valid, TEXT("ProceduralAction Release rejected action [{}]."), InAction)
    {
        Request.TryFireCompletion(InAction, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InAction;
    }
    if (NOT Valid)
    {
        Request.TryFireCompletion(InAction, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InAction;
    }
    InAction.AddOrGet<ck::FFragment_ProceduralAction_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InAction;
}
