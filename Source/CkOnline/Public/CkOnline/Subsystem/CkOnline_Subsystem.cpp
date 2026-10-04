#include "CkOnline_Subsystem.h"

#include <HAL/PlatformTime.h>

auto UCk_Online_Subsystem_UE::TryStart(TSharedRef<ck::IOnlineServiceAdapter> InAdapter) -> bool
{
    if (_Host && _Host->Get_Diagnostics().Get_State() != ECk_OnlineService_State::Stopped)
    { return false; }
    auto Host = ck::FOnlineServiceHost::TryCreate(MoveTemp(InAdapter), FCk_Time{FPlatformTime::Seconds()});
    if (NOT Host)
    { return false; }
    _Host = MoveTemp(Host);
    return true;
}

auto UCk_Online_Subsystem_UE::Deinitialize() -> void
{
    Request_Stop();
    _Host.Reset();
    Super::Deinitialize();
}

auto UCk_Online_Subsystem_UE::TryStartFeature(TSharedRef<ck::IOnlineServiceFeature> InFeature) -> bool
{
    if (_Host && _Host->Get_Diagnostics().Get_State() != ECk_OnlineService_State::Stopped) { return false; }
    auto Host = ck::FOnlineServiceHost::TryCreateFeature(MoveTemp(InFeature), FCk_Time{FPlatformTime::Seconds()});
    if (!Host) { return false; }
    _Host = MoveTemp(Host);
    return true;
}

auto UCk_Online_Subsystem_UE::Tick(float InDeltaTime) -> void
{
    const auto Host = _Host;
    if (Host)
    { Host->Tick(FCk_Time{FPlatformTime::Seconds()}); }
}

auto UCk_Online_Subsystem_UE::IsTickable() const -> bool
{ return NOT IsTemplate() && _Host && _Host->Get_IsAcceptingRequests(); }

auto UCk_Online_Subsystem_UE::IsTickableWhenPaused() const -> bool
{ return true; }

auto UCk_Online_Subsystem_UE::GetStatId() const -> TStatId
{ RETURN_QUICK_DECLARE_CYCLE_STAT(UCk_Online_Subsystem_UE, STATGROUP_Tickables); }

auto UCk_Online_Subsystem_UE::GetTickableGameObjectWorld() const -> UWorld*
{ return GetWorld(); }

auto UCk_Online_Subsystem_UE::Request_Stop() -> void
{
    const auto Host = _Host;
    if (Host)
    { Host->Request_Shutdown(); }
}

auto UCk_Online_Subsystem_UE::Get_Service() const -> FCk_Handle_OnlineService
{ return _Host ? _Host->Get_Handle() : FCk_Handle_OnlineService{}; }

auto UCk_Online_Subsystem_UE::Get_Diagnostics() const -> FCk_OnlineService_Diagnostics
{ return _Host ? _Host->Get_Diagnostics() : FCk_OnlineService_Diagnostics{}; }
