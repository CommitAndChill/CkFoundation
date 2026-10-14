#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control.h"

#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_limb_control
{
    // The player has the same hard maximum retained-instance capacity. Records allocate only as admitted.
    constexpr auto MaximumControllers = 256;

    auto
    IsLive(
        const FCk_Handle& InHandle)
        -> bool
    {
        return ck::IsValid(InHandle) && NOT InHandle.Has<FTag_DestroyEntity_Initiate>();
    }

    auto
    IsValidPolicy(
        const FCk_ProceduralLimbControl_Return& InPolicy)
        -> bool
    {
        return FMath::IsFinite(InPolicy.Get_Duration().Get_Seconds()) && InPolicy.Get_Duration() > FCk_Time{}
            && FMath::IsFinite(InPolicy.Get_Timeout().Get_Seconds()) && InPolicy.Get_Timeout() >= InPolicy.Get_Duration()
            && InPolicy.Get_MaxProbeAttempts() > 0 && InPolicy.Get_MaxProbeAttempts() <= 16
            && FMath::IsFinite(InPolicy.Get_RetryInterval().Get_Seconds()) && InPolicy.Get_RetryInterval() > FCk_Time{}
            && FMath::IsFinite(InPolicy.Get_ContactToleranceCm()) && InPolicy.Get_ContactToleranceCm() >= 0.0f;
    }

    auto
    BlendRigid(
        const FTransform& InFrom,
        const FTransform& InTo,
        double InAlpha)
        -> FTransform
    {
        return FTransform{FQuat::Slerp(InFrom.GetRotation(), InTo.GetRotation(), InAlpha).GetNormalized(),
            FMath::Lerp(InFrom.GetLocation(), InTo.GetLocation(), InAlpha)};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_ProceduralLimbControl_Control::
    Attach(
        FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ProceduralLimbControl_Locomotion& InLocomotion)
    -> bool
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT UCk_Utils_ProceduralArticulation_UE::Has(InSubject)
        || InSubject.Has<ck::FFragment_ProceduralLimbControl>()
        || InSubject.Has<ck::FFragment_ProceduralLimbControl_Params>()
        || NOT ck::procedural_support::ValidatePolicy(InLocomotion.Get_Support())
        || NOT ck::procedural_limb_control::IsValidPolicy(InLocomotion.Get_Return())
        || (InLocomotion.Get_Support().Get_Mode() != ECk_ProceduralLimbControl_SupportMode::Disabled
            && NOT UCk_Utils_ProceduralGait_UE::Has(InSubject)))
    { return false; }
    InSubject.Add<ck::FFragment_ProceduralLimbControl_Params>(InLocomotion.Get_Support(), InLocomotion.Get_Return());
    InSubject.Add<ck::FFragment_ProceduralLimbControl>();
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    TryAcquire(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_Handle& InOwner,
        const FCk_ArticulationChannelSet& InChannels,
        const FCk_ProceduralLimbControl_Return& InDedicatedReturn,
        FCk_ProceduralArticulation_Pose& OutEntry)
    -> ECk_ProceduralLimbControl_AcquireResult
{
    return DoTryAcquire(InSubject, InToken, InOwner, InChannels, InDedicatedReturn, false, OutEntry);
}

auto
    FCk_ProceduralLimbControl_Control::
    DoTryAcquire(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_Handle& InOwner,
        const FCk_ArticulationChannelSet& InChannels,
        const FCk_ProceduralLimbControl_Return& InDedicatedReturn,
        bool InRecoveryOnly,
        FCk_ProceduralArticulation_Pose& OutEntry)
    -> ECk_ProceduralLimbControl_AcquireResult
{
    OutEntry = {};
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return ECk_ProceduralLimbControl_AcquireResult::InvalidSubject; }
    if (NOT ck::procedural_limb_control::IsLive(InOwner)
        || InOwner.Get_RegistryView().Get_RegistryHandle() != InSubject.Get_RegistryView().Get_RegistryHandle())
    { return ECk_ProceduralLimbControl_AcquireResult::InvalidOwner; }
    if (NOT ck::procedural_limb_control::IsValidPolicy(InDedicatedReturn)
        || InChannels.Get_LimbIds().Num() > 64)
    { return ECk_ProceduralLimbControl_AcquireResult::InvalidChannels; }

    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    if (Control._Controllers.Num() >= ck::procedural_limb_control::MaximumControllers
        || Control._Controllers.ContainsByPredicate([InToken](const auto& InController)
            { return InController.Get_Token() == InToken; }))
    { return ECk_ProceduralLimbControl_AcquireResult::Busy; }

    auto LocomotorIds = TArray<FName>{};
    for (const auto Id : InChannels.Get_LimbIds())
    {
        const auto Limb = UCk_Utils_ProceduralArticulation_UE::Get_Limb(Subject, Id);
        if (NOT ck::procedural_limb_control::IsLive(Limb))
        { return ECk_ProceduralLimbControl_AcquireResult::UnavailableLimb; }
        auto Layout = FCk_ProceduralLimb_Layout{};
        if (NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout))
        { return ECk_ProceduralLimbControl_AcquireResult::UnavailableLimb; }
        if (Layout.Get_Role() == ECk_ProceduralLimb_Role::Locomotor)
        { LocomotorIds.Add(Id); }
        else if (InRecoveryOnly)
        { return ECk_ProceduralLimbControl_AcquireResult::InvalidChannels; }
    }

    auto Gait = FCk_Handle_ProceduralGait{};
    auto Snapshot = FCk_ProceduralGaitParticipation_Snapshot{};
    if (NOT LocomotorIds.IsEmpty())
    {
        if (NOT UCk_Utils_ProceduralGait_UE::Has(Subject))
        { return ECk_ProceduralLimbControl_AcquireResult::SupportUnavailable; }
        Gait = UCk_Utils_ProceduralGait_UE::CastChecked(Subject);
        if (FCk_ProceduralGaitParticipation_Control::GetSnapshot(Gait, Snapshot)
            != ECk_ProceduralGaitParticipation_Result::Accepted)
        { return ECk_ProceduralLimbControl_AcquireResult::NotReady; }
    }

    const auto Reserved = FCk_ProceduralArticulation_Control::TryReserveClaims(Subject, InToken, InChannels);
    if (Reserved != ECk_ArticulationControlResult::Accepted)
    {
        if (Reserved == ECk_ArticulationControlResult::Faulted)
        { return ECk_ProceduralLimbControl_AcquireResult::Faulted; }
        if (Reserved == ECk_ArticulationControlResult::Busy)
        { return ECk_ProceduralLimbControl_AcquireResult::NotReady; }
        if (Reserved == ECk_ArticulationControlResult::ClaimConflict)
        { return ECk_ProceduralLimbControl_AcquireResult::Busy; }
        return ECk_ProceduralLimbControl_AcquireResult::InvalidChannels;
    }
    const auto Revision = UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(Subject);
    if (Revision <= 0 || FCk_ProceduralArticulation_Control::CaptureAcceptedEntry(Subject, InToken,
        static_cast<uint64>(Revision), OutEntry) != ECk_ArticulationControlResult::Accepted)
    {
        FCk_ProceduralArticulation_Control::ReleaseClaims(Subject, InToken, InChannels);
        return ECk_ProceduralLimbControl_AcquireResult::NotReady;
    }
    const auto& Locomotion = Subject.Get<ck::FFragment_ProceduralLimbControl_Params>();
    if (NOT LocomotorIds.IsEmpty())
    {
        auto Observation = FCk_ProceduralGaitParticipation_Observation{};
        const auto Withdrawn = InRecoveryOnly
            ? FCk_ProceduralGaitParticipation_Control::TryAcquireRecoveryLease(
                Gait, InToken, LocomotorIds, Locomotion.Get_Support(), Snapshot, Observation)
            : FCk_ProceduralGaitParticipation_Control::TryWithdraw(
                Gait, InToken, LocomotorIds, Locomotion.Get_Support(), Snapshot, Observation);
        if (Withdrawn != ECk_ProceduralGaitParticipation_Result::Accepted)
        {
            FCk_ProceduralArticulation_Control::ReleaseClaims(Subject, InToken, InChannels);
            OutEntry = {};
            return Withdrawn == ECk_ProceduralGaitParticipation_Result::Busy
                ? ECk_ProceduralLimbControl_AcquireResult::Busy
                : ECk_ProceduralLimbControl_AcquireResult::SupportUnavailable;
        }
    }
    auto& Controller = Control._Controllers.Emplace_GetRef(InToken, InOwner, InChannels, InDedicatedReturn);
    Controller._LocomotorIds = MoveTemp(LocomotorIds);
    Controller._LocomotionReturnPolicy = Locomotion.Get_Return();
    Controller._DedicatedReturned = NOT InChannels.Get_Body()
        && Controller._LocomotorIds.Num() == InChannels.Get_LimbIds().Num();
    Controller._LocomotionReturned = Controller._LocomotorIds.IsEmpty();
    Controller._Autonomous = InRecoveryOnly;
    return ECk_ProceduralLimbControl_AcquireResult::Accepted;
}

