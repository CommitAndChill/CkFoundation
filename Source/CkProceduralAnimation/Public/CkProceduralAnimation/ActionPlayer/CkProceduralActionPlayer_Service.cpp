#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Processor.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_Service);

namespace ck
{
    FTechnique_ProceduralActionPlayer_Service::
        FTechnique_ProceduralActionPlayer_Service()
    {
        AddStep(&ThisType::DrainRecoveries);
        AddStep(&ThisType::SnapshotActions);
        AddStep(&ThisType::LoadClips);
        AddStep(&ThisType::SkipIdle);
        AddStep(&ThisType::CloseForReattachment);
        AddStep(&ThisType::ServiceActions);
        AddStep(&ThisType::MeasureOtherLimits);
        AddStep(&ThisType::StageReturns);
        AddStep(&ThisType::StageClips);
        AddStep(&ThisType::DispatchKeys);
    }

    auto
    FProcessor_ProceduralActionPlayer_Service::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer)
        -> void
    {
        const auto TimeValid = FMath::IsFinite(InDeltaT.Get_Seconds())
            && InDeltaT.Get_Seconds() >= 0.0;
        if (NOT TimeValid)
        { return; }

        static auto ServiceTechnique = FTechnique_ProceduralActionPlayer_Service{};
        auto Context = FContext_ProceduralActionPlayer_Service{};
        Context.DeltaT = InDeltaT;
        Context.Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(InHandle);
        Context.PlayerFragment = &InPlayer;
        ServiceTechnique.ProcessAllSteps(Context);
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DrainRecoveries(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        auto& Player = *InCtx.PlayerFragment;
        Player._Receipts.AdvanceService(InCtx.DeltaT);
        for (auto Index = Player._PendingRecoveries.Num() - 1; Index >= 0; --Index)
        {
            const auto& Pending = Player._PendingRecoveries[Index];
            const auto State = FCk_ProceduralLimbControl_Control::Get_ReturnState(InCtx.Subject, Pending.Get_Token());
            if (State != ECk_ProceduralLimbControl_ReturnState::Returned
                && State != ECk_ProceduralLimbControl_ReturnState::Failed)
            { continue; }
            const auto Recovery = State == ECk_ProceduralLimbControl_ReturnState::Returned
                ? ECk_ProceduralAction_Recovery::Succeeded : ECk_ProceduralAction_Recovery::Failed;
            const auto Outcome = Recovery == ECk_ProceduralAction_Recovery::Failed
                && Pending.Get_Outcome() == ECk_ProceduralAction_Outcome::Completed
                ? ECk_ProceduralAction_Outcome::RecoveryFailed : Pending.Get_Outcome();
            const auto Reason = Recovery == ECk_ProceduralAction_Recovery::Failed
                ? FCk_ProceduralLimbControl_Control::Get_ReturnReason(InCtx.Subject, Pending.Get_Token())
                : Pending.Get_Reason();
            Player._Receipts.Finish(Pending.Get_Identity(), FCk_ProceduralAction_Result{
                Outcome, Recovery, Reason});
            FCk_ProceduralLimbControl_Control::ForgetTerminal(InCtx.Subject, Pending.Get_Token());
            Player._PendingRecoveries.RemoveAt(Index);
        }
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    SnapshotActions(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        auto& Player = *InCtx.PlayerFragment;
        Player._Actions.RemoveAll([](const auto& InAction)
        { return ck::Is_NOT_Valid(InAction) || NOT InAction.template Has<FFragment_ProceduralAction>(); });
        Player._Receipts.AcknowledgeAutoDelivered();
        for (auto ActionHandle : Player._Actions)
        {
            if (ActionHandle.Has<FTag_DestroyEntity_Initiate>()
                || ActionHandle.Get<FFragment_ProceduralAction>()._State != ECk_ProceduralAction_State::Finished
                || NOT Player._Receipts.Get_IsTerminalAcknowledged(
                    ActionHandle.Get<FFragment_ProceduralAction_Params>().Get_Identity()))
            { continue; }
            auto Handle = ActionHandle.ConvertToHandle();
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
        }
        // Identities are appended in enqueue order; snapshot traversal also isolates reentrant creation.
        InCtx.ActionSnapshot = Player._Actions;
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    SkipIdle(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        if (NOT InCtx.PlayerFragment->_PendingRecoveries.IsEmpty()
            || FCk_ProceduralLimbControl_Control::HasReturnWork(InCtx.Subject))
        { return EStepResult::Continue; }
        for (const auto& ActionHandle : InCtx.ActionSnapshot)
        {
            if (ck::IsValid(ActionHandle) && ActionHandle.Has<FFragment_ProceduralAction>()
                && NOT ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            {
                const auto State = ActionHandle.Get<FFragment_ProceduralAction>().Get_State();
                if (State != ECk_ProceduralAction_State::Finished
                    && State != ECk_ProceduralAction_State::PendingLoad)
                { return EStepResult::Continue; }
            }
        }
        return EStepResult::Abort;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    CloseForReattachment(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        InCtx.ReattachmentBarrier = FCk_ProceduralLimbControl_Control::HasReattachmentPending(InCtx.Subject);
        if (NOT InCtx.ReattachmentBarrier)
        { return EStepResult::Continue; }

        const auto OutstandingActionStage = InCtx.ActionSnapshot.ContainsByPredicate([](const auto& InAction)
        {
            return ck::IsValid(InAction) && InAction.Has<FFragment_ProceduralAction>()
                && (InAction.Get<FFragment_ProceduralAction>()._PendingStageKey.IsSet()
                    || InAction.Get<FFragment_ProceduralAction>()._PendingHoldStageKey.IsSet());
        });
        if (OutstandingActionStage)
        { return EStepResult::Continue; }

        auto ClosedActiveWindow = false;
        for (auto ActionHandle : InCtx.ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            if (NOT Action._HasClaim || (Action._State != ECk_ProceduralAction_State::Acquiring
                && Action._State != ECk_ProceduralAction_State::Playing))
            { continue; }
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            const auto Reason = FName{TEXT("ReturnReattachmentDiscontinuity")};
            InCtx.PlayerFragment->_Receipts.CloseWindows(Params.Get_Identity(), Reason);
            FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, Action.Get_Token(Params), Reason);
            Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::SubjectPoseRejected,
                ECk_ProceduralAction_Recovery::Pending, Reason};
            Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
            ClosedActiveWindow = true;
        }
        return ClosedActiveWindow ? EStepResult::Abort : EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DispatchKeys(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        if (InCtx.Evaluation.Get_PredictedTokenPoses().Num() != InCtx.Candidates.Num())
        { return DoRecordStall(InCtx, FName{TEXT("PredictionMismatch")}); }
        auto Keys = TArray<FCk_ArticulationStageKey>{};
        const auto Staged = FCk_ProceduralArticulation_Control::TryStageCandidateBatch(
            InCtx.Subject, MoveTemp(InCtx.Evaluation), Keys);
        if (Staged != ECk_ArticulationControlResult::Accepted)
        { return DoRecordStall(InCtx, FName{TEXT("StageRejected")}); }
        FCk_ProceduralLimbControl_Control::Notify_StagedBatch(InCtx.Subject, Keys, InCtx.ReattachmentBarrier);
        for (const auto& Key : Keys)
        {
            for (auto ActionHandle : InCtx.ActionSnapshot)
            {
                if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                    || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
                { continue; }
                auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
                if (Action.Get_Token(ActionHandle.Get<FFragment_ProceduralAction_Params>()) != Key.Get_Token())
                { continue; }
                if (Key.Get_SampleSequence() == 0)
                {
                    if (Action._State == ECk_ProceduralAction_State::Acquiring
                        || Action._State == ECk_ProceduralAction_State::Playing)
                    { Action._PendingHoldStageKey = Key; }
                    break;
                }
                Action._PendingStageKey = Key;
                ++Action._NextSampleSequence;
                break;
            }
        }
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoRecordStall(
        ContextType& InCtx,
        FName InReason)
        -> EStepResult
    {
        for (auto ActionHandle : InCtx.ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            if (Action._State == ECk_ProceduralAction_State::Acquiring
                || Action._State == ECk_ProceduralAction_State::Playing)
            { Action._LastStall = InReason; }
        }
        return EStepResult::Abort;
    }
}
