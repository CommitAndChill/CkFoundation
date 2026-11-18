#include "CkEntityBridge_Fragment_Data.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkCore/Object/CkObject_Utils.h"
#include "CkEcs/CkEcsLog.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <Engine/World.h>

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

    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_Entity);
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Request_SpawnLinkedEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass)
    -> void
{
    DoRequest_SpawnLinkedEntity(InEntityScriptClass, {});
}

auto
    UCk_EntityBridge_ActorComponent_UE::
    Request_SpawnLinkedEntity_WithParams(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams)
    -> void
{
    DoRequest_SpawnLinkedEntity(InEntityScriptClass, InSpawnParams);
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
    DoRequest_SpawnLinkedEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        const FInstancedStruct& InSpawnParams)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InEntityScriptClass),
        TEXT("Request_SpawnLinkedEntity on the Ck Entity Bridge of [{}] was given an invalid EntityScript class"), GetOwner())
    { return {}; }

    auto* ScriptDefault = UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_EntityScript_UE>(InEntityScriptClass);
    const auto IsActorBoundScript = ck::IsValid(ScriptDefault) && ScriptDefault->Get_IsActorBound();

    CK_ENSURE_IF_NOT(IsActorBoundScript,
        TEXT("[{}] is not an actor-bound EntityScript; derive it from UCk_EntityScript_WithActor_UE"), InEntityScriptClass)
    { return {}; }

    CK_ENSURE_IF_NOT(NOT _HasRequested,
        TEXT("Request_SpawnLinkedEntity called more than once on [{}]"), GetOwner())
    { return {}; }

    auto* Owner = GetOwner();
    auto* World = ck::IsValid(Owner) ? Owner->GetWorld() : static_cast<UWorld*>(nullptr);
    auto TransientEntity = ck::IsValid(World) ? UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World) : FCk_Handle{};

    CK_ENSURE_IF_NOT(ck::IsValid(TransientEntity),
        TEXT("Request_SpawnLinkedEntity on [{}] was called before the ECS world exists; call it from BeginPlay or later"), Owner)
    { return {}; }

    _HasRequested = true;
    _RequestedEntityScriptClass = InEntityScriptClass.Get();

    if (const auto* EcsWorld = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        ck::IsValid(EcsWorld) && EcsWorld->Get_LoadHold() == ECk_EcsWorld_LoadHold::Teardown)
    {
        ck::ecs::Verbose(TEXT("Request_SpawnLinkedEntity on [{}] ignored: its world is tearing down"), Owner);
        return {};
    }

    const auto WouldReplicate = Owner->GetIsReplicated() &&
        ScriptDefault->Get_EffectiveReplication() == ECk_Replication::Replicates;

    const auto& WorldSettings = TransientEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
    const auto NetMode = WorldSettings.Get_NetMode();

    if (NetMode == ECk_Net_NetModeType::Client && WouldReplicate && Owner->GetLocalRole() != ROLE_Authority)
    {
        if (_State == ECk_EntityBridge_State::NotRequested)
        { _State = ECk_EntityBridge_State::AwaitingReplica; }

        const auto ReplicaMatchesRequest = [&]() -> bool
        {
            if (ck::Is_NOT_Valid(_Entity) || NOT _Entity.Has<ck::FFragment_EntityScript>())
            { return true; }

            const auto& ReplicaScript = _Entity.Get<ck::FFragment_EntityScript>().Get_Script();
            return ck::Is_NOT_Valid(ReplicaScript) || ReplicaScript->GetClass() == InEntityScriptClass;
        }();

        CK_ENSURE_IF_NOT(ReplicaMatchesRequest,
            TEXT("[{}] requested EntityScript [{}] but the authority's replica Entity [{}] was built from another class"),
            Owner, InEntityScriptClass, _Entity)
        {}

        return {};
    }

    const auto Replication = WouldReplicate && NetMode != ECk_Net_NetModeType::Client
        ? ECk_Replication::Replicates
        : ECk_Replication::DoesNotReplicate;

    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(TransientEntity);
    UCk_Utils_Net_UE::Add(NewEntity, FCk_Net_ConnectionSettings{Replication, NetMode, WorldSettings.Get_NetRole()});

    if (const auto PendingEntity = UCk_Utils_EntityScript_UE::Add_ForBridge(NewEntity, ScriptDefault, InSpawnParams, this, nullptr);
        ck::Is_NOT_Valid(PendingEntity))
    {
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(NewEntity);
        _HasRequested = false;
        return {};
    }

    return NewEntity;
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

