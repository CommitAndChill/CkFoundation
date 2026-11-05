#include "CkEntityLifetime_Utils.h"

#include "CkCore/Format/CkFormat.h" // ck::Format_UE — naming the entity a dropped payload belonged to
#include "CkCore/SharedValues/CkSharedValues.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/CkEcs_Stats.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/Delegates/CkDelegates.h"
#include "CkEcs/EditorSelectionOwner/CkEditorSelectionOwner_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h" // FFragment_EntityScript + FTag_EntityScript_HasBegunPlay (ConstructSpawned stamp)
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Persistence/CkPersistenceHydration.h" // entries queued on a dying entity are a counted loss
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Tag/CkTag_EditorOnly.h"
#include "CkEcs/Tag/CkTag_HydrationQuarantine.h" // leaving the quarantine is part of entering destruction

// --------------------------------------------------------------------------------------------------------------------

DECLARE_CYCLE_STAT(TEXT("EntityLifetime::Request_DestroyEntity"), STAT_CkEcs_Request_DestroyEntity, STATGROUP_CkEcs);
DECLARE_CYCLE_STAT(TEXT("EntityLifetime::Get_WorldForEntity"),    STAT_CkEcs_Get_WorldForEntity,    STATGROUP_CkEcs);
DECLARE_CYCLE_STAT(TEXT("EntityLifetime::Get_EntityNetMode"),     STAT_CkEcs_Get_EntityNetMode,     STATGROUP_CkEcs);

