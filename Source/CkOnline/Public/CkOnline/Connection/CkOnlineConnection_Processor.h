#pragma once

#include "CkOnline/Connection/CkOnlineConnection_Fragment.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"

namespace ck::connection
{
    class FProcessor_OnlineConnection_HandleRequests : public TProcessor<FProcessor_OnlineConnection_HandleRequests,
        TReadWrite<FFragment_OnlineConnection>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineConnection& InConnection,
            const FFragment_OnlineService& InService) -> void;
    };
    class FProcessor_OnlineConnection_Update : public TProcessor<FProcessor_OnlineConnection_Update,
        TReadWrite<FFragment_OnlineConnection>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineConnection& InConnection,
            const FFragment_OnlineService& InService) -> void;
    };
}
