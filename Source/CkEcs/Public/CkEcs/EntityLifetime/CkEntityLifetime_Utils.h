#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Net/CkNet_Common.h"
#include "CkEcs/Net/CkNet_Fragment_Data.h"
#include "CkEcs/Delegates/CkDelegates.h"

#include <CoreMinimal.h>

#include "CkEntityLifetime_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle"))
class CKECS_API UCk_Utils_EntityLifetime_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_EntityLifetime_UE);

    friend struct ck::FFragment_LifetimeOwner;

public:
    using RegistryType          = FCk_Registry;
    using EntityType            = FCk_Entity;
    using HandleType            = FCk_Handle;
    using PostEntityCreatedFunc = TFunction<void(FCk_Handle&)>;

public:
    struct EntityIdHint
    {
        CK_GENERATED_BODY(EntityIdHint);

    private:
        EntityType _Entity;

    public:
        CK_PROPERTY_GET(_Entity);
        CK_DEFINE_CONSTRUCTORS(EntityIdHint, _Entity);
    };

public:
    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][Lifetime] Request Destroy Entity",
              Category = "Ck|Utils|Lifetime")
    static void
    Request_DestroyEntity(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior = ECk_EntityLifetime_DestructionBehavior::ForceDestroy);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][Lifetime] Request Destroy Entities",
              Category = "Ck|Utils|Lifetime")
    static void
    Request_DestroyEntities(
        UPARAM(ref) TArray<FCk_Handle>& InHandles,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior = ECk_EntityLifetime_DestructionBehavior::ForceDestroy);

    /** Admission preflight for a new child; false for invalid owners or world teardown. Does not allocate. */
    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Get Can Create Entity",
              Category = "Ck|Utils|Lifetime")
    static bool
    Get_CanCreateEntity(
        const FCk_Handle& InOwner);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][Lifetime] Request Create New Entity",
              Category = "Ck|Utils|Lifetime",
              meta = (WorldContext = "InWorldContextObject"))
    static FCk_Handle
    Request_CreateEntity(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][Lifetime] Request Create New Entity (Transient Owner)",
              Category = "Ck|Utils|Lifetime",
              meta = (WorldContext = "InWorldContextObject"))
    static FCk_Handle
    Request_CreateEntity_TransientOwner(
        const UObject* InWorldContextObject);

public:
    /*
    * NOT added by design. Every Entity, except the top-level Transient Entity, should have a lifetime owner.
    * To check whether we 'are' the top-level Entity, get the transient entity from the Subsystem and compare
    * OR use the `Get_TransientEntity(...)` found in this utils
    *
    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Has Entity Lifetime Owner",
              Category = "Ck|Utils|Lifetime")
    static FCk_Handle
    Get_HasLifetimeOwner(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy = ECk_PendingKill_Policy::ExcludePendingKill);
    */

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Get Entity Lifetime Owner",
              Category = "Ck|Utils|Lifetime")
    static FCk_Handle
    Get_LifetimeOwner(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy = ECk_PendingKill_Policy::ExcludePendingKill);

    /** The live direct dependents, in attachment order. Allocates; C++ callers that only need to visit them
     *  use ForEach_LifetimeDependent instead. */
    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Get Entity Lifetime Dependents",
              Category = "Ck|Utils|Lifetime")
    static TArray<FCk_Handle>
    Get_LifetimeDependents(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Get Is Entity Pending Destroy",
              Category = "Ck|Utils|Lifetime")
    static bool
    Get_IsPendingDestroy(
        const FCk_Handle& InHandle,
        ECk_EntityLifetime_DestructionPhase InDestructionPhase);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Is Transient Entity?",
              Category = "Ck|Utils|Lifetime")
    static bool
    Get_IsTransientEntity(
        const FCk_Handle& InHandle);

    // True while the entity is inside its own construction window: an EntityScript that has not begun play,
    // or a definition build in progress. This is the FTag_ConstructSpawned provenance-stamp condition AND the
    // load-gate spawn-admission condition (a replayed construction re-creating its deterministic build) — one
    // predicate, so the two sites cannot drift.
    static bool
    Get_IsInsideConstructionWindow(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][Lifetime] Get World For Entity",
              Category = "Ck|Utils|Lifetime")
    static UWorld*
    Get_WorldForEntity(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Lifetime",
              DisplayName = "[Ck][Lifetime] Get Entity In Ownership Chain If",
              meta=(AutoCreateRefTerm="InOptionalPayload, InDelegate"))
    static FCk_Handle
    Get_EntityInOwnershipChain_If(
        UPARAM(ref) FCk_Handle& InHandle,
        const FInstancedStruct& InOptionalPayload,
        const FCk_Predicate_InHandle_OutResult& InPredicate);

public:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Lifetime",
              DisplayName = "[Ck][Lifetime] Bind To OnEntityBeginDestroy")
    static void
    BindTo_OnBeginDestroy(
        UPARAM(ref) FCk_Handle& InHandle,
        const FCk_Delegate_OnBeginDestroy& InDelegate,
        ECk_Signal_BindingPolicy InBehavior = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Lifetime",
              DisplayName = "[Ck][Lifetime] Unbind From OnEntityBeginDestroy")
    static void
    UnbindFrom_OnBeginDestroy(
        UPARAM(ref) FCk_Handle& InHandle,
        const FCk_Delegate_OnBeginDestroy& InDelegate);

public:
    // These functions already exist on Net_Utils (which are now a pass-through). The reason they exist on this
    // utils as well is because CkEcs cannot depend on CkNet
    [[nodiscard]]
    static auto
    Get_EntityNetRole(
        const FCk_Handle& InEntity) -> ECk_Net_EntityNetRole;

    [[nodiscard]]
    static auto
    Get_EntityNetMode(
        const FCk_Handle& InEntity) -> ECk_Net_NetModeType;

public:
    template <typename T_Predicate>
    [[nodiscard]]
    static auto
    Get_EntityInOwnershipChain_If(
        const FCk_Handle& InHandle,
        T_Predicate T_Func,
        ECk_PendingKill_Policy InPendingKillPolicy = ECk_PendingKill_Policy::ExcludePendingKill) -> FCk_Handle;

    /** The nearest entity in the ownership chain (starting at InHandle itself) carrying every listed fragment. */
    template <typename... T_Fragment>
    [[nodiscard]]
    static auto
    Get_EntityInOwnershipChain_With(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy = ECk_PendingKill_Policy::ExcludePendingKill) -> FCk_Handle;

    /**
     * Visits every live direct dependent of InHandle, in attachment order, without allocating.
     *
     * The next dependent is read before InFunc runs, so InFunc may destroy or transfer the dependent it is handed.
     * It must not reparent or destroy OTHER dependents of this owner: the walk would continue into whichever list
     * those links now belong to.
     */
    template <typename T_Func>
    static auto
    ForEach_LifetimeDependent(
        const FCk_Handle& InHandle,
        T_Func InFunc) -> void;

    /**
     * Visits every live descendant of InHandle in post-order (dependents before their owner, siblings in attachment
     * order) — the same leaf-to-root order destruction uses. Same mutation contract as ForEach_LifetimeDependent.
     */
    template <typename T_Func>
    static auto
    ForEach_LifetimeDescendant(
        const FCk_Handle& InHandle,
        T_Func InFunc) -> void;

    /** The first live direct dependent satisfying InPredicate, or an invalid handle. */
    template <typename T_Predicate>
    [[nodiscard]]
    static auto
    Get_LifetimeDependent_If(
        const FCk_Handle& InHandle,
        T_Predicate InPredicate) -> FCk_Handle;

    /** The first live descendant satisfying InPredicate in pre-order (an owner before its dependents), or an
     *  invalid handle. */
    template <typename T_Predicate>
    [[nodiscard]]
    static auto
    Get_LifetimeDescendant_If(
        const FCk_Handle& InHandle,
        T_Predicate InPredicate) -> FCk_Handle;

    /** The first live descendant carrying every listed fragment, in pre-order, or an invalid handle. */
    template <typename... T_Fragment>
    [[nodiscard]]
    static auto
    Get_LifetimeDescendant_With(
        const FCk_Handle& InHandle) -> FCk_Handle;

    /** Number of live direct dependents. Walks the list; does not allocate. */
    [[nodiscard]]
    static auto
    Get_LifetimeDependentsCount(
        const FCk_Handle& InHandle) -> int32;

    template <typename T_TypeSafeHandle>
    [[nodiscard]]
    static auto
    Get_LifetimeOwner_AsTypeSafe(
        const FCk_Handle& InHandle) -> T_TypeSafeHandle;

    template <typename T_TypeSafeHandle>
    [[nodiscard]]
    static auto
    Request_CreateEntity_AsTypeSafe(
        const FCk_Handle& InHandle) -> T_TypeSafeHandle;