auto
    FCk_ProceduralLimbControl_Control::
    BeginReturn(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        FName InReason)
    -> ECk_ProceduralLimbControl_ReturnState
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return ECk_ProceduralLimbControl_ReturnState::Failed; }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    auto* Controller = Control._Controllers.FindByPredicate([InToken](const auto& InEntry)
    { return InEntry.Get_Token() == InToken; });
    if (Controller == nullptr)
    { return ECk_ProceduralLimbControl_ReturnState::None; }
    if (Controller->_ReturnState != ECk_ProceduralLimbControl_ReturnState::None)
    { return Controller->_ReturnState; }
    Controller->_ReturnState = ECk_ProceduralLimbControl_ReturnState::Returning;
    Controller->_ReturnStarted = Control._ServiceTime;
    Controller->_Reason = InReason;
    if (NOT Controller->_LocomotorIds.IsEmpty())
    {
        const auto Interrupted = Get_InterruptionReason(Subject, InToken);
        if (NOT Interrupted.IsNone())
        {
            FCk_ProceduralGaitParticipation_Control::AbortProtection(
                UCk_Utils_ProceduralGait_UE::CastChecked(Subject), InToken, Interrupted);
        }
    }
    const auto Revision = UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(Subject);
    Controller->_ReturnCaptured = Revision > 0
        && FCk_ProceduralArticulation_Control::CaptureAcceptedEntry(Subject, InToken,
            static_cast<uint64>(Revision), Controller->_ReturnStart) == ECk_ArticulationControlResult::Accepted;
    Controller->_ReturnActual = Controller->_ReturnStart;
    return Controller->_ReturnState;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_ReturnState(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> ECk_ProceduralLimbControl_ReturnState
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return ECk_ProceduralLimbControl_ReturnState::Failed; }
    const auto& Control = InSubject.Get<ck::FFragment_ProceduralLimbControl>();
    const auto* Controller = Control._Controllers.FindByPredicate([InToken](const auto& InEntry)
    { return InEntry.Get_Token() == InToken; });
    return Controller != nullptr ? Controller->Get_ReturnState() : ECk_ProceduralLimbControl_ReturnState::None;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_InterruptionReason(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> FName
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return TEXT("InvalidSubject"); }
    const auto* Controller = InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers()
        .FindByPredicate([InToken](const auto& InEntry) { return InEntry.Get_Token() == InToken; });
    if (Controller == nullptr)
    { return NAME_None; }
    if (NOT ck::procedural_limb_control::IsLive(Controller->Get_Owner()))
    { return TEXT("OwnerLost"); }
    if (UCk_Utils_ProceduralArticulation_UE::Get_Health(InSubject) == ECk_ProceduralArticulation_Health::Faulted)
    { return TEXT("ArticulationFaulted"); }
    if (NOT Controller->Get_LocomotorIds().IsEmpty())
    {
        const auto Observation = FCk_ProceduralGaitParticipation_Control::GetObservation(
            UCk_Utils_ProceduralGait_UE::CastChecked(InSubject), InToken);
        if (Observation.Get_Interrupted())
        { return Observation.Get_Reason(); }
    }
    return NAME_None;
}

