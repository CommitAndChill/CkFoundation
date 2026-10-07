#pragma once

#include "CkEntityScript_Fragment.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Request/CkRequest_Completion.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Enums/CkEnums.h"

#include "CkEntityScript_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_EntityScript_UE;
class UWorld;

// Internal C++ admission preflight shared with the queued-request processor.
namespace ck_entity_script_utils
{
    CKECS_API auto TryResolveSpawnOwnerWorld(const FCk_Handle& InEntity, UWorld*& OutWorld) -> bool;
    // Pure admission/drain policy; actual actor policy is resolved after parameter injection.
    CKECS_API auto TryValidateSpawnReplication(
        const FCk_Handle& InDirectOwner, const UCk_EntityScript_UE* InScript,
        const FCk_Handle* InTarget, ECk_Replication InTargetReplication,
        bool InRequireGenericDriver = false, bool InInjectedContext = false) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------

// An additive, native-only observation of Add on a caller-retained target.
// RequestCarrier is the raw allocator return even when admission fails.
// AllocationAttempted distinguishes no allocation from an invalid allocator return.
// Pending is valid only on successful admission. No field grants destroy permission.
struct CKECS_API FCk_EntityScript_AddCapture
{
    FCk_Handle_PendingEntityScript Pending;
    FCk_Handle RequestCarrier;
    bool bAllocationAttempted = false;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_EntityScript"))
class CKECS_API UCk_Utils_EntityScript_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_EntityScript_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_EntityScript);

public:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityScript",
              DisplayName="[Ck][EntityScript] Has Feature")
    static bool
    Has(
        const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|EntityScript",
        DisplayName="[Ck][EntityScript] Cast",
        meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_EntityScript
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|EntityScript",
        DisplayName="[Ck][EntityScript] Handle -> EntityScript Handle",
        meta = (CompactNodeTitle = "<AsEntityScript>", BlueprintAutocast))
    static FCk_Handle_EntityScript
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|EntityScript",
        DisplayName = "[Ck][EntityScript] Get Invalid Handle",
        meta = (CompactNodeTitle = "INVALID_EntityScriptHandle", Keywords = "make"))
    static FCk_Handle_EntityScript
    Get_InvalidHandle() { return {}; };

public:
    // Returns the currently associated script, or null for invalid, retiring, or non-script entities.
    // NotInstanced scripts share their class default object; instanced scripts must still own this entity.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityScript",
              DisplayName="[Ck][EntityScript] Try Get EntityScript")
    static UCk_EntityScript_UE*
    TryGet_EntityScript(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityScript",
              DisplayName="[Ck][EntityScript] Get_ScriptClass")
    static TSubclassOf<UCk_EntityScript_UE>
    Get_ScriptClass(
        const FCk_Handle_EntityScript& InHandle);

