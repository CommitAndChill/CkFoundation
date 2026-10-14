#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECk_ProceduralLimbControl_AcquireResult : uint8
{
    Accepted,
    NotReady,
    Busy,
    InvalidSubject,
    InvalidOwner,
    InvalidChannels,
    UnavailableLimb,
    SupportUnavailable,
    Faulted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralLimbControl_AcquireResult);

// --------------------------------------------------------------------------------------------------------------------

// Internal orchestration facade. Claims remain in Articulation; this layer owns withdrawal and finite recovery.
// The root player gathers these return intents into the same complete batch as its active actions.
class CKPROCEDURALANIMATION_API FCk_ProceduralLimbControl_Control
{
public:
    static auto
        Attach(
            FCk_Handle_ProceduralArticulation& InSubject,
            const FCk_ProceduralLimbControl_Locomotion& InLocomotion = {})
        -> bool;

    static auto
        TryAcquire(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            const FCk_Handle& InOwner,
            const FCk_ArticulationChannelSet& InChannels,
            const FCk_ProceduralLimbControl_Return& InDedicatedReturn,
            FCk_ProceduralArticulation_Pose& OutEntry)
        -> ECk_ProceduralLimbControl_AcquireResult;

    // Idempotent. Captures actual settled endpoints and the original service deadline once.
    static auto
        BeginReturn(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            FName InReason)
        -> ECk_ProceduralLimbControl_ReturnState;

    static auto
        Get_ReturnState(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken)
        -> ECk_ProceduralLimbControl_ReturnState;

    static auto
        Get_InterruptionReason(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken)
        -> FName;

    // Explicitly retries only unplanted locomotor limbs. Repeated calls while returning are rejected as busy.
    static auto
        TryRecover(
            const FCk_Handle_ProceduralArticulation& InSubject,
            TArrayView<const FName> InLimbIds)
        -> ECk_ProceduralLimbControl_AcquireResult;

    static auto
        Get_ReturnReason(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken)
        -> FName;

    static auto
        HasReturnWork(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> bool;

    // Read-only pinned geometry limits and the last actually settled return pose. No action entity is required.
    static auto
        Get_ReturnMotionBudget(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            FCk_ProceduralLimbControl_MotionBudget& OutBudget)
        -> bool;

    // The deadline a failed return missed and how long it had run then; false unless a deadline failed the return.
    static auto
        Get_MissedReturnDeadline(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            FCk_Time& OutElapsed,
            FCk_Time& OutDeadline)
        -> bool;

    static auto
        Get_ReturnActualPose(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            FCk_ProceduralArticulation_Pose& OutPose)
        -> bool;

    // Requests one subject-wide, zero-time reattachment after a measured IK discontinuity. Claimed endpoints remain
    // fixed in world space; only an exact Articulation settlement can update the return baseline.
    static auto
        RequestReattachment(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> bool;

    static auto
        HasReattachmentPending(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> bool;

    static auto
        AppendHeldReturnCandidates(
            const FCk_Handle_ProceduralArticulation& InSubject,
            TArray<FCk_ArticulationTokenCandidate>& InOutCandidates)
        -> bool;

    // Appends complete returning-token candidates; never writes transforms or advances an action clock.
    static auto
        AppendReturnCandidates(
            const FCk_Handle_ProceduralArticulation& InSubject,
            const FCk_ProceduralArticulation_Pose& InDefaultPose,
            TArray<FCk_ArticulationTokenCandidate>& InOutCandidates,
            double InServiceFraction = 1.0)
        -> void;

    static auto
        Notify_StagedBatch(
            const FCk_Handle_ProceduralArticulation& InSubject,
            TArrayView<const FCk_ArticulationStageKey> InKeys,
            bool InReattachment = false)
        -> void;

    // Called once by the root's reconciliation processor before SurfaceMotion; never manually pumped by consumers.
    static auto
        Service(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_Time InDeltaTime)
        -> void;

    static auto
        UpdateRecovery(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> void;

    static auto
        ObserveRecovery(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> void;

    // Terminal recovery observations live on the root until the player's result stream has consumed them.
    static auto
        ForgetTerminal(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken)
        -> bool;

    static auto
        Get_Observation(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FName InLimbId)
        -> FCk_ProceduralLimbControl_Observation;

private:
    enum class EServicePhase : uint8
    {
        Physics,
        Prepare,
        Observe
    };

    static auto
        DoService(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_Time InDeltaTime,
            EServicePhase InPhase)
        -> void;

    static auto
        DoTryAcquire(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FCk_ArticulationControlToken InToken,
            const FCk_Handle& InOwner,
            const FCk_ArticulationChannelSet& InChannels,
            const FCk_ProceduralLimbControl_Return& InDedicatedReturn,
            bool InRecoveryOnly,
            FCk_ProceduralArticulation_Pose& OutEntry)
        -> ECk_ProceduralLimbControl_AcquireResult;
};

// --------------------------------------------------------------------------------------------------------------------
