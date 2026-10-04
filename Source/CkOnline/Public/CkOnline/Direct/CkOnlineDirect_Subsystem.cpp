#include "CkOnlineDirect_Subsystem.h"
#include "CkOnline/Direct/CkOnlineDirect_Feature.h"
#include "CkOnline/Direct/CkOnlineDirect_Utils.h"
#include "CkOnline/Subsystem/CkOnline_Subsystem.h"
#include "Engine/GameInstance.h"

auto UCk_OnlineDirect_Subsystem_UE::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
    InCollection.InitializeDependency<UCk_Online_Subsystem_UE>();
}

auto UCk_OnlineDirect_Subsystem_UE::Deinitialize() -> void
{
    Request_Stop();
    Super::Deinitialize();
}

auto UCk_OnlineDirect_Subsystem_UE::Request_Start(const FCk_OnlineDirect_Spec& InSpec) -> bool
{
    if (!IsInGameThread() || Get_IsStarted()) { return false; }
    auto* Instance = GetGameInstance();
    if (!IsValid(Instance)) { return false; }
    auto* Service = Instance->GetSubsystem<UCk_Online_Subsystem_UE>();
    if (!IsValid(Service)) { return false; }
    const auto Feature = ck::direct::FFeature::TryCreate(InSpec);
    if (!Feature || !Service->TryStartFeature(Feature.ToSharedRef())) { return false; }
    _Generation = Service->Get_Diagnostics().Get_Generation();
    return Get_IsStarted();
}

auto UCk_OnlineDirect_Subsystem_UE::Get_Service() const -> FCk_Handle_OnlineDirect
{
    auto* Instance = GetGameInstance();
    if (!IsInGameThread() || !IsValid(Instance) || !_Generation.IsValid()) { return {}; }
    auto* Service = Instance->GetSubsystem<UCk_Online_Subsystem_UE>();
    if (!IsValid(Service) || Service->Get_Diagnostics().Get_Generation() != _Generation) { return {}; }
    return UCk_Utils_OnlineDirect_UE::Cast(Service->Get_Service());
}

auto UCk_OnlineDirect_Subsystem_UE::Get_IsStarted() const -> bool { return ck::IsValid(Get_Service()); }

auto UCk_OnlineDirect_Subsystem_UE::Request_Stop() -> void
{
    if (!IsInGameThread()) { return; }
    // Do not stop another profile that replaced this facade's generation.
    if (Get_IsStarted())
    {
        auto* Instance = GetGameInstance();
        auto* Service = Instance != nullptr ? Instance->GetSubsystem<UCk_Online_Subsystem_UE>() : nullptr;
        if (IsValid(Service)) { Service->Request_Stop(); }
    }
    _Generation.Invalidate();
}

auto UCk_OnlineDirect_Subsystem_UE::Request(ECk_OnlineDirect_Action InAction,
    const FString& InText, bool InReady) -> bool
{
    auto Handle = Get_Service();
    auto Request = FCk_Request_OnlineDirect{};
    Request.Set_OperationId(FGuid::NewGuid()).Set_Action(InAction).Set_Text(InText).Set_Ready(InReady);
    return UCk_Utils_OnlineDirect_UE::TryRequest(Handle, Request);
}

auto UCk_OnlineDirect_Subsystem_UE::Get_Snapshot() const -> FCk_OnlineDirect_Snapshot
{ return UCk_Utils_OnlineDirect_UE::Get_Snapshot(Get_Service()); }
auto UCk_OnlineDirect_Subsystem_UE::Get_LastCompletion() const -> FCk_OnlineDirect_Completion
{ return UCk_Utils_OnlineDirect_UE::Get_LastCompletion(Get_Service()); }
auto UCk_OnlineDirect_Subsystem_UE::Get_CompletedCount() const -> int32
{ return UCk_Utils_OnlineDirect_UE::Get_CompletedCount(Get_Service()); }
