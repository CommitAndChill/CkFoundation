#include "CkOnlineSteam_Subsystem.h"

#include "CkOnlineSteam_Utils.h"
#include "CkOnline/Steam/CkOnlineSteam_Feature.h"
#include "CkOnline/Subsystem/CkOnline_Subsystem.h"
#include "CkOnline/Steam/CkOnlineSteam_OssProvider.h"
#include "Engine/GameInstance.h"

auto UCk_OnlineSteam_Subsystem_UE::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
    InCollection.InitializeDependency<UCk_Online_Subsystem_UE>();
}

auto UCk_OnlineSteam_Subsystem_UE::Deinitialize() -> void
{
    Request_Stop();
    _Transport.Reset();
    Super::Deinitialize();
}

auto UCk_OnlineSteam_Subsystem_UE::Request_Start(const FCk_OnlineSteam_Spec& InSpec) -> bool
{
    if (!IsInGameThread() || Get_IsStarted()) { return false; }
    if (_Transport)
    {
        const auto State = _Transport->Get_Snapshot().Get_State();
        if (State == ECk_OnlineSteam_State::CleaningUp || State == ECk_OnlineSteam_State::CleanupUnknown)
        { return false; }
        _Transport.Reset();
    }
    auto* Instance = GetGameInstance();
    if (!IsValid(Instance)) { return false; }
    auto* Service = Instance->GetSubsystem<UCk_Online_Subsystem_UE>();
    if (!IsValid(Service)) { return false; }
    const auto Transport = ck::steam::CreateSteamTransport(InSpec);
    if (!Transport) { return false; }
    const auto Feature = ck::steam::FFeature::TryCreate(Transport.ToSharedRef());
    if (!Feature || !Service->TryStartFeature(Feature.ToSharedRef())) { return false; }
    _Transport = Transport;
    _Generation = Service->Get_Diagnostics().Get_Generation();
    return Get_IsStarted();
}

auto UCk_OnlineSteam_Subsystem_UE::Request_Stop() -> void
{
    if (!IsInGameThread()) { return; }
    auto* Instance = GetGameInstance();
    auto* Service = IsValid(Instance) ? Instance->GetSubsystem<UCk_Online_Subsystem_UE>() : nullptr;
    if (IsValid(Service) && _Generation.IsValid() && Service->Get_Diagnostics().Get_Generation() == _Generation)
    { Service->Request_Stop(); }
    _Generation.Invalidate();
}

auto UCk_OnlineSteam_Subsystem_UE::Get_Service() const -> FCk_Handle_OnlineSteam
{
    auto* Instance = GetGameInstance();
    if (!IsInGameThread() || !IsValid(Instance) || !_Generation.IsValid()) { return {}; }
    auto* Service = Instance->GetSubsystem<UCk_Online_Subsystem_UE>();
    if (!IsValid(Service) || Service->Get_Diagnostics().Get_Generation() != _Generation) { return {}; }
    return UCk_Utils_OnlineSteam_UE::Cast(Service->Get_Service());
}

auto UCk_OnlineSteam_Subsystem_UE::Get_IsStarted() const -> bool
{ return ck::IsValid(Get_Service()); }

auto UCk_OnlineSteam_Subsystem_UE::Request(ECk_OnlineSteam_Action InAction, int32 InLobbyIndex,
    FGuid InSearchGeneration, double InTimeoutSeconds, FGuid& OutOperationId) -> bool
{
    OutOperationId.Invalidate();
    if (InAction == ECk_OnlineSteam_Action::Cancel) { return false; }
    auto Handle = Get_Service();
    auto Request = FCk_Request_OnlineSteam{};
    const auto OperationId = FGuid::NewGuid();
    Request.Set_OperationId(OperationId).Set_Action(InAction).Set_LobbyIndex(InLobbyIndex)
        .Set_SearchGeneration(InSearchGeneration).Set_TimeoutSeconds(InTimeoutSeconds);
    if (!UCk_Utils_OnlineSteam_UE::TryRequest(Handle, Request)) { return false; }
    OutOperationId = OperationId;
    return true;
}

auto UCk_OnlineSteam_Subsystem_UE::Request_Cancel(FGuid InTargetOperationId, FGuid& OutOperationId) -> bool
{
    OutOperationId.Invalidate();
    auto Handle = Get_Service();
    const auto OperationId = FGuid::NewGuid();
    auto Request = FCk_Request_OnlineSteam{};
    Request.Set_OperationId(OperationId).Set_Action(ECk_OnlineSteam_Action::Cancel).Set_TargetOperationId(InTargetOperationId);
    if (!UCk_Utils_OnlineSteam_UE::TryRequest(Handle, Request)) { return false; }
    OutOperationId = OperationId;
    return true;
}

auto UCk_OnlineSteam_Subsystem_UE::Get_Snapshot() const -> FCk_OnlineSteam_Snapshot
{
    const auto Service = Get_Service();
    if (ck::IsValid(Service)) { return UCk_Utils_OnlineSteam_UE::Get_Snapshot(Service); }
    if (IsInGameThread() && _Transport)
    {
        auto Snapshot = _Transport->Get_Snapshot();
        Snapshot.Set_ServiceGeneration({});
        return Snapshot;
    }
    return {};
}
auto UCk_OnlineSteam_Subsystem_UE::Get_LastCompletion() const -> FCk_OnlineSteam_Completion
{ return UCk_Utils_OnlineSteam_UE::Get_LastCompletion(Get_Service()); }
auto UCk_OnlineSteam_Subsystem_UE::Get_CompletedCount() const -> int32
{ return UCk_Utils_OnlineSteam_UE::Get_CompletedCount(Get_Service()); }
