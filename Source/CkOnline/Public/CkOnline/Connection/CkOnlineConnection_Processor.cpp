#include "CkOnlineConnection_Processor.h"
#include "CkOnline/Connection/CkOnlineConnection_Feature.h"

namespace ck::connection
{
    auto FProcessor_OnlineConnection_HandleRequests::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineConnection& InConnection, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InConnection._Feature.Pin();
        if (Host && Feature) { Feature->HandleRequests(Host->Get_MonotonicTime().Get_Seconds()); }
    }
    auto FProcessor_OnlineConnection_Update::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineConnection& InConnection, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InConnection._Feature.Pin();
        if (Host && Feature) { Feature->Update(Host->Get_MonotonicTime().Get_Seconds()); }
    }
}
