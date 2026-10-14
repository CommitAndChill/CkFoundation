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
        const FCk_ProceduralAction_PlaySpec& InPlay,
        const FCk_ProceduralAction_ControlSpec& InControl,
        bool InIsClip,
        ck::EProceduralActionDelivery InDelivery,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
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
    if (NOT Valid) { return {}; }

    const auto IsManaged = InDelivery == ck::EProceduralActionDelivery::Managed;
    const auto CanDeliver = NOT IsManaged || InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Managed>();
    CK_ENSURE_IF_NOT(CanDeliver,
        TEXT("ProceduralAction Play rejected player [{}]: it has no managed delivery state."), InPlayer)
    { return {}; }

    auto Player = InPlayer;
    auto& Store = Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts;
    const auto& Defaults = Player.Get<ck::FFragment_ProceduralActionPlayer_Tunables>();
    const auto Tuning = InIsClip && InPlay.Get_TuningMode() == ECk_ProceduralAction_TuningMode::Override
        ? InPlay.Get_Tuning() : Defaults.Get_Tuning();
    const auto TuningRevision = Defaults.Get_Revision();
    auto Identity = FCk_ProceduralAction_Identity{};
    const auto Reservation = InIsClip
        ? Store.ReserveUnbound(Identity)
        : Store.Reserve(InControl.Get_LimbIds(), InControl.Get_BodyChannel() == ECk_EnableDisable::Enable,
            Identity);
    if (Reservation != ck::procedural_action::EReceiptStoreWrite::Accepted) { return {}; }

    // Sealed and registered before the entity exists, so a terminal the loaded path commits synchronously is already
    // owned by managed delivery and is dispatched on the next pass instead of waiting for a consumer.
    if (IsManaged)
    {
        const auto Sealed = Store.SealManaged(Identity);
        CK_ENSURE_IF_NOT(Sealed,
            TEXT("ProceduralAction Play could not seal the freshly reserved stream [{}]."), Identity.Get_Value())
        {
            Store.AbortUnpublished(Identity);
            return {};
        }

        Player.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Add(Identity.Get_Value(),
            ck::FProceduralActionManagedDelivery{InOnCompleted});
    }

    auto ClipParams = ck::FProceduralActionClipParams{InPlay.Get_Clip(), InPlay.Get_SlotBindings(),
        InPlay.Get_AnchorBindings()};
    ClipParams.Set_Mirrors(InPlay.Get_Mirrors());
    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InScope,
        [&](FCk_Handle InNewEntity)
        {
            InNewEntity.Add<ck::FFragment_ProceduralAction_Params>(InPlayer, Identity,
                InIsClip ? ck::EProceduralActionSource::Clip : ck::EProceduralActionSource::Manual,
                ClipParams,
                ck::FProceduralActionManualParams{InControl.Get_LimbIds(),
                    InControl.Get_AnchorBindings(), InControl.Get_BodyChannel(),
                    InControl.Get_InputTimeout()}, Tuning, TuningRevision);
            auto& Action = InNewEntity.Add<ck::FFragment_ProceduralAction>();
            Action._State = InIsClip ? ECk_ProceduralAction_State::PendingLoad
                : ECk_ProceduralAction_State::PendingAdmission;
            if (InIsClip)
            { InNewEntity.Add<ck::FFragment_ProceduralAction_Loading>(Action._Generation); }
            InNewEntity.Add<ck::FFragment_ProceduralAction_Requests>();
        });
    if (ck::Is_NOT_Valid(NewEntity))
    {
        Store.AbortUnpublished(Identity);
        if (IsManaged)
        { Player.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Remove(Identity.Get_Value()); }

        return {};
    }
    auto Action = CastChecked(NewEntity);
    Player.Get<ck::FFragment_ProceduralActionPlayer>()._Actions.Add(Action);
    if (IsManaged)
    {
        auto& Entries = Player.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries;
        Entries.FindChecked(Identity.Get_Value())._Action = Action;
    }

    return Action;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Create_Manual(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_ControlSpec& InSpec)
    -> FCk_Handle_ProceduralAction
{
    auto InvalidField = FName{};
    const auto Valid = ck::procedural_action::ValidateControlSpec(InSpec, InvalidField);
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralAction Create_Manual rejected invalid field [{}]."), InvalidField)
    { return {}; }
    if (NOT Valid) { return {}; }
    return DoCreate(InScope, InPlayer, {}, InSpec, false, ck::EProceduralActionDelivery::Explicit, {});
}

