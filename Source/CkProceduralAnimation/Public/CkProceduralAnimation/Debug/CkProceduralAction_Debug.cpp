#include "CkProceduralAnimation/Debug/CkProceduralAction_Debug.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_debug
{
    auto
        Get_IsLive(
            const FCk_Handle& InHandle)
        -> bool
    {
        return ck::IsValid(InHandle) && NOT InHandle.Has<ck::FTag_DestroyEntity_Initiate>();
    }

    auto
        Get_AreSettledPartsLive(
            const FCk_Handle& InSubject,
            const FCk_ProceduralArticulation_Pose& InPose)
        -> bool
    {
        if (NOT InSubject.Has<ck::FFragment_ProceduralArticulation_Bindings>())
        { return false; }
        const auto& Bindings = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>();
        if (InPose.Get_HasBodyPresentation()
            && NOT Get_IsLive(Bindings.Get_Presentation().ConvertToHandle()))
        { return false; }
        for (const auto& Limb : InPose.Get_Limbs())
        {
            const auto Index = Bindings.Get_Bindings().IndexOfByPredicate([&Limb](const auto& InBinding)
                { return InBinding.Get_LimbId() == Limb.Get_Id(); });
            if (Index == INDEX_NONE || NOT Bindings.Get_Limbs().IsValidIndex(Index))
            { return false; }
            const auto& BoundLimb = Bindings.Get_Limbs()[Index];
            // A detached legacy leg may keep body-owned segment handles alive until the next solve.
            if (NOT Get_IsLive(BoundLimb.ConvertToHandle())
                || BoundLimb.Has<ck::FTag_ProceduralLeg_Detached>())
            { return false; }
            const auto& Binding = Bindings.Get_Bindings()[Index];
            if (Binding.Get_Segments().Num() != Limb.Get_Segments().Num())
            { return false; }
            for (const auto& Segment : Binding.Get_Segments())
            {
                if (NOT Get_IsLive(Segment.ConvertToHandle()))
                { return false; }
            }
            if (Binding.Get_Terminal() != FCk_Handle_Transform{}
                && NOT Get_IsLive(Binding.Get_Terminal().ConvertToHandle()))
            { return false; }
        }
        return true;
    }

    auto
        MakeDeadline(
            FName InName,
            FCk_Time InElapsed,
            FCk_Time InLimit)
        -> FCk_ProceduralAction_DebugDeadline
    {
        return FCk_ProceduralAction_DebugDeadline{InName, InElapsed, InLimit,
            FMath::Max(FCk_Time{}, InLimit - InElapsed)};
    }

    auto
        Get_SupportResultName(
            ck::procedural_support::EResult InResult)
        -> FName
    {
        using EResult = ck::procedural_support::EResult;
        switch (InResult)
        {
        case EResult::Accepted: return TEXT("Accepted");
        case EResult::Disabled: return TEXT("Disabled");
        case EResult::InvalidPolicy: return TEXT("InvalidPolicy");
        case EResult::InvalidFrame: return TEXT("InvalidFrame");
        case EResult::InvalidContact: return TEXT("InvalidContact");
        case EResult::InsufficientContacts: return TEXT("InsufficientContacts");
        case EResult::IncoherentNormals: return TEXT("IncoherentNormals");
        case EResult::DegenerateRegion: return TEXT("DegenerateRegion");
        case EResult::OutsideRegion: return TEXT("OutsideRegion");
        }
        return NAME_None;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralAnimation_Debug_UE::
    Get_ActionSnapshot(
        const FCk_Handle& InAction)
    -> FCk_ProceduralAction_DebugInstance
{
    const auto OnGameThread = IsInGameThread();
    CK_ENSURE_IF_NOT(OnGameThread, TEXT("Procedural action diagnostics must be copied on the game thread."))
    { return {}; }
    if (NOT OnGameThread || NOT ck_procedural_action_debug::Get_IsLive(InAction)
        || NOT InAction.Has<ck::FFragment_ProceduralAction>()
        || NOT InAction.Has<ck::FFragment_ProceduralAction_Params>())
    { return {}; }

    const auto& Action = InAction.Get<ck::FFragment_ProceduralAction>();
    const auto& Params = InAction.Get<ck::FFragment_ProceduralAction_Params>();
    const auto& Watchdogs = Params.Get_Tuning().Get_Watchdogs();
    auto Snapshot = FCk_ProceduralAction_DebugInstance{};
    Snapshot._Available = true;
    Snapshot._Identity = Params.Get_Identity();
    Snapshot._Generation = Action._Generation;
    Snapshot._State = Action._State;
    Snapshot._Result = Action._Result;
    Snapshot._AcceptedTime = Action._AcceptedTime;
    Snapshot._PlaybackBacklog = Action._PlaybackBacklog;
    Snapshot._NextSampleSequence = Action._NextSampleSequence;
    Snapshot._PublicationPending = Action._PendingStageKey.IsSet() || Action._PendingHoldStageKey.IsSet();
    if (InAction.Has<ck::FFragment_ProceduralAction_Compiled>())
    {
        const auto& Clip = InAction.Get<ck::FFragment_ProceduralAction_Compiled>().Get_Clip();
        if (Clip.IsValid())
        {
            const auto Masked = Clip->Get_ContentHash() & static_cast<uint64>(MAX_int64);
            Snapshot._ContentRevision = static_cast<int64>(Masked == 0 ? 1 : Masked);
        }
    }
    auto& Deadlines = Snapshot._Watchdogs;
    using ck_procedural_action_debug::MakeDeadline;
    if (Action._State == ECk_ProceduralAction_State::PendingLoad
        && InAction.Has<ck::FFragment_ProceduralAction_Loading>())
    {
        Deadlines.Add(MakeDeadline(TEXT("Load"),
            InAction.Get<ck::FFragment_ProceduralAction_Loading>()._Elapsed, Watchdogs.Get_LoadTimeout()));
    }
    if (Action._State == ECk_ProceduralAction_State::PendingAdmission)
    {
        Deadlines.Add(MakeDeadline(TEXT("Acquisition"), Action._AdmissionElapsed,
            Watchdogs.Get_AcquisitionTimeout()));
    }
    if (Action._State == ECk_ProceduralAction_State::Acquiring
        || Action._State == ECk_ProceduralAction_State::Playing)
    {
        Deadlines.Add(MakeDeadline(TEXT("Publication"), Action._PublicationElapsed,
            Watchdogs.Get_PublicationTimeout()));
    }
    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralAnimation_Debug_UE::
    Get_ControlSnapshot(
        const FCk_Handle& InSubject)
    -> FCk_ProceduralAction_DebugSubject
{
    const auto OnGameThread = IsInGameThread();
    CK_ENSURE_IF_NOT(OnGameThread, TEXT("Procedural control diagnostics must be copied on the game thread."))
    { return {}; }
    if (NOT OnGameThread || NOT ck_procedural_action_debug::Get_IsLive(InSubject)
        || NOT InSubject.Has<ck::FFragment_ProceduralArticulation_Pose>()
        || NOT InSubject.Has<ck::FFragment_Transform>())
    { return {}; }

    auto Snapshot = FCk_ProceduralAction_DebugSubject{};
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    Snapshot._Available = true;
    Snapshot._RootFrame = InSubject.Get<ck::FFragment_Transform>().Get_Transform();
    Snapshot._SettledRevision = static_cast<int64>(Pose.Get_SettledRevision());
    Snapshot._PublicationPending = Pose.Get_PendingObservation();
    Snapshot._HasSettledPose = Pose.Get_SettledRevision() > 0
        && InSubject.Has<ck::FFragment_ProceduralArticulation_Health>()
        && InSubject.Get<ck::FFragment_ProceduralArticulation_Health>().Get_State()
            == ECk_ProceduralArticulation_Health::Operational
        && ck_procedural_action_debug::Get_AreSettledPartsLive(InSubject,
            Pose.Get_SettledSubjectPose());
    if (Snapshot._HasSettledPose)
    { Snapshot._SettledPose = Pose.Get_SettledSubjectPose(); }
    if (InSubject.Has<ck::FFragment_ProceduralLimbControl>())
    {
        const auto& Control = InSubject.Get<ck::FFragment_ProceduralLimbControl>();
        Snapshot._Controllers.Reserve(Control._Controllers.Num());
        for (const auto& Controller : Control._Controllers)
        {
            auto Row = FCk_ProceduralAction_DebugController{};
            Row._Identity = static_cast<int64>(Controller._Token.Get_InstanceId());
            Row._Generation = static_cast<int64>(Controller._Token.Get_Generation());
            Row._OwnerEntityId = Controller._Owner.Get_Entity().ToString();
            Row._LimbIds = Controller._Channels.Get_LimbIds();
            Row._Body = Controller._Channels.Get_Body();
            Row._ReturnState = Controller._ReturnState;
            Row._Reason = Controller._Reason;
            Row._AwaitingSettlement = Controller._AwaitingSettlement;
            if (Controller._ReturnState == ECk_ProceduralLimbControl_ReturnState::Returning)
            {
                const auto Elapsed = Control._ServiceTime - Controller._ReturnStarted;
                using ck_procedural_action_debug::MakeDeadline;
                if (NOT Controller._DedicatedReturned)
                {
                    Row._Watchdogs.Add(MakeDeadline(TEXT("DedicatedReturn"), Elapsed,
                        Controller._ReturnPolicy.Get_Timeout()));
                }
                const auto HasLocomotion = NOT Controller._LocomotorIds.IsEmpty();
                if (HasLocomotion && NOT Controller._LocomotionReturned)
                {
                    Row._Watchdogs.Add(MakeDeadline(TEXT("LocomotorReturn"), Elapsed,
                        Controller._LocomotionReturnPolicy.Get_Timeout()));
                }
                const auto Overall = HasLocomotion
                    ? FMath::Max(Controller._ReturnPolicy.Get_Timeout(), Controller._LocomotionReturnPolicy.Get_Timeout())
                    : Controller._ReturnPolicy.Get_Timeout();
                Row._Watchdogs.Add(MakeDeadline(TEXT("OverallReturn"), Elapsed, Overall));
            }
            Snapshot._Controllers.Add(MoveTemp(Row));
        }
    }
    if (NOT UCk_Utils_ProceduralGait_UE::Has(InSubject)) { return Snapshot; }
    const auto GaitHandle = UCk_Utils_ProceduralGait_UE::CastChecked(InSubject);
    auto SupportFrame = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto SupportFresh = FCk_ProceduralGaitParticipation_Control::GetSnapshot(GaitHandle, SupportFrame)
        == ECk_ProceduralGaitParticipation_Result::Accepted;
    Snapshot._SupportFresh = SupportFresh;
    if (SupportFresh) { Snapshot._SupportUp = SupportFrame.Get_SupportUp(); }
    const auto& Gait = InSubject.Get<ck::FFragment_ProceduralGait>();
    auto SupportContacts = TArray<ck::procedural_support::FContact, TInlineAllocator<64>>{};
    Snapshot._Contacts.Reserve(Gait._Legs.Num());
    for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
    {
        const auto& Leg = Gait._Legs[Index];
        if (NOT ck::procedural_leg::Get_IsStructurallyEnabled(Leg) || NOT Gait._Solver.IsLegParticipating(Index))
        { continue; }
        const auto& State = Gait._Solver.GetLegState(Index);
        const auto& Swing = State.Get_Swing();
        const auto Trusted = NOT Swing.Get_Active() && State.Get_Plant().Get_Trusted();
        const auto Reserved = Trusted || (Swing.Get_Active() && Swing.Get_TargetTrusted());
        if (NOT Reserved) { continue; }
        const auto& Params = Leg.Get<ck::FFragment_ProceduralLeg_Params>();
        const auto Position = Gait._Basis.RotateVector(Swing.Get_Active()
            ? Swing.Get_CommittedLandingPoint() : State.Get_Plant().Get_Position());
        const auto Normal = Gait._Basis.RotateVector(Swing.Get_Active()
            ? Swing.Get_TargetNormal() : State.Get_Plant().Get_Normal());
        auto Contact = FCk_ProceduralAction_DebugContact{};
        Contact._LimbId = Params.Get_Id();
        Contact._Position = Position;
        Contact._Normal = Normal;
        Contact._TrustedSupport = Trusted;
        Contact._Reserved = Params.Get_FootContactRadius() > 0.0f;
        Contact._ReservationRadiusCm = Params.Get_FootContactRadius();
        Snapshot._Contacts.Add(MoveTemp(Contact));
        if (Trusted) { SupportContacts.Emplace(Position, Normal, true); }
    }
    Snapshot._Leases.Reserve(Gait._ParticipationLeases.Num());
    for (const auto& Lease : Gait._ParticipationLeases)
    {
        auto Row = FCk_ProceduralAction_DebugLease{};
        Row._Identity = static_cast<int64>(Lease._Token.Get_InstanceId());
        Row._Generation = static_cast<int64>(Lease._Token.Get_Generation());
        Row._Policy = Lease._SupportPolicy;
        Row._ProtectionActive = Lease._ProtectionActive;
        Row._Interrupted = Lease._Interrupted;
        Row._Reason = Lease._Reason;
        if (SupportFresh)
        {
            const auto Support = ck::procedural_support::Evaluate(Lease._SupportPolicy, SupportContacts,
                SupportFrame.Get_RootFrame().GetLocation(), SupportFrame.Get_SupportUp(), &Row._ProjectedRegion);
            Row._SupportResult = ck_procedural_action_debug::Get_SupportResultName(Support.Get_Result());
            Row._TrustedContacts = Support.Get_TrustedContacts();
            Row._MinimumEdgeDistanceCm = Support.Get_MinimumEdgeDistanceCm();
        }
        else { Row._SupportResult = TEXT("StaleSupportFrame"); }
        Row._Landings.Reserve(Lease._LimbIds.Num());
        for (auto Index = 0; Index < Lease._LimbIds.Num(); ++Index)
        {
            if (NOT Lease._LandingWorld.IsValidIndex(Index) || NOT Lease._LandingNormalWorld.IsValidIndex(Index))
            { continue; }
            auto Landing = FCk_ProceduralAction_DebugLanding{};
            Landing._LimbId = Lease._LimbIds[Index];
            Landing._Position = Lease._LandingWorld[Index];
            Landing._Normal = Lease._LandingNormalWorld[Index];
            Landing._ActualContactTrusted = Lease._ContactTrusted.IsValidIndex(Index) && Lease._ContactTrusted[Index];
            Row._Landings.Add(MoveTemp(Landing));
        }
        Snapshot._Leases.Add(MoveTemp(Row));
    }
    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
