#include "CkCrowd/Debug/CkCrowd_RemoteDebug_Utils.h"

#include "CkCrowd/Debug/CkCrowd_RemoteDebugRegistry.h"

#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>

namespace ck_crowd_remote_debug_utils
{
    auto
        TryGet_LocalContext(
            APlayerController* InRequestingPC,
            UWorld*& OutWorld,
            ULocalPlayer*& OutLocalPlayer)
        -> bool
    {
        OutWorld = nullptr;
        OutLocalPlayer = nullptr;
        if (!IsInGameThread())
        { return false; }
        if (!IsValid(InRequestingPC))
        { return false; }
        if (InRequestingPC->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }
        if (InRequestingPC->IsActorBeingDestroyed())
        { return false; }
        auto* World = InRequestingPC->GetWorld();
        if (!IsValid(World))
        { return false; }
        if (World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || World->bIsTearingDown)
        { return false; }
        auto* LocalPlayer = InRequestingPC->GetLocalPlayer();
        if (!IsValid(LocalPlayer))
        { return false; }
        if (LocalPlayer->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }
        OutWorld = World;
        OutLocalPlayer = LocalPlayer;
        return true;
    }
}

auto
    UCk_Utils_CrowdRemoteDebug_UE::
    TryCopyRows(
        APlayerController* InRequestingPC,
        TArray<FCkCrowdRemoteAgentRow>& OutRows)
    -> bool
{
    OutRows.Reset();
    UWorld* World = nullptr;
    ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(
        InRequestingPC, World, LocalPlayer))
    { return false; }
    return FCkCrowdRemoteDebugRegistry::TryCopyRows(
        *World, *InRequestingPC, *LocalPlayer, OutRows);
}

auto
    UCk_Utils_CrowdRemoteDebug_UE::
    TryRequestSelection(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteAgentId& InId,
        int64 InSelectionRequestSerial,
        bool bClear)
    -> bool
{
    UWorld* World = nullptr;
    ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(
        InRequestingPC, World, LocalPlayer))
    { return false; }
    return FCkCrowdRemoteDebugRegistry::TryRequestSelection(
        *World, *InRequestingPC, *LocalPlayer, InId, InSelectionRequestSerial, bClear);
}


auto UCk_Utils_CrowdRemoteDebug_UE::TryCopySelectionEnvelope(APlayerController* InRequestingPC,
    FCkCrowdRemoteSelectionEnvelope& OutEnvelope) -> bool
{
    OutEnvelope = {};
    UWorld* World = nullptr; ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(InRequestingPC, World, LocalPlayer)) { return false; }
    return FCkCrowdRemoteDebugRegistry::TryCopySelectionEnvelope(*World, *InRequestingPC, *LocalPlayer, OutEnvelope);
}

auto UCk_Utils_CrowdRemoteDebug_UE::TryAcknowledgeSelection(APlayerController* InRequestingPC,
    const FCkCrowdRemoteSelectionReceipt& InReceipt) -> bool
{
    UWorld* World = nullptr; ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(InRequestingPC, World, LocalPlayer)) { return false; }
    return FCkCrowdRemoteDebugRegistry::TryAcknowledgeSelection(*World, *InRequestingPC, *LocalPlayer, InReceipt);
}

auto UCk_Utils_CrowdRemoteDebug_UE::TryRequestNextSelection(APlayerController* InRequestingPC,
    const FCkCrowdRemoteAgentId& InId, bool bClear, int64& OutIssuedSerial) -> bool
{
    OutIssuedSerial = 0;
    UWorld* World = nullptr; ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(InRequestingPC, World, LocalPlayer)) { return false; }
    return FCkCrowdRemoteDebugRegistry::TryRequestNextSelection(
        *World, *InRequestingPC, *LocalPlayer, InId, bClear, OutIssuedSerial);
}

auto
    UCk_Utils_CrowdRemoteDebug_UE::
    TryCopySelectedDetail(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteAgentId& InId,
        int64 InSelectionRequestSerial,
        FCkCrowdRemoteAgentDetail& OutDetail)
    -> bool
{
    OutDetail = {};
    UWorld* World = nullptr;
    ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(
        InRequestingPC, World, LocalPlayer))
    { return false; }
    return FCkCrowdRemoteDebugRegistry::TryCopySelectedDetail(
        *World, *InRequestingPC, *LocalPlayer, InId, InSelectionRequestSerial, OutDetail);
}

auto
    UCk_Utils_CrowdRemoteDebug_UE::
    TrySubmit(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteCommandRequest& InRequest)
    -> bool
{
    UWorld* World = nullptr;
    ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(
        InRequestingPC, World, LocalPlayer))
    { return false; }
    return FCkCrowdRemoteDebugRegistry::TrySubmit(
        *World, *InRequestingPC, *LocalPlayer, InRequest);
}

auto
    UCk_Utils_CrowdRemoteDebug_UE::
    TryCopyCommandResult(
        APlayerController* InRequestingPC,
        int64 InClientSerial,
        FCkCrowdRemoteCommandResult& OutResult)
    -> bool
{
    OutResult = {};
    UWorld* World = nullptr;
    ULocalPlayer* LocalPlayer = nullptr;
    if (!ck_crowd_remote_debug_utils::TryGet_LocalContext(
        InRequestingPC, World, LocalPlayer))
    { return false; }
    return FCkCrowdRemoteDebugRegistry::TryCopyCommandResult(
        *World, *InRequestingPC, *LocalPlayer, InClientSerial, OutResult);
}
