#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReport.h"
#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "UObject/StrongObjectPtr.h"

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_ProceduralActionPlayer_UE,
    FCk_Handle_ProceduralActionPlayer, ck::FFragment_ProceduralActionPlayer);

namespace ck_procedural_action_player_utils
{
    auto
    Get_IsManagedIdentity(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity)
        -> bool
    {
        return InPlayer.Get<ck::FFragment_ProceduralActionPlayer>().Get_Receipts().Get_IsManagedDelivery(InIdentity);
    }

    template <typename T_Delegate>
    auto
    AddListener(
        TArray<T_Delegate>& InOutListeners,
        const T_Delegate& InDelegate,
        FCk_ProceduralAction_Identity InIdentity)
        -> void
    {
        const auto DelegateIsBound = InDelegate.IsBound();
        CK_ENSURE_IF_NOT(DelegateIsBound,
            TEXT("Managed listener for ProceduralAction identity [{}] is not bound."), InIdentity.Get_Value())
        { return; }

        if (InOutListeners.Contains(InDelegate))
        { return; }

        const auto HasRoom = InOutListeners.Num() < ck::FProceduralActionManagedDelivery::MaxListeners;
        CK_ENSURE_IF_NOT(HasRoom,
            TEXT("ProceduralAction identity [{}] already has [{}] managed listeners of this kind."),
            InIdentity.Get_Value(), InOutListeners.Num())
        { return; }

        InOutListeners.Add(InDelegate);
    }
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Get_AdmissionReport(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_ProceduralAction_PlaySpec& InSpec)
    -> FCk_ProceduralAction_ValidationReport
{
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    const auto AssetPath = InSpec.Get_Clip().ToSoftObjectPath().ToString();
    auto Reject = [&](FName InCode, const FString& InField, const FString& InExpected,
        const FString& InActual) -> FCk_ProceduralAction_ValidationReport
    {
        Diagnostics.Add(ck::procedural_action::MakeReportDiagnostic(InCode, AssetPath,
            InField, {}, InExpected, InActual));
        return ck::procedural_action::MakePublicReport(
            ECk_ProceduralAction_ReportStatus::Invalid, true, Diagnostics);
    };
    if (ck::Is_NOT_Valid(InPlayer) || NOT Has(InPlayer)
        || InPlayer.Has<ck::FTag_DestroyEntity_Initiate>()
        || NOT InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Params>())
    { return Reject(TEXT("Player"), TEXT("Player"), TEXT("live action player"), TEXT("invalid")); }
    if (InSpec.Get_Clip().IsNull())
    { return Reject(TEXT("Clip"), TEXT("Clip"), TEXT("nonnull soft clip path"), TEXT("null")); }
    const auto& Capacity = InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Params>()
        .Get_Capacity();
    if (InSpec.Get_SlotBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { return Reject(TEXT("SlotBindingCount"), TEXT("SlotBindings"), TEXT("at most 64"),
        FString::FromInt(InSpec.Get_SlotBindings().Num())); }
    if (InSpec.Get_AnchorBindings().Num() > ck::procedural_action::receipt_limits::MaxClaimedLimbs)
    { return Reject(TEXT("AnchorBindingCount"), TEXT("AnchorBindings"), TEXT("at most 64"),
        FString::FromInt(InSpec.Get_AnchorBindings().Num())); }
    if (InSpec.Get_TuningMode() != ECk_ProceduralAction_TuningMode::PlayerDefault
        && InSpec.Get_TuningMode() != ECk_ProceduralAction_TuningMode::Override)
    { return Reject(TEXT("TuningMode"), TEXT("TuningMode"), TEXT("PlayerDefault or Override"),
        TEXT("unsupported mode")); }
    if (NOT InSpec.Get_HasConsistentTuningSelection())
    { return Reject(TEXT("InactiveTuning"), TEXT("Tuning"),
        TEXT("default values while TuningMode is PlayerDefault"), TEXT("nondefault inactive tuning")); }
    if (InSpec.Get_TuningMode() == ECk_ProceduralAction_TuningMode::Override)
    {
        auto InvalidField = FName{};
        if (NOT ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField))
        { return Reject(TEXT("Tuning"), InvalidField.ToString(), TEXT("valid tuning"), TEXT("invalid")); }
        if (InSpec.Get_Tuning().Get_Sampling().Get_MaxSamples()
            > Capacity.Get_MaxSamplesPerReceipt())
        { return Reject(TEXT("SampleCapacity"), TEXT("Tuning.Sampling.MaxSamples"),
            FString::FromInt(Capacity.Get_MaxSamplesPerReceipt()),
            FString::FromInt(InSpec.Get_Tuning().Get_Sampling().Get_MaxSamples())); }
    }
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InSpec.Get_Clip().Get()};
    if (NOT Pinned.IsValid())
    { return ck::procedural_action::MakePublicReport(
        ECk_ProceduralAction_ReportStatus::ContentNotResident, true, Diagnostics); }

