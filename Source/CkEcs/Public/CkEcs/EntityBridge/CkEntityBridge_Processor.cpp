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
            const FFragment_EntityBridge& InOwningActorComp)
            -> void
    {
        const auto& EntityOwningActor = InOwningActorComp.Get_Actor().Get();

        if (ck::Is_NOT_Valid(EntityOwningActor))
        { return; }

        ecs::VeryVerbose(TEXT("Destroying Owning Actor [{}] associated with Entity [{}]"), EntityOwningActor, InHandle);

        EntityOwningActor->Destroy();
    }
}

// --------------------------------------------------------------------------------------------------------------------
