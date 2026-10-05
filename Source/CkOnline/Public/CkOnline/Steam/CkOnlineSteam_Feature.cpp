#include "CkOnlineSteam_Feature.h"

#include "CkOnline/Steam/CkOnlineSteam_Processor.h"
#include "CkOnline/CkOnline_Utils.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/Scheduler/CkProcessorGraph.h"
#include "CkEcs/Scheduler/CkProcessorTraits.inl.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

namespace ck::steam
{
    auto FFeature::TryCreate(TSharedRef<ITransport> InTransport) -> TSharedPtr<FFeature>
    {
        if (!IsInGameThread()) { return {}; }
        return MakeShared<FFeature>(MoveTemp(InTransport));
    }

    FFeature::FFeature(TSharedRef<ITransport> InTransport) : _Transport(MoveTemp(InTransport)) {}

    auto FFeature::TryAttach(FCk_Handle_OnlineService InService) -> bool
    {
        if (_Closed || ck::IsValid(_Handle) || !IsInGameThread() ||
            !UCk_Utils_OnlineService_UE::Has(InService) || InService.Has<FFragment_OnlineSteam>()) { return false; }
        auto& Fragment = InService.Add<FFragment_OnlineSteam>();
        Fragment.Feature = AsShared();
        Fragment.Snapshot = _Transport->Get_Snapshot();
        Fragment.Snapshot.Set_ServiceGeneration(
            UCk_Utils_OnlineService_UE::Get_Diagnostics(InService).Get_Generation());
        _Handle = ck::StaticCast<FCk_Handle_OnlineSteam>(InService);
        return true;
    }

