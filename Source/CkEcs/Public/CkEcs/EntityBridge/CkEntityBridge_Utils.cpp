#include "CkEntityBridge_Utils.h"

#include "CkEcs/CkEcsLog.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Fragment.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include <HAL/IConsoleManager.h>
#include <UObject/UObjectIterator.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_entity_bridge_utils
{
    auto
        Get_ConstructionStageText(
            const FCk_Handle& InEntity)
        -> FString
    {
        auto Stages = TArray<FString>{};

        if (InEntity.Has<ck::FTag_EntityScript_ContinueConstruction>())
        { Stages.Emplace(TEXT("ContinueConstruction")); }

        if (InEntity.Has<ck::FTag_EntityScript_FinishConstruction>())
        { Stages.Emplace(TEXT("FinishConstruction")); }

        if (InEntity.Has<ck::FTag_EntityScript_BeginPlay>())
        { Stages.Emplace(TEXT("BeginPlay")); }

        if (InEntity.Has<ck::FTag_EntityScript_HasBegunPlay>())
        { Stages.Emplace(TEXT("HasBegunPlay")); }

        return FString::Join(Stages, TEXT(", "));
    }

    static FAutoConsoleCommand GDumpNotReadyCommand(
        TEXT("ck.EntityBridge.DumpNotReady"),
        TEXT("Logs every Ck Entity Bridge in a game world whose Entity is not Ready."),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            for (const auto* Bridge : TObjectRange<UCk_EntityBridge_ActorComponent_UE>())
            {
                if (ck::Is_NOT_Valid(Bridge) || Bridge->IsTemplate())
                { continue; }

                const auto* World = Bridge->GetWorld();

                if (ck::Is_NOT_Valid(World) || NOT World->IsGameWorld())
                { continue; }

                if (Bridge->Get_State() == ECk_EntityBridge_State::Ready)
                { continue; }

                const auto Entity = Bridge->Get_Entity();
                const auto StageText = Bridge->Get_State() == ECk_EntityBridge_State::Linked && ck::IsValid(Entity)
                    ? Get_ConstructionStageText(Entity)
                    : FString{};

                const auto* RequestedClass = Bridge->Get_RequestedEntityScriptClass().Get(nullptr).Get();

                ck::ecs::Display(TEXT("[EntityBridge] Actor [{}] State [{}] EntityScript [{}] Entity [{}] Stages [{}]"),
                    Bridge->GetOwner(), Bridge->Get_State(), RequestedClass, Entity, StageText);
            }
        }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityBridge_UE::
    Has(
        const FCk_Handle& InHandle)
    -> bool
{
    return InHandle.Has<ck::FFragment_EntityBridge>();
}

auto
    UCk_Utils_EntityBridge_UE::
    Ensure(
        const FCk_Handle& InHandle)
    -> bool
{
    CK_ENSURE_IF_NOT(Has(InHandle), TEXT("Entity [{}] does NOT have an EntityBridge Fragment! The Entity has no associated Actor"), InHandle)
    { return false; }

    return true;
}

auto
    UCk_Utils_EntityBridge_UE::
    TryGet_BridgedEntity_InOwnershipChain(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    return UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_If(InHandle,
    [](const FCk_Handle& InCandidate)
    {
        return Has(InCandidate);
    });
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_Actor(
        const FCk_Handle& InHandle)
    -> AActor*
{
    if (NOT Ensure(InHandle))
    { return {}; }

    return Get_BasicDetails(InHandle).Get_Actor().Get();
}

auto
    UCk_Utils_EntityBridge_UE::
    TryGet_Actor(
        const FCk_Handle& InHandle)
    -> AActor*
{
    if (NOT Has(InHandle))
    { return {}; }

    return Get_BasicDetails(InHandle).Get_Actor().Get();
}

auto
    UCk_Utils_EntityBridge_UE::
    TryGet_Actor_Recursive(
        const FCk_Handle& InHandle)
    -> AActor*
{
    const auto BridgedEntity = TryGet_BridgedEntity_InOwnershipChain(InHandle);

    if (ck::Is_NOT_Valid(BridgedEntity))
    { return {}; }

    return Get_Actor(BridgedEntity);
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_BasicDetails(
        const FCk_Handle& InHandle)
    -> FCk_EntityBridge_BasicDetails
{
    if (NOT Ensure(InHandle))
    { return {}; }

    constexpr auto EvenIfPendingKill = true;
    return FCk_EntityBridge_BasicDetails
    {
        InHandle.Get<ck::FFragment_EntityBridge, ck::IsValid_Policy_IncludePendingKill>().Get_Actor().Get(EvenIfPendingKill), InHandle
    };
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_BasicDetails_FromActor(
        const AActor* InActor)
    -> FCk_EntityBridge_BasicDetails
{
    const auto Entity = Get_Entity(InActor);

    if (ck::Is_NOT_Valid(Entity))
    { return {}; }

    return Get_BasicDetails(Entity);
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_Entity(
        const AActor* InActor)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor, ck::IsValid_Policy_IncludePendingKill{}),
        TEXT("Cannot get the Entity of an Actor because the Actor is invalid!"))
    { return {}; }

    const auto* Bridge = TryGet_Bridge(InActor);

    CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
        TEXT("Actor [{}] does NOT have a Ck Entity Bridge, so it has no Entity."),
        InActor)
    { return {}; }

    return Bridge->Get_Entity();
}

auto
    UCk_Utils_EntityBridge_UE::
    TryGet_Entity(
        const AActor* InActor)
    -> FCk_Handle
{
    const auto* Bridge = TryGet_Bridge(InActor);

    if (ck::Is_NOT_Valid(Bridge))
    { return {}; }

    return Bridge->Get_Entity();
}

auto
    UCk_Utils_EntityBridge_UE::
    TryGet_Bridge(
        const AActor* InActor)
    -> UCk_EntityBridge_ActorComponent_UE*
{
    if (ck::Is_NOT_Valid(InActor, ck::IsValid_Policy_IncludePendingKill{}))
    { return {}; }

    return InActor->FindComponentByClass<UCk_EntityBridge_ActorComponent_UE>();
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_HasEntity(
        const AActor* InActor)
    -> bool
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Cannot check whether an invalid Actor has an Entity"))
    { return {}; }

    const auto* Bridge = TryGet_Bridge(InActor);

    if (ck::Is_NOT_Valid(Bridge))
    { return false; }

    return ck::IsValid(Bridge->Get_Entity());
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_IsEntityReady(
        const AActor* InActor)
    -> bool
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Cannot check whether an invalid Actor's Entity is Ready"))
    { return {}; }

    const auto* Bridge = TryGet_Bridge(InActor);

    if (ck::Is_NOT_Valid(Bridge))
    { return false; }

    return Bridge->Get_State() == ECk_EntityBridge_State::Ready;
}

auto
    UCk_Utils_EntityBridge_UE::
    Promise_OnEntityConstructed(
        AActor* InActor,
        const FCk_Delegate_EntityScript_Constructed& InDelegate)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Promise_OnEntityConstructed called with an invalid Actor"))
    { return; }

    auto* Bridge = TryGet_Bridge(InActor);

    CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
        TEXT("Promise_OnEntityConstructed called on Actor [{}], which has no Ck Entity Bridge. Its Entity can never arrive."),
        InActor)
    { return; }

    Bridge->Promise_OnEntityConstructed(InDelegate);
}

