#include "CkOnlineSteam_Utils.h"

#include "CkOnline/Steam/CkOnlineSteam_Fragment.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

auto UCk_Utils_OnlineSteam_UE::Has(const FCk_Handle& InHandle) -> bool
{
    return IsInGameThread() && ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) &&
        InHandle.Has_All<ck::steam::FFragment_OnlineSteam, ck::FFragment_OnlineService>();
}

auto UCk_Utils_OnlineSteam_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult) -> FCk_Handle_OnlineSteam
{
    auto Service = Cast(InHandle);
    OutResult = ck::IsValid(Service) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Service;
}

auto UCk_Utils_OnlineSteam_UE::DoCastChecked(FCk_Handle InHandle) -> FCk_Handle_OnlineSteam
{ return Cast(InHandle); }

auto UCk_Utils_OnlineSteam_UE::TryRequest(FCk_Handle_OnlineSteam& InService,
    const FCk_Request_OnlineSteam& InRequest) -> bool
{
    const auto ValidTimeout = FMath::IsFinite(InRequest.Get_TimeoutSeconds()) &&
        InRequest.Get_TimeoutSeconds() > 0.0 && InRequest.Get_TimeoutSeconds() <= 120.0;
    if (!Has(InService) || InService.Has<ck::FTag_DestroyEntity_Initiate>() ||
        !InRequest.Get_OperationId().IsValid() || !ValidTimeout)
    { return false; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return false; }
    auto& Fragment = InService.Get<ck::steam::FFragment_OnlineSteam>();
    if (Fragment.Requests.Num() >= 32 || Fragment.AdmittedIds.Num() >= 32 ||
        Fragment.AdmittedIds.Contains(InRequest.Get_OperationId())) { return false; }
    switch (InRequest.Get_Action())
    {
        case ECk_OnlineSteam_Action::Login:
        case ECk_OnlineSteam_Action::CreateLobby:
        case ECk_OnlineSteam_Action::FindLobbies:
        case ECk_OnlineSteam_Action::LeaveLobby: break;
        case ECk_OnlineSteam_Action::JoinLobby:
            if (InRequest.Get_LobbyIndex() < 0 || !InRequest.Get_SearchGeneration().IsValid()) { return false; }
            break;
        case ECk_OnlineSteam_Action::Cancel:
            if (!InRequest.Get_TargetOperationId().IsValid() ||
                !Fragment.AdmittedIds.Contains(InRequest.Get_TargetOperationId())) { return false; }
            break;
        default: return false;
    }
    if (Fragment.AdmittedIds.Num() > 0 && InRequest.Get_Action() != ECk_OnlineSteam_Action::Cancel)
    { return false; }
    Fragment.AdmittedIds.Add(InRequest.Get_OperationId());
    Fragment.Requests.Add(InRequest);
    return true;
}

auto UCk_Utils_OnlineSteam_UE::Get_Snapshot(const FCk_Handle_OnlineSteam& InService) -> FCk_OnlineSteam_Snapshot
{ return Has(InService) ? InService.Get<ck::steam::FFragment_OnlineSteam>().Snapshot : FCk_OnlineSteam_Snapshot{}; }
auto UCk_Utils_OnlineSteam_UE::Get_LastCompletion(const FCk_Handle_OnlineSteam& InService) -> FCk_OnlineSteam_Completion
{ return Has(InService) ? InService.Get<ck::steam::FFragment_OnlineSteam>().LastCompletion : FCk_OnlineSteam_Completion{}; }
auto UCk_Utils_OnlineSteam_UE::Get_CompletedCount(const FCk_Handle_OnlineSteam& InService) -> int32
{ return Has(InService) ? InService.Get<ck::steam::FFragment_OnlineSteam>().CompletedCount : 0; }

auto UCk_Utils_OnlineSteam_UE::BindTo_OnCompleted(FCk_Handle_OnlineSteam& InService,
    const FCk_Delegate_OnlineSteam_Completed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (!Has(InService) || !InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::steam::UUtils_Signal_OnOnlineSteamCompleted, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineSteam_UE::BindTo_OnChanged(FCk_Handle_OnlineSteam& InService,
    const FCk_Delegate_OnlineSteam_Changed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior) -> void
{
    if (!Has(InService) || !InDelegate.IsBound()) { return; }
    const auto Host = InService.Get<ck::FFragment_OnlineService>().Get_Host().Pin();
    if (!Host || !Host->Get_IsAcceptingRequests()) { return; }
    Host->WithDispatchScope([&]() {
        CK_SIGNAL_BIND(ck::steam::UUtils_Signal_OnOnlineSteamChanged, InService, InDelegate, InBindingPolicy, InPostFireBehavior);
    });
}

auto UCk_Utils_OnlineSteam_UE::UnbindFrom_OnCompleted(FCk_Handle_OnlineSteam& InService,
    const FCk_Delegate_OnlineSteam_Completed& InDelegate) -> void
{
    if (!Has(InService)) { return; }
    ck::steam::UUtils_Signal_OnOnlineSteamCompleted::Unbind(InService, InDelegate);
    ck::steam::UUtils_Signal_OnOnlineSteamCompleted_PostFireUnbind::Unbind(InService, InDelegate);
}

auto UCk_Utils_OnlineSteam_UE::UnbindFrom_OnChanged(FCk_Handle_OnlineSteam& InService,
    const FCk_Delegate_OnlineSteam_Changed& InDelegate) -> void
{
    if (!Has(InService)) { return; }
    ck::steam::UUtils_Signal_OnOnlineSteamChanged::Unbind(InService, InDelegate);
    ck::steam::UUtils_Signal_OnOnlineSteamChanged_PostFireUnbind::Unbind(InService, InDelegate);
}
