#include "CkOnlineDirect_Processor.h"
#include "CkOnline/Direct/CkOnlineDirect_Feature.h"

namespace ck::direct
{
    auto FProcessor_OnlineDirect_HandleRequests::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineDirect& InDirect, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InDirect.Feature.Pin();
        if (Host && Feature) { Feature->HandleRequests(Host->Get_MonotonicTime().Get_Seconds()); }
    }
    auto FProcessor_OnlineDirect_Update::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineDirect& InDirect, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InDirect.Feature.Pin();
        if (Host && Feature) { Feature->Update(Host->Get_MonotonicTime().Get_Seconds()); }
    }
}
