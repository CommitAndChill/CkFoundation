#include "CkOnlineSteam_Processor.h"
#include "CkOnline/Steam/CkOnlineSteam_Feature.h"

namespace ck::steam
{
    auto FProcessor_OnlineSteam_HandleRequests::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineSteam& InSteam, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InSteam.Feature.Pin();
        if (Host && Feature) { Feature->HandleRequests(Host->Get_MonotonicTime().Get_Seconds()); }
    }

    auto FProcessor_OnlineSteam_Update::ForEachEntity(TimeType InDeltaTime, HandleType InHandle,
        FFragment_OnlineSteam& InSteam, const FFragment_OnlineService& InService) -> void
    {
        const auto Host = InService.Get_Host().Pin();
        const auto Feature = InSteam.Feature.Pin();
        if (Host && Feature) { Feature->Update(Host->Get_MonotonicTime().Get_Seconds()); }
    }
}
