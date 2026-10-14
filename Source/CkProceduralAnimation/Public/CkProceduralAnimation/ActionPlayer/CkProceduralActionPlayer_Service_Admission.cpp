#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Log.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"

namespace ck_procedural_action_player_service_admission
{
    auto
    AcquireFailureReason(
        ECk_ProceduralLimbControl_AcquireResult InResult)
        -> FName
    {
        switch (InResult)
        {
            case ECk_ProceduralLimbControl_AcquireResult::Busy: return FName{TEXT("Busy")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidSubject: return FName{TEXT("InvalidSubject")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidOwner: return FName{TEXT("InvalidOwner")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidChannels: return FName{TEXT("InvalidChannels")};
            case ECk_ProceduralLimbControl_AcquireResult::UnavailableLimb: return FName{TEXT("UnavailableLimb")};
            case ECk_ProceduralLimbControl_AcquireResult::SupportUnavailable: return FName{TEXT("SupportUnavailable")};
            case ECk_ProceduralLimbControl_AcquireResult::Faulted: return FName{TEXT("Faulted")};
            default: return FName{TEXT("AcquireFailed")};
        }
    }
}

namespace ck
{
    auto
    FTechnique_ProceduralActionPlayer_Service::
    LoadClips(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        auto& Player = *InCtx.PlayerFragment;
        for (auto ActionHandle : InCtx.ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            if (Action._State != ECk_ProceduralAction_State::PendingLoad)
            { continue; }
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            auto Failed = false;
            auto FailureOutcome = ECk_ProceduralAction_Outcome::AssetFailed;
            auto FailureReason = FName{TEXT("AssetLoadFailed")};
            if (NOT ActionHandle.Has<FFragment_ProceduralAction_Loading>())
            { Failed = true; FailureReason = FName{TEXT("MissingLoadingState")}; }
            else
            {
                auto& Loading = ActionHandle.Get<FFragment_ProceduralAction_Loading>();
                if (Loading._Generation != Action._Generation)
                { Failed = true; FailureReason = FName{TEXT("StaleLoadGeneration")}; }
                else
                {
                    Loading._Elapsed += InCtx.DeltaT;
                    if (Loading._Elapsed >= Params.Get_Tuning().Get_Watchdogs().Get_LoadTimeout())
                    {
                        Failed = true;
                        FailureReason = FName{TEXT("LoadTimeout")};
                        Action.BeginFailureContext(ActionHandle, Params)
                            .Set_Deadline(Loading._Elapsed, Params.Get_Tuning().Get_Watchdogs().Get_LoadTimeout());
                    }
                    else
                    {
                        if (NOT Loading._Assets.Get_IsRequested())
                        {
                            Loading._Assets = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
                                FName{TEXT("ProceduralAction.Clip")},
                                TArray<FSoftObjectPath>{Params.Get_Clip().Get_Clip().ToSoftObjectPath()});
                        }
                        if (Loading._Assets.Get_HasFailed())
                        { Failed = true; FailureReason = FName{TEXT("AssetLoadFailed")}; }
                        else if (Loading._Assets.Get_IsReady())
                        {
                            auto* Asset = Cast<UCk_ProceduralAction_Data>(Loading._Assets.Get_ResolvedObject(
                                Params.Get_Clip().Get_Clip().ToSoftObjectPath()));
                            auto Compiled = procedural_action::FCompiledClip{};
                            auto Diagnostics = TArray<procedural_action::FDiagnostic>{};
                            if (Asset == nullptr || NOT procedural_action::Compile(*Asset, Compiled, Diagnostics))
                            {
                                procedural_animation::Display(
                                    TEXT("ProceduralAction clip compile failed: path [{}] class [{}] diagnostics [{}]"),
                                    Params.Get_Clip().Get_Clip().ToSoftObjectPath().ToString(),
                                    Asset == nullptr ? FString{TEXT("<null>")} : Asset->GetClass()->GetPathName(),
                                    Diagnostics.Num());
                                for (auto DiagnosticIndex = 0; DiagnosticIndex < FMath::Min(Diagnostics.Num(), 4);
                                    ++DiagnosticIndex)
                                {
                                    const auto& Diagnostic = Diagnostics[DiagnosticIndex];
                                    procedural_animation::Display(
                                        TEXT("ProceduralAction clip diagnostic: code [{}] field [{}] expected [{}] actual [{}]"),
                                        Diagnostic.Get_Code(), Diagnostic.Get_FieldPath(),
                                        Diagnostic.Get_Expected(), Diagnostic.Get_Actual());
                                }
                                Failed = true;
                                FailureOutcome = ECk_ProceduralAction_Outcome::InvalidContent;
                                FailureReason = FName{TEXT("InvalidClipContent")};
                            }
                            else
                            {
                                auto BoundIds = TArray<FName>{};
                                if (NOT ck_procedural_action_player_service::BindCompiledClip(InCtx.Subject,
                                        Params.Get_Clip(), Compiled, BoundIds)
                                    || Player._Receipts.BindChannels(Params.Get_Identity(), BoundIds,
                                        Compiled.Get_HasBodyChannel())
                                        != procedural_action::EReceiptStoreWrite::Accepted)
                                {
                                    Failed = true;
                                    FailureOutcome = ECk_ProceduralAction_Outcome::Rejected;
                                    FailureReason = FName{TEXT("ClipBindingsRejected")};
                                }
                                else
                                {
                                    TSharedPtr<const procedural_action::FCompiledClip> SharedClip =
                                        MakeShared<procedural_action::FCompiledClip>(MoveTemp(Compiled));
                                    ActionHandle.Add<FFragment_ProceduralAction_Compiled>(
                                        MoveTemp(SharedClip), MoveTemp(BoundIds));
                                    ActionHandle.Remove<FFragment_ProceduralAction_Loading>();
                                    Action.CommitState(ActionHandle, Params,
                                        ECk_ProceduralAction_State::PendingAdmission);
                                }
                            }
                        }
                    }
                }
            }
            if (Failed)
            {
                if (ActionHandle.Has<FFragment_ProceduralAction_Loading>())
                { ActionHandle.Remove<FFragment_ProceduralAction_Loading>(); }
                Action._Result = FCk_ProceduralAction_Result{FailureOutcome,
                    ECk_ProceduralAction_Recovery::NotRequired, FailureReason};
                Player._Receipts.Finish(Params.Get_Identity(), Action._Result, Action._Context);
                Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
            }
        }
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    ServiceActions(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        InCtx.DefaultValid = FCk_ProceduralArticulation_Control::GetDefaultPose(InCtx.Subject, InCtx.DefaultPose)
            == ECk_ArticulationControlResult::Accepted;
        if (InCtx.DefaultValid || InCtx.ReattachmentBarrier)
        { FCk_ProceduralLimbControl_Control::AppendReturnCandidates(InCtx.Subject, InCtx.DefaultPose, InCtx.Candidates); }
        InCtx.ReturnCandidateCount = InCtx.Candidates.Num();
        for (auto Index = 0; Index < InCtx.ReturnCandidateCount; ++Index)
        { InCtx.ReturnTokens.Add(InCtx.Candidates[Index].Get_Token().Get_InstanceId()); }

        for (auto ActionHandle : InCtx.ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            if (InCtx.ReattachmentBarrier && Action._State == ECk_ProceduralAction_State::PendingAdmission)
            { continue; }
            Action._ServiceElapsed += InCtx.DeltaT;
            InCtx.HasUnsettledActionStage |= Action._PendingStageKey.IsSet()
                || Action._PendingHoldStageKey.IsSet();

            const auto Serviced = ActionType{ActionHandle, &Action, &Params, Action.Get_Token(Params)};
            DoInterruptLostClaim(InCtx, Serviced);
            DoAdmit(InCtx, Serviced);
            DoSettleReturn(InCtx, Serviced);
            DoWatchPublication(InCtx, Serviced);
            DoPlanClip(InCtx, Serviced);
            DoHoldClip(InCtx, Serviced);

            if (Action._State == ECk_ProceduralAction_State::Finished && Action._ReleaseWhenFinished)
            {
                auto Handle = ActionHandle.ConvertToHandle();
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
            }
        }
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoInterruptLostClaim(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        if (NOT Action._HasClaim || (Action._State != ECk_ProceduralAction_State::Acquiring
            && Action._State != ECk_ProceduralAction_State::Playing))
        { return; }
        const auto Interruption = FCk_ProceduralLimbControl_Control::Get_InterruptionReason(
            InCtx.Subject, InAction.Token);
        if (Interruption.IsNone())
        { return; }
        InCtx.PlayerFragment->_Receipts.CloseWindows(InAction.Params->Get_Identity(), Interruption);
        FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, InAction.Token, Interruption);
        const auto SupportLost = Interruption == FName{TEXT("SupportLost")}
            || Interruption == FName{TEXT("SupportHoldTimeout")}
            || Interruption == FName{TEXT("SupportEmergencyRequested")};
        Action._Result = FCk_ProceduralAction_Result{
            SupportLost ? ECk_ProceduralAction_Outcome::SupportLost : ECk_ProceduralAction_Outcome::SubjectPoseRejected,
            ECk_ProceduralAction_Recovery::Pending, Interruption};
        Action.CommitState(InAction.Handle, *InAction.Params, ECk_ProceduralAction_State::Cancelling);
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoAdmit(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        const auto& Params = *InAction.Params;
        if (Action._State != ECk_ProceduralAction_State::PendingAdmission
            || NOT InAction.Handle.Has<FFragment_ProceduralAction_Compiled>())
        { return; }
        auto& Receipts = InCtx.PlayerFragment->_Receipts;
        const auto Identity = Params.Get_Identity();
        const auto& Compiled = InAction.Handle.Get<FFragment_ProceduralAction_Compiled>();
        auto Return = FCk_ProceduralLimbControl_Return{};
        Return.Set_Duration(Params.Get_Tuning().Get_Transition().Get_DedicatedReturn());
        Return.Set_Timeout(Params.Get_Tuning().Get_Transition().Get_ReturnTimeout());
        auto Entry = FCk_ProceduralArticulation_Pose{};
        const auto Acquired = FCk_ProceduralLimbControl_Control::TryAcquire(InCtx.Subject, InAction.Token,
            InAction.Handle, FCk_ArticulationChannelSet{Compiled.Get_BoundLimbIds(),
                Compiled.Get_Clip()->Get_HasBodyChannel()}, Return, Entry);
        if (Acquired == ECk_ProceduralLimbControl_AcquireResult::Accepted)
        {
            Action._EntryPose = Entry;
            Action._MostRecentObservedPose = Entry;
            Action._HasClaim = true;
            if (InCtx.DefaultValid && ck_procedural_action_player_service::CaptureClipInitial(InCtx.Subject,
                Compiled, Entry, InCtx.DefaultPose, Action._InitialPose))
            {
                Action._EntryBlendAccepted = FCk_Time{};
                Action.CommitState(InAction.Handle, Params,
                    Params.Get_Tuning().Get_Transition().Get_EntryBlend() > FCk_Time{}
                        ? ECk_ProceduralAction_State::Acquiring
                        : ECk_ProceduralAction_State::Playing);
            }
            else
            {
                const auto Reason = FName{TEXT("EntryCaptureRejected")};
                Action._Result = FCk_ProceduralAction_Result{
                    ECk_ProceduralAction_Outcome::Rejected, ECk_ProceduralAction_Recovery::Pending, Reason};
                Receipts.CloseWindows(Identity, Reason);
                FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, InAction.Token, Reason);
                Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Cancelling);
            }
        }
        else if (Acquired == ECk_ProceduralLimbControl_AcquireResult::NotReady)
        {
            Action._AdmissionElapsed += InCtx.DeltaT;
            const auto Timeout = Params.Get_Tuning().Get_Watchdogs().Get_AcquisitionTimeout();
            if (Action._AdmissionElapsed >= Timeout)
            {
                Action._Result = FCk_ProceduralAction_Result{
                    ECk_ProceduralAction_Outcome::AcquisitionTimeout,
                    ECk_ProceduralAction_Recovery::NotRequired, FName{TEXT("AcquisitionTimeout")}};
                Action.BeginFailureContext(InAction.Handle, Params).Set_Deadline(Action._AdmissionElapsed, Timeout);
                Receipts.Finish(Identity, Action._Result, Action._Context);
                Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Finished);
            }
        }
        else
        {
            Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::Rejected,
                ECk_ProceduralAction_Recovery::NotRequired,
                ck_procedural_action_player_service_admission::AcquireFailureReason(Acquired)};
            Receipts.Finish(Identity, Action._Result);
            Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Finished);
        }
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoSettleReturn(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        const auto& Params = *InAction.Params;
        if (Action._State != ECk_ProceduralAction_State::Returning
            && Action._State != ECk_ProceduralAction_State::Cancelling)
        { return; }
        const auto ReturnState = FCk_ProceduralLimbControl_Control::Get_ReturnState(InCtx.Subject, InAction.Token);
        if (ReturnState != ECk_ProceduralLimbControl_ReturnState::Returned
            && ReturnState != ECk_ProceduralLimbControl_ReturnState::Failed)
        { return; }
        const auto Recovery = ReturnState == ECk_ProceduralLimbControl_ReturnState::Returned
            ? ECk_ProceduralAction_Recovery::Succeeded : ECk_ProceduralAction_Recovery::Failed;
        const auto Outcome = Recovery == ECk_ProceduralAction_Recovery::Failed
            && Action._Result.Get_Outcome() == ECk_ProceduralAction_Outcome::Completed
            ? ECk_ProceduralAction_Outcome::RecoveryFailed : Action._Result.Get_Outcome();
        const auto Reason = Recovery == ECk_ProceduralAction_Recovery::Failed
            ? FCk_ProceduralLimbControl_Control::Get_ReturnReason(InCtx.Subject, InAction.Token)
            : Action._Result.Get_Reason();
        Action._Result = FCk_ProceduralAction_Result{Outcome, Recovery, Reason};
        if (Outcome == ECk_ProceduralAction_Outcome::RecoveryFailed)
        {
            auto& Context = Action.BeginFailureContext(InAction.Handle, Params);
            auto Elapsed = FCk_Time{};
            auto Deadline = FCk_Time{};
            if (FCk_ProceduralLimbControl_Control::Get_MissedReturnDeadline(InCtx.Subject, InAction.Token, Elapsed, Deadline))
            { Context.Set_Deadline(Elapsed, Deadline); }
        }
        InCtx.PlayerFragment->_Receipts.Finish(Params.Get_Identity(), Action._Result, Action._Context);
        Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Finished);
        Action._HasClaim = false;
        FCk_ProceduralLimbControl_Control::ForgetTerminal(InCtx.Subject, InAction.Token);
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoWatchPublication(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        const auto& Params = *InAction.Params;
        if (Action._State != ECk_ProceduralAction_State::Acquiring
            && Action._State != ECk_ProceduralAction_State::Playing)
        { return; }
        Action._PublicationElapsed += InCtx.DeltaT;
        const auto Timeout = Params.Get_Tuning().Get_Watchdogs().Get_PublicationTimeout();
        if (Action._PublicationElapsed < Timeout)
        { return; }
        const auto Reason = FName{TEXT("ClipPublicationTimeout")};
        InCtx.PlayerFragment->_Receipts.CloseWindows(Params.Get_Identity(), Reason);
        FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, InAction.Token, Reason);
        Action._PendingStageKey.Reset();
        Action._Result = FCk_ProceduralAction_Result{
            ECk_ProceduralAction_Outcome::PublicationTimeout, ECk_ProceduralAction_Recovery::Pending, Reason};
        Action.BeginFailureContext(InAction.Handle, Params)
            .Set_Deadline(Action._PublicationElapsed, Timeout)
            .Set_StallReason(Action._LastStall);
        Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Cancelling);
    }
}
