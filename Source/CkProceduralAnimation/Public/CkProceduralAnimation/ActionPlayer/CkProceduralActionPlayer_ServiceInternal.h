#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralRigidTransform.h"

#include "CkCore/Technique/CkTechnique.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_player_service
{
    inline constexpr auto RigidTolerance = ck::procedural_animation::ComposedRotationTolerance;

    struct FClipServicePlan
    {
        FCk_Handle_ProceduralAction Action;
        FCk_Time From;
        FCk_Time Requested;
        bool EntryBlend = false;
        double MaximumFraction = 1.0;
        TArray<double> CriticalFractions;
        TArray<FCk_Time> CriticalTimes;
        FCk_Time MaximumTime;
    };

    // What the clip step search accepted for each plan, in the order of the context's ClipPlans.
    struct FClipStepResult
    {
        double Fraction = 0.0;
        TArray<FCk_Time> StepTimes;
        TArray<TArray<FCk_ProceduralArticulation_Pose>> QualifiedSamples;
        TArray<FCk_ProceduralArticulation_Pose> PredictedByPlan;
        TArray<float> MaxTravels;
        TArray<float> MaxAngularSteps;
    };

    // The pose a candidate's owner last published and the per-step motion it may add to it.
    struct FOtherStepLimit
    {
        FCk_ProceduralArticulation_Pose Previous;
        float MaxTravel = 0.0f;
        float MaxAngular = 0.0f;
    };

    auto
    IsFiniteDirection(
        const FVector& InValue)
        -> bool;

    auto
    ResolveClipFrame(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::procedural_action::FCompiledSlot& InSlot,
        FName InLimbId,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        const FTransform& InBodyWorld,
        FTransform& OutFrame,
        float& OutChainLength,
        FVector& OutHipWorld)
        -> bool;

    auto
    BuildEntryHoldCandidate(
        const TArray<FName>& InLimbIds,
        bool InBodyClaim,
        const FCk_ProceduralArticulation_Pose& InEntry,
        FCk_ArticulationControlToken InToken,
        FCk_ArticulationTokenCandidate& OutCandidate)
        -> bool;

    auto
    BuildClipCandidate(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FFragment_ProceduralAction_Compiled& InCompiled,
        const ck::procedural_action::FEvaluatedPose& InEvaluated,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        FCk_ArticulationControlToken InToken,
        uint64 InSequence,
        FCk_ArticulationTokenCandidate& OutCandidate)
        -> bool;

    auto
    BlendClipEntry(
        const ck::procedural_action::FInitialPose& InInitial,
        const ck::procedural_action::FEvaluatedPose& InTarget,
        double InFraction,
        ck::procedural_action::FEvaluatedPose& OutBlended)
        -> bool;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // Everything one service pass of one player carries from step to step.
    struct FContext_ProceduralActionPlayer_Service
    {
        FCk_Time DeltaT;
        FCk_Handle_ProceduralArticulation Subject;
        FFragment_ProceduralActionPlayer* PlayerFragment = nullptr;
        TArray<FCk_Handle_ProceduralAction> ActionSnapshot;
        bool ReattachmentBarrier = false;
        FCk_ProceduralArticulation_Pose DefaultPose;
        bool DefaultValid = false;
        // Return candidates first (ReturnCandidateCount of them), then the actions' holds.
        TArray<FCk_ArticulationTokenCandidate> Candidates;
        int32 ReturnCandidateCount = 0;
        TSet<uint64> ReturnTokens;
        TArray<ck_procedural_action_player_service::FClipServicePlan> ClipPlans;
        bool HasUnsettledActionStage = false;
        TMap<uint64, ck_procedural_action_player_service::FOtherStepLimit> OtherLimits;
        FCk_ArticulationBatchEvaluation Evaluation;
    };

    // One action of the snapshot, resolved once for the per-action steps of ServiceActions.
    struct FProceduralActionPlayer_ServicedAction
    {
        FCk_Handle_ProceduralAction Handle;
        FFragment_ProceduralAction* Action = nullptr;
        const FFragment_ProceduralAction_Params* Params = nullptr;
        FCk_ArticulationControlToken Token;
    };

    struct FTechnique_ProceduralActionPlayer_Service
        : Technique<FTechnique_ProceduralActionPlayer_Service, FContext_ProceduralActionPlayer_Service&>
    {
        using ThisType = FTechnique_ProceduralActionPlayer_Service;
        using ContextType = FContext_ProceduralActionPlayer_Service;
        using ActionType = FProceduralActionPlayer_ServicedAction;

        FTechnique_ProceduralActionPlayer_Service();

        // CkProceduralActionPlayer_Service.cpp
        static auto DrainRecoveries(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto SnapshotActions(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto SkipIdle(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto CloseForReattachment(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto DispatchKeys(ThisType& InSelf, ContextType& InCtx) -> EStepResult;

        // CkProceduralActionPlayer_Service_Admission.cpp
        static auto LoadClips(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto ServiceActions(ThisType& InSelf, ContextType& InCtx) -> EStepResult;

        // CkProceduralActionPlayer_Service_Planning.cpp
        static auto MeasureOtherLimits(ThisType& InSelf, ContextType& InCtx) -> EStepResult;

        // CkProceduralActionPlayer_Service_Staging.cpp
        static auto StageReturns(ThisType& InSelf, ContextType& InCtx) -> EStepResult;
        static auto StageClips(ThisType& InSelf, ContextType& InCtx) -> EStepResult;

        // ServiceActions runs these on each action in turn, in this order.
        static auto DoInterruptLostClaim(ContextType& InCtx, const ActionType& InAction) -> void;
        static auto DoAdmit(ContextType& InCtx, const ActionType& InAction) -> void;
        static auto DoSettleReturn(ContextType& InCtx, const ActionType& InAction) -> void;
        static auto DoWatchPublication(ContextType& InCtx, const ActionType& InAction) -> void;
        static auto DoPlanClip(ContextType& InCtx, const ActionType& InAction) -> void;
        static auto DoHoldClip(ContextType& InCtx, const ActionType& InAction) -> void;

        // StageClips' outcomes: every planned action rejected, or each one's pending interval written.
        static auto DoRejectClipPlans(ContextType& InCtx) -> void;
        static auto DoPreparePublications(
            ContextType& InCtx,
            const ck_procedural_action_player_service::FClipStepResult& InAccepted) -> void;
        // An interval owns the events after its start up to its end; an action's first one also owns those at zero.
        static auto DoIsInInterval(
            const FFragment_ProceduralAction& InAction,
            FCk_Time InFrom,
            FCk_Time InEventTime,
            FCk_Time InTo) -> bool;

        // Names why this pass published nothing for every action whose publication watchdog is running.
        static auto DoRecordStall(ContextType& InCtx, FName InReason) -> EStepResult;

        static auto DoQualifiesOther(
            const ContextType& InCtx,
            const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
            const FCk_ArticulationBatchEvaluation& InEvaluation,
            int32 InFirst,
            int32 InLast) -> bool;
    };
}

// --------------------------------------------------------------------------------------------------------------------
