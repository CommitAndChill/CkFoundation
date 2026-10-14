#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control.h"

#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Fragment.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_gait_participation
{
    auto
        Get_IsLiveGait(
            const FCk_Handle_ProceduralGait& InGait)
        -> bool
    {
        return ck::IsValid(InGait) && NOT InGait.Has<ck::FTag_DestroyEntity_Initiate>()
            && InGait.Has<ck::FFragment_ProceduralGait>()
            && InGait.Has<ck::FFragment_Transform>()
            && NOT InGait.Has<ck::FFragment_ProceduralGait_Failure>()
            && NOT InGait.Has<ck::FTag_ProceduralGait_NeedsSetup>();
    }

    auto
        Get_ContactSupport(
            const ck::FProceduralGaitSolver& InSolver,
            TArrayView<const FCk_Handle_ProceduralLeg> InLegs,
            TArrayView<const int32> InExcluded,
            int32 InProspectiveTakeoff,
            const FQuat& InSolverBasis,
            const FTransform& InRoot,
            const FVector& InSupportUp,
            const FCk_ProceduralLimbControl_Support& InPolicy)
        -> bool
    {
        auto Contacts = TArray<ck::procedural_support::FContact, TInlineAllocator<64>>{};
        Contacts.Reserve(InLegs.Num());
        for (auto Index = 0; Index < InLegs.Num(); ++Index)
        {
            if (Index == InProspectiveTakeoff || InExcluded.Contains(Index)
                || NOT ck::procedural_leg::Get_IsStructurallyEnabled(InLegs[Index])
                || NOT InSolver.IsLegParticipating(Index))
            { continue; }
            const auto& State = InSolver.GetLegState(Index);
            if (State.Get_Swing().Get_Active() || NOT State.Get_Plant().Get_Trusted())
            { continue; }
            Contacts.Emplace(InSolverBasis.RotateVector(State.Get_Plant().Get_Position()),
                InSolverBasis.RotateVector(State.Get_Plant().Get_Normal()), true);
        }
        const auto Result = ck::procedural_support::Evaluate(InPolicy, Contacts, InRoot.GetLocation(), InSupportUp);
        return Result.Get_Result() == ck::procedural_support::EResult::Accepted
            || Result.Get_Result() == ck::procedural_support::EResult::Disabled;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_ProceduralGaitParticipation_Control::
    GetSnapshot(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ProceduralGaitParticipation_Snapshot& OutSnapshot)
    -> ECk_ProceduralGaitParticipation_Result
{
    OutSnapshot = {};
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }

    const auto Root = InGait.Get<ck::FFragment_Transform>().Get_Transform();
    if (Root.ContainsNaN() || NOT Root.GetRotation().IsNormalized())
    { return ECk_ProceduralGaitParticipation_Result::NotReady; }
    auto Up = Root.GetRotation().GetAxisZ();
    if (InGait.Has<ck::FFragment_SurfaceMotion_Support>())
    {
        const auto& Surface = InGait.Get<ck::FFragment_SurfaceMotion_Support>();
        if (Surface.Get_EvaluatedFrame() != GFrameCounter || NOT Surface.Get_EvaluatedBody().Equals(Root, 1.0e-3)
            || UCk_Utils_SurfaceMotion_UE::Get_Support(UCk_Utils_SurfaceMotion_UE::Cast(InGait))
                != ECk_SurfaceMotion_Support::Grounded)
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        Up = UCk_Utils_SurfaceMotion_UE::Get_SupportNormal(UCk_Utils_SurfaceMotion_UE::Cast(InGait));
    }
    if (Up.ContainsNaN() || NOT Up.IsNormalized())
    { return ECk_ProceduralGaitParticipation_Result::NotReady; }
    OutSnapshot = FCk_ProceduralGaitParticipation_Snapshot{
        Root, InGait.Get<ck::FFragment_ProceduralGait>().Get_SolveSequence(), Up};
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    TryWithdraw(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        TArrayView<const FName> InLimbIds,
        const FCk_ProceduralLimbControl_Support& InPolicy,
        const FCk_ProceduralGaitParticipation_Snapshot& InExpected,
        FCk_ProceduralGaitParticipation_Observation& OutObservation)
    -> ECk_ProceduralGaitParticipation_Result
{
    OutObservation = {};
    auto Current = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto Snap = GetSnapshot(InGait, Current);
    if (Snap != ECk_ProceduralGaitParticipation_Result::Accepted)
    { return Snap; }
    if (NOT Current.Get_RootFrame().Equals(InExpected.Get_RootFrame(), 1.0e-3)
        || Current.Get_SolveSequence() != InExpected.Get_SolveSequence()
        || NOT Current.Get_SupportUp().Equals(InExpected.Get_SupportUp(), 1.0e-3))
    { return ECk_ProceduralGaitParticipation_Result::Stale; }
    if (InToken.Get_InstanceId() == 0 || InToken.Get_Generation() == 0
        || InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::Disabled
        || NOT ck::procedural_support::ValidatePolicy(InPolicy))
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (InLimbIds.IsEmpty() || InLimbIds.Num() > 64)
    { return ECk_ProceduralGaitParticipation_Result::InvalidLimbSet; }

    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    if (Fragment._ParticipationLeases.ContainsByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; }))
    { return ECk_ProceduralGaitParticipation_Result::Busy; }

    auto Indices = TArray<int32, TInlineAllocator<64>>{};
    for (const auto Id : InLimbIds)
    {
        auto Found = int32{INDEX_NONE};
        for (auto Index = 0; Index < Fragment._Legs.Num(); ++Index)
        {
            const auto& Leg = Fragment._Legs[Index];
            if (ck::procedural_leg::Get_IsLive(Leg) && Leg.Has<ck::FFragment_ProceduralLeg_Params>()
                && Leg.Get<ck::FFragment_ProceduralLeg_Params>().Get_Id() == Id)
            { Found = Index; break; }
        }
        if (Id.IsNone() || Found == INDEX_NONE || Indices.Contains(Found)
            || NOT ck::procedural_leg::Get_IsStructurallyEnabled(Fragment._Legs[Found])
            || NOT Fragment._Solver.IsLegParticipating(Found)
            || (Fragment._UnplantedMask & (uint64{1} << Found)) != 0)
        { return ECk_ProceduralGaitParticipation_Result::InvalidLimbSet; }
        Indices.Add(Found);
    }
    auto Excluded = TArray<int32, TInlineAllocator<64>>{};
    for (const auto& Lease : Fragment._ParticipationLeases)
    {
        for (const auto Index : Lease._LegIndices)
        { Excluded.AddUnique(Index); }
    }
    for (const auto Index : Indices)
    {
        if (Excluded.Contains(Index))
        { return ECk_ProceduralGaitParticipation_Result::Busy; }
        Excluded.Add(Index);
    }
    for (auto Index = 0; Index < Fragment._Legs.Num(); ++Index)
    {
        if ((Fragment._UnplantedMask & (uint64{1} << Index)) != 0)
        { Excluded.AddUnique(Index); }
    }
    for (const auto& Lease : Fragment._ParticipationLeases)
    {
        if (Lease._ProtectionActive && NOT ck_procedural_gait_participation::Get_ContactSupport(
            Fragment._Solver, Fragment._Legs, Excluded, INDEX_NONE, Fragment._Basis, Current.Get_RootFrame(),
            Current.Get_SupportUp(), Lease._SupportPolicy))
        { return ECk_ProceduralGaitParticipation_Result::SupportUnavailable; }
    }
    if (NOT ck_procedural_gait_participation::Get_ContactSupport(Fragment._Solver, Fragment._Legs,
        Excluded, INDEX_NONE, Fragment._Basis, Current.Get_RootFrame(), Current.Get_SupportUp(), InPolicy))
    { return ECk_ProceduralGaitParticipation_Result::SupportUnavailable; }

    auto Lease = ck::FFragment_ProceduralGait::FParticipationLease{};
    Lease._Token = InToken;
    Lease._LegIndices.Append(Indices);
    Lease._LimbIds.Append(InLimbIds.GetData(), InLimbIds.Num());
    Lease._SupportPolicy = InPolicy;
    Fragment._ParticipationLeases.Add(MoveTemp(Lease));
    Fragment._ReachStance = {};
    if (Gait.Has<ck::FFragment_ProceduralGait_FeetPlane>())
    { Gait.Get<ck::FFragment_ProceduralGait_FeetPlane>() = {}; }
    OutObservation = GetObservation(Gait, InToken);
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_ProceduralGaitParticipation_Control::
    TryAcquireRecoveryLease(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        TArrayView<const FName> InLimbIds,
        const FCk_ProceduralLimbControl_Support& InPolicy,
        const FCk_ProceduralGaitParticipation_Snapshot& InExpected,
        FCk_ProceduralGaitParticipation_Observation& OutObservation)
    -> ECk_ProceduralGaitParticipation_Result
{
    OutObservation = {};
    auto Current = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto Snap = GetSnapshot(InGait, Current);
    if (Snap != ECk_ProceduralGaitParticipation_Result::Accepted)
    { return Snap; }
    if (NOT Current.Get_RootFrame().Equals(InExpected.Get_RootFrame(), 1.0e-3)
        || Current.Get_SolveSequence() != InExpected.Get_SolveSequence()
        || NOT Current.Get_SupportUp().Equals(InExpected.Get_SupportUp(), 1.0e-3))
    { return ECk_ProceduralGaitParticipation_Result::Stale; }
    if (InToken.Get_InstanceId() == 0 || InToken.Get_Generation() == 0
        || InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::Disabled
        || NOT ck::procedural_support::ValidatePolicy(InPolicy))
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (InLimbIds.IsEmpty() || InLimbIds.Num() > 64)
    { return ECk_ProceduralGaitParticipation_Result::InvalidLimbSet; }

    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    if (Fragment._ParticipationLeases.ContainsByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; }))
    { return ECk_ProceduralGaitParticipation_Result::Busy; }
    auto Indices = TArray<int32, TInlineAllocator<64>>{};
    for (const auto Id : InLimbIds)
    {
        auto Found = int32{INDEX_NONE};
        for (auto Index = 0; Index < Fragment._Legs.Num(); ++Index)
        {
            const auto& Leg = Fragment._Legs[Index];
            if (ck::procedural_leg::Get_IsLive(Leg) && Leg.Has<ck::FFragment_ProceduralLeg_Params>()
                && Leg.Get<ck::FFragment_ProceduralLeg_Params>().Get_Id() == Id)
            { Found = Index; break; }
        }
        if (Id.IsNone() || Found == INDEX_NONE || Indices.Contains(Found)
            || NOT ck::procedural_leg::Get_IsStructurallyEnabled(Fragment._Legs[Found])
            || (Fragment._UnplantedMask & (uint64{1} << Found)) == 0)
        { return ECk_ProceduralGaitParticipation_Result::InvalidLimbSet; }
        Indices.Add(Found);
    }
    auto Excluded = TArray<int32, TInlineAllocator<64>>{};
    for (const auto& Lease : Fragment._ParticipationLeases)
    {
        for (const auto Index : Lease._LegIndices)
        { Excluded.AddUnique(Index); }
    }
    for (const auto Index : Indices)
    {
        if (Excluded.Contains(Index))
        { return ECk_ProceduralGaitParticipation_Result::Busy; }
        Excluded.Add(Index);
    }
    for (auto Index = 0; Index < Fragment._Legs.Num(); ++Index)
    {
        if ((Fragment._UnplantedMask & (uint64{1} << Index)) != 0)
        { Excluded.AddUnique(Index); }
    }
    for (const auto& Lease : Fragment._ParticipationLeases)
    {
        if (Lease._ProtectionActive && NOT ck_procedural_gait_participation::Get_ContactSupport(
            Fragment._Solver, Fragment._Legs, Excluded, INDEX_NONE, Fragment._Basis, Current.Get_RootFrame(),
            Current.Get_SupportUp(), Lease._SupportPolicy))
        { return ECk_ProceduralGaitParticipation_Result::SupportUnavailable; }
    }
    if (NOT ck_procedural_gait_participation::Get_ContactSupport(Fragment._Solver, Fragment._Legs,
        Excluded, INDEX_NONE, Fragment._Basis, Current.Get_RootFrame(), Current.Get_SupportUp(), InPolicy))
    { return ECk_ProceduralGaitParticipation_Result::SupportUnavailable; }

    auto Lease = ck::FFragment_ProceduralGait::FParticipationLease{};
    Lease._Token = InToken;
    Lease._LegIndices.Append(Indices);
    Lease._LimbIds.Append(InLimbIds.GetData(), InLimbIds.Num());
    Lease._SupportPolicy = InPolicy;
    Lease._RecoveryLease = true;
    Fragment._ParticipationLeases.Add(MoveTemp(Lease));
    Fragment._ReachStance = {};
    if (Gait.Has<ck::FFragment_ProceduralGait_FeetPlane>())
    { Gait.Get<ck::FFragment_ProceduralGait_FeetPlane>() = {}; }
    OutObservation = GetObservation(Gait, InToken);
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    GetObservation(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken)
    -> FCk_ProceduralGaitParticipation_Observation
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return {}; }
    const auto& Gait = InGait.Get<ck::FFragment_ProceduralGait>();
    const auto* Lease = Gait._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr)
    { return {}; }
    const auto LimbUnavailable = Lease->_LegIndices.ContainsByPredicate([&Gait](const int32 InIndex)
    {
        return NOT Gait._Legs.IsValidIndex(InIndex)
            || NOT ck::procedural_leg::Get_IsStructurallyEnabled(Gait._Legs[InIndex]);
    });
    const auto Interrupted = Lease->_Interrupted || LimbUnavailable;
    const auto Reason = LimbUnavailable ? FName{TEXT("LimbUnavailable")} : Lease->_Reason;
    auto Trusted = Lease->_ReadyForHandoff;
    for (const auto Contact : Lease->_ContactTrusted)
    { Trusted &= Contact; }
    Trusted &= NOT Interrupted;
    const auto State = Interrupted ? ECk_ProceduralGaitParticipation_State::Interrupted
        : Lease->_ReadyForHandoff ? ECk_ProceduralGaitParticipation_State::ReadyForHandoff
        : Lease->_Recovering ? ECk_ProceduralGaitParticipation_State::Recovering
        : Lease->_ProtectionActive ? ECk_ProceduralGaitParticipation_State::Protected
        : ECk_ProceduralGaitParticipation_State::Unplanted;
    return FCk_ProceduralGaitParticipation_Observation{State, Lease->_ProtectionActive, Trusted,
        Interrupted, Reason, Lease->_RecoveryGeneration, Gait._SolveSequence, Lease->_AcceptedElapsed};
}