auto
    UCk_Utils_EntityBridge_UE::
    Promise_OnEntityReady(
        AActor* InActor,
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Promise_OnEntityReady called with an invalid Actor"))
    { return; }

    auto* Bridge = TryGet_Bridge(InActor);

    CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
        TEXT("Promise_OnEntityReady called on Actor [{}], which has no Ck Entity Bridge. Its Entity can never arrive."),
        InActor)
    { return; }

    Bridge->Promise_OnEntityReady(InDelegate);
}

auto
    UCk_Utils_EntityBridge_UE::
    Promise_OnEntityReady(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Promise_OnEntityReady called with an invalid Actor"))
    { return; }

    CK_ENSURE_IF_NOT(static_cast<bool>(InCallback),
        TEXT("Promise_OnEntityReady called with an empty callback for Actor [{}]"), InActor)
    { return; }

    auto* Bridge = TryGet_Bridge(InActor);

    CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
        TEXT("Promise_OnEntityReady called on Actor [{}], which has no Ck Entity Bridge. Its Entity can never arrive."),
        InActor)
    { return; }

    if (Bridge->_State == ECk_EntityBridge_State::Ready)
    {
        InCallback(InActor, Bridge->_Entity);
        return;
    }

    Bridge->_PendingReady_Callbacks.Emplace(MoveTemp(InCallback));
}

