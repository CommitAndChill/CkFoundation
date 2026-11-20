#include "CkHexGrid_Utils.h"

#include "CkHexGrid_Fragment.h"
#include "CkHexGrid_Rules.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/Persistence/CkPersistenceHydration_Processor.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"
#include "Engine/World.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGrid_Projection.h"

#include "CkEcs/Persistence/CkPersistenceInitialization.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridObjectBinding_Utils.h"

auto ck::hex_grid::ReadinessFor(const ck::persistence::FInitializationStatus& InStatus) -> FCk_HexGridReadiness_Result
{
    using EDisposition = ck::persistence::EInitializationDisposition;
    using EReason = ck::persistence::EInitializationFailure;
    using EState = ECk_HexGridReadiness;
    using EFailureCode = ECk_HexGridFailure;
    if (InStatus.Get_Disposition() == EDisposition::Fresh ||
        InStatus.Get_Disposition() == EDisposition::Released)
    { return {EState::Ready, EFailureCode::None, {}}; }
    if (InStatus.Get_Disposition() == EDisposition::Unavailable && InStatus.Get_IsReleased())
    {
        switch (InStatus.Get_Failure())
        {
            case EReason::TimedOut: return {EState::Unavailable, EFailureCode::RestoreTimedOut, {}};
            case EReason::Cancelled: return {EState::Unavailable, EFailureCode::Cancelled, {}};
            case EReason::Rejected: return {EState::Unavailable, EFailureCode::InvalidProjection, {}};
            default: return {EState::Unavailable, EFailureCode::RestoreBaselineMissing, {}};
        }
    }
    return {EState::AwaitingRestore, EFailureCode::NotReady, {}};
}

FCk_HexGridReadiness_Result UCk_Utils_HexGrid_UE::Get_Readiness(const FCk_Handle_HexGrid& InGrid)
{
    if (!Has(InGrid)) { return {}; }
    if (!ck::hex_rules::IsLive(InGrid))
    { return {ECk_HexGridReadiness::Destroying, ECk_HexGridFailure::Destroyed, {}}; }
    const auto& Readiness = InGrid.Get<ck::FFragment_HexGridReadiness>();
    if (Readiness._ObservesRestore)
    {
        // A sibling's release callback may query this board before its own callback has run.
        const auto Status = ck::persistence::Query(InGrid, FCk_RepData_HexGridState::StaticStruct());
        if (Status.Get_Disposition() == ck::persistence::EInitializationDisposition::Unavailable &&
            Status.Get_IsReleased() && Readiness._FailureEpoch == Status.Get_Epoch() && Readiness._FailureEpoch != 0)
        { return Readiness._RestoreFailure; }
        return ck::hex_grid::ReadinessFor(Status);
    }
    return Readiness._Current;
}

void ck::hex_grid::RecordRestoreFailure(FCk_Handle_HexGrid& InBoard, ECk_HexGridFailure InFailure, FGuid InPlacementId)
{
    if (!UCk_Utils_HexGrid_UE::Has(InBoard)) { return; }
    const auto Status = ck::persistence::Query(InBoard, FCk_RepData_HexGridState::StaticStruct());
    if (Status.Get_Epoch() == 0 || Status.Get_IsReleased()) { return; }
    auto& State = InBoard.Get<FFragment_HexGridReadiness>();
    State._FailureEpoch = Status.Get_Epoch();
    State._RestoreFailure = {ECk_HexGridReadiness::Unavailable, InFailure, InPlacementId};
}

bool UCk_Utils_HexGrid_UE::Get_IsReady(const FCk_Handle_HexGrid& InGrid)
{
    return Get_Readiness(InGrid).Get_State() == ECk_HexGridReadiness::Ready;
}

bool UCk_Utils_HexGrid_UE::Get_IsPending(const FCk_Handle_HexGrid& InGrid)
{
    const auto State = Get_Readiness(InGrid).Get_State();
    return State == ECk_HexGridReadiness::AwaitingRestore || State == ECk_HexGridReadiness::AwaitingReplication;
}