DECLARE_DWORD_COUNTER_STAT(TEXT("Ecs Entities Destroyed"), STAT_CkEcs_EntitiesDestroyed, STATGROUP_CkEcs);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ecs Entities Spawned"),   STAT_CkEcs_EntitiesSpawned,   STATGROUP_CkEcs);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_entity_lifetime_utils
{
    auto
        Get_CanCreateEntityInWorld(
            const FCk_Registry& InRegistry)
        -> bool
    {
        if (ck::Is_NOT_Valid(InRegistry))
        { return false; }

        const auto TransientEntity = FCk_Handle{InRegistry.Get_TransientEntity(), InRegistry.Get_RegistryHandle()};
        if (NOT TransientEntity.IsValid(ck::IsValid_Policy_IncludePendingKill{})
            || NOT TransientEntity.Has<TWeakObjectPtr<UWorld>>())
        { return true; }

        const auto* World = TransientEntity.Get<TWeakObjectPtr<UWorld>>().Get();
        if (ck::Is_NOT_Valid(World))
        { return true; }

        const auto* EcsWorld = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        return ck::Is_NOT_Valid(EcsWorld) || EcsWorld->Get_LoadHold() != ECk_EcsWorld_LoadHold::Teardown;
    }

    auto
        Get_HasDependentCount(
            const FCk_Handle& InOwner,
            const FCk_Handle& InDependent,
            int32 InExpectedCount)
        -> bool
    {
        if (ck::Is_NOT_Valid(InOwner, ck::IsValid_Policy_IncludePendingKill{}))
        { return false; }

        if (NOT InOwner.Has<ck::FFragment_LifetimeDependents>())
        { return InExpectedCount == 0; }

        auto Count = int32{0};
        for (const auto& Dependent : InOwner.Get<ck::FFragment_LifetimeDependents>().Get_Entities())
        {
            if (Dependent == InDependent)
            { ++Count; }
        }
        return Count == InExpectedCount;
    }

    // Entering destruction leaves the hydration quarantine, and that is what makes the 143 destruction-pipeline
    // processors need ZERO exemptions: a destroying entity is simply not quarantined, so every *_EndPlay body sees
    // it exactly as it does today. It also means an entity destroyed mid-load cannot hold the tag forever, since
    // the hydration dispatcher's view carries CK_IGNORE_PENDING_KILL and would keep draining it while nothing
    // released it. Paired with the count so the O(1) "is a quarantine active" gate stays truthful.
    auto
        DoLeave_HydrationQuarantine(
            FCk_Handle& InHandle)
        -> void
    {
        if (NOT InHandle.Try_Remove<ck::FTag_Hydration_Quarantine>())
        { return; }

        auto Registry = InHandle.Get_RegistryView();
        auto& Ctx = Registry.SetContext<ck::FCtx_HydrationQuarantine>();
        Ctx._Count = FMath::Max(0, Ctx._Count - 1);
    }

    // Both halves, at every site that stamps destroy-initiate.
    auto
        DoEnter_Destruction(
            FCk_Handle& InHandle)
        -> void
    {
        DoLeave_HydrationQuarantine(InHandle);

        // Owned by the persistence layer, because it is a statement about payload accounting rather than about
        // lifetime — and because the hydration dispatcher is its second caller (an apply that destroys its own
        // entity reaches it re-entrantly, and it defers until the dispatch is off the stack).
        ck::DoAbandon_PendingHydration(InHandle);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityLifetime_UE::
    Get_IsOwnershipChainValid(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy,
        const FCk_Handle& InExcludedEntity)
    -> bool
{
    auto Visited = TSet<FCk_Handle>{};
    auto Current = InHandle;
    while (true)
    {
        switch (InPendingKillPolicy)
        {
            case ECk_PendingKill_Policy::ExcludePendingKill:
            {
                if (ck::Is_NOT_Valid(Current))
                { return false; }
                break;
            }
            case ECk_PendingKill_Policy::IncludePendingKill:
            {
                if (ck::Is_NOT_Valid(Current, ck::IsValid_Policy_IncludePendingKill{}))
                { return false; }
                break;
            }
            default:
            { return false; }
        }

        if (Current == InExcludedEntity || Visited.Contains(Current))
        { return false; }

        Visited.Add(Current);
        if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
        { return true; }

        Current = Current.Get<ck::FFragment_LifetimeOwner, ck::IsValid_Policy_IncludePendingKill>().Get_Entity();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityLifetime_UE::
    Get_CanCreateEntity(
        const FCk_Handle& InOwner)
    -> bool
{
    if (ck::Is_NOT_Valid(InOwner))
    { return false; }

    return ck_entity_lifetime_utils::Get_CanCreateEntityInWorld(InOwner.Get_RegistryView());
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_DestroyEntity(
        FCk_Handle& InHandle,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior)
    -> void
{
    SCOPE_CYCLE_COUNTER(STAT_CkEcs_Request_DestroyEntity);

    if (ck::Is_NOT_Valid(InHandle))
    { return; }

    if (InHandle.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>())
    { return; }

    switch(InDestructionBehavior)
    {
        case ECk_EntityLifetime_DestructionBehavior::ForceDestroy:
        {
            break;
        }
        case ECk_EntityLifetime_DestructionBehavior::DestroyOnlyIfOrphan:
        {
            if (NOT InHandle.Orphan())
            { return; }

            break;
        }
        default:
        {
            CK_INVALID_ENUM(InDestructionBehavior);
            return;
        }
    }

    ck::ecs::VeryVerbose(TEXT("Entity [{}] set to 'Initiate Destruction'"), InHandle);
    InHandle.AddOrGet<ck::FTag_DestroyEntity_Initiate>();
    ck_entity_lifetime_utils::DoEnter_Destruction(InHandle);
    INC_DWORD_STAT(STAT_CkEcs_EntitiesDestroyed);

    auto LifetimeDependents = Get_LifetimeDependents(InHandle);
    {
        QUICK_SCOPE_CYCLE_COUNTER(EntityLifetime_RequestDestroyDependents)
        Request_DestroyEntities(LifetimeDependents);
    }

    // broadcast AFTER walking the dependents since destruction order should be leaf-to-root
    if (ck::UUtils_Signal_OnEntityBeginDestroy::Has(InHandle))
    {
        QUICK_SCOPE_CYCLE_COUNTER(EntityLifetime_OnBeginDestroyCallbacks)
        ck::UUtils_Signal_OnEntityBeginDestroy::Broadcast(InHandle, ck::MakePayload(InHandle));
    }
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_DestroyEntities(
        TArray<FCk_Handle>& InHandles,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(Request_Destroy_Entities)
    for (auto& Handle : InHandles)
    {
        Request_DestroyEntity(Handle, InDestructionBehavior);
    }
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    return Request_CreateEntity(InHandle, [](auto){});
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity_TransientOwner(
        const UObject* InWorldContextObject)
    -> FCk_Handle
{
    return Request_CreateEntity_TransientOwner(InWorldContextObject, PostEntityCreatedFunc{});
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeOwner(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(
        (InPendingKillPolicy == ECk_PendingKill_Policy::ExcludePendingKill && ck::IsValid(InHandle)) ||
        (InPendingKillPolicy == ECk_PendingKill_Policy::IncludePendingKill &&
            ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{})),
        TEXT("Cannot get lifetime ownership for an invalid Entity or pending-kill policy"))
    { return {}; }

    CK_ENSURE_IF_NOT(InHandle.Has<ck::FFragment_LifetimeOwner>(),
        TEXT("The Entity [{}] does NOT have a LifetimeOwner. Was this Entity created by Request_CreateEntity(RegistryType)?"),
        InHandle)
    { return {}; }

    switch(InPendingKillPolicy)
    {
        case ECk_PendingKill_Policy::ExcludePendingKill:
        {
            return InHandle.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        }
        case ECk_PendingKill_Policy::IncludePendingKill:
        {
            return InHandle.Get<ck::FFragment_LifetimeOwner, ck::IsValid_Policy_IncludePendingKill>().Get_Entity();
        }
        default:
        {
            CK_INVALID_ENUM(InPendingKillPolicy);
            return {};
        }
    }
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeDependents(
        const FCk_Handle& InHandle)
    -> TArray<FCk_Handle>
{
    if (NOT InHandle.Has<ck::FFragment_LifetimeDependents>())
    { return {}; }

    const auto& Dependents = InHandle.Get<ck::FFragment_LifetimeDependents>();

    auto Ret = TArray<FCk_Handle>{};

    for (const auto Dependent : Dependents.Get_Entities())
    {
        // The owner's dependents array is deliberately never pruned (perf) — stale entries are
        // skipped here instead.
        //
        // NOT ck::IsValid(...) rather than ck::Is_NOT_Valid(...): this is the two-argument
        // (entity, context) overload, which only ck::IsValid provides — Is_NOT_Valid's second
        // parameter is the validation POLICY, so the handle would bind there instead.
        if (NOT ck::IsValid(Dependent, InHandle))
        { continue; }

        Ret.Emplace(Dependent);
    }

    return Ret;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_IsPendingDestroy(
        const FCk_Handle& InHandle,
        ECk_EntityLifetime_DestructionPhase InDestructionPhase)
    -> bool
{
    switch(InDestructionPhase)
    {
        case ECk_EntityLifetime_DestructionPhase::BeginDestroy:
        {
            return InHandle.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay>();
        }
        case ECk_EntityLifetime_DestructionPhase::Teardown:
        {
            return InHandle.Has_Any<ck::FTag_DestroyEntity_Teardown>();
        }
        case ECk_EntityLifetime_DestructionPhase::Destroyed:
        {
            return InHandle.Has_Any<ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>();
        }
        default:
        {
            CK_INVALID_ENUM(InDestructionPhase);
            return {};
        }
    }
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_IsTransientEntity(
        const FCk_Handle& InHandle)
    -> bool
{
    if (ck::Is_NOT_Valid(InHandle))
    { return {}; }

    return Get_TransientEntity(InHandle.Get_RegistryView()) == InHandle;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_IsInsideConstructionWindow(
        const FCk_Handle& InHandle)
    -> bool
{
    if (ck::Is_NOT_Valid(InHandle))
    { return false; }

    return (InHandle.Has<ck::FFragment_EntityScript>() &&
            NOT InHandle.Has<ck::FTag_EntityScript_HasBegunPlay>()) ||
           InHandle.Has<ck::FTag_DefinitionBuild_InProgress>();
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_WorldForEntity(
        const FCk_Handle& InHandle)
    -> UWorld*
{
    SCOPE_CYCLE_COUNTER(STAT_CkEcs_Get_WorldForEntity);

    CK_ENSURE_IF_NOT(ck::IsValid(InHandle, ck::IsValid_Policy_IncludePendingKill{}),
        TEXT("Cannot resolve World for an invalid Entity"))
    { return {}; }

    if (InHandle.Has<TWeakObjectPtr<UWorld>>())
    { return InHandle.Get<TWeakObjectPtr<UWorld>>().Get(); }

    CK_ENSURE_IF_NOT(NOT Get_IsTransientEntity(InHandle),
        TEXT("Failed to find a valid World reference while going up an Entity lifetime ownership chain!\n"
             "The Transient [{}] is expected to always have a valid reference to the current Game World! "),
        InHandle)
    {  return {}; }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InHandle, ECk_PendingKill_Policy::IncludePendingKill),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while resolving World for [{}]"), InHandle)
    { return {}; }

    const auto LifeTimeOwner = Get_LifetimeOwner(InHandle, ECk_PendingKill_Policy::IncludePendingKill);
    if (ck::Is_NOT_Valid(LifeTimeOwner, ck::IsValid_Policy_IncludePendingKill{}))
    { return {}; }

    return Get_WorldForEntity(LifeTimeOwner);
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_EntityInOwnershipChain_If(
        FCk_Handle& InHandle,
        const FInstancedStruct& InOptionalPayload,
        const FCk_Predicate_InHandle_OutResult& InPredicate)
    -> FCk_Handle
{
    return Get_EntityInOwnershipChain_If(InHandle,
    [&](const FCk_Handle& InAttribute)  -> bool
    {
        const FCk_SharedBool PredicateResult;

        if (InPredicate.IsBound())
        {
            InPredicate.Execute(InAttribute, PredicateResult, InOptionalPayload);
        }

        return *PredicateResult;
    });
}

auto
    UCk_Utils_EntityLifetime_UE::
    BindTo_OnBeginDestroy(
        FCk_Handle& InHandle,
        const FCk_Delegate_OnBeginDestroy& InDelegate,
        ECk_Signal_BindingPolicy InBehavior,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> void
{
    ck::UUtils_Signal_OnEntityBeginDestroy::Bind(InHandle, InDelegate, InBehavior);
}

auto
    UCk_Utils_EntityLifetime_UE::
    UnbindFrom_OnBeginDestroy(
        FCk_Handle& InHandle,
        const FCk_Delegate_OnBeginDestroy& InDelegate)
    -> void
{
    ck::UUtils_Signal_OnEntityBeginDestroy::Unbind(InHandle, InDelegate);
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_EntityNetRole(
        const FCk_Handle& InEntity)
    -> ECk_Net_EntityNetRole
{
    if (ck::Is_NOT_Valid(InEntity))
    { return ECk_Net_EntityNetRole::None; }

    if (InEntity.Has<ck::FFragment_Net_Params>())
    { return InEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings().Get_NetRole(); }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InEntity, ECk_PendingKill_Policy::IncludePendingKill),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while resolving net role for [{}]"), InEntity)
    { return ECk_Net_EntityNetRole::None; }

    if (NOT InEntity.Has<ck::FFragment_LifetimeOwner>())
    { return ECk_Net_EntityNetRole::None; }

    return Get_EntityNetRole(Get_LifetimeOwner(InEntity));
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_EntityNetMode(
        const FCk_Handle& InEntity)
    -> ECk_Net_NetModeType
{
    SCOPE_CYCLE_COUNTER(STAT_CkEcs_Get_EntityNetMode);

    if (ck::Is_NOT_Valid(InEntity))
    { return ECk_Net_NetModeType::Unknown; }

    if (InEntity.Has<ck::FFragment_Net_Params>())
    { return InEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings().Get_NetMode(); }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InEntity, ECk_PendingKill_Policy::IncludePendingKill),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while resolving net mode for [{}]"), InEntity)
    { return ECk_Net_NetModeType::Unknown; }

    if (NOT InEntity.Has<ck::FFragment_LifetimeOwner>())
    { return ECk_Net_NetModeType::Unknown; }

    return Get_EntityNetMode(Get_LifetimeOwner(InEntity));
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity_TransientOwner(
        const UObject* InWorldContextObject,
        PostEntityCreatedFunc InFunc)
    -> HandleType
{
    CK_ENSURE_IF_NOT(ck::IsValid(InWorldContextObject), TEXT("Cannot create new Entity because an INVALID WorldContextObject was passed in"))
    { return {}; }

    const auto& TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorldContextObject->GetWorld());

    auto NewEntityWithTransientOwner = Request_CreateEntity(TransientEntity, InFunc);
    if (ck::Is_NOT_Valid(NewEntityWithTransientOwner))
    { return {}; }

    UCk_Utils_Handle_UE::Set_DebugName(NewEntityWithTransientOwner, TEXT("NO NAME [Transient Owner]"));

    return NewEntityWithTransientOwner;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity(
        const FCk_Handle& InHandle,
        PostEntityCreatedFunc InFunc)
    -> HandleType
{
    QUICK_SCOPE_CYCLE_COUNTER(Request_Create_Entity)

    CK_ENSURE_IF_NOT(ck::IsValid(InHandle), TEXT("Cannot create Entity with Invalid Handle"))
    { return {}; }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InHandle, ECk_PendingKill_Policy::IncludePendingKill),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while creating a child of [{}]"), InHandle)
    { return {}; }

    auto RegistryView = InHandle.Get_RegistryView();
    const auto NewEntity = Request_CreateEntity(RegistryView, [&](FCk_Handle InNewEntity)
    {
        Request_SetupEntityWithLifetimeOwner(InNewEntity, InHandle);

        if (InFunc)
        {
            InFunc(InNewEntity);
        }
    });

    return NewEntity;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity(
        RegistryType& InRegistry,
        PostEntityCreatedFunc InFunc)
    -> HandleType
{
    QUICK_SCOPE_CYCLE_COUNTER(Request_Create_Entity)

    CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_CanCreateEntityInWorld(InRegistry),
        TEXT("Request_CreateEntity rejected new world population after ECS world teardown began"))
    { return {}; }

    const auto& NewEntity = InRegistry.CreateEntity();
    INC_DWORD_STAT(STAT_CkEcs_EntitiesSpawned);

    auto NewEntityHandle = HandleType{ NewEntity, InRegistry.Get_RegistryHandle() };
    UCk_Utils_Handle_UE::Set_DebugName(NewEntityHandle, TEXT("NO NAME"));
    NewEntityHandle.Add<ck::FTag_EntityJustCreated>();

    if (InFunc)
    {
        InFunc(NewEntityHandle);
    }

    return NewEntityHandle;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity(
        RegistryType& InRegistry,
        const EntityIdHint& InEntityHint,
        PostEntityCreatedFunc InFunc)
    -> HandleType
{
    QUICK_SCOPE_CYCLE_COUNTER(Request_Create_Entity)

    CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_CanCreateEntityInWorld(InRegistry),
        TEXT("Request_CreateEntity with ID hint rejected new world population after ECS world teardown began"))
    { return {}; }

    const auto& NewEntity = InRegistry.CreateEntity(InEntityHint.Get_Entity());
    INC_DWORD_STAT(STAT_CkEcs_EntitiesSpawned);

    auto NewEntityHandle = HandleType{ NewEntity, InRegistry.Get_RegistryHandle() };
    NewEntityHandle.Add<ck::FTag_EntityJustCreated>();

    if (InFunc)
    {
        InFunc(NewEntityHandle);
    }

    return NewEntityHandle;
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_TransientEntity(
        const RegistryType& InRegistry)
    -> HandleType
{
    return HandleType{InRegistry.Get_TransientEntity(), InRegistry.Get_RegistryHandle()};
}

auto
    UCk_Utils_EntityLifetime_UE::
    Get_TransientEntity(
        const HandleType& InHandle)
    -> HandleType
{
    // The early-out keeps a default-constructed input from comparing equal to a
    // default-constructed transient handle.
    if (ck::Is_NOT_Valid(InHandle))
    { return {}; }

    return Get_TransientEntity(InHandle.Get_RegistryView());
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_SetupEntityWithLifetimeOwner(
        FCk_Handle& InNewEntity,
        const FCk_Handle& InLifetimeOwner)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InNewEntity), TEXT("Cannot setup lifetime ownership for an invalid Entity"))
    { return; }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InLifetimeOwner,
        ECk_PendingKill_Policy::IncludePendingKill, InNewEntity),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while setting up [{}] with [{}]"),
        InNewEntity, InLifetimeOwner)
    { return; }

    if (InNewEntity.Has<ck::FFragment_LifetimeOwner>())
    {
        CK_ENSURE_IF_NOT(InNewEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InLifetimeOwner,
            TEXT("Trying to Setup Entity [{}] with LifetimeOwner [{}] but it is already setup with [{}]"),
            InNewEntity, InLifetimeOwner, InNewEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity())
        { return; }

        CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_HasDependentCount(InLifetimeOwner, InNewEntity,
            InNewEntity.Get_RegistryView().Get_RegistryHandle() == InLifetimeOwner.Get_RegistryView().Get_RegistryHandle() ? 1 : 0),
            TEXT("Lifetime ownership reverse link is inconsistent for [{}] and [{}]"), InNewEntity, InLifetimeOwner)
        { return; }

        return;
    }

    CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_HasDependentCount(InLifetimeOwner, InNewEntity, 0),
        TEXT("Lifetime ownership reverse link already exists before setup for [{}] and [{}]"), InNewEntity, InLifetimeOwner)
    { return; }

    InNewEntity.Add<ck::FFragment_LifetimeOwner>(InLifetimeOwner);

    // Provenance stamp: a child born inside the owner's construction window is part of the owner's deterministic
    // build and is re-created by the owner's replayed construction on load, so the save adopts it by identity
    // rather than respawning a recipe. No stamp → RuntimeSpawned by default. See ck::FTag_ConstructSpawned.
    if (Get_IsInsideConstructionWindow(InLifetimeOwner))
    { InNewEntity.Add<ck::FTag_ConstructSpawned>(); }

    if (UCk_Utils_ContextOwner_UE::Has(InLifetimeOwner))
    {
        const auto& LifetimeOwnerContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(InLifetimeOwner);
        UCk_Utils_ContextOwner_UE::Request_SetupEntityWithContextOwner(InNewEntity, LifetimeOwnerContextOwner);
    }
    else if (Get_IsTransientEntity(InLifetimeOwner))
    {
        UCk_Utils_ContextOwner_UE::Request_SetupEntityWithContextOwner(InNewEntity, InNewEntity);
    }

#if WITH_EDITOR
    // Inherited the same way as ContextOwner — see FFragment_EditorSelectionOwner.
    if (UCk_Utils_EditorSelectionOwner_UE::Has(InLifetimeOwner))
    {
        UCk_Utils_EditorSelectionOwner_UE::Request_SetupEntityWithEditorSelectionOwner(InNewEntity,
            UCk_Utils_EditorSelectionOwner_UE::TryGet_SelectionOwnerWeak(InLifetimeOwner));
    }
#endif

#if NOT CK_DISABLE_NET_PARAM_COPY_PER_ENTITY
    // If we copy NetParams from a TransientEntity, we give the wrong impression to the Entity that it can
    // replicate. But we don't have an Actor channel to use for replication.
    if (NOT Get_IsTransientEntity(InLifetimeOwner) && InLifetimeOwner.Has<ck::FFragment_Net_Params>())
    {
        const auto& ConnectionSettings = InLifetimeOwner.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
        if (ConnectionSettings.Get_NetRole() == ECk_Net_EntityNetRole::Authority)
        {
            InNewEntity.Add<ck::FTag_HasAuthority>();
        }

        switch(ConnectionSettings.Get_NetMode())
        {
            case ECk_Net_NetModeType::Unknown: break;
            case ECk_Net_NetModeType::Client:
            {
                InNewEntity.Add<ck::FTag_NetMode_IsClient>();
                break;
            }
            case ECk_Net_NetModeType::Host:
            {
                InNewEntity.Add<ck::FTag_NetMode_IsHost>();
                break;
            }
            case ECk_Net_NetModeType::ClientAndHost:
            {
                InNewEntity.Add<ck::FTag_NetMode_IsHost>();
                InNewEntity.Add<ck::FTag_NetMode_IsClient>();
                break;
            }
        }

        InNewEntity.Add<ck::FFragment_Net_Params>(InLifetimeOwner.Get<ck::FFragment_Net_Params>());
    }
#endif

    if (InLifetimeOwner.Has<ck::FTag_EditorOnlyEntity>())
    { InNewEntity.Add<ck::FTag_EditorOnlyEntity>(); }

    // Register the reverse dependent link ONLY for a same-registry child: FFragment_LifetimeDependents (and its
    // serialization) assumes same-registry handles, and a restored foreign handle re-homes onto an unrelated
    // entity — including the owner itself, forming a self-cycle that stack-overflows the dependents walk.
    const auto SameRegistry =
        InNewEntity.Get_RegistryView().Get_RegistryHandle() == InLifetimeOwner.Get_RegistryView().Get_RegistryHandle();
    if (SameRegistry)
    {
        // Not doing something like this because it is undefined behavior: *const_cast<FCk_Handle*>(&InHandle)
        auto NonConstLifetimeOwnerHandle = InLifetimeOwner;
        NonConstLifetimeOwnerHandle.AddOrGet<ck::FFragment_LifetimeDependents,
            ck::IsValid_Policy_IncludePendingKill>()._Entities.Emplace(InNewEntity);
    }

    // Finish ownership and inherited fragments before Teardown makes the child invalid to regular mutations.
    if (InLifetimeOwner.Has_Any<ck::FTag_DestroyEntity_Initiate>())
    {
        InNewEntity.AddOrGet<ck::FTag_DestroyEntity_Initiate>();
        ck_entity_lifetime_utils::DoEnter_Destruction(InNewEntity);
    }

    if (InLifetimeOwner.Has_Any<ck::FTag_DestroyEntity_Teardown>())
    { InNewEntity.AddOrGet<ck::FTag_DestroyEntity_Teardown>(); }
}

auto
    UCk_Utils_EntityLifetime_UE::
    Request_TransferLifetimeOwner(
        FCk_Handle& InEntity,
        const FCk_Handle& InNewLifetimeOwner)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(InEntity), TEXT("Cannot transfer lifetime ownership for an invalid Entity"))
    { return; }

    CK_ENSURE_IF_NOT(InEntity.Has<ck::FFragment_LifetimeOwner>(),
        TEXT("Cannot transfer lifetime ownership for [{}] without an existing owner"), InEntity)
    { return; }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InEntity, ECk_PendingKill_Policy::IncludePendingKill),
        TEXT("Lifetime ownership would contain an invalid entity or cycle in the current chain of [{}]"), InEntity)
    { return; }

    CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(InNewLifetimeOwner,
        ECk_PendingKill_Policy::IncludePendingKill, InEntity),
        TEXT("Lifetime ownership would contain an invalid entity or cycle while transferring [{}] to [{}]"),
        InEntity, InNewLifetimeOwner)
    { return; }

    auto CurrentLifetimeOwnerEntity = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
    const auto OldOwnerSameRegistry =
        InEntity.Get_RegistryView().Get_RegistryHandle() == CurrentLifetimeOwnerEntity.Get_RegistryView().Get_RegistryHandle();
    const auto NewOwnerSameRegistry =
        InEntity.Get_RegistryView().Get_RegistryHandle() == InNewLifetimeOwner.Get_RegistryView().Get_RegistryHandle();

    CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_HasDependentCount(CurrentLifetimeOwnerEntity,
        InEntity, OldOwnerSameRegistry ? 1 : 0),
        TEXT("Lifetime ownership reverse link is inconsistent for [{}] and [{}]"), InEntity, CurrentLifetimeOwnerEntity)
    { return; }

    if (InNewLifetimeOwner == CurrentLifetimeOwnerEntity)
    { return; }

    CK_ENSURE_IF_NOT(ck_entity_lifetime_utils::Get_HasDependentCount(InNewLifetimeOwner, InEntity, 0),
        TEXT("Lifetime ownership reverse link already exists before transfer for [{}] and [{}]"), InEntity, InNewLifetimeOwner)
    { return; }

    if (OldOwnerSameRegistry)
    {
        CurrentLifetimeOwnerEntity.Get<ck::FFragment_LifetimeDependents,
            ck::IsValid_Policy_IncludePendingKill>()._Entities.RemoveSingle(InEntity);
    }

    InEntity.Replace<ck::FFragment_LifetimeOwner>(InNewLifetimeOwner);

    if (NewOwnerSameRegistry)
    {
        auto NonConstNewLifetimeOwnerHandle = InNewLifetimeOwner;
        NonConstNewLifetimeOwnerHandle.AddOrGet<ck::FFragment_LifetimeDependents,
            ck::IsValid_Policy_IncludePendingKill>()._Entities.Emplace(InEntity);
    }

    if (InNewLifetimeOwner.Has_Any<ck::FTag_DestroyEntity_Initiate>())
    {
        InEntity.AddOrGet<ck::FTag_DestroyEntity_Initiate>();
        ck_entity_lifetime_utils::DoEnter_Destruction(InEntity);
    }

    if (InNewLifetimeOwner.Has_Any<ck::FTag_DestroyEntity_Teardown>())
    { InEntity.AddOrGet<ck::FTag_DestroyEntity_Teardown>(); }
}

// --------------------------------------------------------------------------------------------------------------------