auto
    UCk_Utils_EntityBridge_UE::
    Promise_OnEntityLinked(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActor), TEXT("Promise_OnEntityLinked called with an invalid Actor"))
    { return; }

    CK_ENSURE_IF_NOT(static_cast<bool>(InCallback),
        TEXT("Promise_OnEntityLinked called with an empty callback for Actor [{}]"), InActor)
    { return; }

    auto* Bridge = TryGet_Bridge(InActor);

    CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
        TEXT("Promise_OnEntityLinked called on Actor [{}], which has no Ck Entity Bridge. Its Entity can never arrive."),
        InActor)
    { return; }

    if (Bridge->_State == ECk_EntityBridge_State::Linked || Bridge->_State == ECk_EntityBridge_State::Ready)
    {
        InCallback(InActor, Bridge->_Entity);
        return;
    }

    Bridge->_PendingLinked_Callbacks.Emplace(MoveTemp(InCallback));
}

auto
    UCk_Utils_EntityBridge_UE::
    IsEqual(
        const FCk_EntityBridge_BasicDetails& InBasicDetailsA,
        const FCk_EntityBridge_BasicDetails& InBasicDetailsB)
    -> bool
{
    return InBasicDetailsA == InBasicDetailsB;
}

auto
    UCk_Utils_EntityBridge_UE::
    IsNotEqual(
        const FCk_EntityBridge_BasicDetails& InBasicDetailsA,
        const FCk_EntityBridge_BasicDetails& InBasicDetailsB)
    -> bool
{
    return InBasicDetailsA != InBasicDetailsB;
}

auto
    UCk_Utils_EntityBridge_UE::
    Get_Entity_FromSelf(
        const AActor* InActor)
    -> FCk_Handle
{
    return Get_Entity(InActor);
}

auto
    UCk_Utils_EntityBridge_UE::
    DoLink(
        FCk_Handle& InEntity,
        UCk_EntityBridge_ActorComponent_UE* InBridge)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InBridge), TEXT("Cannot link Entity [{}] to an invalid Ck Entity Bridge"), InEntity)
    { return; }

    auto* Actor = InBridge->GetOwner();

    CK_ENSURE_IF_NOT(ck::IsValid(Actor), TEXT("Cannot link Entity [{}]: its Ck Entity Bridge [{}] has no owning Actor"),
        InEntity, InBridge)
    { return; }

    InEntity.Add<ck::FFragment_EntityBridge>(InBridge);
    InEntity.AddOrGet<TWeakObjectPtr<UWorld>>(Actor->GetWorld());

    InBridge->_Entity = InEntity;
    InBridge->_State = ECk_EntityBridge_State::Linked;

    // Move out before executing: a callback that queues another promise must not mutate the container being iterated.
    const auto PendingLinkedCallbacks = MoveTemp(InBridge->_PendingLinked_Callbacks);
    InBridge->_PendingLinked_Callbacks.Reset();

    for (const auto& Callback : PendingLinkedCallbacks)
    {
        Callback(Actor, InEntity);
    }

    const auto PendingConstructedDelegates = MoveTemp(InBridge->_PendingConstructed_Delegates);
    InBridge->_PendingConstructed_Delegates.Reset();

    for (const auto& Delegate : PendingConstructedDelegates)
    {
        ck::UUtils_Signal_OnConstructed_PostFireUnbind::Bind(InEntity, Delegate, ECk_Signal_BindingPolicy::FireIfPayloadInFlight);
    }
}

