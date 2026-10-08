#pragma once

#include "CkCrowd/Debug/CkCrowd_RemoteDebugContract.h"

#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkCrowd_RemoteDebug_Utils.generated.h"

class APlayerController;

// Consumer facade. Provider registration remains a native integration protocol.
UCLASS()
class CKCROWD_API UCk_Utils_CrowdRemoteDebug_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool
    TryCopyRows(
        APlayerController* InRequestingPC,
        TArray<FCkCrowdRemoteAgentRow>& OutRows);

    // True means the local provider queued a selection request, not server acceptance.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool
    TryRequestSelection(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteAgentId& InId,
        int64 InSelectionRequestSerial,
        bool bClear = false);


    UFUNCTION(BlueprintPure, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool TryCopySelectionEnvelope(APlayerController* InRequestingPC,
        FCkCrowdRemoteSelectionEnvelope& OutEnvelope);

    // Explicit receipt only after installing the entire copied envelope.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool TryAcknowledgeSelection(APlayerController* InRequestingPC,
        const FCkCrowdRemoteSelectionReceipt& InReceipt);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool TryRequestNextSelection(APlayerController* InRequestingPC,
        const FCkCrowdRemoteAgentId& InId, bool bClear, int64& OutIssuedSerial);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool
    TryCopySelectedDetail(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteAgentId& InId,
        int64 InSelectionRequestSerial,
        FCkCrowdRemoteAgentDetail& OutDetail);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool
    TrySubmit(
        APlayerController* InRequestingPC,
        const FCkCrowdRemoteCommandRequest& InRequest);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|CrowdRemoteDebug")
    static bool
    TryCopyCommandResult(
        APlayerController* InRequestingPC,
        int64 InClientSerial,
        FCkCrowdRemoteCommandResult& OutResult);
};
