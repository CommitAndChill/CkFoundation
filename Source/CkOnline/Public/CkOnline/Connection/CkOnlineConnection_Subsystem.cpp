#include "CkOnlineConnection_Subsystem.h"

#include "CkOnline/Connection/CkOnlineConnection_Feature.h"
#include "CkOnline/Connection/CkOnlineConnection_Utils.h"
#include "CkOnline/Direct/CkOnlineDirect_Subsystem.h"
#include "HAL/PlatformTime.h"

auto UCk_OnlineConnection_Subsystem_UE::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
    InCollection.InitializeDependency<UCk_OnlineDirect_Subsystem_UE>();
    const auto Transport = ck::connection::TryCreateIpTransport(GetGameInstance());
    if (NOT Transport) { return; }
    const auto Feature = ck::connection::FFeature::TryCreate(Transport.ToSharedRef());
    if (NOT Feature) { Transport->Shutdown(); return; }
    _Host = ck::FOnlineServiceHost::TryCreateFeature(Feature.ToSharedRef(), FCk_Time{FPlatformTime::Seconds()});
    if (NOT _Host) { Transport->Shutdown(); }
}

auto UCk_OnlineConnection_Subsystem_UE::Deinitialize() -> void
{
    const auto Host = _Host;
    if (Host) { Host->Request_Shutdown(); }
    _Host.Reset();
    Super::Deinitialize();
}

auto UCk_OnlineConnection_Subsystem_UE::Tick(float InDeltaTime) -> void
{
    const auto Host = _Host;
    if (Host) { Host->Tick(FCk_Time{FPlatformTime::Seconds()}); }
}
auto UCk_OnlineConnection_Subsystem_UE::IsTickable() const -> bool
{ return NOT IsTemplate() && _Host && _Host->Get_IsAcceptingRequests(); }
auto UCk_OnlineConnection_Subsystem_UE::GetStatId() const -> TStatId
{ RETURN_QUICK_DECLARE_CYCLE_STAT(UCk_OnlineConnection_Subsystem_UE, STATGROUP_Tickables); }
auto UCk_OnlineConnection_Subsystem_UE::GetTickableGameObjectWorld() const -> UWorld* { return GetWorld(); }
auto UCk_OnlineConnection_Subsystem_UE::Get_Service() const -> FCk_Handle_OnlineConnection
{ return IsInGameThread() && _Host ? UCk_Utils_OnlineConnection_UE::Cast(_Host->Get_Handle()) : FCk_Handle_OnlineConnection{}; }
auto UCk_OnlineConnection_Subsystem_UE::TryRequest(const FCk_Request_OnlineConnection& InRequest,
    FCk_OnlineConnection_Operation& OutOperation) -> bool
{
    auto Service = Get_Service();
    return UCk_Utils_OnlineConnection_UE::TryRequest(Service, InRequest, OutOperation);
}
auto UCk_OnlineConnection_Subsystem_UE::Get_Snapshot() const -> FCk_OnlineConnection_Snapshot
{ return UCk_Utils_OnlineConnection_UE::Get_Snapshot(Get_Service()); }
auto UCk_OnlineConnection_Subsystem_UE::Get_LastCompletion() const -> FCk_OnlineConnection_Completion
{ return UCk_Utils_OnlineConnection_UE::Get_LastCompletion(Get_Service()); }
auto UCk_OnlineConnection_Subsystem_UE::Get_CompletedCount() const -> int32
{ return UCk_Utils_OnlineConnection_UE::Get_CompletedCount(Get_Service()); }
auto UCk_OnlineConnection_Subsystem_UE::Get_IsIdle() const -> bool
{ return UCk_Utils_OnlineConnection_UE::Get_IsIdle(Get_Service()); }
