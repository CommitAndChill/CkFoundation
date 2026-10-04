#include "CkOnline_Processor.h"

namespace ck
{
    auto FProcessor_OnlineService_HandleRequests::ForEachEntity(
        TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineService& InState,
        FFragment_OnlineService_Requests& InRequests) -> void
    {
        if (const auto Host = InState.Get_Host().Pin())
        { Host->DoHandleRequests(); }
    }

    auto FProcessor_OnlineService_Update::ForEachEntity(
        TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineService& InState) -> void
    {
        if (const auto Host = InState.Get_Host().Pin())
        { Host->DoUpdate(); }
    }
}
