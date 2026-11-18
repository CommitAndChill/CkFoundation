#include "CkEntityBridge_Processor.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_EntityBridge_Destroy);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_EntityBridge_Destroy::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_EntityBridge& InEntityBridge)
            -> void
    {
        const auto& BridgedActor = InEntityBridge.Get_Actor().Get();

        if (ck::Is_NOT_Valid(BridgedActor))
        { return; }

        ecs::VeryVerbose(TEXT("Destroying Actor [{}] bridged to Entity [{}]"), BridgedActor, InHandle);

        BridgedActor->Destroy();
    }
}

// --------------------------------------------------------------------------------------------------------------------
