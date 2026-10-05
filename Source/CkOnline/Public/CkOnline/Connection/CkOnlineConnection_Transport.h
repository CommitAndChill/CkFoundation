#pragma once

#include "CkOnline/Connection/CkOnlineConnection_Fragment_Data.h"

class UGameInstance;

namespace ck::connection
{
    enum class EReturnAdmission : uint8 { Started, Busy, Rejected };
    struct FEntryIntent
    {
        FCk_Handle_OnlineDirect DirectService;
        FCk_OnlineDirect_ReadyLobbyBinding Binding;
        int32 ExpectedMembers = 0;
        bool Hosting = false;
        FString GameplayMap;
        FString GameplayMode;
        FString EntryUrl;
        FString Endpoint;
        int32 ListenPort = 0;
        FString ReturnMap;
        FString ReturnMode;
        double EntryTimeoutSeconds = 0.0;
        double ReturnTimeoutSeconds = 0.0;
    };

    struct FObservation
    {
        bool TransportEstablished = false;
        bool TransportLost = false;
        bool ReturnEstablished = false;
        FString Error;
    };

    /** Engine callbacks collect bounded facts; only owning ticks submit travel. */
    class CKONLINE_API ITransport
    {
    public:
        virtual ~ITransport() = default;
        virtual auto CanEnter(const FEntryIntent& InIntent) const -> bool = 0;
        virtual auto IsIdle() const -> bool = 0;
        virtual auto BeginEntry(const FEntryIntent& InIntent) -> bool = 0;
        virtual auto Observe() -> FObservation = 0;
        virtual auto BeginReturn(const FEntryIntent& InIntent) -> EReturnAdmission = 0;
        virtual auto Shutdown() -> void = 0;
    };

    CKONLINE_API auto TryCreateIpTransport(UGameInstance* InInstance) -> TSharedPtr<ITransport>;
}
