#pragma once

#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"

namespace ck
{
    class FProcessor_OnlineService_HandleRequests
        : public TProcessor<FProcessor_OnlineService_HandleRequests,
            TReadWrite<FFragment_OnlineService>, TReadWrite<FFragment_OnlineService_Requests>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineService& InState,
            FFragment_OnlineService_Requests& InRequests) -> void;
    };

    class FProcessor_OnlineService_Update
        : public TProcessor<FProcessor_OnlineService_Update, TReadWrite<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineService& InState) -> void;
    };
}