auto
    FCk_ProceduralGaitParticipation_Control::
    GetLimbObservation(
        const FCk_Handle_ProceduralGait& InGait,
        FName InLimbId)
    -> FCk_ProceduralGaitParticipation_LimbObservation
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait) || InLimbId.IsNone())
    { return {}; }
    const auto& Gait = InGait.Get<ck::FFragment_ProceduralGait>();
    for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
    {
        const auto& Leg = Gait._Legs[Index];
        if (NOT ck::procedural_leg::Get_IsLive(Leg) || NOT Leg.Has<ck::FFragment_ProceduralLeg_Params>()
            || Leg.Get<ck::FFragment_ProceduralLeg_Params>().Get_Id() != InLimbId)
        { continue; }
        const auto Structural = ck::procedural_leg::Get_IsStructurallyEnabled(Leg);
        const auto Unplanted = (Gait._UnplantedMask & (uint64{1} << Index)) != 0;
        const auto Leased = Gait._ParticipationLeases.ContainsByPredicate([Index](const auto& InLease)
            { return InLease._LegIndices.Contains(Index); });
        const auto Participating = Structural && NOT Unplanted && NOT Leased
            && Gait._Solver.IsLegParticipating(Index);
        const auto& State = Gait._Solver.GetLegState(Index);
        const auto Trusted = Participating && NOT State.Get_Swing().Get_Active() && State.Get_Plant().Get_Trusted();
        const auto Protected = Gait._ParticipationLeases.ContainsByPredicate([Index](const auto& InLease)
            { return InLease._ProtectionActive && InLease._LegIndices.Contains(Index); });
        const auto Reason = Unplanted && Gait._UnplantedReasons.IsValidIndex(Index)
            ? Gait._UnplantedReasons[Index] : NAME_None;
        return FCk_ProceduralGaitParticipation_LimbObservation{
            true, Structural, Participating, Trusted, Unplanted, Protected, Reason};
    }
    return {};
}