    auto FFeature::Get_LocalProcessors() const -> TArray<FProcessorDescriptor>
    {
        return {
            BuildDescriptor<FProcessor_OnlineSteam_HandleRequests>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineSteam_HandleRequests{InRegistry}; }),
            BuildDescriptor<FProcessor_OnlineSteam_Update>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineSteam_Update{InRegistry}; })
        };
    }

    auto FFeature::SetCompletion(const FCk_Request_OnlineSteam& InRequest,
        ECk_OnlineSteam_Result InResult, const FString& InDetail) -> void
    {
        auto Completion = FCk_OnlineSteam_Completion{};
        Completion.Set_OperationId(InRequest.Get_OperationId()).Set_Action(InRequest.Get_Action())
            .Set_Result(InResult).Set_Detail(InDetail);
        _Dispatch.Add({InRequest, MoveTemp(Completion)});
        if (ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        { _Handle.Get<FFragment_OnlineSteam>().AdmittedIds.Remove(InRequest.Get_OperationId()); }
    }

    auto FFeature::CanDispatchWork() const -> bool
    {
        if (_Closed || !IsInGameThread() || ck::Is_NOT_Valid(_Handle) ||
            _Handle.Has<FTag_DestroyEntity_Initiate>()) { return false; }
        const auto Host = _Handle.Get<FFragment_OnlineService>().Get_Host().Pin();
        return Host && Host->Get_IsAcceptingRequests();
    }

    auto FFeature::HandleRequests(double InNow) -> void
    {
        if (!CanDispatchWork()) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineSteam>();
        auto Requests = MoveTemp(Fragment.Requests);
        Fragment.Requests.Reset();
        for (const auto& Request : Requests)
        {
            if (!CanDispatchWork())
            { SetCompletion(Request, ECk_OnlineSteam_Result::Cancelled, TEXT("Profile stopped.")); continue; }
            if (Request.Get_Action() == ECk_OnlineSteam_Action::Cancel)
            {
                auto Ticket = FGuid{};
                for (const auto& Entry : _Pending)
                {
                    if (Entry.Value.Get_OperationId() == Request.Get_TargetOperationId())
                    { Ticket = Entry.Key; break; }
                }
                if (Ticket.IsValid())
                {
                    Complete(Ticket, ECk_OnlineSteam_Result::Cancelled, TEXT("Operation cancelled."));
                    _Transport->Cancel(Ticket);
                    SetCompletion(Request, CanDispatchWork() ? ECk_OnlineSteam_Result::Succeeded : ECk_OnlineSteam_Result::Cancelled,
                        TEXT("Cancellation accepted; provider cleanup may still be pending."));
                }
                else
                { SetCompletion(Request, ECk_OnlineSteam_Result::Rejected, TEXT("Target operation is no longer pending.")); }
                continue;
            }

            const auto Ticket = FGuid::NewGuid();
            const auto Timeout = Request.Get_TimeoutSeconds();
            const auto Deadline = InNow + Timeout;
            const auto ValidDeadline = FMath::IsFinite(InNow) && FMath::IsFinite(Timeout) &&
                Timeout > 0.0 && FMath::IsFinite(Deadline) && Deadline > InNow;
            if (NOT ValidDeadline)
            {
                SetCompletion(Request, ECk_OnlineSteam_Result::Rejected, TEXT("Operation deadline is invalid."));
                continue;
            }
            _Pending.Add(Ticket, Request);
            _Deadlines.Add(Ticket, Deadline);
            _Transport->Execute(Request, Ticket, InNow);
        }
    }

    auto FFeature::Complete(FGuid InTicket, ECk_OnlineSteam_Result InResult, const FString& InDetail) -> void
    {
        auto Request = FCk_Request_OnlineSteam{};
        if (!_Pending.RemoveAndCopyValue(InTicket, Request)) { return; }
        _Deadlines.Remove(InTicket);
        if (ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        { _Handle.Get<FFragment_OnlineSteam>().AdmittedIds.Remove(Request.Get_OperationId()); }
        SetCompletion(Request, InResult, InDetail);
    }

    auto FFeature::Update(double InNow) -> void
    {
        if (!CanDispatchWork()) { return; }
        _Transport->Poll(InNow);
        if (!CanDispatchWork()) { return; }
        auto Expired = TArray<FGuid>{};
        for (const auto& Entry : _Deadlines)
        { if (InNow >= Entry.Value) { Expired.Add(Entry.Key); } }
        for (const auto& Ticket : Expired)
        {
            Complete(Ticket, ECk_OnlineSteam_Result::TimedOut, TEXT("Operation deadline expired."));
            _Transport->Cancel(Ticket);
            if (!CanDispatchWork()) { return; }
        }
        const auto Completions = _Transport->DrainCompletions();
        if (!CanDispatchWork()) { return; }
        for (const auto& Completion : Completions)
        { Complete(Completion.Ticket, Completion.Result, Completion.Detail); }
        CaptureSnapshot();
    }

    auto FFeature::CaptureSnapshot() -> void
    {
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{})) { return; }
        auto Snapshot = _Transport->Get_Snapshot();
        auto& Fragment = _Handle.Get<FFragment_OnlineSteam>();
        Snapshot.Set_ServiceGeneration(Fragment.Snapshot.Get_ServiceGeneration());
        if (Snapshot.Get_Revision() != Fragment.Snapshot.Get_Revision() ||
            Snapshot.Get_State() != Fragment.Snapshot.Get_State() ||
            Snapshot.Get_AccountId() != Fragment.Snapshot.Get_AccountId() ||
            Snapshot.Get_AccountGeneration() != Fragment.Snapshot.Get_AccountGeneration() ||
            Snapshot.Get_LobbyId() != Fragment.Snapshot.Get_LobbyId() ||
            Snapshot.Get_SearchGeneration() != Fragment.Snapshot.Get_SearchGeneration() ||
            Snapshot.Get_LastError() != Fragment.Snapshot.Get_LastError())
        { _Changed = true; }
        Fragment.Snapshot = MoveTemp(Snapshot);
    }

    auto FFeature::Dispatch() -> void
    {
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        { _Dispatch.Reset(); _Changed = false; return; }
        const auto Values = MoveTemp(_Dispatch);
        _Dispatch.Reset();
        const auto Changed = _Changed;
        _Changed = false;
        for (const auto& Value : Values)
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineSteam>();
            Fragment.LastCompletion = Value.Completion;
            if (Fragment.CompletedCount < MAX_int32) { ++Fragment.CompletedCount; }
            const auto Result = Value.Completion.Get_Result() == ECk_OnlineSteam_Result::Succeeded
                ? ECk_Request_OperationResult::Succeeded
                : Value.Completion.Get_Result() == ECk_OnlineSteam_Result::Cancelled
                    ? ECk_Request_OperationResult::Failed_Cancelled : ECk_Request_OperationResult::Failed;
            // Generic completion is claimed after state/identity reconciliation and before CK listeners.
            Value.Request.TryFireCompletion(_Handle, Result);
            UUtils_Signal_OnOnlineSteamCompleted::Broadcast(_Handle, MakePayload(_Handle, Value.Completion));
        }
        if (Changed)
        {
            const auto Snapshot = _Handle.Get<FFragment_OnlineSteam>().Snapshot;
            UUtils_Signal_OnOnlineSteamChanged::Broadcast(_Handle, MakePayload(_Handle, Snapshot));
        }
    }

    auto FFeature::BeginShutdown() -> void
    {
        if (_Closed) { return; }
        _Closed = true;
        if (ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineSteam>();
            auto Requests = MoveTemp(Fragment.Requests);
            Fragment.Requests.Reset();
            for (const auto& Request : Requests)
            { SetCompletion(Request, ECk_OnlineSteam_Result::Cancelled, TEXT("Profile stopped.")); }
        }
        auto Tickets = TArray<FGuid>{};
        _Pending.GetKeys(Tickets);
        for (const auto& Ticket : Tickets)
        { Complete(Ticket, ECk_OnlineSteam_Result::Cancelled, TEXT("Profile stopped.")); }
        _Deadlines.Reset();
        _Transport->Shutdown();
        CaptureSnapshot();
    }
}
