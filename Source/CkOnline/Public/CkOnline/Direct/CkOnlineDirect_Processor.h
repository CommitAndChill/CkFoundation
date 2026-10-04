#pragma once
#include "CkOnline/Direct/CkOnlineDirect_Fragment.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"

namespace ck::direct
{
    class FProcessor_OnlineDirect_HandleRequests : public TProcessor<FProcessor_OnlineDirect_HandleRequests,
        TReadWrite<FFragment_OnlineDirect>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineDirect& InDirect,
            const FFragment_OnlineService& InService) -> void;
    };
    class FProcessor_OnlineDirect_Update : public TProcessor<FProcessor_OnlineDirect_Update,
        TReadWrite<FFragment_OnlineDirect>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineDirect& InDirect,
            const FFragment_OnlineService& InService) -> void;
    };
}
