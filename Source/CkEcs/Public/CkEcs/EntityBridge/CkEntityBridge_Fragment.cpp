#include "CkEntityBridge_Fragment.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    FFragment_EntityBridge::
        FFragment_EntityBridge(
            AActor* InActor,
            UCk_EntityBridge_ActorComponent_UE* InBridge)
        : _Actor(InActor)
        , _Bridge(InBridge)
    {
    }
}

// --------------------------------------------------------------------------------------------------------------------