public:
    [[nodiscard]]
    static auto
    Request_CreateEntity_TransientOwner(
        const UObject* InWorldContextObject,
        PostEntityCreatedFunc InFunc) -> HandleType;

    [[nodiscard]]
    static auto
    Request_CreateEntity(
        const FCk_Handle& InHandle,
        PostEntityCreatedFunc InFunc) -> HandleType;

    [[nodiscard]]
    static auto
    Request_CreateEntity(
        RegistryType& InRegistry,
        PostEntityCreatedFunc InFunc = PostEntityCreatedFunc{}) -> HandleType;

    [[nodiscard]]
    static auto
    Request_CreateEntity(
        RegistryType& InRegistry,
        const EntityIdHint& InEntityHint,
        PostEntityCreatedFunc InFunc = PostEntityCreatedFunc{}) -> HandleType;

    static auto
    Request_SetupEntityWithLifetimeOwner(
        FCk_Handle& InNewEntity,
        const FCk_Handle& InLifetimeOwner) -> void;

    static auto
    Request_TransferLifetimeOwner(
        FCk_Handle& InEntity,
        const FCk_Handle& InNewLifetimeOwner) -> void;

public:
    [[nodiscard]]
    static auto
    Get_TransientEntity(
        const RegistryType& InRegistry) -> HandleType;

    [[nodiscard]]
    static auto
    Get_TransientEntity(
        const HandleType& InHandle) -> HandleType;

private:
    static auto
    Get_IsOwnershipChainValid(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy,
        const FCk_Handle& InExcludedEntity = {}) -> bool;

    // The reverse link is the dependent's membership in its owner's intrusive dependents list. Both are O(1):
    // linking appends, unlinking stitches the neighbours around the dependent and clears its own links.
    static auto
    DoLink_LifetimeDependent(
        FCk_Handle& InDependent,
        const FCk_Handle& InLifetimeOwner) -> void;

    static auto
    DoUnlink_LifetimeDependent(
        FCk_Handle& InDependent) -> void;

    // Structural O(1) answer to "is InDependent currently in InOwner's list" — the former linear count.
    static auto
    Get_IsLinkedAsLifetimeDependent(
        const FCk_Handle& InOwner,
        const FCk_Handle& InDependent) -> bool;

    // Advances a list walk from InLinked to its successor, ensuring on a link that no longer names a live entity
    // (the on_destroy unlink makes that impossible; a hit means the links were corrupted).
    static auto
    Get_NextLinkedLifetimeDependent(
        const FCk_Handle& InOwner,
        const FCk_Handle& InLinked) -> FCk_Handle;

    template <typename T_Func>
    static auto
    DoForEach_LifetimeDescendant(
        const FCk_Handle& InHandle,
        T_Func& InFunc,
        int32 InDepth,
        int32 InMaxDepth) -> void;

    template <typename T_Predicate>
    [[nodiscard]]
    static auto
    DoGet_LifetimeDescendant_If(
        const FCk_Handle& InHandle,
        T_Predicate& InPredicate,
        int32 InDepth,
        int32 InMaxDepth) -> FCk_Handle;
};

