#include "CkEntityScript_Utils.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Reflection/CkReflection_Utils.h"
#include "CkCore/Validation/CkUntracedStructSafety.h"

#include "Misc/MTAccessDetector.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h" // ck::FTag_EntityJustCreated
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript.h"             // UCk_EntityScript_UE (complete type)
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment_Data.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Fragment.h"
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Fragment.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/CkEcsLog.h"                       // load-gate spawn suppression Warning
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h" // load-gate + spawn-window queries

#include <Engine/BlueprintGeneratedClass.h>
#include <UObject/ObjectPtr.h>

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_EntityScript_UE, FCk_Handle_EntityScript, ck::FFragment_EntityScript);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_entity_script_utils
{
    auto RetireRejectedOwnedTarget(FCk_Handle& InTarget, bool InDestroyTargetOnFailure) -> void
    {
        if (InDestroyTargetOnFailure && ck::IsValid(InTarget))
        { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InTarget); }
    }

    struct FSpawnOwnerContext
    {
        UWorld* World = nullptr;
        ECk_Net_NetModeType NetMode = ECk_Net_NetModeType::Unknown;
        bool HasNetMode = false;
    };

    auto Get_IsUsableNetMode(ECk_Net_NetModeType InNetMode) -> bool
    {
        return InNetMode == ECk_Net_NetModeType::Client ||
            InNetMode == ECk_Net_NetModeType::Host ||
            InNetMode == ECk_Net_NetModeType::ClientAndHost;
    }

    // Walk only live handles in this registry; malformed or circular ownership is not an admissible spawn context.
    auto TryResolveSpawnOwnerContext(const FCk_Handle& InEntity, FSpawnOwnerContext& OutContext) -> bool
    {
        if (NOT ck::IsValid(InEntity))
        { return false; }

        const auto RegistryHandle = InEntity.Get_RegistryView().Get_RegistryHandle();
        TArray<FCk_Handle> Visited;
        auto Current = InEntity;
        while (true)
        {
            if (NOT ck::IsValid(Current) ||
                Current.Get_RegistryView().Get_RegistryHandle() != RegistryHandle ||
                Current.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                    ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                    ck::FTag_DestroyEntity_Finalize>() ||
                Visited.Contains(Current))
            { return false; }
            Visited.Add(Current);

            if (NOT OutContext.HasNetMode && Current.Has<ck::FFragment_Net_Params>())
            {
                OutContext.NetMode = Current.Get<ck::FFragment_Net_Params>()
                    .Get_ConnectionSettings().Get_NetMode();
                OutContext.HasNetMode = true;
            }
            if (Current.Has<TWeakObjectPtr<UWorld>>())
            {
                auto* const CurrentWorld = Current.Get<TWeakObjectPtr<UWorld>>().Get();
                if (NOT ck::IsValid(CurrentWorld) ||
                    (OutContext.World != nullptr && CurrentWorld != OutContext.World))
                { return false; }
                OutContext.World = CurrentWorld;
            }
            // Context fragments never terminate validation: the remaining owner chain may be stale or circular.
            if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
            { return OutContext.World != nullptr; }
            const auto Next = Current.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
            // A world-rooted self-owner is valid even when no net settings exist.
            if (Next == Current && OutContext.World != nullptr)
            { return true; }
            Current = Next;
        }
    }

    auto TryResolveSpawnOwnerWorld(const FCk_Handle& InEntity, UWorld*& OutWorld) -> bool
    {
        OutWorld = nullptr;
        auto Context = FSpawnOwnerContext{};
        if (NOT TryResolveSpawnOwnerContext(InEntity, Context))
        { return false; }
        OutWorld = Context.World;
        return true;
    }

    // Matches the actor-channel search used by TryAddReplicatedFragment, with live/exact-world
    // prerequisites and a cycle bound. Actor ownership is not an ECS lifetime-owner fallback.
    auto Get_HasLiveActorWireChannel(const AActor* InActor, const UWorld* InWorld) -> bool
    {
        if (NOT ck::IsValid(InWorld)) { return false; }
        if (InWorld->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) { return false; }
        const auto NeedsAuthorityChannel = InWorld->IsNetMode(NM_DedicatedServer) || InWorld->IsNetMode(NM_ListenServer);
        TArray<const AActor*> Visited;
        auto* Current = InActor;
        while (ck::IsValid(Current))
        {
            if (Current->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || Current->IsActorBeingDestroyed()) { return false; }
            if (Current->GetWorld() != InWorld || Visited.Contains(Current)) { return false; }
            Visited.Add(Current);
            if (NOT NeedsAuthorityChannel) { return true; }
            if (Current->GetRemoteRole() != ROLE_None &&
                (Current->GetLocalRole() == ROLE_Authority || Current->GetLocalRole() == ROLE_AutonomousProxy))
            { return true; }
            Current = Current->GetOwner();
        }
        return false;
    }

    auto TryValidateSpawnReplication(
        const FCk_Handle& InDirectOwner, const UCk_EntityScript_UE* InScript,
        const FCk_Handle* InTarget, ECk_Replication InTargetReplication,
        bool InRequireGenericDriver, bool InInjectedContext) -> bool
    {
        if (NOT ck::IsValid(InScript) || NOT ck::IsValid(InDirectOwner)) { return false; }
        auto* OwnerWorld = static_cast<UWorld*>(nullptr);
        if (NOT TryResolveSpawnOwnerWorld(InDirectOwner, OwnerWorld)) { return false; }
        if (InTarget)
        {
            if (NOT ck::IsValid(*InTarget)) { return false; }
            if (NOT InTarget->Has<ck::FFragment_LifetimeOwner>()) { return false; }
            if (InTarget->Get<ck::FFragment_LifetimeOwner>().Get_Entity() != InDirectOwner) { return false; }
            auto* TargetWorld = static_cast<UWorld*>(nullptr);
            if (NOT TryResolveSpawnOwnerWorld(*InTarget, TargetWorld) || TargetWorld != OwnerWorld) { return false; }
        }
        const auto WantsScriptWire = InScript->Get_EffectiveReplication() == ECk_Replication::Replicates;
        if (NOT WantsScriptWire) { return true; }
        const auto ActorOrder = InScript->Get_RequiresOwningActorBeforeReplicationDriver();
        // An archetype cannot yet see the injected actor; a local actor may resolve its policy to DNR.
        if (ActorOrder && NOT InInjectedContext) { return true; }
        if (InTargetReplication != ECk_Replication::Replicates) { return false; }
        if (ActorOrder) { return true; }
        if (InTarget && InTarget->Has<ck::FFragment_OwningActor>())
        {
            const auto Actor = InTarget->Get<ck::FFragment_OwningActor>().Get_EntityOwningActor();
            if (NOT ck::IsValid(Actor)) { return false; }
            return Get_HasLiveActorWireChannel(Actor.Get(), OwnerWorld);
        }
        if (NOT InDirectOwner.Has<ck::FFragment_Net_Params>()) { return false; }
        const auto& OwnerSettings = InDirectOwner.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
        if (OwnerSettings.Get_Replication() != ECk_Replication::Replicates) { return false; }
        if (InRequireGenericDriver &&
            (OwnerWorld->IsNetMode(NM_DedicatedServer) || OwnerWorld->IsNetMode(NM_ListenServer)))
        {
            if (NOT InDirectOwner.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>()) { return false; }
            const auto Driver = InDirectOwner.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
            if (NOT ck::IsValid(Driver)) { return false; }
            if (Driver->Get_AssociatedEntity() != InDirectOwner) { return false; }
            const auto* Actor = Driver->GetOwningActor();
            if (NOT ck::IsValid(Actor)) { return false; }
            if (NOT Get_HasLiveActorWireChannel(Actor, OwnerWorld)) { return false; }
        }
        return true;
    }

    // While a CkSnapshot load is REBUILDING the world, the loader is the SOLE creator of world population: saved
    // rows respawn through its own window (FCk_ScopedLoaderSpawnWindow) and replayed constructions re-create their
    // ConstructSpawned children. During DEMOLITION, no new population is admitted at all. Any other spawn is world
    // POLICY reacting to
    // the half-rebuilt world — a population census reading near-zero, an adopt-or-spawn task whose adopt scan
    // ran before the loader materialized the restored copy. Admitting those double-populates the world and
    // the next capture saves both copies (the save-inflation incident, 2026-07-29: +77 NPCs and a doubled
    // StoreDriver subordinate family in one save->load->save cycle). Suppressed spawns return an INVALID
    // pending handle; Promise_OnConstructed no-ops on it and the completion delegate reports
    // Failed_NotEnqueued — reconcile-shaped callers converge on their next real-world evaluation.
    //
    // The window stops at the rebuild, deliberately: once payloads start applying, a restored feature's own
    // composition legitimately spawns (a payload's deferred request, a construction the apply drives), and
    // refusing those breaks the restore itself. A PRODUCER that must not seed for the whole load asks
    // UCk_Utils_Snapshot_UE::Get_IsRebuildInProgress, which is true in every phase.
    auto Get_IsSpawnSuppressedByLoadGate(const FCk_Handle& InLifetimeOwner,
        const UObject* InEntityScriptClassOrArchetype, UWorld* InWorld) -> bool
    {
        auto* World = InWorld;

        const auto* EcsWorld = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (ck::Is_NOT_Valid(EcsWorld))
        { return false; }

        const auto Hold = EcsWorld->Get_LoadHold();
        if (Hold == ECk_EcsWorld_LoadHold::Teardown)
        {
            ck::ecs::Verbose(TEXT("Request_SpawnEntity of [{}] under [{}] suppressed because ECS world teardown has begun"),
                InEntityScriptClassOrArchetype, InLifetimeOwner);
            return true;
        }

        const auto LoaderOwnsPopulation = Hold == ECk_EcsWorld_LoadHold::Rebuilding
            || Hold == ECk_EcsWorld_LoadHold::Escalated;

        if (NOT LoaderOwnsPopulation)
        { return false; }

        if (EcsWorld->Get_IsInLoaderSpawnWindow())
        { return false; }

        if (EcsWorld->Get_IsInRendezvousSpawnWindow())
        { return false; }

        if (UCk_Utils_EntityLifetime_UE::Get_IsInsideConstructionWindow(InLifetimeOwner))
        { return false; }

        ck::ecs::Warning(TEXT("Request_SpawnEntity of [{}] under [{}] SUPPRESSED: a CkSnapshot load owns the world. "
            "This spawn is a world-policy decision made against the half-rebuilt world — the loader respawns what "
            "the save recorded; spawn-on-demand logic must re-evaluate once the world is coherent. If this spawn "
            "re-creates IDENTITY-BEARING content the loader adopts (a SaveKey / adopt-label rendezvous target), "
            "declare it: Request_SpawnEntity_LoadRendezvous, or FCk_ScopedRendezvousSpawnWindow in C++."),
            InEntityScriptClassOrArchetype, InLifetimeOwner);
        return true;
    }

    auto ValidateRetainedSpawnParams(const TCHAR* InOperation, const FInstancedStruct& InSpawnParams) -> bool
    {
        if (NOT InSpawnParams.IsValid())
        { return true; }

        const auto* ScriptStruct = InSpawnParams.GetScriptStruct();
        const auto HasScriptStruct = ScriptStruct != nullptr;
        CK_ENSURE_IF_NOT(HasScriptStruct,
            TEXT("{} rejected EntityScript spawn params without a reflected struct type"), InOperation) {}
        if (NOT HasScriptStruct)
        { return false; }

        const auto Safety = ck::Analyze_UntracedStructSafety(ScriptStruct);
        const auto IsSpawnParamsSafe = Safety.IsGcIndependent();
        CK_ENSURE_IF_NOT(IsSpawnParamsSafe,
            TEXT("{} rejected unsafe EntityScript spawn params [{}]; [{}]: {}"),
            InOperation,
            ScriptStruct->GetName(),
            Safety.FailurePath,
            Safety.FailureReason) {}
        if (NOT IsSpawnParamsSafe)
        { return false; }

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityScript_UE::
    TryGet_EntityScript(
        const FCk_Handle& InHandle)
    -> UCk_EntityScript_UE*
{
    if (NOT ck::IsValid(InHandle))
    { return nullptr; }

    if (InHandle.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
        ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>())
    { return nullptr; }

    if (NOT InHandle.Has<ck::FFragment_EntityScript>())
    { return nullptr; }

    const auto Script = InHandle.Get<ck::FFragment_EntityScript>().Get_Script().Get();
    if (NOT ck::IsValid(Script))
    { return nullptr; }

    if (Script->Get_InstancingPolicy() != ECk_EntityScript_InstancingPolicy::NotInstanced &&
        Script->Get_AssociatedEntity() != InHandle)
    { return nullptr; }

    return Script;
}

auto
    UCk_Utils_EntityScript_UE::
    Get_ScriptClass(
        const FCk_Handle_EntityScript& InHandle)
    -> TSubclassOf<UCk_EntityScript_UE>
{
    const auto& Current = InHandle.Get<ck::FFragment_EntityScript>();

    CK_ENSURE_IF_NOT(ck::IsValid(Current.Get_Script()), TEXT("The EntityScript [{}] for Handle [{}] is NOT valid"),
        Current.Get_Script(), InHandle)
    { return {}; }

    return InHandle.Get<ck::FFragment_EntityScript>().Get_Script()->GetClass();
}

auto
    UCk_Utils_EntityScript_UE::
    Request_SpawnEntity(
        FCk_Handle& InLifetimeOwner,
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_PendingEntityScript
{
    if (NOT ck_entity_script_utils::ValidateRetainedSpawnParams(TEXT("Request_SpawnEntity"), InSpawnParams))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto EntityScriptClassIsValid = ck::IsValid(InEntityScriptClass);
    CK_ENSURE_IF_NOT(EntityScriptClassIsValid,
        TEXT("EntityScriptClass [{}] is INVALID. Unable to SpawnEntity using LifetimeOwner [{}]."),
        InEntityScriptClass, InLifetimeOwner) {}
    if (NOT EntityScriptClassIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto LifetimeOwnerIsValid = ck::IsValid(InLifetimeOwner);
    CK_ENSURE_IF_NOT(LifetimeOwnerIsValid,
        TEXT("LifetimeOwner is INVALID. Unable to SpawnEntity using EntityScriptClass [{}]."),
        InEntityScriptClass) {}
    if (NOT LifetimeOwnerIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto OwnerNotDestroying = NOT InLifetimeOwner.Has_Any<
        ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
        ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
        ck::FTag_DestroyEntity_Finalize>();
    CK_ENSURE_IF_NOT(OwnerNotDestroying, TEXT("EntityScript spawn owner is retiring")) {}
    if (NOT OwnerNotDestroying)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto OwnerCanCreate = UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InLifetimeOwner);
    CK_ENSURE_IF_NOT(OwnerCanCreate, TEXT("EntityScript spawn owner world is tearing down")) {}
    if (NOT OwnerCanCreate)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    auto SpawnContext = ck_entity_script_utils::FSpawnOwnerContext{};
    const auto OwnerContextIsValid =
        ck_entity_script_utils::TryResolveSpawnOwnerContext(InLifetimeOwner, SpawnContext);
    CK_ENSURE_IF_NOT(OwnerContextIsValid, TEXT("EntityScript spawn owner has no safe live World chain")) {}
    if (NOT OwnerContextIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    if (ck_entity_script_utils::Get_IsSpawnSuppressedByLoadGate(InLifetimeOwner, InEntityScriptClass, SpawnContext.World))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    auto* const DefaultObject = InEntityScriptClass->GetDefaultObject<UCk_EntityScript_UE>();
    const auto CDOIsValid = ck::IsValid(DefaultObject);
    CK_ENSURE_IF_NOT(CDOIsValid, TEXT("EntityScript class has no valid CDO")) {}
    if (NOT CDOIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    if (NOT ck_entity_script_utils::TryValidateSpawnReplication(
        InLifetimeOwner, DefaultObject, nullptr, DefaultObject->Get_EffectiveReplication()))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto NeedsNetMode = DefaultObject->Get_EffectiveReplication() == ECk_Replication::Replicates;
    const auto NetModeReady = NOT NeedsNetMode || (SpawnContext.HasNetMode &&
        ck_entity_script_utils::Get_IsUsableNetMode(SpawnContext.NetMode));
    CK_ENSURE_IF_NOT(NetModeReady, TEXT("Replicated EntityScript owner has no net-mode source")) {}
    if (NOT NetModeReady)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    if (NeedsNetMode && SpawnContext.NetMode == ECk_Net_NetModeType::Client)
    {
        const auto PendingEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
        const auto PendingIsValid = ck::IsValid(PendingEntity);
        CK_ENSURE_IF_NOT(PendingIsValid, TEXT("Failed to allocate client pending script target")) {}
        if (NOT PendingIsValid)
        {
            InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
            return {};
        }
        auto& PendingFragment = InLifetimeOwner.AddOrGet<ck::FFragment_PendingReplication>();
        PendingFragment.Add(InEntityScriptClass.Get(), PendingEntity, InSpawnParams);

        // Completion is LOCAL-machine: on a client the authority owns this spawn and nothing is queued
        // here, so there is no local processing to report. Promise_OnConstructed on the returned pending
        // handle is what resolves when the replicated entity arrives.
        InDelegate.ExecuteIfBound(PendingEntity, ECk_Request_OperationResult::Failed_NotEnqueued);

        return FCk_Handle_PendingEntityScript{PendingEntity};
    }

    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
    const auto NewEntityIsValid = ck::IsValid(NewEntity);
    CK_ENSURE_IF_NOT(NewEntityIsValid, TEXT("Failed to allocate EntityScript target")) {}
    if (NOT NewEntityIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    // Fresh targets inherit their owner's net settings and tags during lifetime setup.
    // The authored script policy owns this target; TryAdmit stamps its actual world settings.
    if (NewEntity.Has<ck::FFragment_Net_Params>())
    { NewEntity.Remove<ck::FFragment_Net_Params>(); }
    if (NewEntity.Has<ck::FTag_HasAuthority>())
    { NewEntity.Remove<ck::FTag_HasAuthority>(); }
    if (NewEntity.Has<ck::FTag_NetMode_IsHost>())
    { NewEntity.Remove<ck::FTag_NetMode_IsHost>(); }
    if (NewEntity.Has<ck::FTag_NetMode_IsClient>())
    { NewEntity.Remove<ck::FTag_NetMode_IsClient>(); }

    CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, NewEntity,
        TEXT("Request_SpawnEntity called with class: {}"), InEntityScriptClass);

    auto Pending = TryAdmit(NewEntity, MakeWeakObjectPtr(DefaultObject), InSpawnParams, nullptr, InDelegate, true);
    if (NOT ck::IsValid(Pending))
    {
        if (ck::IsValid(NewEntity))
        { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(NewEntity); }
        InDelegate.ExecuteIfBound(NewEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
    }
    return Pending;
}

auto
    UCk_Utils_EntityScript_UE::
    Request_SpawnEntity_LoadRendezvous(
        FCk_Handle& InLifetimeOwner,
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_PendingEntityScript
{
    auto EcsWorld = static_cast<UCk_EcsWorld_Subsystem_UE*>(nullptr);
    auto SpawnContext = ck_entity_script_utils::FSpawnOwnerContext{};
    if (ck_entity_script_utils::TryResolveSpawnOwnerContext(InLifetimeOwner, SpawnContext))
    { EcsWorld = SpawnContext.World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>(); }

    const auto RendezvousWindow = FCk_ScopedRendezvousSpawnWindow{EcsWorld};
    return Request_SpawnEntity(InLifetimeOwner, InEntityScriptClass, InSpawnParams, InDelegate);
}

auto
    UCk_Utils_EntityScript_UE::
    Request_SpawnEntity_Archetype(
        FCk_Handle& InLifetimeOwner,
        UCk_EntityScript_UE* InEntityScriptClassArchetype,
        FInstancedStruct InSpawnParams,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_PendingEntityScript
{
    if (NOT ck_entity_script_utils::ValidateRetainedSpawnParams(TEXT("Request_SpawnEntity_Archetype"), InSpawnParams))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto ArchetypeIsValid = ck::IsValid(InEntityScriptClassArchetype);
    CK_ENSURE_IF_NOT(ArchetypeIsValid,
        TEXT("EntityScriptClass [{}] is INVALID. Unable to SpawnEntity using LifetimeOwner [{}]."),
        InEntityScriptClassArchetype, InLifetimeOwner) {}
    if (NOT ArchetypeIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto LifetimeOwnerIsValid = ck::IsValid(InLifetimeOwner);
    CK_ENSURE_IF_NOT(LifetimeOwnerIsValid,
        TEXT("LifetimeOwner is INVALID. Unable to SpawnEntity using EntityScriptClass [{}]."),
        InEntityScriptClassArchetype) {}
    if (NOT LifetimeOwnerIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto OwnerNotDestroying = NOT InLifetimeOwner.Has_Any<
        ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
        ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
        ck::FTag_DestroyEntity_Finalize>();
    CK_ENSURE_IF_NOT(OwnerNotDestroying, TEXT("EntityScript spawn owner is retiring")) {}
    if (NOT OwnerNotDestroying)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto OwnerCanCreate = UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InLifetimeOwner);
    CK_ENSURE_IF_NOT(OwnerCanCreate, TEXT("EntityScript spawn owner world is tearing down")) {}
    if (NOT OwnerCanCreate)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    auto SpawnContext = ck_entity_script_utils::FSpawnOwnerContext{};
    const auto OwnerContextIsValid =
        ck_entity_script_utils::TryResolveSpawnOwnerContext(InLifetimeOwner, SpawnContext);
    CK_ENSURE_IF_NOT(OwnerContextIsValid, TEXT("EntityScript spawn owner has no safe live World chain")) {}
    if (NOT OwnerContextIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    if (ck_entity_script_utils::Get_IsSpawnSuppressedByLoadGate(InLifetimeOwner, InEntityScriptClassArchetype, SpawnContext.World))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    if (NOT ck_entity_script_utils::TryValidateSpawnReplication(
        InLifetimeOwner, InEntityScriptClassArchetype, nullptr, InEntityScriptClassArchetype->Get_EffectiveReplication()))
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    const auto NeedsNetMode =
        InEntityScriptClassArchetype->Get_EffectiveReplication() == ECk_Replication::Replicates;
    const auto NetModeReady = NOT NeedsNetMode || (SpawnContext.HasNetMode &&
        ck_entity_script_utils::Get_IsUsableNetMode(SpawnContext.NetMode));
    CK_ENSURE_IF_NOT(NetModeReady, TEXT("Replicated EntityScript owner has no net-mode source")) {}
    if (NOT NetModeReady)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    if (NeedsNetMode && SpawnContext.NetMode == ECk_Net_NetModeType::Client)
    {
        const auto PendingEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
        const auto PendingIsValid = ck::IsValid(PendingEntity);
        CK_ENSURE_IF_NOT(PendingIsValid, TEXT("Failed to allocate client pending script target")) {}
        if (NOT PendingIsValid)
        {
            InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
            return {};
        }
        auto& PendingFragment = InLifetimeOwner.AddOrGet<ck::FFragment_PendingReplication>();
        PendingFragment.Add(InEntityScriptClassArchetype->GetClass(), PendingEntity, InSpawnParams);

        // See Request_SpawnEntity: nothing is queued locally on a client, so there is no local outcome.
        InDelegate.ExecuteIfBound(PendingEntity, ECk_Request_OperationResult::Failed_NotEnqueued);

        return FCk_Handle_PendingEntityScript{PendingEntity};
    }

    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
    const auto NewEntityIsValid = ck::IsValid(NewEntity);
    CK_ENSURE_IF_NOT(NewEntityIsValid, TEXT("Failed to allocate EntityScript target")) {}
    if (NOT NewEntityIsValid)
    {
        InDelegate.ExecuteIfBound(InLifetimeOwner, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    // Fresh targets inherit their owner's net settings and tags during lifetime setup.
    // The authored script policy owns this target; TryAdmit stamps its actual world settings.
    if (NewEntity.Has<ck::FFragment_Net_Params>())
    { NewEntity.Remove<ck::FFragment_Net_Params>(); }
    if (NewEntity.Has<ck::FTag_HasAuthority>())
    { NewEntity.Remove<ck::FTag_HasAuthority>(); }
    if (NewEntity.Has<ck::FTag_NetMode_IsHost>())
    { NewEntity.Remove<ck::FTag_NetMode_IsHost>(); }
    if (NewEntity.Has<ck::FTag_NetMode_IsClient>())
    { NewEntity.Remove<ck::FTag_NetMode_IsClient>(); }

    CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, NewEntity,
        TEXT("Request_SpawnEntity called with class: {}"), InEntityScriptClassArchetype);

    auto Pending = TryAdmit(NewEntity, InEntityScriptClassArchetype, InSpawnParams, nullptr, InDelegate, true);
    if (NOT ck::IsValid(Pending))
    {
        if (ck::IsValid(NewEntity))
        { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(NewEntity); }
        InDelegate.ExecuteIfBound(NewEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
    }
    return Pending;
}

auto
    UCk_Utils_EntityScript_UE::
    TryGet_Entity_EntityScript_InOwnershipChain(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    return UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_If(InHandle,
    [&](const FCk_Handle& Handle)
    {
        return Handle.Has<ck::FFragment_EntityScript>();
    });
}

auto
    UCk_Utils_EntityScript_UE::
    Add(
        FCk_Handle& InScriptEntity,
        const TSubclassOf<UCk_EntityScript_UE>& InEntityScriptClass,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc,
        const FCk_Delegate_Request_OnCompleted& InDelegate,
        bool InDestroyTargetOnFailure)
    -> FCk_Handle_PendingEntityScript
{
    const auto ClassIsValid = ck::IsValid(InEntityScriptClass);
    CK_ENSURE_IF_NOT(ClassIsValid, TEXT("Invalid EntityScript class supplied to Add")) {}
    if (NOT ClassIsValid)
    {
        ck_entity_script_utils::RetireRejectedOwnedTarget(InScriptEntity, InDestroyTargetOnFailure);
        InDelegate.ExecuteIfBound(InScriptEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    auto* const CDO = InEntityScriptClass.GetDefaultObject();
    const auto CDOIsValid = ck::IsValid(CDO);
    CK_ENSURE_IF_NOT(CDOIsValid, TEXT("EntityScript class supplied to Add has no valid CDO")) {}
    if (NOT CDOIsValid)
    {
        ck_entity_script_utils::RetireRejectedOwnedTarget(InScriptEntity, InDestroyTargetOnFailure);
        InDelegate.ExecuteIfBound(InScriptEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    auto Pending = TryAdmit(InScriptEntity, MakeWeakObjectPtr(CDO), InSpawnParams, InOptionalFunc, InDelegate, InDestroyTargetOnFailure);
    if (NOT ck::IsValid(Pending))
    {
        ck_entity_script_utils::RetireRejectedOwnedTarget(InScriptEntity, InDestroyTargetOnFailure);
        InDelegate.ExecuteIfBound(InScriptEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
    }
    return Pending;
}


auto
    UCk_Utils_EntityScript_UE::
    TryAddCaptured(
        FCk_Handle& InScriptEntity,
        const TSubclassOf<UCk_EntityScript_UE>& InEntityScriptClass,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_EntityScript_AddCapture
{
    auto Result = FCk_EntityScript_AddCapture{};
    // A failed captured admission does not call back or destroy caller-owned
    // entities. Peek at an already-existing CDO; never instantiate one here.
    if (NOT IsInGameThread() || InScriptEntity == FCk_Handle{})
    { return Result; }
    const auto RegistryHandle = InScriptEntity.Get_RegistryView().Get_RegistryHandle();
    if (ck::registry_table::Get_IsInParallelRegion(RegistryHandle) ||
        NOT ck::IsValid(InScriptEntity) || NOT ck::IsValid(InEntityScriptClass))
    { return Result; }
    auto* const Class = InEntityScriptClass.Get();
    if (Class->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
        NOT Class->IsChildOf(UCk_EntityScript_UE::StaticClass()) ||
        InScriptEntity.Has_Any<ck::FTag_DestroyEntity_Initiate,
            ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
            ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>() ||
        NOT InScriptEntity.Has<ck::FFragment_LifetimeOwner>())
    { return Result; }
    auto* const CDO = ::Cast<UCk_EntityScript_UE>(Class->GetDefaultObject(false));
    if (NOT ck::IsValid(CDO) ||
        CDO->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
        CDO->GetClass() != Class)
    { return Result; }
    Result.Pending = TryAdmit(InScriptEntity, MakeWeakObjectPtr(CDO),
        InSpawnParams, InOptionalFunc, InDelegate, false, &Result);
    return Result;
}

auto
    UCk_Utils_EntityScript_UE::
    TryAdmit(
        FCk_Handle& InScriptEntity,
        const TWeakObjectPtr<UCk_EntityScript_UE>& InEntityScriptClassArchetype,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc,
        const FCk_Delegate_Request_OnCompleted& InDelegate,
        bool InDestroyTargetOnFailure,
        FCk_EntityScript_AddCapture* OutCapture)
    -> FCk_Handle_PendingEntityScript
{
    if (NOT ck_entity_script_utils::ValidateRetainedSpawnParams(TEXT("Add EntityScript"), InSpawnParams))
    {
        return {};
    }

    const auto TargetIsValid = ck::IsValid(InScriptEntity);
    CK_ENSURE_IF_NOT(TargetIsValid, TEXT("Cannot Add EntityScript to invalid target")) {}
    if (NOT TargetIsValid)
    {
        return {};
    }
    const auto ArchetypeIsValid = ck::IsValid(InEntityScriptClassArchetype);
    CK_ENSURE_IF_NOT(ArchetypeIsValid, TEXT("Invalid EntityScript archetype supplied to Add")) {}
    if (NOT ArchetypeIsValid)
    {
        return {};
    }
    const auto TargetNotDestroying = NOT InScriptEntity.Has_Any<
        ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
        ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
        ck::FTag_DestroyEntity_Finalize>();
    CK_ENSURE_IF_NOT(TargetNotDestroying, TEXT("Cannot Add EntityScript to retiring target")) {}
    if (NOT TargetNotDestroying)
    {
        return {};
    }
    const auto TargetCanCreate = UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InScriptEntity);
    CK_ENSURE_IF_NOT(TargetCanCreate, TEXT("Cannot Add EntityScript after world teardown")) {}
    if (NOT TargetCanCreate)
    {
        return {};
    }
    const auto AlreadyHasScript = Has(InScriptEntity);
    CK_ENSURE_IF_NOT(NOT AlreadyHasScript, TEXT("Entity [{}] already has an EntityScript"), InScriptEntity) {}
    if (AlreadyHasScript)
    {
        return {};
    }
    auto AlreadyQueued = false;
    for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InScriptEntity))
    {
        if (ck::IsValid(Dependent) &&
            Dependent.Has<ck::FFragment_EntityScript_RequestSpawnEntity>() &&
            Dependent.Get<ck::FFragment_EntityScript_RequestSpawnEntity>().Get_NewEntity() == InScriptEntity)
        { AlreadyQueued = true; break; }
    }
    CK_ENSURE_IF_NOT(NOT AlreadyQueued, TEXT("Entity [{}] already has a queued EntityScript"), InScriptEntity) {}
    if (AlreadyQueued)
    {
        return {};
    }
    const auto HasLifetimeOwner = InScriptEntity.Has<ck::FFragment_LifetimeOwner>();
    CK_ENSURE_IF_NOT(HasLifetimeOwner, TEXT("EntityScript target [{}] has no lifetime owner"), InScriptEntity) {}
    if (NOT HasLifetimeOwner)
    {
        return {};
    }
    const auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InScriptEntity);
    const auto LifetimeOwnerIsValid = ck::IsValid(LifetimeOwner);
    CK_ENSURE_IF_NOT(LifetimeOwnerIsValid, TEXT("EntityScript target [{}] has invalid owner"), InScriptEntity) {}
    if (NOT LifetimeOwnerIsValid)
    {
        return {};
    }

    auto TargetContext = ck_entity_script_utils::FSpawnOwnerContext{};
    const auto TargetContextIsValid =
        ck_entity_script_utils::TryResolveSpawnOwnerContext(InScriptEntity, TargetContext);
    CK_ENSURE_IF_NOT(TargetContextIsValid, TEXT("EntityScript target has no safe live World chain")) {}
    if (NOT TargetContextIsValid)
    {
        return {};
    }

    // Resolve all recoverable prerequisites before mutating the caller-owned target.
    const auto NeedsNetParams = NOT InScriptEntity.Has<ck::FFragment_Net_Params>();
    auto NewConnectionSettings = FCk_Net_ConnectionSettings{};
    if (NeedsNetParams)
    {
        const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InScriptEntity);
        const auto TransientHasNetParams = ck::IsValid(Transient) &&
            Transient.Has<ck::FFragment_Net_Params>();
        CK_ENSURE_IF_NOT(TransientHasNetParams, TEXT("EntityScript target has no transient net settings")) {}
        if (NOT TransientHasNetParams)
        {
            return {};
        }
        const auto& Settings = Transient.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
        const auto Replication = InEntityScriptClassArchetype->Get_EffectiveReplication();
        auto NetRole = Settings.Get_NetRole();
        if (Replication == ECk_Replication::Replicates &&
            Settings.Get_NetMode() == ECk_Net_NetModeType::Client)
        { NetRole = ECk_Net_EntityNetRole::Proxy; }
        NewConnectionSettings = FCk_Net_ConnectionSettings{
            Replication, Settings.Get_NetMode(), NetRole};
    }

    const auto& EffectiveSettings = NeedsNetParams ? NewConnectionSettings :
        InScriptEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
    const auto NetModeReady = EffectiveSettings.Get_Replication() != ECk_Replication::Replicates ||
        ck_entity_script_utils::Get_IsUsableNetMode(EffectiveSettings.Get_NetMode());
    CK_ENSURE_IF_NOT(NetModeReady, TEXT("Replicated EntityScript target has no usable net mode")) {}
    if (NOT NetModeReady)
    { return {}; }

    if (NOT ck_entity_script_utils::TryValidateSpawnReplication(
        LifetimeOwner, InEntityScriptClassArchetype.Get(), &InScriptEntity, EffectiveSettings.Get_Replication()))
    { return {}; }

    if (OutCapture != nullptr) { OutCapture->bAllocationAttempted = true; }
    auto RequestEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InScriptEntity);
    if (OutCapture != nullptr) { OutCapture->RequestCarrier = RequestEntity; }
    const auto RequestEntityIsValid = ck::IsValid(RequestEntity);
    CK_ENSURE_IF_NOT(RequestEntityIsValid, TEXT("Failed to allocate EntityScript request")) {}
    if (NOT RequestEntityIsValid)
    {
        return {};
    }
    if (NeedsNetParams)
    { UCk_Utils_Net_UE::Add(InScriptEntity, NewConnectionSettings); }
    UCk_Utils_Handle_UE::Set_DebugName(InScriptEntity,
        *ck::Format_UE(TEXT("{}"), InEntityScriptClassArchetype));
    CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, InScriptEntity,
        TEXT("Add() creating request entity for class: {}"), InEntityScriptClassArchetype);

    const auto Request = FCk_Request_EntityScript_SpawnEntity{
                             InScriptEntity,
                             LifetimeOwner,
                             InEntityScriptClassArchetype, InDestroyTargetOnFailure}
                        .Set_ContextOwner(LifetimeOwner)
                        .Set_SpawnParams(InSpawnParams)
                        .Set_PostConstruction_Func(InOptionalFunc);

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    RequestEntity.Add<ck::FFragment_EntityScript_RequestSpawnEntity>(Request);

    CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, InScriptEntity,
        TEXT("Spawn request entity created, awaiting processing"));

    const auto Pending = FCk_Handle_PendingEntityScript{InScriptEntity};
    if (OutCapture != nullptr) { OutCapture->Pending = Pending; }
    return Pending;
}

auto
    UCk_Utils_EntityScript_UE::
    Add(
        FCk_Handle& InScriptEntity,
        const TWeakObjectPtr<UCk_EntityScript_UE>& InEntityScriptClassArchetype,
        const FInstancedStruct& InSpawnParams,
        const FCk_EntityScript_PostConstruction_Func& InOptionalFunc,
        const FCk_Delegate_Request_OnCompleted& InDelegate,
        bool InDestroyTargetOnFailure)
    -> FCk_Handle_PendingEntityScript
{
    auto Pending = TryAdmit(InScriptEntity, InEntityScriptClassArchetype,
        InSpawnParams, InOptionalFunc, InDelegate, InDestroyTargetOnFailure);
    if (NOT ck::IsValid(Pending))
    {
        ck_entity_script_utils::RetireRejectedOwnedTarget(InScriptEntity, InDestroyTargetOnFailure);
        InDelegate.ExecuteIfBound(InScriptEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
    }
    return Pending;
}

auto
    UCk_Utils_EntityScript_UE::
    TryInjectEntityScriptSpawnParams(
        UCk_EntityScript_UE* InEntityScript,
        const FInstancedStruct& InSpawnParams)
    -> void
{
    if (ck::Is_NOT_Valid(InEntityScript) || ck::Is_NOT_Valid(InSpawnParams))
    { return; }

    if (const auto& SpawnParamsStruct = InSpawnParams.GetScriptStruct();
        ck::IsValid(SpawnParamsStruct))
    {
        QUICK_SCOPE_CYCLE_COUNTER(TryInjectEntityScriptSpawnParams)
        const auto& SpawnParamsData = InSpawnParams.GetMemory();

        for (TFieldIterator<FProperty> PropIt(SpawnParamsStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
        {
            const auto* SpawnParamsProp = *PropIt;

            const auto& PropertyName = UCk_Utils_Reflection_UE::Get_SanitizedUserDefinedPropertyName(SpawnParamsProp);
            const auto* EntityScriptProp = UCk_Utils_Reflection_UE::Get_PropertyBySanitizedName(InEntityScript, PropertyName);

            if (ck::Is_NOT_Valid(EntityScriptProp))
            {
                // BP must have all properties in struct; code-authored (C++/AS) classes may legitimately receive
                // a shared struct and ignore extra fields. CLASS_CompiledFromBlueprint cannot be used to tell
                // them apart — the AS engine fork sets that flag on AngelScript classes too.
                CK_ENSURE_IF_NOT(ck::Is_NOT_Valid(::Cast<UBlueprintGeneratedClass>(InEntityScript->GetClass()), ck::IsValid_Policy_NullptrOnly{}),
                    TEXT("Failed to find ExposedOnSpawn Property [{}] on BP Entity Script [{}]. Cannot inject this SpawnParam"),
                    PropertyName,
                    InEntityScript)
                {}
                continue;
            }

            auto* EntityScriptPropAddr = EntityScriptProp->ContainerPtrToValuePtr<uint8>(InEntityScript);
            const auto* SpawnParamsPropAddr = SpawnParamsProp->ContainerPtrToValuePtr<uint8>(SpawnParamsData);

            // A request may retain a weak UObject reference even when the constructed script owns the object
            // strongly. Resolve the weak identity only here, while writing into the GC-traced EntityScript
            // UObject — this avoids retaining a bare actor address in the EnTT request queue.
            if (const auto* SpawnWeakObjectProp = CastField<FWeakObjectProperty>(SpawnParamsProp))
            {
                if (const auto* ScriptObjectProp = CastField<FObjectPropertyBase>(EntityScriptProp))
                {
                    ScriptObjectProp->SetObjectPropertyValue(
                        EntityScriptPropAddr,
                        SpawnWeakObjectProp->GetObjectPropertyValue(SpawnParamsPropAddr));
                    continue;
                }
            }

#if ENABLE_MT_DETECTOR
            // ---- Delegate MRSW bypass ----
            // Delegates start with an FMRSWRecursiveAccessDetector; a raw memory copy through FInstancedStruct
            // can leave it in a stale "writer active" state, causing false-positive race ensures. Initialize the
            // destination for a clean detector, then memcpy only the data fields that live after it.
            if (CastField<FDelegateProperty>(SpawnParamsProp))
            {
                const auto PropertySize = SpawnParamsProp->GetSize();
                EntityScriptProp->InitializeValue(EntityScriptPropAddr);

                constexpr auto DetectorSize = sizeof(FMRSWRecursiveAccessDetector);
                const auto DataSize = PropertySize - DetectorSize;
                if (DataSize > 0)
                {
                    FMemory::Memcpy(
                        EntityScriptPropAddr + DetectorSize,
                        SpawnParamsPropAddr + DetectorSize,
                        DataSize);
                }
                continue;
            }
#endif

            EntityScriptProp->CopyCompleteValue(EntityScriptPropAddr, SpawnParamsPropAddr);
        }
    }
}

auto
    UCk_Utils_EntityScript_UE::
    Request_ReplicateEntityScript(
        FCk_Handle& InHandle,
        const FCk_Handle& InReplicatedOwner,
        UCk_EntityScript_UE* InEntityScript,
        const FInstancedStruct& InSpawnParams,
        const TOptional<FCk_Handle>& InContextOwnerOverride)
    -> void
{
    if (NOT ck_entity_script_utils::ValidateRetainedSpawnParams(TEXT("Request_ReplicateEntityScript"), InSpawnParams))
    { return; }

    CK_ENSURE_IF_NOT(ck::IsValid(InEntityScript),
        TEXT("Cannot replicate EntityScript for [{}]: the EntityScript instance is invalid"), InHandle)
    { return; }

    auto ReplicatedOwner = InReplicatedOwner;
    const auto IsSelfOwned = ReplicatedOwner == InHandle;

    if (NOT IsSelfOwned)
    {
        CK_ENSURE_IF_NOT(UCk_Utils_Net_UE::Get_Replication(ReplicatedOwner) == ECk_Replication::Replicates,
            TEXT("Cannot replicate EntityScript for [{}]: replicated owner [{}] does NOT replicate"),
            InHandle, ReplicatedOwner)
        { return; }
    }

    CK_ENSURE_IF_NOT(InHandle.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>(),
        TEXT("Cannot replicate EntityScript for [{}]: entity has no ReplicationDriver"), InHandle)
    { return; }

    const auto& OwnerReplicationDriver = IsSelfOwned
        ? InHandle.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>()
        : ReplicatedOwner.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();

    CK_ENSURE_IF_NOT(ck::IsValid(OwnerReplicationDriver),
        TEXT("Cannot replicate EntityScript for [{}]: owner [{}] ReplicationDriver is invalid"),
        InHandle, ReplicatedOwner)
    { return; }

    if (ReplicatedOwner.Has<ck::FTag_EntityJustCreated>())
    {
        OwnerReplicationDriver->Set_ExpectedNumberOfDependentReplicationDrivers(
            OwnerReplicationDriver->Get_ExpectedNumberOfDependentReplicationDrivers() + 1);
    }

    UCk_Utils_EntityReplicationDriver_UE::Request_Replicate(
        InHandle, ReplicatedOwner, InEntityScript->GetClass(), InSpawnParams, InContextOwnerOverride);

    InHandle.Add<ck::FTag_EntityReplicationDriver_FireOnDependentReplicationComplete>();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_PendingEntityScript_UE::
    Promise_OnConstructed(
        FCk_Handle_PendingEntityScript& InPendingEntityScript,
        const FCk_Delegate_EntityScript_Constructed& InDelegate)
    -> void
{
    // Request_SpawnEntity returns an INVALID pending handle when the spawn was suppressed (notably during a
    // CkSnapshot load's world reconstitution) or after it already ensured on a bad class/owner. Binding would
    // then AddOrGet the signal fragment on a TOMBSTONE handle and ensure; no-op is correct — nothing spawned.
    if (ck::Is_NOT_Valid(InPendingEntityScript.Get_EntityUnderConstruction()))
    { return; }

    ck::UUtils_Signal_OnConstructed_PostFireUnbind::Bind(
        InPendingEntityScript.Get_EntityUnderConstruction(), InDelegate, ECk_Signal_BindingPolicy::FireIfPayloadInFlight);
}

auto
    UCk_Utils_PendingEntityScript_UE::
    Get_IsValid(
        const FCk_Handle_PendingEntityScript& InHandle)
    -> bool
{
    return ck::IsValid(InHandle);
}

auto
    UCk_Utils_PendingEntityScript_UE::
    Request_DestroyEntity(
        FCk_Handle_PendingEntityScript& InHandle,
        ECk_EntityLifetime_DestructionBehavior InDestructionBehavior)
    -> void
{
    auto EntityUnderConstruction = InHandle.Get_EntityUnderConstruction();
    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(EntityUnderConstruction, InDestructionBehavior);
}

// --------------------------------------------------------------------------------------------------------------------