public:
    // Hidden in the editor through the DefaultCkFoundation.ini Config file (see: BlueprintEditor.Menu section)
    // The completion delegate reports that the spawn REQUEST was drained, not that construction finished —
    // a script returning ECk_EntityScript_ConstructionFlow::Continue still completes with Succeeded.
    // Promise_OnConstructed on the returned pending handle remains the construction-finished channel.
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|EntityScript|Private",
        DisplayName="[CK][EntityScript] Request SpawnEntity",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_PendingEntityScript
    Request_SpawnEntity(
        UPARAM(ref) FCk_Handle& InLifetimeOwner,
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    // Request_SpawnEntity for world-bootstrap sites that re-create IDENTITY-BEARING content: the spawned
    // entity carries (or acquires during construction) a SaveKey or adopt-label, so during a CkSnapshot load
    // the loader ADOPTS it instead of respawning it — this variant passes the load-gate spawn suppression.
    // Identical to Request_SpawnEntity when no load is active. Census/count-driven population spawns must
    // never use it; they are the doubling class the suppression exists to stop.
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|EntityScript",
        DisplayName="[CK][EntityScript] Request SpawnEntity (Load Rendezvous)",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_PendingEntityScript
    Request_SpawnEntity_LoadRendezvous(
        UPARAM(ref) FCk_Handle& InLifetimeOwner,
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    // Hidden in the editor through the DefaultCkFoundation.ini Config file (see: BlueprintEditor.Menu section)
    // Completion semantics: see Request_SpawnEntity.
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|EntityScript|Private",
        DisplayName="[CK][EntityScript] Request SpawnEntity (Archetype)",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_PendingEntityScript
    Request_SpawnEntity_Archetype(
        UPARAM(ref) FCk_Handle& InLifetimeOwner,
        UCk_EntityScript_UE* InEntityScriptClassArchetype,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityScript",
              DisplayName="[Ck][EntityScript] Try Get Entity With EntityScript In Ownership Chain")
    static FCk_Handle
    TryGet_Entity_EntityScript_InOwnershipChain(
        const FCk_Handle& InHandle);

private:
    // No early-failure user callback: spawn wrappers retire their own rejected target first.
    static auto
    TryAdmit(
        FCk_Handle& InScriptEntity,
        const TWeakObjectPtr<UCk_EntityScript_UE>& InEntityScriptClassArchetype,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc,
        const FCk_Delegate_Request_OnCompleted& InDelegate,
        bool InDestroyTargetOnFailure,
        FCk_EntityScript_AddCapture* OutCapture = nullptr) -> FCk_Handle_PendingEntityScript;

public:
    // Native-only ownership opt-in for a fresh target. Failure retires it before the one completion callback.
    // Existing callers retain their target and settings by leaving the final argument false.
    static auto
    Add(
        FCk_Handle& InScriptEntity,
        const TSubclassOf<UCk_EntityScript_UE>& InEntityScriptClass,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc = nullptr,
        const FCk_Delegate_Request_OnCompleted& InDelegate = {},
        bool InDestroyTargetOnFailure = false) -> FCk_Handle_PendingEntityScript;

    static auto
    Add(
        FCk_Handle& InScriptEntity,
        const TWeakObjectPtr<UCk_EntityScript_UE>& InEntityScriptClassArchetype,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc = nullptr,
        const FCk_Delegate_Request_OnCompleted& InDelegate = {},
        bool InDestroyTargetOnFailure = false) -> FCk_Handle_PendingEntityScript;

    // Native-only captured Add. Caller has already retained the fresh target and
    // deliberately removed inherited FFragment_Net_Params, FTag_HasAuthority,
    // FTag_NetMode_IsHost and FTag_NetMode_IsClient when that authored policy
    // is required. This API does not do those four removals. The class must
    // already have a valid materialized CDO; this path will not create one.
    // No early-failure delegate callback and no target destruction. A failure
    // result still retains an attempted raw request carrier for caller proof.
    static auto
    TryAddCaptured(
        FCk_Handle& InScriptEntity,
        const TSubclassOf<UCk_EntityScript_UE>& InEntityScriptClass,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc = nullptr,
        const FCk_Delegate_Request_OnCompleted& InDelegate = {})
        -> FCk_EntityScript_AddCapture;

    static auto
    TryInjectEntityScriptSpawnParams(
        UCk_EntityScript_UE* InEntityScript,
        const FInstancedStruct& InSpawnParams) -> void;

    // Establishes entity-script replication for an already-constructed, driver-bearing entity on the authority:
    // resolves the owning driver, accounts for a just-created owner's dependent count, calls Request_Replicate and
    // marks it to fire OnDependentsReplicationComplete. Caller must ensure InHandle already has a driver fragment.
    static auto
    Request_ReplicateEntityScript(
        FCk_Handle& InHandle,
        const FCk_Handle& InReplicatedOwner,
        UCk_EntityScript_UE* InEntityScript,
        const FInstancedStruct& InSpawnParams,
        const TOptional<FCk_Handle>& InContextOwnerOverride = {}) -> void;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_PendingEntityScript"))
class CKECS_API UCk_Utils_PendingEntityScript_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_PendingEntityScript_UE);

public:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|EntityScript",
              DisplayName = "[Ck][EntityScript] Promise/Future OnConstructed",
              meta = (Keywords = "bind, wait, when, finish"))
    static void
    Promise_OnConstructed(
        UPARAM(ref) FCk_Handle_PendingEntityScript& InPendingEntityScript,
        const FCk_Delegate_EntityScript_Constructed& InDelegate);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][EntityScript] Get Is Valid (Pending Entity Script)",
              Category = "Ck|Utils|EntityScript",
              meta = (CompactNodeTitle = "IsValid"))
    static bool
    Get_IsValid(
        const FCk_Handle_PendingEntityScript& InHandle);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][EntityScript] Request Destroy Pending Entity Script",
              Category = "Ck|Utils|EntityScript")
    static void
    Request_DestroyEntity(
        UPARAM(ref) FCk_Handle_PendingEntityScript& InHandle,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior = ECk_EntityLifetime_DestructionBehavior::ForceDestroy);
};

// --------------------------------------------------------------------------------------------------------------------