auto
    FCk_ProceduralGaitParticipation_Control::
    CanTakeoff(
        const FCk_Handle_ProceduralGait& InGait,
        const ck::FProceduralGaitSolver& InSolver,
        int32 InLegIndex,
        const FCk_ProceduralGaitParticipation_Snapshot& InSnapshot)
    -> bool
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return false; }
    const auto& Gait = InGait.Get<ck::FFragment_ProceduralGait>();
    if ((InLegIndex < 0 && InLegIndex != INDEX_NONE) || InLegIndex >= Gait._Legs.Num())
    { return false; }
    auto Excluded = TArray<int32, TInlineAllocator<64>>{};
    for (const auto& Lease : Gait._ParticipationLeases)
    {
        for (const auto Index : Lease._LegIndices)
        { Excluded.AddUnique(Index); }
    }
    for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
    {
        if ((Gait._UnplantedMask & (uint64{1} << Index)) != 0)
        { Excluded.AddUnique(Index); }
    }
    for (const auto& Lease : Gait._ParticipationLeases)
    {
        if (Lease._ProtectionActive && NOT ck_procedural_gait_participation::Get_ContactSupport(
            InSolver, Gait._Legs, Excluded, InLegIndex, InSnapshot.Get_RootFrame().GetRotation(),
            InSnapshot.Get_RootFrame(),
            InSnapshot.Get_SupportUp(), Lease._SupportPolicy))
        { return false; }
    }
    return true;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    RequestInterrupt(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FName InReason)
    -> bool
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return false; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr)
    { return false; }
    Lease->_Interrupted = true;
    if (Lease->_Reason.IsNone())
    { Lease->_Reason = InReason; }
    return true;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    AbortProtection(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FName InReason)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    Lease->_ProtectionActive = false;
    Lease->_Interrupted = true;
    if (Lease->_Reason.IsNone())
    { Lease->_Reason = InReason; }
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    BeginRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled,
        const FCk_ProceduralLimbControl_Return& InPinnedPolicy)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    if (InPinnedPolicy.Get_Duration() <= FCk_Time{} || InPinnedPolicy.Get_Timeout() <= FCk_Time{}
        || InPinnedPolicy.Get_MaxProbeAttempts() < 1 || InPinnedPolicy.Get_MaxProbeAttempts() > 16
        || InPinnedPolicy.Get_RetryInterval() < FCk_Time{}
        || NOT FMath::IsFinite(InPinnedPolicy.Get_ContactToleranceCm())
        || InPinnedPolicy.Get_ContactToleranceCm() < 0.0f)
    { return ECk_ProceduralGaitParticipation_Result::InvalidLimbSet; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::Accepted; }
    if (Lease->_Interrupted && Lease->_ProtectionActive)
    { return ECk_ProceduralGaitParticipation_Result::Busy; }

    auto Start = TArray<FTransform>{};
    Start.Reserve(Lease->_LimbIds.Num());
    for (const auto Id : Lease->_LimbIds)
    {
        const auto* Limb = InActualSettled.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
        if (Limb == nullptr || Limb->Get_AcceptedEndpoint().ContainsNaN()
            || NOT Limb->Get_AcceptedEndpoint().GetRotation().IsNormalized())
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        Start.Add(Limb->Get_AcceptedEndpoint());
    }

    Lease->_StartWorld = Start;
    Lease->_AcceptedWorld = MoveTemp(Start);
    Lease->_ReturnPolicy = InPinnedPolicy;
    Lease->_AcceptedElapsed = FCk_Time{};
    Lease->_RecoveryOrigin = FCk_Time{};
    Lease->_Recovering = true;
    Lease->_ReadyForHandoff = false;
    Lease->_LandingWorld.Reset();
    Lease->_LandingNormalWorld.Reset();
    Lease->_LandingProbePending = true;
    Lease->_ContactTrusted.Init(false, Lease->_LimbIds.Num());
    ++Lease->_RecoveryGeneration;
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    RequestLandingRetry(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr || NOT Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    auto Start = TArray<FTransform>{};
    Start.Reserve(Lease->_LimbIds.Num());
    for (const auto Id : Lease->_LimbIds)
    {
        const auto* Limb = InActualSettled.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
        if (Limb == nullptr || Limb->Get_AcceptedEndpoint().ContainsNaN()
            || NOT Limb->Get_AcceptedEndpoint().GetRotation().IsNormalized())
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        Start.Add(Limb->Get_AcceptedEndpoint());
    }
    Lease->_StartWorld = MoveTemp(Start);
    Lease->_RecoveryOrigin = Lease->_AcceptedElapsed;
    Lease->_LandingWorld.Reset();
    Lease->_LandingNormalWorld.Reset();
    Lease->_LandingProbePending = true;
    for (const auto Index : Lease->_LegIndices)
    { Fragment._Footholds[Index] = ck::FProceduralFootholdState{}; }
    Lease->_ContactTrusted.Init(false, Lease->_LimbIds.Num());
    Lease->_ReadyForHandoff = false;
    ++Lease->_RecoveryGeneration;
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    RebaseRecoveryStart(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
    { return InLease._Token == InToken; });
    if (Lease == nullptr || NOT Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    auto Start = TArray<FTransform>{};
    Start.Reserve(Lease->_LimbIds.Num());
    for (const auto Id : Lease->_LimbIds)
    {
        const auto* Limb = InActualSettled.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
        { return InLimb.Get_Id() == Id; });
        if (Limb == nullptr || Limb->Get_AcceptedEndpoint().ContainsNaN()
            || NOT Limb->Get_AcceptedEndpoint().GetRotation().IsNormalized())
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        Start.Add(Limb->Get_AcceptedEndpoint());
    }
    Lease->_StartWorld = Start;
    Lease->_AcceptedWorld = MoveTemp(Start);
    Lease->_RecoveryOrigin = Lease->_AcceptedElapsed;
    Lease->_ReadyForHandoff = false;
    Lease->_ContactTrusted.Init(false, Lease->_LimbIds.Num());
    ++Lease->_RecoveryGeneration;
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    EvaluateRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FCk_Time InAcceptedElapsed,
        FCk_Time InProposedElapsed,
        FCk_ProceduralGaitParticipation_RecoveryCandidate& OutCandidate)
    -> ECk_ProceduralGaitParticipation_Result
{
    OutCandidate = {};
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    const auto& Gait = InGait.Get<ck::FFragment_ProceduralGait>();
    const auto* Lease = Gait._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr || NOT Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (InAcceptedElapsed < Lease->_AcceptedElapsed || InProposedElapsed < InAcceptedElapsed
        || InAcceptedElapsed < Lease->_RecoveryOrigin)
    { return ECk_ProceduralGaitParticipation_Result::Stale; }
    if (Lease->_LandingWorld.Num() != Lease->_LimbIds.Num()
        || Lease->_LandingNormalWorld.Num() != Lease->_LimbIds.Num()
        || Lease->_LatestInputs.Num() != Lease->_LimbIds.Num()
        || Lease->_StartWorld.Num() != Lease->_LimbIds.Num())
    { return ECk_ProceduralGaitParticipation_Result::NotReady; }
    const auto Basis = Gait._Basis;
    const auto InverseBasis = Basis.Inverse();
    const auto Phase = static_cast<float>(FMath::Clamp(
        (InProposedElapsed - Lease->_RecoveryOrigin) / Lease->_ReturnPolicy.Get_Duration(), 0.0, 1.0));
    auto Targets = TArray<FCk_ArticulationLimbTarget>{};
    Targets.Reserve(Lease->_LimbIds.Num());
    for (auto Entry = 0; Entry < Lease->_LimbIds.Num(); ++Entry)
    {
        const auto Landing = InverseBasis.RotateVector(Lease->_LandingWorld[Entry]);
        const auto Normal = InverseBasis.RotateVector(Lease->_LandingNormalWorld[Entry]).GetSafeNormal();
        auto Input = Lease->_LatestInputs[Entry];
        Input.Set_IdealTarget(Landing).Set_GroundNormal(Normal).Set_TargetValid(true)
            .Set_TargetTrusted(true).Set_TargetIsFoothold(true);
        auto Swing = ck::FProceduralGaitLegSwing{};
        Swing.Set_Active(true).Set_Phase(Phase)
            .Set_StartPosition(InverseBasis.RotateVector(Lease->_StartWorld[Entry].GetLocation()))
            .Set_StartRotation((InverseBasis * Lease->_StartWorld[Entry].GetRotation()).GetNormalized())
            .Set_Target(Landing).Set_ValidatedTarget(Landing).Set_LandingPoint(Landing)
            .Set_TargetNormal(Normal).Set_TargetTrusted(true).Set_LandsOnTarget(true);
        auto State = ck::FProceduralGaitLegState{};
        State.Set_Swing(Swing);
        auto Output = ck::FProceduralGaitLegOutput{};
        if (NOT Gait._Solver.SamplePreparedSwingPose(State, Input, Gait._Solver.Get_LastCadenceScale(), Output))
        { return ECk_ProceduralGaitParticipation_Result::Faulted; }
        const auto Endpoint = FTransform{(Basis * Output.Get_Rotation()).GetNormalized(),
            Basis.RotateVector(Output.Get_Position())};
        Targets.Emplace(Lease->_LimbIds[Entry], Endpoint, TOptional<FVector>{},
            ECk_ArticulationReachPolicy::ProjectToReach);
    }
    OutCandidate = FCk_ProceduralGaitParticipation_RecoveryCandidate{
        InToken, Lease->_RecoveryGeneration, Gait._SolveSequence, InProposedElapsed, MoveTemp(Targets)};
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    PrepareTrustedHandoff(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FCk_ProceduralGaitParticipation_HandoffKey& OutKey)
    -> ECk_ProceduralGaitParticipation_Result
{
    OutKey = {};
    auto Snapshot = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto Result = GetSnapshot(InGait, Snapshot);
    if (Result != ECk_ProceduralGaitParticipation_Result::Accepted)
    { return Result; }
    const auto& Gait = InGait.Get<ck::FFragment_ProceduralGait>();
    const auto* Lease = Gait._ParticipationLeases.FindByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Lease == nullptr || NOT Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (NOT Lease->_ReadyForHandoff || Lease->_AcceptedWorld.Num() != Lease->_LegIndices.Num()
        || Lease->_ContactTrusted.Num() != Lease->_LegIndices.Num()
        || Lease->_ContactTrusted.Contains(false))
    { return ECk_ProceduralGaitParticipation_Result::NotReady; }
    OutKey = FCk_ProceduralGaitParticipation_HandoffKey{
        InToken, Lease->_RecoveryGeneration, Gait._SolveSequence, Snapshot.Get_RootFrame()};
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    CompleteTrustedHandoff(
        const FCk_Handle_ProceduralGait& InGait,
        const FCk_ProceduralGaitParticipation_HandoffKey& InKey)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    const auto LeaseIndex = Fragment._ParticipationLeases.IndexOfByPredicate([&InKey](const auto& InLease)
        { return InLease._Token == InKey.Get_Token(); });
    if (LeaseIndex == INDEX_NONE)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    const auto& Lease = Fragment._ParticipationLeases[LeaseIndex];
    auto Snapshot = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto SnapshotResult = GetSnapshot(Gait, Snapshot);
    auto Valid = SnapshotResult == ECk_ProceduralGaitParticipation_Result::Accepted
        && Snapshot.Get_RootFrame().Equals(InKey.Get_RootFrame(), 1.0e-3)
        && Fragment._SolveSequence == InKey.Get_SolveSequence()
        && Lease._RecoveryGeneration == InKey.Get_Generation()
        && Lease._ReadyForHandoff && Lease._AcceptedWorld.Num() == Lease._LegIndices.Num()
        && Lease._LandingNormalWorld.Num() == Lease._LegIndices.Num()
        && Lease._ContactTrusted.Num() == Lease._LegIndices.Num() && NOT Lease._ContactTrusted.Contains(false);
    for (auto Entry = 0; Valid && Entry < Lease._LegIndices.Num(); ++Entry)
    {
        const auto Index = Lease._LegIndices[Entry];
        Valid &= ck::procedural_leg::Get_IsStructurallyEnabled(Fragment._Legs[Index])
            && Fragment._Solver.IsLegEnabled(Index) && NOT Fragment._Solver.IsLegParticipating(Index)
            && NOT Lease._AcceptedWorld[Entry].ContainsNaN()
            && Lease._AcceptedWorld[Entry].GetRotation().IsNormalized()
            && NOT Lease._LandingNormalWorld[Entry].ContainsNaN()
            && Lease._LandingNormalWorld[Entry].IsNormalized();
    }
    if (Valid)
    {
        const auto InverseBasis = Fragment._Basis.Inverse();
        auto Replants = TArray<ck::FProceduralGaitTrustedReplant, TInlineAllocator<64>>{};
        Replants.Reserve(Lease._LegIndices.Num());
        for (auto Entry = 0; Entry < Lease._LegIndices.Num(); ++Entry)
        {
            const auto Index = Lease._LegIndices[Entry];
            const auto& Actual = Lease._AcceptedWorld[Entry];
            Replants.Emplace(Index,
                InverseBasis.RotateVector(Actual.GetLocation()),
                (InverseBasis * Actual.GetRotation()).GetNormalized(),
                InverseBasis.RotateVector(Lease._LandingNormalWorld[Entry]).GetSafeNormal());
        }
        Valid = Fragment._Solver.AdmitTrustedReplants(Replants);
        if (Valid)
        {
            for (const auto Index : Lease._LegIndices)
            {
                Fragment._UnplantedMask &= ~(uint64{1} << Index);
                if (Fragment._UnplantedReasons.IsValidIndex(Index))
                { Fragment._UnplantedReasons[Index] = NAME_None; }
            }
        }
    }
    if (NOT Valid)
    {
        Fragment._UnplantedReasons.SetNum(Fragment._Legs.Num());
        for (const auto Index : Lease._LegIndices)
        {
            Fragment._UnplantedMask |= uint64{1} << Index;
            Fragment._UnplantedReasons[Index] = TEXT("HandoffStale");
        }
    }
    Fragment._ParticipationLeases.RemoveAt(LeaseIndex);
    Fragment._ReachStance = {};
    if (Gait.Has<ck::FFragment_ProceduralGait_FeetPlane>())
    { Gait.Get<ck::FFragment_ProceduralGait_FeetPlane>() = {}; }
    return Valid ? ECk_ProceduralGaitParticipation_Result::Accepted
        : ECk_ProceduralGaitParticipation_Result::Stale;
}

auto
    FCk_ProceduralGaitParticipation_Control::
    FailRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FName InReason)
    -> ECk_ProceduralGaitParticipation_Result
{
    if (NOT ck_procedural_gait_participation::Get_IsLiveGait(InGait))
    { return ECk_ProceduralGaitParticipation_Result::InvalidGait; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    const auto Index = Fragment._ParticipationLeases.IndexOfByPredicate([InToken](const auto& InLease)
        { return InLease._Token == InToken; });
    if (Index == INDEX_NONE)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    Fragment._UnplantedReasons.SetNum(Fragment._Legs.Num());
    for (const auto LegIndex : Fragment._ParticipationLeases[Index].Get_LegIndices())
    {
        Fragment._UnplantedMask |= uint64{1} << LegIndex;
        Fragment._UnplantedReasons[LegIndex] = InReason;
    }
    Fragment._ParticipationLeases.RemoveAt(Index);
    Fragment._ReachStance = {};
    if (Gait.Has<ck::FFragment_ProceduralGait_FeetPlane>())
    { Gait.Get<ck::FFragment_ProceduralGait_FeetPlane>() = {}; }
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

// --------------------------------------------------------------------------------------------------------------------
