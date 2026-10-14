#include "CkProceduralAnimation/Action/CkProceduralAction_Processor.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/TypeTraits/CkTypeTraits.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralAction_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralAction_EndPlay);

namespace ck
{
    auto
    FProcessor_ProceduralAction_HandleRequests::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralAction_Params& InParams,
        FFragment_ProceduralAction& InAction,
        FFragment_ProceduralAction_Requests& InRequests)
        -> void
    {
        auto Pending = MoveTemp(InRequests._Requests);
        InRequests._Requests.Reset();
        const auto PlayerHandle = InParams.Get_Player();
        const auto PlayerValid = ck::IsValid(PlayerHandle)
            && PlayerHandle.Has<FFragment_ProceduralActionPlayer>();
        auto Player = PlayerHandle;
        auto Subject = PlayerValid ? UCk_Utils_ProceduralArticulation_UE::CastChecked(Player)
            : FCk_Handle_ProceduralArticulation{};
        const auto Token = FCk_ArticulationControlToken{
            static_cast<uint64>(InParams.Get_Identity().Get_Value()),
            static_cast<uint64>(InAction._Generation)};
        const auto IsStillLive = [&]() -> bool
        {
            return ck::IsValid(InHandle) && NOT InHandle.Has<FTag_DestroyEntity_Initiate>()
                && ck::IsValid(PlayerHandle) && NOT PlayerHandle.Has<FTag_DestroyEntity_Initiate>()
                && PlayerHandle.Has<FFragment_ProceduralActionPlayer>();
        };

        algo::ForEachRequest(Pending, ck::Visitor(ck::algo::Overload{
        [&](const FCk_Request_ProceduralAction_Cancel& InRequest) -> void
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);
            if (NOT IsStillLive())
            {
                Result = ECk_Request_OperationResult::Failed_Cancelled;
                return;
            }
            if (NOT PlayerValid || InAction._State == ECk_ProceduralAction_State::Finished
                || InAction._State == ECk_ProceduralAction_State::Cancelling
                || InAction._State == ECk_ProceduralAction_State::Returning)
            { return; }
            InAction._PendingStageKey.Reset();
            InAction._PendingHoldStageKey.Reset();
            InAction._Result = FCk_ProceduralAction_Result{
                ECk_ProceduralAction_Outcome::Cancelled, ECk_ProceduralAction_Recovery::Pending,
                InRequest.Get_Reason()};
            if (InAction._HasClaim)
            {
                Player.Get<FFragment_ProceduralActionPlayer>()._Receipts.CloseWindows(
                    InParams.Get_Identity(), InRequest.Get_Reason());
                FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, InRequest.Get_Reason());
                InAction.CommitState(InHandle, InParams, ECk_ProceduralAction_State::Cancelling);
            }
            else
            {
                if (InHandle.Has<FFragment_ProceduralAction_Loading>())
                { InHandle.Remove<FFragment_ProceduralAction_Loading>(); }
                InAction._Result = FCk_ProceduralAction_Result{
                    ECk_ProceduralAction_Outcome::Cancelled, ECk_ProceduralAction_Recovery::NotRequired,
                    InRequest.Get_Reason()};
                Player.Get<FFragment_ProceduralActionPlayer>()._Receipts.Finish(
                    InParams.Get_Identity(), InAction._Result);
                InAction.CommitState(InHandle, InParams, ECk_ProceduralAction_State::Finished);
            }
            Result = ECk_Request_OperationResult::Succeeded;
        },
        [&](const FCk_Request_ProceduralAction_Release& InRequest) -> void
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);
            if (NOT IsStillLive())
            {
                Result = ECk_Request_OperationResult::Failed_Cancelled;
                return;
            }
            if (NOT PlayerValid || InAction._ReleaseWhenFinished) { return; }
            InAction._ReleaseWhenFinished = true;
            InAction._PendingStageKey.Reset();
            InAction._PendingHoldStageKey.Reset();
            if (InAction._State == ECk_ProceduralAction_State::Finished)
            {
                auto Handle = InHandle.ConvertToHandle();
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
            }
            else if (InAction._State != ECk_ProceduralAction_State::Returning
                && InAction._State != ECk_ProceduralAction_State::Cancelling)
            {
                if (InAction._HasClaim)
                {
                    Player.Get<FFragment_ProceduralActionPlayer>()._Receipts.CloseWindows(
                        InParams.Get_Identity(), TEXT("Released"));
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, TEXT("Released"));
                    InAction._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::Cancelled, ECk_ProceduralAction_Recovery::Pending,
                        TEXT("Released")};
                    InAction.CommitState(InHandle, InParams, ECk_ProceduralAction_State::Cancelling);
                }
                else
                {
                    if (InHandle.Has<FFragment_ProceduralAction_Loading>())
                    { InHandle.Remove<FFragment_ProceduralAction_Loading>(); }
                    InAction._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::Cancelled, ECk_ProceduralAction_Recovery::NotRequired,
                        TEXT("Released")};
                    Player.Get<FFragment_ProceduralActionPlayer>()._Receipts.Finish(
                        InParams.Get_Identity(), InAction._Result);
                    InAction.CommitState(InHandle, InParams, ECk_ProceduralAction_State::Finished);
                    auto Handle = InHandle.ConvertToHandle();
                    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
                }
            }
            Result = ECk_Request_OperationResult::Succeeded;
        }}), policy::DontResetContainer{});
        if (InRequests._Requests.IsEmpty()) { InHandle.Remove<MarkedDirtyBy>(); }
    }

    auto
    FProcessor_ProceduralAction_EndPlay::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralAction_Params& InParams,
        const FFragment_ProceduralAction& InAction)
        -> void
    {
        const auto PendingRequests = InHandle.Has<FFragment_ProceduralAction_Requests>()
            ? InHandle.Get<FFragment_ProceduralAction_Requests>().Get_Requests()
            : TArray<FFragment_ProceduralAction_Requests::RequestType>{};
        auto Player = InParams.Get_Player();
        if (ck::IsValid(Player) && Player.Has<FFragment_ProceduralActionPlayer>())
        {
            auto& PlayerFragment = Player.Get<FFragment_ProceduralActionPlayer>();
            const auto Identity = InParams.Get_Identity();
            const auto Token = FCk_ArticulationControlToken{static_cast<uint64>(Identity.Get_Value()),
                static_cast<uint64>(InAction._Generation)};
            if (NOT InAction._OwnerLossReconciled && InAction._State != ECk_ProceduralAction_State::Finished)
            {
                if (InAction._HasClaim)
                {
                    const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(Player);
                    PlayerFragment._Receipts.CloseWindows(Identity, TEXT("OwnerLost"));
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, TEXT("OwnerLost"));
                    PlayerFragment._PendingRecoveries.Add(FProceduralActionPendingRecovery{Identity, Token,
                        ECk_ProceduralAction_Outcome::OwnerLost, TEXT("OwnerLost")});
                }
                else
                {
                    PlayerFragment._Receipts.Finish(Identity, FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::OwnerLost,
                        ECk_ProceduralAction_Recovery::NotRequired, TEXT("OwnerLost")});
                }
            }
            PlayerFragment._Receipts.MarkEntityRemoved(Identity);
        }
        request::FireCancelledForPending(InHandle, PendingRequests);
    }
}
