#pragma once

#include "CkOnline/Steam/CkOnlineSteam_Fragment_Data.h"

namespace ck::steam
{
    struct FTransportCompletion
    {
        FGuid Ticket;
        ECk_OnlineSteam_Result Result = ECk_OnlineSteam_Result::Failed;
        FString Detail;
    };

    class CKONLINE_API ITransport
    {
    public:
        virtual ~ITransport() = default;
        virtual auto Execute(const FCk_Request_OnlineSteam& InRequest, FGuid InTicket, double InNow) -> void = 0;
        virtual auto Poll(double InNow) -> void = 0;
        virtual auto Cancel(FGuid InTicket) -> void = 0;
        virtual auto Shutdown() -> void = 0;
        virtual auto DrainCompletions() -> TArray<FTransportCompletion> = 0;
        virtual auto Get_Snapshot() const -> FCk_OnlineSteam_Snapshot = 0;
    };
}