FCk_Handle_HexGrid UCk_Utils_HexGrid_UE::BindTo_OnReadinessChanged(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_ReadinessChanged& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (Has(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGrid_ReadinessChanged, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGrid_UE::UnbindFrom_OnReadinessChanged(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_ReadinessChanged& InDelegate)
{
    if (Has(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGrid_ReadinessChanged, InGrid, InDelegate); }
    return InGrid;
}

bool ck::hex_grid::CanReceiveNetBaseline(const FCk_Handle_HexGrid& InBoard)
{
    if (!UCk_Utils_HexGrid_UE::Has(InBoard) ||
        !UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InBoard)) { return false; }
    const auto& State = InBoard.Get<FFragment_HexGridReadiness>();
    if (!State._ObservesReplica || State._NetFeatureRemoved) { return false; }
    const auto Root = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InBoard);
    if (!hex_rules::IsLive(Root) || !Root.Has<TWeakObjectPtr<UWorld>>()) { return false; }
    const auto* World = Root.Get<TWeakObjectPtr<UWorld>>().Get();
    const auto* Subsystem = World ? World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>() : nullptr;
    return Subsystem && !World->bIsTearingDown && Subsystem->Get_LoadHold() != ECk_EcsWorld_LoadHold::Teardown;
}

namespace ck::hex_grid
{
    static auto PublishReadiness(FCk_Handle_HexGrid& InBoard, const FCk_HexGridReadiness_Result& InState,
        bool InFromReplica) -> bool;
}

void ck::hex_grid::StartNetBaselineDeadline(FCk_Handle& InBoard)
{
    InBoard.Add<FFragment_HexGridBaselineDeadline>()._ExpiresAt = FPlatformTime::Seconds() + PendingApplyTimeoutSeconds + 5.0;
}

void ck::hex_grid::OnNetBaselineAccepted(FCk_Handle_HexGrid& InBoard)
{
    if (!CanReceiveNetBaseline(InBoard)) { return; }
    InBoard.Try_Remove<FFragment_HexGridBaselineDeadline>();
    PublishReadiness(InBoard, {ECk_HexGridReadiness::Ready, ECk_HexGridFailure::None, {}}, true);
}

void ck::hex_grid::OnNetBaselineRejected(FCk_Handle_HexGrid& InBoard, ECk_HexGridFailure InFailure, FGuid InPlacementId)
{
    if (!CanReceiveNetBaseline(InBoard)) { return; }
    InBoard.Try_Remove<FFragment_HexGridBaselineDeadline>();
    const FCk_HexGridReadiness_Result Rejected{ECk_HexGridReadiness::Unavailable, InFailure, InPlacementId};
    if (!UCk_Utils_HexGrid_UE::Get_IsReady(InBoard))
    { PublishReadiness(InBoard, Rejected, true); }
    if (CanReceiveNetBaseline(InBoard))
    { UUtils_Signal_HexGrid_ReplicationBaselineRejected::Broadcast(InBoard, ck::MakePayload(InBoard, Rejected)); }
}

void ck::hex_grid::OnNetBaselineRemoved(FCk_Handle_HexGrid& InBoard)
{
    if (!CanReceiveNetBaseline(InBoard)) { return; }
    InBoard.Try_Remove<FFragment_HexGridBaselineDeadline>();
    InBoard.Get<FFragment_HexGridReadiness>()._NetFeatureRemoved = true;
    TArray<FCk_Handle> Retired;
    ClearReplicaProjection(InBoard, Retired);
    PublishReadiness(InBoard, {ECk_HexGridReadiness::Unavailable, ECk_HexGridFailure::Destroyed, {}}, true);
    RetireReplicaProjectionEntities(Retired);
}

FCk_Handle_HexGrid UCk_Utils_HexGrid_UE::BindTo_OnReplicationBaselineRejected(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_ReplicationBaselineRejected& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (Has(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGrid_ReplicationBaselineRejected, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGrid_UE::UnbindFrom_OnReplicationBaselineRejected(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_ReplicationBaselineRejected& InDelegate)
{
    if (Has(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGrid_ReplicationBaselineRejected, InGrid, InDelegate); }
    return InGrid;
}

bool ck::hex_grid::SetReadiness(FCk_Handle_HexGrid& InBoard, const FCk_HexGridReadiness_Result& InState)
{
    return PublishReadiness(InBoard, InState, false);
}

bool ck::hex_grid::PublishReadiness(FCk_Handle_HexGrid& InBoard, const FCk_HexGridReadiness_Result& InState,
    bool InFromReplica)
{
    if (!hex_rules::IsLive(InBoard) || !UCk_Utils_HexGrid_UE::Has(InBoard)) { return false; }
    if (InBoard.Get<FFragment_HexGridReadiness>()._ObservesReplica != InFromReplica) { return false; }
    using State = ECk_HexGridReadiness;
    using Failure = ECk_HexGridFailure;
    auto Normalized = InState;
    switch (InState.Get_State())
    {
        case State::Ready:
            Normalized = {State::Ready, Failure::None, {}};
            break;
        case State::AwaitingRestore:
        case State::AwaitingReplication:
            Normalized = {InState.Get_State(), Failure::NotReady, {}};
            break;
        case State::Unavailable:
            if (InState.Get_Failure() == Failure::None || InState.Get_Failure() == Failure::NotReady) { return false; }
            break;
        default:
            return false;
    }
    auto& Current = InBoard.Get<FFragment_HexGridReadiness>()._Current;
    if (Current.Get_State() == Normalized.Get_State() && Current.Get_Failure() == Normalized.Get_Failure()
        && Current.Get_PlacementId() == Normalized.Get_PlacementId()) { return true; }
    if (Current.Get_State() == State::Unavailable && !InFromReplica)
    { return false; }
    Current = Normalized;
    UUtils_Signal_HexGrid_ReadinessChanged::Broadcast(InBoard, ck::MakePayload(InBoard, Normalized));
    if (Normalized.Get_State() == State::Ready && hex_rules::IsLive(InBoard) && UCk_Utils_HexGrid_UE::Get_IsReady(InBoard))
    {
        ArmBoardReplication(InBoard);
        FlushBindingConflicts(InBoard);
    }
    return true;
}