    auto Compiled = ck::procedural_action::FCompiledClip{};
    if (NOT ck::procedural_action::Compile(*Pinned, Compiled, Diagnostics))
    { return ck::procedural_action::MakePublicReport(
        ECk_ProceduralAction_ReportStatus::Invalid, true, Diagnostics); }
    const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(InPlayer);
    auto ClipParams = ck::FProceduralActionClipParams{InSpec.Get_Clip(),
        InSpec.Get_SlotBindings(), InSpec.Get_AnchorBindings()};
    ClipParams.Set_Mirrors(InSpec.Get_Mirrors());
    if (NOT ck_procedural_action_player_service::ExpandClipMirrors(Subject, ClipParams, Compiled, &Diagnostics))
    { return ck::procedural_action::MakePublicReport(
        ECk_ProceduralAction_ReportStatus::Invalid, true, Diagnostics); }
    auto BoundIds = TArray<FName>{};
    if (NOT ck_procedural_action_player_service::BindCompiledClip(Subject, ClipParams,
        Compiled, Capacity, BoundIds, &Diagnostics))
    { return ck::procedural_action::MakePublicReport(
        ECk_ProceduralAction_ReportStatus::Invalid, true, Diagnostics); }
    return ck::procedural_action::MakePublicReport(
        ECk_ProceduralAction_ReportStatus::Valid, true, Diagnostics);
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Add(
        FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ProceduralActionPlayer_Spec& InSpec)
    -> FCk_Handle_ProceduralActionPlayer
{
    auto InvalidField = FName{};
    const auto SubjectLive = ck::IsValid(InSubject);
    const auto LegacyBodyPoseIdle = SubjectLive &&
        (NOT InSubject.Has<ck::FFragment_ProceduralBodyPose_Action>()
            || InSubject.Get<ck::FFragment_ProceduralBodyPose_Action>()
                .Get_Offset().Equals(FTransform::Identity, KINDA_SMALL_NUMBER))
        && (NOT InSubject.Has<ck::FFragment_ProceduralBodyPose_Requests>()
            || InSubject.Get<ck::FFragment_ProceduralBodyPose_Requests>()
                .Get_Requests().IsEmpty());
    const auto Valid = SubjectLive
        && NOT InSubject.Has<ck::FTag_DestroyEntity_Initiate>()
        && NOT Has(InSubject)
        && LegacyBodyPoseIdle
        && ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField)
        && ck::procedural_action::ValidateCapacity(InSpec.Get_Capacity(), InvalidField)
        && InSpec.Get_Tuning().Get_Sampling().Get_MaxSamples()
            <= InSpec.Get_Capacity().Get_MaxSamplesPerReceipt();
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralActionPlayer Add rejected subject [{}], invalid field [{}]."), InSubject, InvalidField)
    { return {}; }
    if (NOT Valid) { return {}; }
    if (NOT FCk_ProceduralLimbControl_Control::Attach(InSubject, InSpec.Get_Locomotion())) { return {}; }

    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Params>(InSpec.Get_Capacity());
    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Tunables>(InSpec.Get_Tuning());
    auto& Player = InSubject.Add<ck::FFragment_ProceduralActionPlayer>();
    if (NOT Player._Receipts.Configure(InSpec.Get_Capacity()))
    {
        InSubject.Remove<ck::FFragment_ProceduralActionPlayer>();
        InSubject.Remove<ck::FFragment_ProceduralActionPlayer_Tunables>();
        InSubject.Remove<ck::FFragment_ProceduralActionPlayer_Params>();
        return {};
    }
    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Requests>();
    InSubject.Add<ck::FFragment_ProceduralActionPlayer_Managed>();
    return CastChecked(InSubject);
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Make_DedicatedPlayerSpec(
        const FCk_ProceduralAction_Tuning& InTuning)
    -> FCk_ProceduralActionPlayer_Spec
{
    auto Spec = FCk_ProceduralActionPlayer_Spec{};
    Spec.Set_Tuning(InTuning);
    return Spec;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Make_BorrowingPlayerSpec(
        const FCk_ProceduralActionPlayer_BorrowingSpec& InSpec)
    -> FCk_ProceduralActionPlayer_Spec
{
    auto Locomotion = FCk_ProceduralLimbControl_Locomotion{};
    Locomotion.Set_Support(InSpec.Get_Support());
    Locomotion.Set_Return(InSpec.Get_Return());
    auto Spec = FCk_ProceduralActionPlayer_Spec{};
    Spec.Set_Tuning(InSpec.Get_Tuning());
    Spec.Set_Locomotion(Locomotion);
    return Spec;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Make_CountOnlySupport(
        int32 InMinimumTrustedContacts)
    -> FCk_ProceduralLimbControl_Support
{
    const auto Support = FCk_ProceduralLimbControl_Support{
        ECk_ProceduralLimbControl_SupportMode::CountOnly, InMinimumTrustedContacts};
    const auto SupportIsValid = ck::procedural_support::ValidatePolicy(Support);
    CK_ENSURE_IF_NOT(SupportIsValid,
        TEXT("Count-only support rejected minimum trusted contacts [{}]; it must lie in 1..64."),
        InMinimumTrustedContacts)
    { return Support; }

    return Support;
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
    if (NOT Valid)
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }

    const auto IsExplicit = NOT ck_procedural_action_player_utils::Get_IsManagedIdentity(InPlayer,
        InRequest.Get_ActionIdentity());
    CK_ENSURE_IF_NOT(IsExplicit,
        TEXT("ProceduralActionPlayer ConsumeReceipts rejected managed identity [{}]: managed delivery acknowledges it."),
        InRequest.Get_ActionIdentity().Get_Value())
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }

    InPlayer.AddOrGet<ck::FFragment_ProceduralActionPlayer_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InPlayer;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    BindTo_OnManagedRecords(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    const auto PlayerIsLive = ck::IsValid(InPlayer) && Has(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Managed>()
        && NOT InPlayer.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(PlayerIsLive,
        TEXT("ProceduralActionPlayer BindTo_OnManagedRecords rejected player [{}]."), InPlayer)
    { return InPlayer; }

    auto* Entry = InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(InIdentity.Get_Value());
    const auto IsManaged = Entry != nullptr && NOT Entry->_Completed;
    CK_ENSURE_IF_NOT(IsManaged,
        TEXT("ProceduralActionPlayer BindTo_OnManagedRecords: identity [{}] is not an active managed delivery."),
        InIdentity.Get_Value())
    { return InPlayer; }

    ck_procedural_action_player_utils::AddListener(Entry->_RecordListeners, InDelegate, InIdentity);
    return InPlayer;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    UnbindFrom_OnManagedRecords(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    if (ck::Is_NOT_Valid(InPlayer) || NOT InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Managed>())
    { return InPlayer; }

    auto* Entry = InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(InIdentity.Get_Value());
    if (Entry != nullptr)
    { Entry->_RecordListeners.Remove(InDelegate); }

    return InPlayer;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    BindTo_OnManagedAcceptedPose(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedAcceptedPose& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    const auto PlayerIsLive = ck::IsValid(InPlayer) && Has(InPlayer)
        && InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Managed>()
        && NOT InPlayer.Has<ck::FTag_DestroyEntity_Initiate>();
    CK_ENSURE_IF_NOT(PlayerIsLive,
        TEXT("ProceduralActionPlayer BindTo_OnManagedAcceptedPose rejected player [{}]."), InPlayer)
    { return InPlayer; }

    auto* Entry = InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(InIdentity.Get_Value());
    const auto IsManaged = Entry != nullptr && NOT Entry->_Completed;
    CK_ENSURE_IF_NOT(IsManaged,
        TEXT("ProceduralActionPlayer BindTo_OnManagedAcceptedPose: identity [{}] is not an active managed delivery."),
        InIdentity.Get_Value())
    { return InPlayer; }

    ck_procedural_action_player_utils::AddListener(Entry->_PoseListeners, InDelegate, InIdentity);
    return InPlayer;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    UnbindFrom_OnManagedAcceptedPose(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_Delegate_ProceduralAction_OnManagedAcceptedPose& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    if (ck::Is_NOT_Valid(InPlayer) || NOT InPlayer.Has<ck::FFragment_ProceduralActionPlayer_Managed>())
    { return InPlayer; }

    auto* Entry = InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(InIdentity.Get_Value());
    if (Entry != nullptr)
    { Entry->_PoseListeners.Remove(InDelegate); }

    return InPlayer;
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Get_IsManagedDelivery(
        const FCk_Handle_ProceduralActionPlayer& InPlayer,
        FCk_ProceduralAction_Identity InIdentity)
    -> bool
{
    return ck::IsValid(InPlayer) && Has(InPlayer)
        && ck_procedural_action_player_utils::Get_IsManagedIdentity(InPlayer, InIdentity);
}

auto
    UCk_Utils_ProceduralActionPlayer_UE::
    Request_UpdateTuning(
        FCk_Handle_ProceduralActionPlayer& InPlayer,
        const FCk_Request_ProceduralActionPlayer_UpdateTuning& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralActionPlayer
{
    auto Request = InRequest;
    if (InDelegate.IsBound()) { Request.Set_CompletionDelegate(InDelegate); }
    auto InvalidField = FName{};
    const auto Valid = ck::IsValid(InPlayer) && Has(InPlayer)
        && NOT InPlayer.Has<ck::FTag_DestroyEntity_Initiate>()
        && ck::procedural_action::ValidateTuning(InRequest.Get_Tuning(), InvalidField)
        && InRequest.Get_Tuning().Get_Sampling().Get_MaxSamples()
            <= InPlayer.Get<ck::FFragment_ProceduralActionPlayer_Params>()
                .Get_Capacity().Get_MaxSamplesPerReceipt();
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralActionPlayer UpdateTuning rejected player [{}], invalid field [{}]."), InPlayer, InvalidField)
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }
    if (NOT Valid)
    {
        Request.TryFireCompletion(InPlayer, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InPlayer;
    }
    InPlayer.AddOrGet<ck::FFragment_ProceduralActionPlayer_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InPlayer;
}
