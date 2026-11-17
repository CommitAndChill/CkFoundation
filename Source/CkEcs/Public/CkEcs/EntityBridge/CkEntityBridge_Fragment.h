#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Format/CkFormat.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_EntityBridge_UE;

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
        explicit FFragment_EntityBridge(
            AActor* InActor);

    private:
        TWeakObjectPtr<AActor> _Actor;

    public:
        CK_PROPERTY_GET(_Actor);
    };
}

// --------------------------------------------------------------------------------------------------------------------
