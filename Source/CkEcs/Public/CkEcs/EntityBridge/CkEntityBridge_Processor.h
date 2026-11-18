#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Fragment.h"

#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // NOTE: CK_IF_TEARING_DOWN on purpose since we only destroy the actor AFTER EndPlay finishes
    class CKECS_API FProcessor_EntityBridge_Destroy : public TProcessor<
        FProcessor_EntityBridge_Destroy,
        ck::TReadOnly<FFragment_EntityBridge>,
        CK_IF_TEARING_DOWN>
    {
    public:
        using Group = FGroup_DestructionPipeline;

    public:
        using TProcessor::TProcessor;

    public:
        static auto 
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_EntityBridge& InEntityBridge) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
