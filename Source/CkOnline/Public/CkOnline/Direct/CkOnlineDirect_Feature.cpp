#include "CkOnlineDirect_Feature.h"

#include "CkOnline/Direct/CkOnlineDirect_Processor.h"
#include "CkOnline/CkOnline_Utils.h"
#include "CkEcs/Scheduler/CkProcessorGraph.h"
#include "CkEcs/Scheduler/CkProcessorTraits.inl.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

namespace ck::direct
{
    auto FFeature::TryCreate(const FCk_OnlineDirect_Spec& InSpec) -> TSharedPtr<FFeature>
    {
        if (!IsInGameThread() || !FTransport::ValidateSpec(InSpec)) { return {}; }
        return MakeShared<FFeature>(InSpec);
    }

    FFeature::FFeature(const FCk_OnlineDirect_Spec& InSpec) : _Transport(InSpec) {}
    FFeature::~FFeature() { _Transport.Close(); }

    auto FFeature::TryAttach(FCk_Handle_OnlineService InService) -> bool
    {
        if (_Closed || ck::IsValid(_Handle) || !IsInGameThread() ||
            !UCk_Utils_OnlineService_UE::Has(InService) || InService.Has<FFragment_OnlineDirect>()) { return false; }
        auto& Fragment = InService.Add<FFragment_OnlineDirect>();
        Fragment.Feature = AsShared();
        Fragment.Snapshot.Set_ServiceGeneration(
            UCk_Utils_OnlineService_UE::Get_Diagnostics(InService).Get_Generation());
        _Handle = ck::StaticCast<FCk_Handle_OnlineDirect>(InService);
        return true;
    }

    auto FFeature::Get_LocalProcessors() const -> TArray<FProcessorDescriptor>
    {
        return {
            BuildDescriptor<FProcessor_OnlineDirect_HandleRequests>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineDirect_HandleRequests{InRegistry}; }),
            BuildDescriptor<FProcessor_OnlineDirect_Update>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineDirect_Update{InRegistry}; })
        };
    }

    auto FFeature::HandleRequests(double InNow) -> void
    {
        if (_Closed || ck::Is_NOT_Valid(_Handle)) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineDirect>();
        auto Requests = MoveTemp(Fragment.Requests);
        Fragment.Requests.Reset();
        for (const auto& Request : Requests)
        {
            const auto Ticket = FGuid::NewGuid();
            _Pending.Add(Ticket, Request);
            // Ticket never equals the caller's operation ID and is never reused after completion.
            _Transport.Execute(Request.Get_Action(), Ticket, Request.Get_Text(), Request.Get_Ready(), InNow);
        }
    }

    auto FFeature::Complete(FGuid InTicket, ECk_OnlineDirect_Result InResult, const FString& InDetail) -> void
    {
        auto Request = FCk_Request_OnlineDirect{};
        if (!_Pending.RemoveAndCopyValue(InTicket, Request)) { return; }
        if (ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineDirect>();
            Fragment.AdmittedIds.Remove(Request.Get_OperationId());
        }
        auto Completion = FCk_OnlineDirect_Completion{};
        Completion.Set_OperationId(Request.Get_OperationId()).Set_Result(InResult).Set_Detail(InDetail);
        _Dispatch.Add({Request, Completion});
    }

    auto FFeature::CaptureSnapshot() -> void
    {
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{})) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineDirect>();
        auto Snapshot = _Transport.Get_Snapshot();
        const auto Generation = Fragment.Snapshot.Get_ServiceGeneration();
        const auto Different = Snapshot.Get_Revision() != Fragment.Snapshot.Get_Revision() ||
            Snapshot.Get_State() != Fragment.Snapshot.Get_State();
        Snapshot.Set_ServiceGeneration(Generation);
        Fragment.Snapshot = MoveTemp(Snapshot);
        _Changed |= Different;
    }

    auto FFeature::Update(double InNow) -> void
    {
        if (_Closed || ck::Is_NOT_Valid(_Handle)) { return; }
        _Transport.Poll(InNow);
        for (const auto& Completion : _Transport.DrainCompletions())
        { Complete(Completion.Ticket, Completion.Result, Completion.Detail); }
        CaptureSnapshot();
    }

    auto FFeature::Dispatch() -> void
    {
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        { _Dispatch.Reset(); _Changed = false; return; }
        const auto Values = MoveTemp(_Dispatch);
        _Dispatch.Reset();
        const auto Changed = _Changed;
        _Changed = false;
        // Copy all externally observable values before invoking callbacks that can stop the service.
        const auto Snapshot = _Handle.Get<FFragment_OnlineDirect>().Snapshot;
        for (const auto& Value : Values)
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineDirect>();
            Fragment.LastCompletion = Value.Completion;
            if (Fragment.CompletedCount < MAX_int32) { ++Fragment.CompletedCount; }
            const auto Generic = Value.Completion.Get_Result() == ECk_OnlineDirect_Result::Succeeded
                ? ECk_Request_OperationResult::Succeeded : Value.Completion.Get_Result() == ECk_OnlineDirect_Result::Cancelled
                ? ECk_Request_OperationResult::Failed_Cancelled : ECk_Request_OperationResult::Failed;
            Value.Request.TryFireCompletion(_Handle, Generic);
            UUtils_Signal_OnOnlineDirectCompleted::Broadcast(_Handle, MakePayload(_Handle, Value.Completion));
        }
        if (Changed) { UUtils_Signal_OnOnlineDirectChanged::Broadcast(_Handle, MakePayload(_Handle, Snapshot)); }
    }

    auto FFeature::BeginShutdown() -> void
    {
        if (_Closed) { return; }
        _Closed = true;
        if (ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineDirect>();
            auto Requests = MoveTemp(Fragment.Requests);
            Fragment.Requests.Reset();
            for (const auto& Request : Requests) { _Pending.Add(FGuid::NewGuid(), Request); }
        }
        _Transport.Close();
        // Own terminal results before callbacks; queued and in-flight work is cancelled exactly once.
        auto Tickets = TArray<FGuid>{};
        _Pending.GetKeys(Tickets);
        for (const auto Ticket : Tickets) { Complete(Ticket, ECk_OnlineDirect_Result::Cancelled, TEXT("Service stopped.")); }
        _Transport.DrainCompletions();
        CaptureSnapshot();
    }
}
