#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"

// Internal publisher/claim bridge. Game callers use the reflected Utils and observation values.
class CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_Control
{
public:
    static auto
    TryReserveClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_ArticulationChannelSet& InChannels)
        -> ECk_ArticulationControlResult;

    static auto
    CaptureAcceptedEntry(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        uint64 InExpectedSettledRevision,
        FCk_ProceduralArticulation_Pose& OutPose)
        -> ECk_ArticulationControlResult;

    // Re-evaluates current gait/rest targets and environmental BodyPose without an action override.
    static auto
    GetDefaultPose(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ProceduralArticulation_Pose& OutPose)
        -> ECk_ArticulationControlResult;

    static auto
    TryEvaluateCandidateBatch(
        const FCk_Handle_ProceduralArticulation& InSubject,
        TArrayView<const FCk_ArticulationTokenCandidate> InCandidates,
        FCk_ArticulationBatchEvaluation& OutEvaluation)
        -> ECk_ArticulationControlResult;

    static auto
    TryStageCandidateBatch(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationBatchEvaluation&& InEvaluation,
        TArray<FCk_ArticulationStageKey>& OutKeys)
        -> ECk_ArticulationControlResult;

    static auto
    ObserveSettlement(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ArticulationStageKey& InKey,
        uint64& OutSubjectRevision,
        FCk_ProceduralArticulation_Pose& OutActualPose)
        -> ECk_ArticulationSettlement;

    static auto
    TryTransferClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InFrom,
        FCk_ArticulationControlToken InTo,
        const FCk_ArticulationChannelSet& InChannels)
        -> ECk_ArticulationControlResult;

    static auto
    ReleaseClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_ArticulationChannelSet& InChannels)
        -> ECk_ArticulationControlResult;

    // Releases every claim whose limb is no longer live and records the owning tokens as lost.
    // Runs at the top of FProcessor_ProceduralArticulation_Update::ForEachEntity, before any hold.
    static auto
    ReleaseDeadClaims(
        FCk_Handle_ProceduralArticulation& InSubject)
        -> void;

    static auto
    FailPendingPublication(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
        -> bool;

    static auto
    GetClaimant(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FName InLimbId,
        bool InBody)
        -> FCk_ArticulationControlToken;

    static auto
    GetControlState(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
        -> FCk_ArticulationControlState;
};
