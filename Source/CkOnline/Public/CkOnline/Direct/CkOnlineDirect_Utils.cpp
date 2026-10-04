#include "CkOnlineDirect_Utils.h"

#include "CkOnline/Direct/CkOnlineDirect_Fragment.h"
#include "CkOnline/Direct/CkOnlineDirect_Transport.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

auto UCk_Utils_OnlineDirect_UE::Has(const FCk_Handle& InHandle) -> bool
{
    return IsInGameThread() && ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) &&
        InHandle.Has_All<ck::direct::FFragment_OnlineDirect, ck::FFragment_OnlineService>();
}

auto UCk_Utils_OnlineDirect_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult) -> FCk_Handle_OnlineDirect
{
    auto Service = Cast(InHandle);
    OutResult = ck::IsValid(Service) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Service;
}

auto UCk_Utils_OnlineDirect_UE::DoCastChecked(FCk_Handle InHandle) -> FCk_Handle_OnlineDirect { return Cast(InHandle); }

auto UCk_Utils_OnlineDirect_UE::TryRequest(FCk_Handle_OnlineDirect& InService,
    const FCk_Request_OnlineDirect& InRequest) -> bool
{
    if (!Has(InService) || InService.Has<ck::FTag_DestroyEntity_Initiate>() || !InRequest.Get_OperationId().IsValid())
    { return false; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return false; }
    auto& Fragment = InService.Get<ck::direct::FFragment_OnlineDirect>();
    if (Fragment.Requests.Num() >= 64 || Fragment.AdmittedIds.Num() >= 64 ||
        Fragment.AdmittedIds.Contains(InRequest.Get_OperationId())) { return false; }
    switch (InRequest.Get_Action())
    {
        case ECk_OnlineDirect_Action::Host:
        case ECk_OnlineDirect_Action::RenewInvite:
        case ECk_OnlineDirect_Action::Leave:
        case ECk_OnlineDirect_Action::SetReady: break;
        case ECk_OnlineDirect_Action::Join:
            if (!ck::direct::FTransport::ValidateDescriptor(InRequest.Get_Text())) { return false; }
            break;
        case ECk_OnlineDirect_Action::SendText:
            if (!ck::direct::FTransport::ValidateText(InRequest.Get_Text(), 240)) { return false; }
            break;
        default: return false;
    }
    Fragment.AdmittedIds.Add(InRequest.Get_OperationId());
    Fragment.Requests.Add(InRequest);
    return true;
}

auto UCk_Utils_OnlineDirect_UE::Get_Snapshot(const FCk_Handle_OnlineDirect& InService) -> FCk_OnlineDirect_Snapshot
{ return Has(InService) ? InService.Get<ck::direct::FFragment_OnlineDirect>().Snapshot : FCk_OnlineDirect_Snapshot{}; }

auto UCk_Utils_OnlineDirect_UE::Get_LastCompletion(const FCk_Handle_OnlineDirect& InService) -> FCk_OnlineDirect_Completion
{ return Has(InService) ? InService.Get<ck::direct::FFragment_OnlineDirect>().LastCompletion : FCk_OnlineDirect_Completion{}; }

auto UCk_Utils_OnlineDirect_UE::Get_CompletedCount(const FCk_Handle_OnlineDirect& InService) -> int32
{ return Has(InService) ? InService.Get<ck::direct::FFragment_OnlineDirect>().CompletedCount : 0; }

auto UCk_Utils_OnlineDirect_UE::BindTo_OnChanged(FCk_Handle_OnlineDirect& InService,
    const FCk_Delegate_OnlineDirect_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (!Has(InService) || !InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::direct::UUtils_Signal_OnOnlineDirectChanged, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineDirect_UE::BindTo_OnCompleted(FCk_Handle_OnlineDirect& InService,
    const FCk_Delegate_OnlineDirect_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (!Has(InService) || !InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::direct::UUtils_Signal_OnOnlineDirectCompleted, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineDirect_UE::UnbindFrom_OnChanged(FCk_Handle_OnlineDirect& InService,
    const FCk_Delegate_OnlineDirect_Changed& InDelegate) -> void
{
    if (!Has(InService)) { return; }
    ck::direct::UUtils_Signal_OnOnlineDirectChanged::Unbind(InService, InDelegate);
    ck::direct::UUtils_Signal_OnOnlineDirectChanged_PostFireUnbind::Unbind(InService, InDelegate);
}

auto UCk_Utils_OnlineDirect_UE::UnbindFrom_OnCompleted(FCk_Handle_OnlineDirect& InService,
    const FCk_Delegate_OnlineDirect_Completed& InDelegate) -> void
{
    if (!Has(InService)) { return; }
    ck::direct::UUtils_Signal_OnOnlineDirectCompleted::Unbind(InService, InDelegate);
    ck::direct::UUtils_Signal_OnOnlineDirectCompleted_PostFireUnbind::Unbind(InService, InDelegate);
}
