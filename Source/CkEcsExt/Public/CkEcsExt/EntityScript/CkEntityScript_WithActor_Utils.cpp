#include "CkEntityScript_WithActor_Utils.h"

#include "CkEntityScript_WithActor.h"
#include "CkEntityScript_WithActor_Data.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <GameFramework/Actor.h>
#include <StructUtils/InstancedStruct.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityScript_WithActor_UE::
    Request_SpawnEntityScript_OnActor(
        AActor* InActor,
        TSubclassOf<UCk_EntityScript_WithActor_UE> InEntityScriptClass)
    -> FCk_Handle_PendingEntityScript
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor),
        TEXT("Request_SpawnEntityScript_OnActor called with an invalid Actor"))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InEntityScriptClass),
        TEXT("Request_SpawnEntityScript_OnActor called with an invalid EntityScript class for Actor [{}]"), InActor)
    { return {}; }

    auto* Bridge = UCk_Utils_EntityBridge_UE::DoGetOrAdd_Bridge(InActor);

    if (ck::Is_NOT_Valid(Bridge))
    { return {}; }

    return FCk_Handle_PendingEntityScript{Bridge->DoRequest_SpawnLinkedEntity(InEntityScriptClass, {})};
}

// --------------------------------------------------------------------------------------------------------------------