// --------------------------------------------------------------------------------------------------------------------

template <typename T_Predicate>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_EntityInOwnershipChain_If(
        const FCk_Handle& InHandle,
        T_Predicate T_Func,
        ECk_PendingKill_Policy InPendingKillPolicy)
    -> FCk_Handle
{
#if NOT CK_DISABLE_ENSURE_CHECKS
    auto Visited = TSet<FCk_Handle>{};
#endif
    auto Current = InHandle;
    while (true)
    {
        CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(Current, InPendingKillPolicy),
            TEXT("Lifetime ownership would contain an invalid entity or cycle while traversing [{}]"), Current)
        { return {}; }

        CK_ENSURE_IF_NOT(NOT Visited.Contains(Current),
            TEXT("Lifetime ownership would contain an invalid entity or cycle: revisited [{}] during query"), Current)
        { return {}; }

#if NOT CK_DISABLE_ENSURE_CHECKS
        Visited.Add(Current);
#endif

        const auto Matches = T_Func(Current);

        // The caller may mutate ownership or destroy the current entity. Validate the resulting chain
        // before returning a match or following the next edge; never pass rejected state to another callback.
        CK_ENSURE_IF_NOT(Get_IsOwnershipChainValid(Current, InPendingKillPolicy),
            TEXT("Lifetime ownership would contain an invalid entity or cycle after the predicate for [{}]"), Current)
        { return {}; }

        if (Matches)
        { return Current; }

        if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
        { return {}; }

        Current = Get_LifetimeOwner(Current, InPendingKillPolicy);
    }
}

template <typename... T_Fragment>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_EntityInOwnershipChain_With(
        const FCk_Handle& InHandle,
        ECk_PendingKill_Policy InPendingKillPolicy)
    -> FCk_Handle
{
    return Get_EntityInOwnershipChain_If(InHandle, [](const FCk_Handle& InCurrent)
    {
        return InCurrent.Has_All<T_Fragment...>();
    }, InPendingKillPolicy);
}

template <typename T_Func>
auto
    UCk_Utils_EntityLifetime_UE::
    ForEach_LifetimeDependent(
        const FCk_Handle& InHandle,
        T_Func InFunc)
    -> void
{
    // An owner that is itself pending kill still has a list to walk — the destroy cascade is the first caller.
    if (ck::Is_NOT_Valid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) ||
        NOT InHandle.Has<ck::FFragment_LifetimeDependents>())
    { return; }

    auto Current = InHandle.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent();
    while (ck::IsValid(Current, ck::IsValid_Policy_IncludePendingKill{}))
    {
        auto Next = Get_NextLinkedLifetimeDependent(InHandle, Current);
        InFunc(Current);
        Current = MoveTemp(Next);
    }
}

template <typename T_Func>
auto
    UCk_Utils_EntityLifetime_UE::
    ForEach_LifetimeDescendant(
        const FCk_Handle& InHandle,
        T_Func InFunc)
    -> void
{
    // Every mutation of the lifetime graph rejects cycles, so recursion terminates; the bound only turns a
    // corrupted graph into an ensure instead of a stack overflow.
    constexpr auto MaxDescendantDepth = 1024;
    DoForEach_LifetimeDescendant(InHandle, InFunc, 0, MaxDescendantDepth);
}

