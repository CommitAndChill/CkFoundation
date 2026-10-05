#pragma once
#include "CkOnline/Steam/CkOnlineSteam_Fragment.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"

namespace ck::steam
{
    class FProcessor_OnlineSteam_HandleRequests : public TProcessor<FProcessor_OnlineSteam_HandleRequests,
        TReadWrite<FFragment_OnlineSteam>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineSteam& InSteam,
            const FFragment_OnlineService& InService) -> void;
    };
    class FProcessor_OnlineSteam_Update : public TProcessor<FProcessor_OnlineSteam_Update,
        TReadWrite<FFragment_OnlineSteam>, TReadOnly<FFragment_OnlineService>>
    {
    public:
        using TProcessor::TProcessor;
        auto ForEachEntity(TimeType InDeltaTime, HandleType InHandle, FFragment_OnlineSteam& InSteam,
            const FFragment_OnlineService& InService) -> void;
    };
}
