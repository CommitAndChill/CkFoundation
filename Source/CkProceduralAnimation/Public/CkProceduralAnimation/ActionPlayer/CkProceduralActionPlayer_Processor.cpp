#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Processor.h"

#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/TypeTraits/CkTypeTraits.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_EndPlay);

namespace ck
{
    auto
    FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer)
        -> void
    {
        const auto Snapshot = InPlayer._Actions;
        auto NeedsObservation = false;
        for (const auto ActionHandle : Snapshot)
        {
            if (ck::IsValid(ActionHandle) && ActionHandle.Has<FTag_DestroyEntity_Initiate>()
                && ActionHandle.Has<FFragment_ProceduralAction>()
                && ActionHandle.Get<FFragment_ProceduralAction>()._PendingStageKey.IsSet())
            { NeedsObservation = true; break; }
        }
        if (NeedsObservation)
        { FProcessor_ProceduralActionPlayer_Observe::ForEachEntity(InDeltaT, InHandle, InPlayer); }
        const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(InHandle);
        for (auto ActionHandle : Snapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FTag_DestroyEntity_Initiate>()
                || NOT ActionHandle.Has<FFragment_ProceduralAction>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            if (Action._OwnerLossReconciled || Action._State == ECk_ProceduralAction_State::Finished)
            { continue; }
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            const auto Identity = Params.Get_Identity();
            const auto Token = FCk_ArticulationControlToken{
                static_cast<uint64>(Identity.Get_Value()), static_cast<uint64>(Action._Generation)};
            Action._OwnerLossReconciled = true;
            Action._Context = {};
            Action._PendingStageKey.Reset();
            Action._PendingHoldStageKey.Reset();
            if (Action._HasClaim)
            {
                InPlayer._Receipts.CloseWindows(Identity, FName{TEXT("OwnerLost")});
                FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                    FName{TEXT("OwnerLost")});
                InPlayer._PendingRecoveries.Add(FProceduralActionPendingRecovery{Identity, Token,
                    ECk_ProceduralAction_Outcome::OwnerLost, FName{TEXT("OwnerLost")}});
                Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::OwnerLost,
                    ECk_ProceduralAction_Recovery::Pending, FName{TEXT("OwnerLost")}};
                Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
            }
            else
            {
                Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::OwnerLost,
                    ECk_ProceduralAction_Recovery::NotRequired, FName{TEXT("OwnerLost")}};
                InPlayer._Receipts.Finish(Identity, Action._Result);
                Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
            }
        }
    }

    auto
    FProcessor_ProceduralActionPlayer_HandleRequests::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer,
        FFragment_ProceduralActionPlayer_Requests& InRequests)
        -> void
    {
        auto Pending = MoveTemp(InRequests._Requests);
        InRequests._Requests.Reset();
        algo::ForEachRequest(Pending, ck::Visitor(ck::algo::Overload{
        [&](const FCk_Request_ProceduralActionPlayer_ConsumeReceipts& InRequest) -> void
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);
            if (InPlayer._Receipts.Consume(InRequest.Get_ActionIdentity(),
                InRequest.Get_ThroughStreamSequence()) == ECk_ProceduralAction_ReadResult::Succeeded)
            { Result = ECk_Request_OperationResult::Succeeded; }
        }}), policy::DontResetContainer{});
        if (InRequests._Requests.IsEmpty()) { InHandle.Remove<MarkedDirtyBy>(); }
    }

    auto
    FProcessor_ProceduralActionPlayer_EndPlay::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralActionPlayer& InPlayer)
        -> void
    {
        for (const auto& Action : InPlayer._Actions)
        {
            if (ck::IsValid(Action) && NOT Action.Has<FTag_DestroyEntity_Initiate>()
                && Action.Has<FFragment_ProceduralAction_Params>()
                && InPlayer._Receipts.Get_IsAutoDelivered(Action.Get<FFragment_ProceduralAction_Params>().Get_Identity()))
            {
                auto Handle = Action.ConvertToHandle();
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
            }
        }
    }

    auto
    FProcessor_ProceduralActionPlayer_CancelPendingRequests::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralActionPlayer_Requests& InRequests)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequests._Requests);
    }
}
