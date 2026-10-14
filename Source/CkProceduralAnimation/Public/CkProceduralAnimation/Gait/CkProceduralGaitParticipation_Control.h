#pragma once

#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control_Data.h"

// Internal locomotion lease and recovery protocol; gameplay uses public Utils/observations.
class CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_Control
{
public:
    static auto
    GetSnapshot(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ProceduralGaitParticipation_Snapshot& OutSnapshot)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    TryWithdraw(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        TArrayView<const FName> InLimbIds,
        const FCk_ProceduralLimbControl_Support& InPolicy,
        const FCk_ProceduralGaitParticipation_Snapshot& InExpected,
        FCk_ProceduralGaitParticipation_Observation& OutObservation)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    TryAcquireRecoveryLease(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        TArrayView<const FName> InLimbIds,
        const FCk_ProceduralLimbControl_Support& InPolicy,
        const FCk_ProceduralGaitParticipation_Snapshot& InExpected,
        FCk_ProceduralGaitParticipation_Observation& OutObservation)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    GetObservation(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken)
        -> FCk_ProceduralGaitParticipation_Observation;
    static auto
    GetLimbObservation(
        const FCk_Handle_ProceduralGait& InGait,
        FName InLimbId)
        -> FCk_ProceduralGaitParticipation_LimbObservation;
    // Called by the gait solver's non-owning takeoff callback against its evolving state.
    static auto
    CanTakeoff(
        const FCk_Handle_ProceduralGait& InGait,
        const ck::FProceduralGaitSolver& InSolver,
        int32 InLegIndex,
        const FCk_ProceduralGaitParticipation_Snapshot& InSnapshot)
        -> bool;
    // Stops support protection, but retains the token's lease and recovery identity until a terminal operation.
    static auto
    AbortProtection(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FName InReason)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    BeginRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled,
        const FCk_ProceduralLimbControl_Return& InPinnedPolicy)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    RequestLandingRetry(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
        -> ECk_ProceduralGaitParticipation_Result;
    // Reanchors an existing recovery after a separately published environmental IK reattachment. It preserves the
    // admitted landing, probe attempts and the controller's original deadline; stale recovery candidates are invalidated.
    static auto
    RebaseRecoveryStart(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    EvaluateRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FCk_Time InAcceptedElapsed,
        FCk_Time InProposedElapsed,
        FCk_ProceduralGaitParticipation_RecoveryCandidate& OutCandidate)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    AcceptRecoverySettlement(
        const FCk_Handle_ProceduralGait& InGait,
        const FCk_ProceduralGaitParticipation_RecoveryCandidate& InCandidate,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    PrepareTrustedHandoff(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FCk_ProceduralGaitParticipation_HandoffKey& OutKey)
        -> ECk_ProceduralGaitParticipation_Result;
    // Called only after whole-token Articulation ReleaseClaims has accepted.
    static auto
    CompleteTrustedHandoff(
        const FCk_Handle_ProceduralGait& InGait,
        const FCk_ProceduralGaitParticipation_HandoffKey& InKey)
        -> ECk_ProceduralGaitParticipation_Result;
    static auto
    FailRecovery(
        const FCk_Handle_ProceduralGait& InGait,
        FCk_ArticulationControlToken InToken,
        FName InReason)
        -> ECk_ProceduralGaitParticipation_Result;
};

// --------------------------------------------------------------------------------------------------------------------
