#include "CkOnlineConnection_Utils.h"

#include "CkOnline/Connection/CkOnlineConnection_Feature.h"
#include "CkOnline/CkOnline_Fragment.h"

auto UCk_Utils_OnlineConnection_UE::Has(const FCk_Handle& InHandle) -> bool
{
    return IsInGameThread() && ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) &&
        InHandle.Has_All<ck::connection::FFragment_OnlineConnection, ck::FFragment_OnlineService>();
}

auto UCk_Utils_OnlineConnection_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult) -> FCk_Handle_OnlineConnection
{
    auto Service = Cast(InHandle);
    OutResult = ck::IsValid(Service) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Service;
}
auto UCk_Utils_OnlineConnection_UE::DoCastChecked(FCk_Handle InHandle) -> FCk_Handle_OnlineConnection { return Cast(InHandle); }

auto UCk_Utils_OnlineConnection_UE::TryRequest(FCk_Handle_OnlineConnection& InService,
    const FCk_Request_OnlineConnection& InRequest, FCk_OnlineConnection_Operation& OutOperation) -> bool
{
    if (NOT Has(InService)) { OutOperation = {}; return false; }
    const auto Feature = InService.Get<ck::connection::FFragment_OnlineConnection>()._Feature.Pin();
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (NOT Feature || NOT Host || NOT Host->Get_IsAcceptingRequests()) { OutOperation = {}; return false; }
    auto Admitted = false;
    Host->WithDispatchScope([&]() { Admitted = Feature->TryQueue(InRequest, OutOperation); });
    return Admitted;
}

auto UCk_Utils_OnlineConnection_UE::Get_IsIdle(const FCk_Handle_OnlineConnection& InService) -> bool
{
    if (NOT Has(InService)) { return false; }
    const auto Feature = InService.Get<ck::connection::FFragment_OnlineConnection>()._Feature.Pin();
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (NOT Feature || NOT Host || NOT Host->Get_IsAcceptingRequests()) { return false; }
    auto Idle = false;
    Host->WithDispatchScope([&]() { Idle = Feature->IsIdle(); });
    return Idle && Host->Get_IsAcceptingRequests();
}

auto UCk_Utils_OnlineConnection_UE::Get_Snapshot(const FCk_Handle_OnlineConnection& InService) -> FCk_OnlineConnection_Snapshot
{ return Has(InService) ? InService.Get<ck::connection::FFragment_OnlineConnection>()._Snapshot : FCk_OnlineConnection_Snapshot{}; }

auto UCk_Utils_OnlineConnection_UE::Get_LastCompletion(const FCk_Handle_OnlineConnection& InService) -> FCk_OnlineConnection_Completion
{ return Has(InService) ? InService.Get<ck::connection::FFragment_OnlineConnection>()._LastCompletion : FCk_OnlineConnection_Completion{}; }

auto UCk_Utils_OnlineConnection_UE::Get_CompletedCount(const FCk_Handle_OnlineConnection& InService) -> int32
{ return Has(InService) ? InService.Get<ck::connection::FFragment_OnlineConnection>()._CompletedCount : 0; }

auto UCk_Utils_OnlineConnection_UE::BindTo_OnCompleted(FCk_Handle_OnlineConnection& InService,
    const FCk_Delegate_OnlineConnection_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (NOT Has(InService) || NOT InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (NOT Host || NOT Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::connection::UUtils_Signal_OnOnlineConnectionCompleted, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineConnection_UE::UnbindFrom_OnCompleted(FCk_Handle_OnlineConnection& InService,
    const FCk_Delegate_OnlineConnection_Completed& InDelegate) -> void
{
    if (NOT Has(InService)) { return; }
    ck::connection::UUtils_Signal_OnOnlineConnectionCompleted::Unbind(InService, InDelegate);
    ck::connection::UUtils_Signal_OnOnlineConnectionCompleted_PostFireUnbind::Unbind(InService, InDelegate);
}

auto UCk_Utils_OnlineConnection_UE::BindTo_OnChanged(FCk_Handle_OnlineConnection& InService,
    const FCk_Delegate_OnlineConnection_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (NOT Has(InService) || NOT InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (NOT Host || NOT Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::connection::UUtils_Signal_OnOnlineConnectionChanged, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineConnection_UE::UnbindFrom_OnChanged(FCk_Handle_OnlineConnection& InService,
    const FCk_Delegate_OnlineConnection_Changed& InDelegate) -> void
{
    if (NOT Has(InService)) { return; }
    ck::connection::UUtils_Signal_OnOnlineConnectionChanged::Unbind(InService, InDelegate);
    ck::connection::UUtils_Signal_OnOnlineConnectionChanged_PostFireUnbind::Unbind(InService, InDelegate);
}
