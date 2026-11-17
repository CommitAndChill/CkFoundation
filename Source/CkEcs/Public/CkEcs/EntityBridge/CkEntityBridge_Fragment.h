#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Format/CkFormat.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_EntityBridge_UE;
class UCk_EntityBridge_ActorComponent_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    struct CKECS_API FFragment_EntityBridge
    {
    public:
        CK_GENERATED_BODY(FFragment_EntityBridge);

    public:
        friend class UCk_Utils_EntityBridge_UE;

    public:
        FFragment_EntityBridge() = default;
        FFragment_EntityBridge(
            AActor* InActor,
            UCk_EntityBridge_ActorComponent_UE* InBridge);

    private:
        TWeakObjectPtr<AActor> _Actor;
        TWeakObjectPtr<UCk_EntityBridge_ActorComponent_UE> _Bridge;

    public:
        CK_PROPERTY_GET(_Actor);
        CK_PROPERTY_GET(_Bridge);
    };
}

// --------------------------------------------------------------------------------------------------------------------
