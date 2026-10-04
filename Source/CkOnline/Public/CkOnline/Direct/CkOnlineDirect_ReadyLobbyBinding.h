#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"

#include "CkOnlineDirect_ReadyLobbyBinding.generated.h"

/** Ready-lobby correlation values; capture does not authenticate gameplay or prove current liveness. */
USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_ReadyLobbyBinding
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_ReadyLobbyBinding);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Generation;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Epoch;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _LocalMemberId;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _HostAddress;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FGuid> _Members;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _Hosting = false;

public:
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_Epoch);
    CK_PROPERTY_GET(_LocalMemberId);
    CK_PROPERTY_GET(_HostAddress);
    CK_PROPERTY_GET(_Members);
    CK_PROPERTY_GET(_Hosting);

    static auto TryCapture(
        const FCk_OnlineDirect_Snapshot& InSnapshot,
        bool InHosting,
        int32 InExpectedMembers,
        FCk_OnlineDirect_ReadyLobbyBinding& OutBinding) -> bool;

    auto Matches(const FCk_OnlineDirect_Snapshot& InSnapshot) const -> bool;
    auto Get_IsValid() const -> bool;
};
