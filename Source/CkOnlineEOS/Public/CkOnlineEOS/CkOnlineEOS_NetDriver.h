#pragma once

#include "NetDriverEOS.h"
#include "CkOnlineEOS_NetDriver.generated.h"

UCLASS(Transient, Config = Engine)
class CKONLINEEOS_API UCk_OnlineEOS_NetDriver_UE : public UNetDriverEOS
{
    GENERATED_BODY()

public:
    UCk_OnlineEOS_NetDriver_UE(const FObjectInitializer& ObjectInitializer);

    auto IsAvailable() const -> bool override;
    auto GetSocketSubsystem() -> ISocketSubsystem* override;
    auto Shutdown() -> void override;
    auto InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) -> bool override;
    auto InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool InReuseAddressAndPort, FString& Error) -> bool override;
};
