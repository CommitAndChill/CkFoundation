#include "CkEntityBridge_Fragment_Data.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "Net/UnrealNetwork.h"

// --------------------------------------------------------------------------------------------------------------------

UCk_EntityBridge_ActorComponent_UE::
    UCk_EntityBridge_ActorComponent_UE()
{
    SetIsReplicated(false);
    bReplicateUsingRegisteredSubObjectList = true;
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    EndPlay(
        const EEndPlayReason::Type InEndPlayReason)
    -> void
{
    Super::EndPlay(InEndPlayReason);

    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_EntityHandle);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    DoHandle_LinkedEntityReplicationComplete(
        FCk_Handle InEntity)
    -> void
{
    UCk_Utils_EntityBridge_UE::DoFlush_PendingEcsReady_ValuesReplicated(this, GetOwner(), InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

FCk_EntityBridge_BasicDetails::
    FCk_EntityBridge_BasicDetails(
        AActor* InActor,
        FCk_Handle InHandle)
    : _Actor(InActor)
    , _Handle(MoveTemp(InHandle))
{
}

auto
    FCk_EntityBridge_BasicDetails::
    operator==(
        const ThisType& InOther) const
    -> bool
{
    return Get_Actor() == InOther.Get_Actor() && Get_Handle() == InOther.Get_Handle();
}

auto
    GetTypeHash(
        const FCk_EntityBridge_BasicDetails& InBasicDetails)
    -> uint32
{
    return GetTypeHash(InBasicDetails.Get_Actor()) + GetTypeHash(InBasicDetails.Get_Handle());
}

// --------------------------------------------------------------------------------------------------------------------