auto
    FCk_ProceduralLimbControl_Control::
    TryRecover(
        const FCk_Handle_ProceduralArticulation& InSubject,
        TArrayView<const FName> InLimbIds)
    -> ECk_ProceduralLimbControl_AcquireResult
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return ECk_ProceduralLimbControl_AcquireResult::InvalidSubject; }
    if (InLimbIds.IsEmpty() || InLimbIds.Num() > 64)
    { return ECk_ProceduralLimbControl_AcquireResult::InvalidChannels; }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    // Managed action instance IDs use positive int64. The high bit separates root-owned recovery identities.
    if (Control._NextRecoveryGeneration > static_cast<uint64>(MAX_int64))
    { return ECk_ProceduralLimbControl_AcquireResult::Faulted; }
    const auto Generation = Control._NextRecoveryGeneration++;
    const auto Token = FCk_ArticulationControlToken{(uint64{1} << 63) | Generation, Generation};
    auto Entry = FCk_ProceduralArticulation_Pose{};
    const auto Result = DoTryAcquire(Subject, Token, Subject,
        FCk_ArticulationChannelSet{TArray<FName>{InLimbIds}, false},
        Subject.Get<ck::FFragment_ProceduralLimbControl_Params>().Get_Return(), true, Entry);
    if (Result == ECk_ProceduralLimbControl_AcquireResult::Accepted)
    { BeginReturn(Subject, Token, TEXT("ExplicitRecovery")); }
    return Result;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_ReturnReason(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> FName
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return FName{TEXT("InvalidSubject")}; }
    const auto& Control = InSubject.Get<ck::FFragment_ProceduralLimbControl>();
    const auto* Controller = Control._Controllers.FindByPredicate([InToken](const auto& InEntry)
    { return InEntry.Get_Token() == InToken; });
    return Controller != nullptr ? Controller->Get_Reason() : FName{TEXT("ControllerNotFound")};
}