auto
    UCk_Utils_EntityBridge_UE::
    DoRelease_Request(
        UCk_EntityBridge_ActorComponent_UE* InBridge)
    -> void
{
    InBridge->_RequestedEntityScriptClass.Reset();
}

auto
    UCk_Utils_EntityBridge_UE::
    DoNotify_EntityBegunPlay(
        const FCk_Handle& InEntity)
    -> void
{
    auto* Bridge = InEntity.Get<ck::FFragment_EntityBridge>().Get_Bridge().Get();

    if (ck::Is_NOT_Valid(Bridge))
    { return; }

    Bridge->_EntityHasBegunPlay = true;
    DoTryBecomeReady(Bridge);
}

auto
    UCk_Utils_EntityBridge_UE::
    DoTryBecomeReady(
        UCk_EntityBridge_ActorComponent_UE* InBridge)
    -> void
{
    if (InBridge->_State == ECk_EntityBridge_State::Ready)
    { return; }

    if (NOT InBridge->_EntityHasBegunPlay)
    { return; }

    if (DoGet_ShouldDeferUntilReplicationComplete(InBridge->_Entity))
    {
        DoBind_ReplicationCompleteTrampoline(InBridge, InBridge->_Entity);
        return;
    }

    InBridge->_State = ECk_EntityBridge_State::Ready;

    auto* Actor = InBridge->GetOwner();
    const auto Entity = InBridge->_Entity;

    const auto PendingReadyDelegates = MoveTemp(InBridge->_PendingReady_Delegates);
    const auto PendingReadyCallbacks = MoveTemp(InBridge->_PendingReady_Callbacks);
    InBridge->_PendingReady_Delegates.Reset();
    InBridge->_PendingReady_Callbacks.Reset();

    for (const auto& Delegate : PendingReadyDelegates)
    {
        Delegate.ExecuteIfBound(Actor, Entity);
    }

    for (const auto& Callback : PendingReadyCallbacks)
    {
        Callback(Actor, Entity);
    }
}

auto
    UCk_Utils_EntityBridge_UE::
    DoGet_ShouldDeferUntilReplicationComplete(
        const FCk_Handle& InEntity)
    -> bool
{
    // Host-gated, NOT authority-gated (Get_HasAuthority admits clients, Get_IsEntityNetMode_Host does
    // not): on the host replicated values are written locally, so there is nothing to await.
    if (UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(InEntity))
    { return false; }

    if (UCk_Utils_EntityReplicationDriver_UE::Has(InEntity))
    { return NOT UCk_Utils_EntityReplicationDriver_UE::Get_IsReplicationComplete(InEntity); }

    // No driver yet: fall back to the Entity's replication setting to decide whether OnReplicationComplete
    // will ever fire.
    return UCk_Utils_Net_UE::Has(InEntity) &&
        UCk_Utils_Net_UE::Get_Replication(InEntity) == ECk_Replication::Replicates;
}

auto
    UCk_Utils_EntityBridge_UE::
    DoBind_ReplicationCompleteTrampoline(
        UCk_EntityBridge_ActorComponent_UE* InBridge,
        const FCk_Handle& InEntity)
    -> void
{
    if (InBridge->_ReplicationCompleteTrampolineBound)
    { return; }

    auto Delegate = FCk_Delegate_EntityReplicationDriver_OnReplicationComplete{};
    Delegate.BindDynamic(InBridge, &UCk_EntityBridge_ActorComponent_UE::DoHandle_EntityReplicationComplete);

    auto Entity = InEntity;
    UCk_Utils_EntityReplicationDriver_UE::Promise_OnReplicationComplete(Entity, Delegate);

    InBridge->_ReplicationCompleteTrampolineBound = true;
}

// --------------------------------------------------------------------------------------------------------------------
