#include "CkOnline_Utils.h"

#include "CkOnline/CkOnline_Fragment.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

auto UCk_Utils_OnlineService_UE::Has(const FCk_Handle& InHandle) -> bool
{
    return ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) &&
        InHandle.Has_All<ck::FFragment_OnlineService, ck::FFragment_OnlineService_Requests>();
}

auto UCk_Utils_OnlineService_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult)
    -> FCk_Handle_OnlineService
{
    auto Service = Cast(InHandle);
    OutResult = ck::IsValid(Service) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Service;
}

auto UCk_Utils_OnlineService_UE::DoCastChecked(FCk_Handle InHandle) -> FCk_Handle_OnlineService
{
    return CastChecked(InHandle);
}

namespace
{
    constexpr auto MaxQueuedRequests = 64;

    auto Get_CanEnqueue(const FCk_Handle_OnlineService& InService) -> bool
    {
        if (NOT IsInGameThread() || ck::Is_NOT_Valid(InService))
        { return false; }
        if (InService.Has<ck::FTag_DestroyEntity_Initiate>())
        { return false; }
        if (NOT InService.Has<ck::FFragment_OnlineService>() ||
            NOT InService.Has<ck::FFragment_OnlineService_Requests>())
        { return false; }

        const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
        return Host.IsValid() && Host->Get_IsAcceptingRequests() && Host->Get_SupportsGenericOperations() &&
            InService.Get<ck::FFragment_OnlineService_Requests>().Get_Requests().Num() < MaxQueuedRequests;
    }
}

auto UCk_Utils_OnlineService_UE::Request_Execute(
    FCk_Handle_OnlineService& InService, const FCk_Request_OnlineService_Execute& InRequest,
    const FCk_Delegate_Request_OnCompleted& InDelegate) -> FCk_Handle_OnlineService
{
    if (NOT IsInGameThread())
    { return InService; }
    const auto IsValidRequest = InRequest.Get_OperationId().IsValid() &&
        FMath::IsFinite(InRequest.Get_Timeout().Get_Seconds()) && InRequest.Get_Timeout().Get_Seconds() > 0.0;
    CK_ENSURE_IF_NOT(IsValidRequest, TEXT("Online operation requires a valid ID and a positive finite timeout.")) {}
    if (NOT IsValidRequest || NOT Get_CanEnqueue(InService))
    {
        InDelegate.ExecuteIfBound(InService, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InService;
    }

    const auto& State = InService.Get<ck::FFragment_OnlineService>();
    const auto& Queue = InService.Get<ck::FFragment_OnlineService_Requests>();
    auto DuplicatePending = false;
    for (const auto& Entry : State.Get_Pending())
    { DuplicatePending |= Entry.Value.Get_Request().Get_OperationId() == InRequest.Get_OperationId(); }
    const auto DuplicateQueued = Queue.Get_Requests().ContainsByPredicate([&](const auto& Entry)
    {
        return std::holds_alternative<FCk_Request_OnlineService_Execute>(Entry) &&
            std::get<FCk_Request_OnlineService_Execute>(Entry).Get_OperationId() == InRequest.Get_OperationId();
    });
    if (DuplicatePending || DuplicateQueued || State.Get_Pending().Num() + Queue.Get_Requests().Num() >= MaxQueuedRequests)
    {
        InDelegate.ExecuteIfBound(InService, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InService;
    }

    auto Request = InRequest;
    Request.Set_CompletionDelegate(InDelegate);
    InService.Get<ck::FFragment_OnlineService_Requests>()._Requests.Emplace(Request);
    return InService;
}

auto UCk_Utils_OnlineService_UE::Request_Cancel(
    FCk_Handle_OnlineService& InService, const FCk_Request_OnlineService_Cancel& InRequest,
    const FCk_Delegate_Request_OnCompleted& InDelegate) -> FCk_Handle_OnlineService
{
    if (NOT IsInGameThread())
    { return InService; }
    if (NOT InRequest.Get_OperationId().IsValid() || NOT Get_CanEnqueue(InService))
    {
        InDelegate.ExecuteIfBound(InService, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InService;
    }
    auto Request = InRequest;
    Request.Set_CompletionDelegate(InDelegate);
    InService.Get<ck::FFragment_OnlineService_Requests>()._Requests.Emplace(Request);
    return InService;
}

auto UCk_Utils_OnlineService_UE::Get_Diagnostics(const FCk_Handle_OnlineService& InService)
    -> FCk_OnlineService_Diagnostics
{
    if (NOT IsInGameThread() || ck::Is_NOT_Valid(InService) ||
        NOT InService.Has<ck::FFragment_OnlineService>() || NOT InService.Has<ck::FFragment_OnlineService_Requests>())
    { return {}; }
    auto Diagnostics = InService.Get<ck::FFragment_OnlineService>().Get_Diagnostics();
    Diagnostics.Set_Pending(InService.Get<ck::FFragment_OnlineService>().Get_Pending().Num());
    Diagnostics.Set_Queued(InService.Get<ck::FFragment_OnlineService_Requests>().Get_Requests().Num());
    return Diagnostics;
}

auto UCk_Utils_OnlineService_UE::BindTo_OnCompleted(FCk_Handle_OnlineService& InService,
    const FCk_Delegate_OnlineService_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (NOT IsInGameThread() || ck::Is_NOT_Valid(InService) ||
        NOT InService.Has<ck::FFragment_OnlineService>() || NOT InDelegate.IsBound())
    { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (NOT Host || NOT Host->Get_IsAcceptingRequests())
    { return; }
    // Bind can synchronously replay a retained payload, so it has the same teardown boundary as broadcast.
    Host->WithDispatchScope([&]()
    {
        CK_SIGNAL_BIND(ck::UUtils_Signal_OnOnlineServiceCompleted, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineService_UE::UnbindFrom_OnCompleted(FCk_Handle_OnlineService& InService,
    const FCk_Delegate_OnlineService_Completed& InDelegate) -> void
{
    if (NOT IsInGameThread() || ck::Is_NOT_Valid(InService) || NOT InService.Has<ck::FFragment_OnlineService>())
    { return; }
    ck::UUtils_Signal_OnOnlineServiceCompleted::Unbind(InService, InDelegate);
    ck::UUtils_Signal_OnOnlineServiceCompleted_PostFireUnbind::Unbind(InService, InDelegate);
}