auto
    FCk_ProceduralLimbControl_Control::
    HasReturnWork(
        const FCk_Handle_ProceduralArticulation& InSubject)
    -> bool
{
    return ck::procedural_limb_control::IsLive(InSubject)
        && InSubject.Has<ck::FFragment_ProceduralLimbControl>()
        && InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers().ContainsByPredicate(
            [](const auto& InController)
            { return InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returning; });
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_ReturnMotionBudget(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        FCk_ProceduralAction_Sampling& OutBudget)
    -> bool
{
    OutBudget = {};
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>()
        || NOT InSubject.Has<ck::FFragment_ProceduralActionPlayer_Params>())
    { return false; }
    const auto* Controller = InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers()
        .FindByPredicate([InToken](const auto& InEntry)
        { return InEntry.Get_Token() == InToken; });
    if (Controller == nullptr || Controller->Get_ReturnState() != ECk_ProceduralLimbControl_ReturnState::Returning)
    { return false; }
    OutBudget = InSubject.Get<ck::FFragment_ProceduralActionPlayer_Params>().Get_Tuning().Get_Sampling();
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_MissedReturnDeadline(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        FCk_Time& OutElapsed,
        FCk_Time& OutDeadline)
    -> bool
{
    OutElapsed = {};
    OutDeadline = {};
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    const auto* Controller = InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers()
        .FindByPredicate([InToken](const auto& InEntry) { return InEntry.Get_Token() == InToken; });
    if (Controller == nullptr || Controller->Get_ReturnState() != ECk_ProceduralLimbControl_ReturnState::Failed
        || Controller->_MissedDeadline <= FCk_Time{})
    { return false; }
    OutElapsed = Controller->_ElapsedAtMissedDeadline;
    OutDeadline = Controller->_MissedDeadline;
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_ReturnActualPose(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        FCk_ProceduralArticulation_Pose& OutPose)
    -> bool
{
    OutPose = {};
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    const auto* Controller = InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers()
        .FindByPredicate([InToken](const auto& InEntry)
        { return InEntry.Get_Token() == InToken; });
    if (Controller == nullptr || Controller->Get_ReturnState() != ECk_ProceduralLimbControl_ReturnState::Returning
        || NOT Controller->_ReturnCaptured)
    { return false; }
    OutPose = Controller->_ReturnActual;
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    RequestReattachment(
        const FCk_Handle_ProceduralArticulation& InSubject)
    -> bool
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    auto Subject = InSubject;
    auto& Controllers = Subject.Get<ck::FFragment_ProceduralLimbControl>()._Controllers;
    if (NOT Controllers.ContainsByPredicate([](const auto& InController)
        { return InController._ReturnState == ECk_ProceduralLimbControl_ReturnState::Returning; }))
    { return false; }
    for (auto& Controller : Controllers)
    {
        if (Controller._ReturnState == ECk_ProceduralLimbControl_ReturnState::Returning)
        { Controller._ReattachmentNeeded = true; }
    }
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    HasReattachmentPending(
        const FCk_Handle_ProceduralArticulation& InSubject)
    -> bool
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    return InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers()
        .ContainsByPredicate([](const auto& InController)
        { return InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returning
            && InController._ReattachmentNeeded; });
}

auto
    FCk_ProceduralLimbControl_Control::
    AppendHeldReturnCandidates(
        const FCk_Handle_ProceduralArticulation& InSubject,
        TArray<FCk_ArticulationTokenCandidate>& InOutCandidates)
    -> bool
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    auto Held = TArray<FCk_ArticulationTokenCandidate>{};
    for (const auto& Controller : InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers())
    {
        if (Controller.Get_ReturnState() != ECk_ProceduralLimbControl_ReturnState::Returning)
        { continue; }
        if (NOT Controller._ReturnCaptured || Controller._AwaitingSettlement || Controller._ReleasePending)
        { return false; }
        auto Targets = TArray<FCk_ArticulationLimbTarget>{};
        Targets.Reserve(Controller.Get_Channels().Get_LimbIds().Num());
        for (const auto Id : Controller.Get_Channels().Get_LimbIds())
        {
            const auto* Limb = Controller._ReturnActual.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
            if (Limb == nullptr) { return false; }
            Targets.Emplace(Id, Limb->Get_AcceptedEndpoint(), TOptional<FVector>{},
                ECk_ArticulationReachPolicy::ProjectToReach);
        }
        auto Body = TOptional<FTransform>{};
        if (Controller.Get_Channels().Get_Body())
        {
            if (NOT Controller._ReturnActual.Get_HasBodyPresentation()) { return false; }
            Body = Controller._ReturnActual.Get_BodyPresentation();
        }
        Held.Emplace(Controller.Get_Token(), 0, MoveTemp(Targets), Body);
    }
    if (Held.IsEmpty()) { return false; }
    InOutCandidates.Append(MoveTemp(Held));
    return true;
}

auto
    FCk_ProceduralLimbControl_Control::
    Service(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_Time InDeltaTime)
    -> void
{
    DoService(InSubject, InDeltaTime, EServicePhase::Physics);
}

auto
    FCk_ProceduralLimbControl_Control::
    UpdateRecovery(
        const FCk_Handle_ProceduralArticulation& InSubject)
    -> void
{
    DoService(InSubject, FCk_Time{}, EServicePhase::Prepare);
}

auto
    FCk_ProceduralLimbControl_Control::
    ObserveRecovery(
        const FCk_Handle_ProceduralArticulation& InSubject)
    -> void
{
    DoService(InSubject, FCk_Time{}, EServicePhase::Observe);
}

auto
    FCk_ProceduralLimbControl_Control::
    DoService(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_Time InDeltaTime,
        EServicePhase InPhase)
    -> void
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>()
        || NOT FMath::IsFinite(InDeltaTime.Get_Seconds()) || InDeltaTime < FCk_Time{})
    { return; }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    if (InPhase == EServicePhase::Physics)
    {
        Control._ServiceTime += InDeltaTime;
        Control._LastDeltaTime = InDeltaTime;
    }
    for (auto& Controller : Control._Controllers)
    {
        if (Controller._ReturnState == ECk_ProceduralLimbControl_ReturnState::None
            && NOT ck::procedural_limb_control::IsLive(Controller._Owner))
        { BeginReturn(Subject, Controller._Token, TEXT("OwnerLost")); }
        if (Controller._ReturnState != ECk_ProceduralLimbControl_ReturnState::Returning)
        { continue; }

        const auto HasLocomotion = NOT Controller._LocomotorIds.IsEmpty();
        const auto Gait = HasLocomotion
            ? UCk_Utils_ProceduralGait_UE::CastChecked(Subject) : FCk_Handle_ProceduralGait{};
        const auto Elapsed = Control._ServiceTime - Controller._ReturnStarted;
        const auto OverallTimeout = HasLocomotion
            ? FMath::Max(Controller._ReturnPolicy.Get_Timeout(), Controller._LocomotionReturnPolicy.Get_Timeout())
            : Controller._ReturnPolicy.Get_Timeout();
        const auto RecordMissedDeadline = [&](FCk_Time InDeadline)
        {
            Controller._MissedDeadline = InDeadline;
            Controller._ElapsedAtMissedDeadline = Elapsed;
        };
        const auto Get_EarliestDeadline = [&]() -> FCk_Time
        {
            auto Deadline = OverallTimeout;
            if (NOT Controller._DedicatedReturned && Controller._ReturnPolicy.Get_Timeout() < Deadline)
            { Deadline = Controller._ReturnPolicy.Get_Timeout(); }
            if (NOT Controller._LocomotionReturned && Controller._LocomotionReturnPolicy.Get_Timeout() < Deadline)
            { Deadline = Controller._LocomotionReturnPolicy.Get_Timeout(); }
            return Deadline;
        };
        if (HasLocomotion)
        {
            const auto Interruption = Get_InterruptionReason(Subject, Controller._Token);
            if (NOT Interruption.IsNone())
            { FCk_ProceduralGaitParticipation_Control::AbortProtection(Gait, Controller._Token, Interruption); }
        }

        const auto TryRelease = [&]()
        {
            auto Handoff = FCk_ProceduralGaitParticipation_HandoffKey{};
            if (HasLocomotion && NOT Controller._ReturnAborted)
            {
                const auto Prepared = FCk_ProceduralGaitParticipation_Control::PrepareTrustedHandoff(
                    Gait, Controller._Token, Handoff);
                if (Prepared != ECk_ProceduralGaitParticipation_Result::Accepted)
                {
                    Controller._LocomotionReturned = false;
                    Controller._ReleasePending = false;
                    Controller._RecoveryNeedsRetry = true;
                    return;
                }
            }
            Controller._ReleasePending = true;
            auto Released = FCk_ProceduralArticulation_Control::ReleaseClaims(
                Subject, Controller._Token, Controller._Channels);
            // A publication already issued this frame must settle before ownership can change.
            // Keep the root-owned record until cleanup actually succeeds; never orphan a busy claim.
            if (Released == ECk_ArticulationControlResult::Busy)
            {
                if (Elapsed < OverallTimeout)
                { return; }
                if (NOT FCk_ProceduralArticulation_Control::FailPendingPublication(Subject, Controller._Token))
                { return; }
                Controller._ReturnAborted = true;
                Controller._Reason = TEXT("ReturnSettlementTimeout");
                RecordMissedDeadline(OverallTimeout);
                Released = FCk_ProceduralArticulation_Control::ReleaseClaims(Subject,
                    Controller._Token, Controller._Channels);
                if (Released == ECk_ArticulationControlResult::Busy)
                { return; }
            }
            if (HasLocomotion)
            {
                if (Released == ECk_ArticulationControlResult::Accepted && NOT Controller._ReturnAborted)
                {
                    if (FCk_ProceduralGaitParticipation_Control::CompleteTrustedHandoff(Gait, Handoff)
                        != ECk_ProceduralGaitParticipation_Result::Accepted)
                    {
                        Controller._ReturnAborted = true;
                        Controller._Reason = TEXT("ReturnHandoffStale");
                    }
                }
                if (Controller._ReturnAborted || Released != ECk_ArticulationControlResult::Accepted)
                { FCk_ProceduralGaitParticipation_Control::FailRecovery(Gait, Controller._Token, Controller._Reason); }
            }
            Controller._ReturnState = Released == ECk_ArticulationControlResult::Accepted
                && NOT Controller._ReturnAborted ? ECk_ProceduralLimbControl_ReturnState::Returned
                    : ECk_ProceduralLimbControl_ReturnState::Failed;
            Controller._AwaitingSettlement = false;
        };
        const auto Fail = [&](FName InReason)
        {
            Controller._ReturnAborted = true;
            Controller._Reason = InReason;
            TryRelease();
        };
        const auto FailOnDeadline = [&](FName InReason)
        {
            RecordMissedDeadline(Get_EarliestDeadline());
            Fail(InReason);
        };
        if (Controller._ReleasePending
            && (NOT HasLocomotion || Controller._ReturnAborted || InPhase != EServicePhase::Physics))
        { TryRelease(); continue; }
        if (UCk_Utils_ProceduralArticulation_UE::Get_Health(Subject) == ECk_ProceduralArticulation_Health::Faulted)
        { Fail(TEXT("ArticulationFaulted")); continue; }

        if (HasLocomotion && InPhase == EServicePhase::Physics)
        {
            if ((NOT Controller._DedicatedReturned && Elapsed >= Controller._ReturnPolicy.Get_Timeout())
                || (NOT Controller._LocomotionReturned && Elapsed >= Controller._LocomotionReturnPolicy.Get_Timeout())
                || Elapsed >= OverallTimeout)
            { FailOnDeadline(Controller._AwaitingSettlement ? TEXT("ReturnSettlementTimeout") : TEXT("ReturnTimeout")); }
            continue;
        }

        if (Controller._AwaitingSettlement)
        {
            if (HasLocomotion && InPhase != EServicePhase::Observe)
            { continue; }
            auto Actual = FCk_ProceduralArticulation_Pose{};
            auto Revision = uint64{0};
            const auto Result = FCk_ProceduralArticulation_Control::ObserveSettlement(
                Subject, Controller._ReturnKey, Revision, Actual);
            if (Result == ECk_ArticulationSettlement::Pending)
            {
                if ((NOT Controller._DedicatedReturned && Elapsed >= Controller._ReturnPolicy.Get_Timeout())
                    || (NOT Controller._LocomotionReturned && Elapsed >= Controller._LocomotionReturnPolicy.Get_Timeout())
                    || Elapsed >= OverallTimeout)
                { FailOnDeadline(TEXT("ReturnSettlementTimeout")); }
                continue;
            }
            Controller._AwaitingSettlement = false;
            if (Result != ECk_ArticulationSettlement::Settled)
            { Fail(TEXT("ReturnSettlementFailed")); continue; }
            if (Controller._PendingReattachment)
            {
                Controller._PendingReattachment = false;
                auto HeldEndpointsSettled = Actual.Get_Limbs().Num() == Controller._ReturnActual.Get_Limbs().Num()
                    && Actual.Get_HasBodyPresentation() == Controller._ReturnActual.Get_HasBodyPresentation();
                for (const auto& Limb : Controller._ReturnActual.Get_Limbs())
                {
                    const auto* Match = Actual.Get_Limbs().FindByPredicate([&Limb](const auto& InLimb)
                    { return InLimb.Get_Id() == Limb.Get_Id(); });
                    HeldEndpointsSettled &= Match != nullptr
                        && FVector::Distance(Limb.Get_AcceptedEndpoint().GetLocation(),
                            Match->Get_AcceptedEndpoint().GetLocation()) <= 0.02
                        && FMath::RadiansToDegrees(Limb.Get_AcceptedEndpoint().GetRotation().AngularDistance(
                            Match->Get_AcceptedEndpoint().GetRotation())) <= 0.1;
                }
                if (Controller._ReturnActual.Get_HasBodyPresentation())
                { HeldEndpointsSettled &= Actual.Get_BodyPresentation().Equals(
                    Controller._ReturnActual.Get_BodyPresentation(), 0.02); }
                if (NOT HeldEndpointsSettled)
                { Fail(TEXT("ReturnReattachmentEndpointChanged")); continue; }
                Controller._ReattachmentNeeded = false;
                Controller._ReattachmentSettledRevision = Revision;
                Controller._ReturnActual = Actual;
                Controller._ReturnStart = Actual;
                Controller._DedicatedReturnOriginTime = Controller._AcceptedReturnTime;
                Controller._DedicatedReturned = Controller._Channels.Get_LimbIds().Num()
                    == Controller._LocomotorIds.Num() && NOT Controller._Channels.Get_Body();
                if (HasLocomotion) { Controller._LocomotionReturned = false; }
                if (HasLocomotion && Controller._RecoveryStarted
                    && FCk_ProceduralGaitParticipation_Control::RebaseRecoveryStart(
                        Gait, Controller._Token, Actual) != ECk_ProceduralGaitParticipation_Result::Accepted)
                { Fail(TEXT("ReturnReattachmentFailed")); }
                continue;
            }
            Controller._AcceptedReturnTime = Controller._PendingReturnTime;
            Controller._ReturnActual = Actual;
            Controller._DedicatedReturned |= Controller._AcceptedReturnTime - Controller._DedicatedReturnOriginTime
                >= Controller._ReturnPolicy.Get_Duration();
            if (HasLocomotion && Controller._PendingRecoveryPrepared)
            {
                const auto Accepted = FCk_ProceduralGaitParticipation_Control::AcceptRecoverySettlement(
                    Gait, Controller._RecoveryCandidate, Actual);
                const auto Observation = FCk_ProceduralGaitParticipation_Control::GetObservation(Gait, Controller._Token);
                // A stale candidate can have settled geometrically without being accepted by the current recovery.
                // Keep the controller clock aligned with the gait's committed trajectory, including terminal misses.
                Controller._AcceptedLocomotionTime = Observation.Get_AcceptedElapsed();
                auto Handoff = FCk_ProceduralGaitParticipation_HandoffKey{};
                Controller._LocomotionReturned = Accepted == ECk_ProceduralGaitParticipation_Result::Accepted
                    && FCk_ProceduralGaitParticipation_Control::PrepareTrustedHandoff(
                        Gait, Controller._Token, Handoff) == ECk_ProceduralGaitParticipation_Result::Accepted;
                Controller._RecoveryNeedsRetry = Accepted != ECk_ProceduralGaitParticipation_Result::Accepted;
            }
            if (Controller._DedicatedReturned && Controller._LocomotionReturned)
            {
                TryRelease();
                continue;
            }
        }
        if (InPhase == EServicePhase::Observe)
        { continue; }
        if ((NOT Controller._DedicatedReturned && Elapsed >= Controller._ReturnPolicy.Get_Timeout())
            || (NOT Controller._LocomotionReturned && Elapsed >= Controller._LocomotionReturnPolicy.Get_Timeout())
            || Elapsed >= OverallTimeout)
        { FailOnDeadline(TEXT("ReturnTimeout")); continue; }
        if (NOT Controller._ReturnCaptured)
        {
            const auto Revision = UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(Subject);
            Controller._ReturnCaptured = Revision > 0
                && FCk_ProceduralArticulation_Control::CaptureAcceptedEntry(Subject, Controller._Token,
                    static_cast<uint64>(Revision), Controller._ReturnStart) == ECk_ArticulationControlResult::Accepted;
            if (Controller._ReturnCaptured)
            { Controller._ReturnActual = Controller._ReturnStart; }
        }
        if (HasLocomotion && Controller._ReturnCaptured && NOT Controller._RecoveryStarted)
        {
            const auto Started = FCk_ProceduralGaitParticipation_Control::BeginRecovery(
                Gait, Controller._Token, Controller._ReturnStart, Controller._LocomotionReturnPolicy);
            if (Started != ECk_ProceduralGaitParticipation_Result::Accepted)
            { Fail(TEXT("ReturnPreparationFailed")); continue; }
            Controller._RecoveryStarted = true;
            Controller._ProbeAttempts = 1;
            Controller._NextProbeTime = Control._ServiceTime + Controller._LocomotionReturnPolicy.Get_RetryInterval();
        }
        if (HasLocomotion && Controller._RecoveryStarted && NOT Controller._LocomotionReturned)
        {
            auto ProbeCandidate = FCk_ProceduralGaitParticipation_RecoveryCandidate{};
            const auto Prepared = FCk_ProceduralGaitParticipation_Control::EvaluateRecovery(Gait, Controller._Token,
                Controller._AcceptedLocomotionTime, Controller._AcceptedLocomotionTime + Control._LastDeltaTime, ProbeCandidate);
            const auto NeedsRetry = Controller._RecoveryNeedsRetry
                || Prepared != ECk_ProceduralGaitParticipation_Result::Accepted;
            if (NeedsRetry && Control._ServiceTime >= Controller._NextProbeTime)
            {
                if (Controller._ProbeAttempts >= Controller._LocomotionReturnPolicy.Get_MaxProbeAttempts())
                { Fail(TEXT("ReturnProbeAttemptsExhausted")); continue; }
                auto Actual = FCk_ProceduralArticulation_Pose{};
                const auto Revision = UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(Subject);
                if (Revision > 0 && FCk_ProceduralArticulation_Control::CaptureAcceptedEntry(Subject,
                    Controller._Token, static_cast<uint64>(Revision), Actual) == ECk_ArticulationControlResult::Accepted)
                {
                    ++Controller._ProbeAttempts;
                    Controller._NextProbeTime = Control._ServiceTime + Controller._LocomotionReturnPolicy.Get_RetryInterval();
                    Controller._RecoveryNeedsRetry = FCk_ProceduralGaitParticipation_Control::RequestLandingRetry(
                        Gait, Controller._Token, Actual) != ECk_ProceduralGaitParticipation_Result::Accepted;
                }
            }
        }
    }
    Control._Controllers.RemoveAll([](const auto& InController)
    {
        return InController._Autonomous && (InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returned
            || InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Failed);
    });
}

