#include "CkEntityBridge_Fragment_Data.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <Engine/World.h>
#include <TimerManager.h>

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
    BeginPlay()
    -> void
{
    Super::BeginPlay();

    if (_RequestTiming != ECk_EntityBridge_RequestTiming::DuringBeginPlay)
    { return; }

    const auto* Owner = GetOwner();

    // On a client of a replicated Actor the call is optional: the authority's Entity arrives either way.
    if (ck::IsValid(Owner) && Owner->GetIsReplicated() && Owner->GetLocalRole() != ROLE_Authority)
    { return; }

    GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<ThisType>{this}]()
    {
        if (ck::Is_NOT_Valid(WeakThis) || ck::Is_NOT_Valid(WeakThis->GetOwner()))
        { return; }

        const auto WasRequestedOrLinked = WeakThis->_HasRequested || WeakThis->_State != ECk_EntityBridge_State::NotRequested;

        CK_ENSURE_IF_NOT(WasRequestedOrLinked,
            TEXT("[{}] has a Ck Entity Bridge but did not call Request_SpawnEntity during BeginPlay. "
                 "Call it, or set the bridge's Request Timing to Deferred."),
            WeakThis->GetOwner())
        {}
    });
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    EndPlay(
        const EEndPlayReason::Type InEndPlayReason)
    -> void
{
    Super::EndPlay(InEndPlayReason);

    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_Entity);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Request_SpawnEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass)
    -> void
{
    DoRequest_SpawnEntity(InEntityScriptClass, {});
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Request_SpawnEntity_WithParams(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams)
    -> void
{
    DoRequest_SpawnEntity(InEntityScriptClass, InSpawnParams);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Promise_OnEntityConstructed(
        const FCk_Delegate_EntityScript_Constructed& InDelegate)
    -> void
{
    CK_ENSURE_IF_NOT(InDelegate.IsBound(),
        TEXT("Promise_OnEntityConstructed called with an unbound delegate on the Ck Entity Bridge of [{}]"), GetOwner())
    { return; }

    if (ck::IsValid(_Entity))
    {
        ck::UUtils_Signal_OnConstructed_PostFireUnbind::Bind(_Entity, InDelegate, ECk_Signal_BindingPolicy::FireIfPayloadInFlight);
        return;
    }

    _PendingConstructed_Delegates.Emplace(InDelegate);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Promise_OnEntityReady(
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate)
    -> void
{
    CK_ENSURE_IF_NOT(InDelegate.IsBound(),
        TEXT("Promise_OnEntityReady called with an unbound delegate on the Ck Entity Bridge of [{}]"), GetOwner())
    { return; }

    if (_State == ECk_EntityBridge_State::Ready)
    {
        InDelegate.ExecuteIfBound(GetOwner(), _Entity);
        return;
    }

    _PendingReady_Delegates.Emplace(InDelegate);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Get_Entity() const
    -> FCk_Handle
{
    return _Entity;
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Get_State() const
    -> ECk_EntityBridge_State
{
    return _State;
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    DoRequest_SpawnEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        const FInstancedStruct& InSpawnParams)
    -> FCk_Handle
{
    return {};
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    DoHandle_EntityReplicationComplete(
        FCk_Handle InEntity)
    -> void
{
    UCk_Utils_EntityBridge_UE::DoTryBecomeReady(this);
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