auto
    UCk_Utils_ProceduralAction_UE::
    Create_FromClip(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec)
    -> FCk_Handle_ProceduralAction
{
    return DoCreate_FromClip(InScope, InPlayer, InSpec, ck::EProceduralActionDelivery::Explicit, {});
}

auto
    UCk_Utils_ProceduralAction_UE::
    DoCreate_FromClip(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec,
        ck::EProceduralActionDelivery InDelivery,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    -> FCk_Handle_ProceduralAction
{
    auto InvalidField = FName{};
    if (InSpec.Get_SlotBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { InvalidField = TEXT("Play.SlotBindings"); }
    else if (InSpec.Get_AnchorBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { InvalidField = TEXT("Play.AnchorBindings"); }
    else if (InSpec.Get_Mirrors().Num() > ck::procedural_action::content_limits::MaxSlots)
    { InvalidField = TEXT("Play.Mirrors"); }
    else if (NOT InSpec.Get_HasConsistentTuningSelection())
    { InvalidField = TEXT("Play.TuningMode"); }
    const auto Valid = InvalidField.IsNone() && NOT InSpec.Get_Clip().IsNull()
        && ck::IsValid(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Params>()
        && (InSpec.Get_TuningMode() == ECk_ProceduralAction_TuningMode::PlayerDefault
            || InSpec.Get_TuningMode() == ECk_ProceduralAction_TuningMode::Override)
        && (InSpec.Get_TuningMode() != ECk_ProceduralAction_TuningMode::Override
            || (ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField)
                && InSpec.Get_Tuning().Get_Sampling().Get_MaxSamples()
                    <= InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Params>()
                        .Get_Capacity().Get_MaxSamplesPerReceipt()));
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralAction Create_FromClip rejected clip or tuning [{}]."), InvalidField)
    { return {}; }
    if (NOT Valid) { return {}; }
    return DoCreate(InScope, InPlayer, InSpec, {}, true, InDelivery, InOnCompleted);
}

auto
    UCk_Utils_ProceduralAction_UE::
    Create_FromLoadedClip(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_LoadedPlaySpec& InSpec)
    -> FCk_Handle_ProceduralAction
{
    return DoCreate_Loaded(InScope, InPlayer, InSpec, ck::EProceduralActionDelivery::Explicit, {});
}

auto
    UCk_Utils_ProceduralAction_UE::
    DoCreate_Loaded(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_LoadedPlaySpec& InSpec,
        ck::EProceduralActionDelivery InDelivery,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    -> FCk_Handle_ProceduralAction
{
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InSpec.Get_Clip()};
    auto InvalidField = FName{};
    if (InSpec.Get_SlotBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { InvalidField = TEXT("Play.SlotBindings"); }
    else if (InSpec.Get_AnchorBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { InvalidField = TEXT("Play.AnchorBindings"); }
    else if (InSpec.Get_Mirrors().Num() > ck::procedural_action::content_limits::MaxSlots)
    { InvalidField = TEXT("Play.Mirrors"); }
    else if (NOT InSpec.Get_HasConsistentTuningSelection())
    { InvalidField = TEXT("Play.TuningMode"); }
    const auto Valid = InvalidField.IsNone() && Pinned.IsValid()
        && ck::IsValid(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Params>()
        && (InSpec.Get_TuningMode() == ECk_ProceduralAction_TuningMode::PlayerDefault
            || InSpec.Get_TuningMode() == ECk_ProceduralAction_TuningMode::Override)
        && (InSpec.Get_TuningMode() != ECk_ProceduralAction_TuningMode::Override
            || (ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField)
                && InSpec.Get_Tuning().Get_Sampling().Get_MaxSamples()
                    <= InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Params>()
                        .Get_Capacity().Get_MaxSamplesPerReceipt()));
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralAction Create_FromLoadedClip rejected clip or tuning [{}]."), InvalidField)
    { return {}; }
    if (NOT Valid) { return {}; }

    // Preserve the source path for provenance only. Playback owns detached compiled values.
    auto SoftSpec = FCk_ProceduralAction_PlaySpec{
        TSoftObjectPtr<UCk_ProceduralAction_Data>{Pinned.Get()}};
    static_cast<FCk_ProceduralAction_PlayOptions&>(SoftSpec) = InSpec;
    auto ActionHandle = DoCreate(InScope, InPlayer, SoftSpec, {}, true, InDelivery, InOnCompleted);
    if (ck::Is_NOT_Valid(ActionHandle)) { return {}; }

    auto Compiled = ck::procedural_action::FCompiledClip{};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    const auto Compiles = ck::procedural_action::Compile(*Pinned, Compiled, Diagnostics);
    auto BoundIds = TArray<FName>{};
    auto Player = InPlayer;
    const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(Player);
    const auto& Params = ActionHandle.Get<ck::FFragment_ProceduralAction_Params>();
    const auto& Capacity = Player.Get<ck::FFragment_ProceduralActionPlayer_Params>().Get_Capacity();
    const auto Mirrors = Compiles
        && ck_procedural_action_player_service::ExpandClipMirrors(Subject, Params.Get_Clip(), Compiled);
    const auto Binds = Mirrors
        && ck_procedural_action_player_service::BindCompiledClip(Subject, Params.Get_Clip(),
            Compiled, Capacity, BoundIds)
        && Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts.BindChannels(
            Params.Get_Identity(), BoundIds, Compiled.Get_HasBodyChannel())
            == ck::procedural_action::EReceiptStoreWrite::Accepted;
    ActionHandle.Remove<ck::FFragment_ProceduralAction_Loading>();
    auto& Action = ActionHandle.Get<ck::FFragment_ProceduralAction>();
    if (NOT Binds)
    {
        const auto Reason = NOT Compiles ? FName{TEXT("InvalidClipContent")}
            : NOT Mirrors ? FName{TEXT("MirrorRejected")}
            : FName{TEXT("ClipBindingsRejected")};
        Action._Result = FCk_ProceduralAction_Result{
            Compiles ? ECk_ProceduralAction_Outcome::Rejected
                : ECk_ProceduralAction_Outcome::InvalidContent,
            ECk_ProceduralAction_Recovery::NotRequired, Reason};
        Player.Get<ck::FFragment_ProceduralActionPlayer>()._Receipts.Finish(
            Params.Get_Identity(), Action._Result);
        Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
        return ActionHandle;
    }

    TSharedPtr<const ck::procedural_action::FCompiledClip> SharedClip =
        MakeShared<ck::procedural_action::FCompiledClip>(MoveTemp(Compiled));
    ActionHandle.Add<ck::FFragment_ProceduralAction_Compiled>(MoveTemp(SharedClip), MoveTemp(BoundIds));
    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::PendingAdmission);
    return ActionHandle;
}

auto
    UCk_Utils_ProceduralAction_UE::
    Play(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    -> FCk_ProceduralAction_ManagedStart
{
    const auto Action = DoCreate_FromClip(InScope, InPlayer, InSpec,
        ck::EProceduralActionDelivery::Managed, InOnCompleted);
    return DoMake_ManagedStart(Action, InSpec.Get_Clip().ToSoftObjectPath().ToString());
}

auto
    UCk_Utils_ProceduralAction_UE::
    Play_Loaded(
        FCk_Handle& InScope,
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_LoadedPlaySpec& InSpec,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    -> FCk_ProceduralAction_ManagedStart
{
    const auto Action = DoCreate_Loaded(InScope, InPlayer, InSpec,
        ck::EProceduralActionDelivery::Managed, InOnCompleted);
    const auto AssetPath = ck::IsValid(InSpec.Get_Clip())
        ? InSpec.Get_Clip()->GetPathName() : FString{};
    return DoMake_ManagedStart(Action, AssetPath);
}

auto
    UCk_Utils_ProceduralAction_UE::
    DoMake_ManagedStart(
        const FCk_Handle_ProceduralAction& InAction,
        const FString& InAssetPath)
    -> FCk_ProceduralAction_ManagedStart
{
    if (ck::IsValid(InAction))
    {
        return FCk_ProceduralAction_ManagedStart{InAction, Get_Identity(InAction),
            ck::procedural_action::MakePublicReport(ECk_ProceduralAction_ReportStatus::Valid, false, {})};
    }

    const auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{
        ck::procedural_action::MakeReportDiagnostic(TEXT("StartRejected"), InAssetPath, TEXT("Play"), {},
            TEXT("valid scope, player, spec and receipt capacity"), TEXT("rejected; the log names the cause"))};
    return FCk_ProceduralAction_ManagedStart{{}, {},
        ck::procedural_action::MakePublicReport(ECk_ProceduralAction_ReportStatus::Invalid, false, Diagnostics)};
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
    Request_SubmitPoseBatch(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_SubmitPoseBatch& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InAction) && Has(InAction)
        && NOT InAction.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(Valid, TEXT("ProceduralAction SubmitPoseBatch rejected action [{}]."), InAction)
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
    Request_End(
        FCk_Handle_ProceduralAction& InAction,
        const FCk_Request_ProceduralAction_End& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralAction
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InAction) && Has(InAction)
        && NOT InAction.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(Valid, TEXT("ProceduralAction End rejected action [{}]."), InAction)
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