template <typename T_Predicate>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeDependent_If(
        const FCk_Handle& InHandle,
        T_Predicate InPredicate)
    -> FCk_Handle
{
    if (ck::Is_NOT_Valid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) ||
        NOT InHandle.Has<ck::FFragment_LifetimeDependents>())
    { return {}; }

    auto Current = InHandle.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent();
    while (ck::IsValid(Current, ck::IsValid_Policy_IncludePendingKill{}))
    {
        if (InPredicate(Current))
        { return Current; }

        Current = Get_NextLinkedLifetimeDependent(InHandle, Current);
    }

    return {};
}

template <typename T_Predicate>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeDescendant_If(
        const FCk_Handle& InHandle,
        T_Predicate InPredicate)
    -> FCk_Handle
{
    constexpr auto MaxDescendantDepth = 1024;
    return DoGet_LifetimeDescendant_If(InHandle, InPredicate, 0, MaxDescendantDepth);
}

template <typename... T_Fragment>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeDescendant_With(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    return Get_LifetimeDescendant_If(InHandle, [](const FCk_Handle& InDescendant)
    {
        return InDescendant.Has_All<T_Fragment...>();
    });
}

template <typename T_Func>
auto
    UCk_Utils_EntityLifetime_UE::
    DoForEach_LifetimeDescendant(
        const FCk_Handle& InHandle,
        T_Func& InFunc,
        int32 InDepth,
        int32 InMaxDepth)
    -> void
{
    CK_ENSURE_IF_NOT(InDepth < InMaxDepth,
        TEXT("Lifetime descendants of [{}] exceed depth [{}]: the lifetime graph must be a tree"), InHandle, InMaxDepth)
    { return; }

    ForEach_LifetimeDependent(InHandle, [&](FCk_Handle& InDependent)
    {
        DoForEach_LifetimeDescendant(InDependent, InFunc, InDepth + 1, InMaxDepth);
        InFunc(InDependent);
    });
}

template <typename T_Predicate>
auto
    UCk_Utils_EntityLifetime_UE::
    DoGet_LifetimeDescendant_If(
        const FCk_Handle& InHandle,
        T_Predicate& InPredicate,
        int32 InDepth,
        int32 InMaxDepth)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(InDepth < InMaxDepth,
        TEXT("Lifetime descendants of [{}] exceed depth [{}]: the lifetime graph must be a tree"), InHandle, InMaxDepth)
    { return {}; }

    if (ck::Is_NOT_Valid(InHandle, ck::IsValid_Policy_IncludePendingKill{}) ||
        NOT InHandle.Has<ck::FFragment_LifetimeDependents>())
    { return {}; }

    auto Current = InHandle.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent();
    while (ck::IsValid(Current, ck::IsValid_Policy_IncludePendingKill{}))
    {
        if (InPredicate(Current))
        { return Current; }

        if (auto Found = DoGet_LifetimeDescendant_If(Current, InPredicate, InDepth + 1, InMaxDepth);
            ck::IsValid(Found, ck::IsValid_Policy_IncludePendingKill{}))
        { return Found; }

        Current = Get_NextLinkedLifetimeDependent(InHandle, Current);
    }

    return {};
}

template <typename T_TypeSafeHandle>
auto
    UCk_Utils_EntityLifetime_UE::
    Get_LifetimeOwner_AsTypeSafe(
        const FCk_Handle& InHandle)
    -> T_TypeSafeHandle
{
    auto LifetimeOwner = Get_LifetimeOwner(InHandle);
    return ck::StaticCast<T_TypeSafeHandle>(LifetimeOwner);
}

template <typename T_TypeSafeHandle>
auto
    UCk_Utils_EntityLifetime_UE::
    Request_CreateEntity_AsTypeSafe(
        const FCk_Handle& InHandle)
    -> T_TypeSafeHandle
{
    QUICK_SCOPE_CYCLE_COUNTER(Request_Create_Entity)

    auto NewEntity = Request_CreateEntity(InHandle);
    return ck::StaticCast<T_TypeSafeHandle>(NewEntity);
}

// --------------------------------------------------------------------------------------------------------------------
