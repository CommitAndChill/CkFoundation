#include "CkEntityBridge_Fragment.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    FFragment_EntityBridge::
        FFragment_EntityBridge(
            UCk_EntityBridge_ActorComponent_UE* InBridge)
        : _Bridge(InBridge)
    {
    }

    auto
        FFragment_EntityBridge::
        Get_Actor() const
        -> TWeakObjectPtr<AActor>
    {
        constexpr auto EvenIfPendingKill = true;
        const auto* Bridge = _Bridge.Get(EvenIfPendingKill);

        if (ck::Is_NOT_Valid(Bridge, ck::IsValid_Policy_IncludePendingKill{}))
        { return {}; }

        return Bridge->GetOwner();
    }
}

// --------------------------------------------------------------------------------------------------------------------