auto
    FCk_ProceduralLimbControl_Control::
    AppendReturnCandidates(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ProceduralArticulation_Pose& InDefaultPose,
        TArray<FCk_ArticulationTokenCandidate>& InOutCandidates,
        double InServiceFraction)
    -> void
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>()
        || NOT FMath::IsFinite(InServiceFraction) || InServiceFraction <= 0.0 || InServiceFraction > 1.0)
    { return; }
    if (HasReattachmentPending(InSubject))
    {
        AppendHeldReturnCandidates(InSubject, InOutCandidates);
        return;
    }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    for (auto& Controller : Control._Controllers)
    {
        if (Controller._ReturnState != ECk_ProceduralLimbControl_ReturnState::Returning
            || NOT Controller._ReturnCaptured || Controller._AwaitingSettlement || Controller._ReleasePending)
        { continue; }
        Controller._PendingReturnTime = Controller._AcceptedReturnTime + Control._LastDeltaTime * InServiceFraction;
        const auto Alpha = FMath::Clamp((Controller._PendingReturnTime - Controller._DedicatedReturnOriginTime)
            / Controller._ReturnPolicy.Get_Duration(), 0.0, 1.0);
        auto Targets = TArray<FCk_ArticulationLimbTarget>{};
        auto Complete = true;
        Targets.Reserve(Controller._Channels.Get_LimbIds().Num());
        Controller._PendingRecoveryPrepared = false;
        if (NOT Controller._LocomotorIds.IsEmpty())
        {
            Controller._PendingLocomotionTime = Controller._AcceptedLocomotionTime + Control._LastDeltaTime * InServiceFraction;
            Controller._PendingRecoveryPrepared = Controller._RecoveryStarted
                && FCk_ProceduralGaitParticipation_Control::EvaluateRecovery(
                    UCk_Utils_ProceduralGait_UE::CastChecked(Subject), Controller._Token,
                    Controller._AcceptedLocomotionTime, Controller._PendingLocomotionTime, Controller._RecoveryCandidate)
                        == ECk_ProceduralGaitParticipation_Result::Accepted;
            if (Controller._PendingRecoveryPrepared)
            { Targets.Append(Controller._RecoveryCandidate.Get_Targets()); }
            else
            {
                for (const auto Id : Controller._LocomotorIds)
                {
                    const auto* Held = Controller._ReturnActual.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
                    { return InLimb.Get_Id() == Id; });
                    if (Held == nullptr)
                    { Complete = false; break; }
                    Targets.Emplace(Id, Held->Get_AcceptedEndpoint(), TOptional<FVector>{},
                        ECk_ArticulationReachPolicy::ProjectToReach);
                }
            }
        }
        for (const auto Id : Controller._Channels.Get_LimbIds())
        {
            if (Controller._LocomotorIds.Contains(Id))
            { continue; }
            const auto* Start = Controller._ReturnStart.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
            const auto* Target = InDefaultPose.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
            if (Start == nullptr || Target == nullptr)
            { Complete = false; break; }
            Targets.Emplace(Id, ck::procedural_limb_control::BlendRigid(
                Start->Get_AcceptedEndpoint(), Target->Get_AcceptedEndpoint(), Alpha),
                TOptional<FVector>{}, ECk_ArticulationReachPolicy::ProjectToReach);
        }
        if (NOT Complete)
        { continue; }
        auto Body = TOptional<FTransform>{};
        if (Controller._Channels.Get_Body())
        {
            if (NOT Controller._ReturnStart.Get_HasBodyPresentation() || NOT InDefaultPose.Get_HasBodyPresentation())
            { continue; }
            Body = ck::procedural_limb_control::BlendRigid(Controller._ReturnStart.Get_BodyPresentation(),
                InDefaultPose.Get_BodyPresentation(), Alpha);
        }
        InOutCandidates.Emplace(Controller._Token, uint64{0}, MoveTemp(Targets), Body);
    }
}

