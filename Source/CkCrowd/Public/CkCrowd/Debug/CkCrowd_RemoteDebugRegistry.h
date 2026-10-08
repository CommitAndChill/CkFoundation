#pragma once

#include "CkCrowd/Debug/CkCrowd_RemoteDebugContract.h"

#include <UObject/WeakObjectPtr.h>

// Native-only registration proof. The token must match the same world and owner on removal.
struct CKCROWD_API FCkCrowdRemoteProviderToken
{
    uint64 Value = 0;
    auto
        IsValid() const
        -> bool
    {
        return Value != 0;
    }
};

class CKCROWD_API FCkCrowdRemoteDebugRegistry
{
public:
    static auto
        TryRegister(UWorld& World, UObject& ProviderOwner,
            FCkCrowdRemoteProviderToken& OutToken)
        -> bool;
    static auto
        TryUnregister(
            const UWorld& World, const UObject& ProviderOwner,
            FCkCrowdRemoteProviderToken Token
        )
        -> bool;

    static auto
        TryCopyRows(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, TArray<FCkCrowdRemoteAgentRow>& Out
        )
        -> bool;
    static auto
        TryRequestSelection(
            const UWorld& World, APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
            int64 SelectionRequestSerial, bool bClear = false)
        -> bool;

    static auto TryCopySelectionEnvelope(const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, FCkCrowdRemoteSelectionEnvelope& Out) -> bool;
    static auto TryAcknowledgeSelection(const UWorld& World, APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteSelectionReceipt& Receipt) -> bool;
    static auto TryRequestNextSelection(const UWorld& World, APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
        bool bClear, int64& OutIssuedSerial) -> bool;
    static auto
        TryCopySelectedDetail(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
            int64 SelectionRequestSerial, FCkCrowdRemoteAgentDetail& Out
        )
        -> bool;
    static auto
        TrySubmit(
            const UWorld& World, APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteCommandRequest& Request
        )
        -> bool;
    static auto
        TryCopyCommandResult(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, int64 ClientSerial,
            FCkCrowdRemoteCommandResult& Out
        )
        -> bool;

private:
    static auto
        TryFind(
            const UWorld& World
        )
        -> ICkCrowdRemoteDebugProvider*;
    static auto
        HasLocalContext(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer
        )
        -> bool;
};