auto
    FCk_ProceduralLimbControl_Control::
    Notify_StagedBatch(
        const FCk_Handle_ProceduralArticulation& InSubject,
        TArrayView<const FCk_ArticulationStageKey> InKeys,
        bool InReattachment)
    -> void
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return; }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    for (auto& Controller : Control._Controllers)
    {
        if (Controller._ReturnState != ECk_ProceduralLimbControl_ReturnState::Returning)
        { continue; }
        const auto* Key = InKeys.FindByPredicate([&Controller](const auto& InKey)
        { return InKey.Get_Token() == Controller._Token && InKey.Get_SampleSequence() == 0; });
        if (Key != nullptr)
        {
            Controller._ReturnKey = *Key;
            Controller._AwaitingSettlement = true;
            Controller._PendingReattachment = InReattachment;
        }
    }
}

auto
    FCk_ProceduralLimbControl_Control::
    ForgetTerminal(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> bool
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    { return false; }
    auto Subject = InSubject;
    auto& Control = Subject.Get<ck::FFragment_ProceduralLimbControl>();
    return Control._Controllers.RemoveAll([InToken](const auto& InController)
    {
        return InController.Get_Token() == InToken
            && (InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returned
                || InController.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Failed);
    }) > 0;
}

auto
    FCk_ProceduralLimbControl_Control::
    Get_Observation(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FName InLimbId)
    -> FCk_ProceduralLimbControl_Observation
{
    if (NOT ck::procedural_limb_control::IsLive(InSubject))
    { return {}; }
    const auto Limb = UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, InLimbId);
    if (NOT ck::procedural_limb_control::IsLive(Limb))
    { return {}; }
    auto Layout = FCk_ProceduralLimb_Layout{};
    if (NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout))
    { return FCk_ProceduralLimbControl_Observation{ECk_ProceduralLimbControl_State::Unavailable, {}, 0,
        false, false, false, TEXT("InvalidLayout")}; }
    const auto Locomotor = Layout.Get_Role() == ECk_ProceduralLimb_Role::Locomotor;
    const auto GaitObservation = Locomotor && UCk_Utils_ProceduralGait_UE::Has(InSubject)
        ? FCk_ProceduralGaitParticipation_Control::GetLimbObservation(
            UCk_Utils_ProceduralGait_UE::CastChecked(InSubject), InLimbId)
        : FCk_ProceduralGaitParticipation_LimbObservation{};
    if (Locomotor && (NOT GaitObservation.Get_Exists() || NOT GaitObservation.Get_StructurallyEnabled()))
    { return FCk_ProceduralLimbControl_Observation{ECk_ProceduralLimbControl_State::Unavailable, {}, 0,
        true, false, false, TEXT("LimbUnavailable")}; }
    if (InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    {
        for (const auto& Controller : InSubject.Get<ck::FFragment_ProceduralLimbControl>().Get_Controllers())
        {
            if (NOT Controller.Get_Channels().Get_LimbIds().Contains(InLimbId)
                || Controller.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returned
                || Controller.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Failed)
            { continue; }
            return FCk_ProceduralLimbControl_Observation{
                Controller.Get_ReturnState() == ECk_ProceduralLimbControl_ReturnState::Returning
                    ? ECk_ProceduralLimbControl_State::Returning : ECk_ProceduralLimbControl_State::Controlled,
                Controller.Get_Owner(), static_cast<int64>(Controller.Get_Token().Get_Generation()), Locomotor,
                false, false, Controller.Get_Reason()};
        }
    }
    return FCk_ProceduralLimbControl_Observation{GaitObservation.Get_Unplanted()
        ? ECk_ProceduralLimbControl_State::Unplanted : ECk_ProceduralLimbControl_State::Default, {}, 0,
        Locomotor, GaitObservation.Get_Participating(), GaitObservation.Get_TrustedSupport(), GaitObservation.Get_Reason()};
}

// --------------------------------------------------------------------------------------------------------------------
