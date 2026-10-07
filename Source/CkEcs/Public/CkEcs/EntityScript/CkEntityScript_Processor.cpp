#include "CkEntityScript_Processor.h"

#include "CkEntityScript.h"
#include "CkEntityScript_Utils.h"
#include "CkGenericEntityScript.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Time/CkTime_Utils.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h" // FTag_DestroyEntity_* (owner-liveness cancel)
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_SpawnRecipe.h" // FFragment_SpawnRecipe retention (RuntimeSpawned)
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/TransientEntity/CkTransientEntity_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Tag/CkTag_EditorOnly.h"

#include <HAL/IConsoleManager.h>

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_SpawnEntity_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_SpawnEntity_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_ContinueConstruction);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_Replicate);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_FinishConstruction);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_PendingReplicationRetry);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_BeginPlay);
CK_REGISTER_PROCESSOR(ck::FProcessor_EntityScript_EndPlay);
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_entity_script_spawn_trace
{
    static TAutoConsoleVariable<int32>
        CVar_LogSpawnRequestVisits(
            TEXT("ck.EntityScript.LogSpawnRequestVisits"),
            0,
            TEXT(
                "Logs every EntityScript spawn-request visit with its class, "
                "owner, new entity, request entity, frame, and disposition. "
                "Diagnostic only; leave disabled during performance capture."),
            ECVF_Default);

    auto
    LogVisit(
        const TCHAR* InDisposition,
        const FCk_Handle& InRequestEntity,
        const FCk_Request_EntityScript_SpawnEntity&
            InRequest)
        -> void
    {
        if (CVar_LogSpawnRequestVisits
                .GetValueOnGameThread()
            <= 0)
        { return; }

        ck::ecs::Display(
            TEXT(
                "[EntityScript SpawnTrace] frame=[{}] disposition=[{}] "
                "script=[{}] entity=[{}] owner=[{}] request=[{}]"),
            GFrameCounter,
            InDisposition,
            InRequest.Get_EntityScriptClassArchetype(),
            InRequest.Get_NewEntity(),
            InRequest.Get_Owner(),
            InRequestEntity);
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_EntityScript_SpawnEntity_HandleRequests::
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity& InRequestFragment)
        -> void
    {
        // The request entity can still be visited while tagged Initiate. Reject both a dying
        // lifetime owner and a separately destroyed pending target before creating the script or
        // calling Construct. Check handle validity before querying tags: finalized handles are tombstones.
        const auto& LifetimeOwner = InRequestFragment.Get_Owner();
        const auto& NewEntity = InRequestFragment.Get_NewEntity();
        const auto OwnerReady = ck::IsValid(LifetimeOwner) &&
            NOT LifetimeOwner.Has_Any<FTag_DestroyEntity_Initiate, FTag_DestroyEntity_EndPlay,
                FTag_DestroyEntity_Teardown, FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>();
        const auto TargetReady = ck::IsValid(NewEntity) &&
            NOT NewEntity.Has_Any<FTag_DestroyEntity_Initiate, FTag_DestroyEntity_EndPlay,
                FTag_DestroyEntity_Teardown, FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>();
        if (NOT OwnerReady || NOT TargetReady)
        {
            ck_entity_script_spawn_trace::LogVisit(
                TEXT("cancelled"),
                InHandle,
                InRequestFragment);

            ck::ecs::Verbose(TEXT("Cancelling EntityScript spawn: lifetime owner or pending target is "
                "destroyed or pending-destroy"));

            InRequestFragment.TryFireCompletion(InRequestFragment.Get_NewEntity(),
                ECk_Request_OperationResult::Failed_Cancelled);

            if (InRequestFragment.Get_DestroyTargetOnFailure())
            {
                if (auto OrphanedEntity = InRequestFragment.Get_NewEntity();
                    ck::IsValid(OrphanedEntity))
                { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(OrphanedEntity); }
            }

            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InHandle);
            InHandle.Remove<MarkedDirtyBy>();
            return;
        }

        // A child EntityScript spawned from inside a parent's Construct callback must wait for the parent to
        // finish its own pipeline — these two tags are the true "construction in progress" markers.
        if (LifetimeOwner.Has_Any<FTag_EntityScript_ContinueConstruction, FTag_EntityScript_FinishConstruction>())
        {
            ck_entity_script_spawn_trace::LogVisit(
                TEXT("deferred"),
                InHandle,
                InRequestFragment);
            return;
        }

        ck_entity_script_spawn_trace::LogVisit(
            TEXT("execute"),
            InHandle,
            InRequestFragment);
        DoHandleRequest(InHandle, InRequestFragment);
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InHandle);
        InHandle.Remove<MarkedDirtyBy>();
    }

    auto
        FProcessor_EntityScript_SpawnEntity_HandleRequests::
        DoHandleRequest(
            HandleType InHandle,
            const FCk_Request_EntityScript_SpawnEntity& InRequest)
        -> void
    {
        QUICK_SCOPE_CYCLE_COUNTER(FCk_Request_EntityScript_SpawnEntity)

        // The guard holds Result by reference and fires at scope exit, so it must stay declared after it.
        // Every early-out below is a genuine failure to spawn; reaching the end IS the success condition.
        auto Result = ECk_Request_OperationResult::Failed;
        const auto Guard = MakeCompletionGuard(InRequest, InRequest.Get_NewEntity(), Result);

        // Spawn wrappers and explicitly owning Add callers transfer their newly allocated target.
        // Ordinary direct Add callers retain their target and its existing settings.
        const auto RetireOwnedTarget = [&]() -> void
        {
            if (NOT InRequest.Get_DestroyTargetOnFailure())
            { return; }
            auto Target = InRequest.Get_NewEntity();
            if (ck::IsValid(Target))
            { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Target); }
        };

        const auto EntityScriptClassArchetype = InRequest.Get_EntityScriptClassArchetype();

#if WITH_EDITOR
        // Editor preview churn is expected. The same ownership rule still applies.
        if (NOT ck::IsValid(EntityScriptClassArchetype) && InHandle.Has<FTag_EditorOnlyEntity>())
        {
            ck::ecs::Verbose(TEXT("Dropping editor-only spawn request [{}] — instanced archetype died before processing (drag-drop preview churn)"), InHandle);
            RetireOwnedTarget();
            return;
        }
#endif

        const auto ArchetypeReady = ck::IsValid(EntityScriptClassArchetype);
        CK_ENSURE_IF_NOT(ArchetypeReady,
            TEXT("EntityScriptArchetype [{}] is INVALID. Cannot Spawn Entity"), EntityScriptClassArchetype) {}
        if (NOT ArchetypeReady)
        {
            RetireOwnedTarget();
            return;
        }

        auto NewEntity = InRequest.Get_NewEntity();
        const auto LifetimeOwner = InRequest.Get_Owner();
        const auto OwnerReady = ck::IsValid(LifetimeOwner) &&
            NOT LifetimeOwner.Has_Any<FTag_DestroyEntity_Initiate, FTag_DestroyEntity_EndPlay,
                FTag_DestroyEntity_Teardown, FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>();
        const auto TargetReady = ck::IsValid(NewEntity) &&
            NOT NewEntity.Has_Any<FTag_DestroyEntity_Initiate, FTag_DestroyEntity_EndPlay,
                FTag_DestroyEntity_Teardown, FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>();
        if (NOT OwnerReady || NOT TargetReady)
        {
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }
        const auto OwnerCanCreate = UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(LifetimeOwner);
        if (NOT OwnerCanCreate)
        {
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }

        auto* OwnerWorld = static_cast<UWorld*>(nullptr);
        auto* TargetWorld = static_cast<UWorld*>(nullptr);
        const auto OwnerContextReady =
            ck_entity_script_utils::TryResolveSpawnOwnerWorld(LifetimeOwner, OwnerWorld);
        CK_ENSURE_IF_NOT(OwnerContextReady, TEXT("Queued EntityScript owner has no safe live World chain")) {}
        if (NOT OwnerContextReady)
        {
            RetireOwnedTarget();
            return;
        }
        const auto TargetContextReady =
            ck_entity_script_utils::TryResolveSpawnOwnerWorld(NewEntity, TargetWorld);
        CK_ENSURE_IF_NOT(TargetContextReady, TEXT("Queued EntityScript target has no safe live World chain")) {}
        if (NOT TargetContextReady)
        {
            RetireOwnedTarget();
            return;
        }
        const auto WorldsMatch = OwnerWorld == TargetWorld;
        CK_ENSURE_IF_NOT(WorldsMatch, TEXT("Queued EntityScript owner and target resolve different Worlds")) {}
        if (NOT WorldsMatch)
        {
            RetireOwnedTarget();
            return;
        }

        const auto HasNetParams = NewEntity.Has<ck::FFragment_Net_Params>();
        CK_ENSURE_IF_NOT(HasNetParams,
            TEXT("Entity [{}] is missing NetParams before construction. EntityScript: [{}]. "
                 "Did a caller enqueue FFragment_EntityScript_RequestSpawnEntity directly instead "
                 "of going through UCk_Utils_EntityScript_UE::Add?"),
            NewEntity, EntityScriptClassArchetype) {}
        if (NOT HasNetParams)
        {
            RetireOwnedTarget();
            return;
        }
        const auto& TargetSettings = NewEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
        const auto TargetNetMode = TargetSettings.Get_NetMode();
        const auto NetModeReady = TargetSettings.Get_Replication() != ECk_Replication::Replicates ||
            TargetNetMode == ECk_Net_NetModeType::Client ||
            TargetNetMode == ECk_Net_NetModeType::Host ||
            TargetNetMode == ECk_Net_NetModeType::ClientAndHost;
        CK_ENSURE_IF_NOT(NetModeReady, TEXT("Queued replicated EntityScript target has invalid net mode")) {}
        if (NOT NetModeReady)
        {
            RetireOwnedTarget();
            return;
        }
        const auto HasNoScript = NOT NewEntity.Has<FFragment_EntityScript>();
        CK_ENSURE_IF_NOT(HasNoScript, TEXT("Queued EntityScript target already has a script")) {}
        if (NOT HasNoScript)
        {
            RetireOwnedTarget();
            return;
        }

        const auto ReplicationReady = ck_entity_script_utils::TryValidateSpawnReplication(
            LifetimeOwner, EntityScriptClassArchetype.Get(), &NewEntity, TargetSettings.Get_Replication(), true);
        if (NOT ReplicationReady)
        {
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }

        const auto InstancingPolicy = EntityScriptClassArchetype->Get_InstancingPolicy();
        const auto PolicyReady =
            InstancingPolicy == ECk_EntityScript_InstancingPolicy::NotInstanced ||
            InstancingPolicy == ECk_EntityScript_InstancingPolicy::InstancedPerEntity ||
            InstancingPolicy == ECk_EntityScript_InstancingPolicy::InstancedPerEntity_Poolable;
        CK_ENSURE_IF_NOT(PolicyReady, TEXT("Queued EntityScript has invalid instancing policy")) {}
        if (NOT PolicyReady)
        {
            RetireOwnedTarget();
            return;
        }

        auto* NewEntityScript = static_cast<UCk_EntityScript_UE*>(nullptr);
        if (InstancingPolicy == ECk_EntityScript_InstancingPolicy::NotInstanced)
        { NewEntityScript = EntityScriptClassArchetype.Get(); }
        else
        {
            QUICK_SCOPE_CYCLE_COUNTER(FCk_Request_EntityScript_SpawnEntity_CreateObject)
            const auto IsPoolable = InstancingPolicy ==
                ECk_EntityScript_InstancingPolicy::InstancedPerEntity_Poolable;
            const auto PoolParams = IsPoolable
                ? FCk_ObjectPooling_PoolParams{EntityScriptClassArchetype->Get_PoolParams()}
                    .Set_RecyclePolicy(ECk_ObjectPooling_RecyclePolicy::Recycle)
                : FCk_ObjectPooling_PoolParams{}
                    .Set_RecyclePolicy(ECk_ObjectPooling_RecyclePolicy::DestroyOnRelease);
            NewEntityScript = UCk_Utils_Object_UE::Request_CreateNewObject<UCk_EntityScript_UE>(
                OwnerWorld, EntityScriptClassArchetype->GetClass(), EntityScriptClassArchetype.Get(),
                PoolParams, nullptr);
        }

        const auto ScriptReady = ck::IsValid(NewEntityScript);
        CK_ENSURE_IF_NOT(ScriptReady,
            TEXT("Failed to Spawn New Entity using EntityScript [{}]"), EntityScriptClassArchetype) {}
        if (NOT ScriptReady)
        {
            RetireOwnedTarget();
            return;
        }

        if (const auto& SpawnParams = InRequest.Get_SpawnParams(); ck::IsValid(SpawnParams))
        { UCk_Utils_EntityScript_UE::TryInjectEntityScriptSpawnParams(NewEntityScript, SpawnParams); }

        // Object acquisition and property injection can invoke external code. No script has been
        // associated or published yet, so the instance can still be released safely on cancellation.
        const auto StillReady = ck::IsValid(NewEntity) &&
            NOT NewEntity.Has_Any<FTag_DestroyEntity_Initiate, FTag_DestroyEntity_EndPlay,
                FTag_DestroyEntity_Teardown, FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>();
        if (NOT StillReady)
        {
            if (InstancingPolicy != ECk_EntityScript_InstancingPolicy::NotInstanced)
            { UCk_Utils_Object_UE::TryReleaseToPool(NewEntityScript); }
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }

        const auto ConstructionContextReady = ck::IsValid(NewEntityScript) &&
            NewEntityScript->Get_IsConstructionContextReady(TargetWorld);
        if (NOT ConstructionContextReady)
        {
            if (InstancingPolicy != ECk_EntityScript_InstancingPolicy::NotInstanced)
            { UCk_Utils_Object_UE::TryReleaseToPool(NewEntityScript); }
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }

        const auto InjectedReplicationReady = NewEntity.Has<ck::FFragment_Net_Params>() &&
            ck_entity_script_utils::TryValidateSpawnReplication(LifetimeOwner, NewEntityScript, &NewEntity,
                NewEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings().Get_Replication(), true, true);
        if (NOT InjectedReplicationReady)
        {
            if (InstancingPolicy != ECk_EntityScript_InstancingPolicy::NotInstanced)
            { UCk_Utils_Object_UE::TryReleaseToPool(NewEntityScript); }
            Result = ECk_Request_OperationResult::Failed_Cancelled;
            RetireOwnedTarget();
            return;
        }

        NewEntityScript->_AssociatedEntity = NewEntity;
        NewEntity.Add<FFragment_EntityScript>(NewEntityScript);

        CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, NewEntity,
            TEXT("EntityScript created: {}"), EntityScriptClassArchetype);

        // ---- Retain the construction recipe (save/load rebuild+hydrate, RuntimeSpawned) ----------
        // The recipe is pinned by a GC-safe holder UObject because fragments are not GC-traced. Always stamped;
        // the save capture filters by provenance, so entities that carry one needlessly never consult it.
        {
            auto* Recipe = NewObject<UCk_EntityScript_SpawnRecipe_UE>(TargetWorld);
            Recipe->Populate(EntityScriptClassArchetype->GetClass(), InRequest.Get_SpawnParams());
            NewEntity.Add<ck::FFragment_SpawnRecipe>(TStrongObjectPtr<UCk_EntityScript_SpawnRecipe_UE>{Recipe});
        }

        ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] SpawnProcessor — Entity=[{}] Replication=[{}]"),
            NewEntity, NewEntityScript->Get_EffectiveReplication());

        // ---- Replication Driver (before Construct) ----------------------------------------
        // Non-actor-bearing replicated entities need their driver present BEFORE Construct so utilities called
        // during Construct find it. Entities that receive their own OwningActor later during Construct (WithActor)
        // defer driver creation until OwningActor::Add in Construct, even if an ancestor already owns an actor.
        if (NOT NewEntityScript->Get_RequiresOwningActorBeforeReplicationDriver())
        { UCk_Utils_EntityReplicationDriver_UE::TryAdd(NewEntity); }

        // ---- Construct --------------------------------------------------------------------
        switch (NewEntityScript->Construct(NewEntity, InRequest.Get_SpawnParams()))
        {
            case ECk_EntityScript_ConstructionFlow::Finished:
            {
                const auto& ContinueConstructionFuncName = GET_FUNCTION_NAME_CHECKED(UCk_GenericEntityScript_UE, DoContinueConstruction);
                CK_ENSURE_IF_NOT(NOT NewEntityScript->GetClass()->IsFunctionImplementedInScript(ContinueConstructionFuncName),
                    TEXT("EntityScript [{}] Construction is FINISHED, but the script [{}] implements the [ContinueConstruction] event!\n"
                         "This event will be ignored as it is only invoked for ONGOING construction of EntityScript"),
                NewEntity, NewEntityScript) {}

                CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, NewEntity,
                    TEXT("Construct() returned Finished"));
                NewEntity.Add<FTag_EntityScript_FinishConstruction>();
                break;
            }
            case ECk_EntityScript_ConstructionFlow::Continue:
            {
                CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, NewEntity,
                    TEXT("Construct() returned Continue"));
                NewEntity.Add<FTag_EntityScript_ContinueConstruction>();
                break;
            }
        }

        // ---- Replication Infrastructure (after Construct) ---------------------------------
        // The driver already exists — added pre-Construct, or during Construct via UCk_Utils_OwningActor_UE::Add.
        if (NewEntityScript->Get_EffectiveReplication() == ECk_Replication::Replicates)
        {
            // The entity's live ContextOwner fragment is authoritative: it reflects the inherited value AND any
            // retarget the spawner applied. Leave the override unset when the entity is its own ContextOwner —
            // OnRep then maps it back to self on the client rather than to the replicated lifetime-owner.
            auto ContextOwnerOverride = TOptional<FCk_Handle>{};
            if (UCk_Utils_ContextOwner_UE::Has(NewEntity))
            {
                if (const auto EntityContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(NewEntity);
                    EntityContextOwner != NewEntity)
                { ContextOwnerOverride = EntityContextOwner; }
            }

            const auto HasOwningActor = UCk_Utils_OwningActor_UE::Has(NewEntity);
            ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] SpawnProcessor — HasOwningActor=[{}]"), HasOwningActor);

            if (HasOwningActor)
            {
                const auto* OwningActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(NewEntity);
                const auto* World = OwningActor->GetWorld();

                const auto IsNetworkedAuthority =
                    World->IsNetMode(NM_DedicatedServer) || World->IsNetMode(NM_ListenServer);

                ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] SpawnProcessor — WithActor path, IsNetworkedAuthority=[{}]"),
                    IsNetworkedAuthority);

                // Only on a networked authority: NM_Standalone has no net driver, and on NM_Client
                // authority-side replication is not our concern.
                if (IsNetworkedAuthority)
                {
                    NewEntity.Add<ck::FRequest_EntityScript_Replicate>(
                        NewEntity, ContextOwnerOverride, InRequest.Get_SpawnParams(), NewEntityScript);
                }
            }
            else
            {
                auto ReplicatedOwner = InRequest.Get_Owner();
                const auto IsHost = UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(ReplicatedOwner);
                ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] SpawnProcessor — Non-WithActor path, Owner=[{}] IsHost=[{}]"),
                    ReplicatedOwner, IsHost);

                if (IsHost)
                {
                    NewEntity.Add<ck::FRequest_EntityScript_Replicate>(
                        ReplicatedOwner, ContextOwnerOverride, InRequest.Get_SpawnParams(), NewEntityScript);
                }
                else
                {
                    ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] SpawnProcessor — SKIPPED replication request (owner is not host)"));
                }
            }
        }

        if (InRequest.Get_PostConstruction_Func())
        {
            InRequest.Get_PostConstruction_Func()(NewEntity);
        }

        Result = ECk_Request_OperationResult::Succeeded;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_SpawnEntity_CancelPendingRequests::
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity& InRequestFragment)
        -> void
    {
        InRequestFragment.TryFireCompletion(InRequestFragment.Get_NewEntity(),
            ECk_Request_OperationResult::Failed_Cancelled);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_ContinueConstruction::
        ForEachEntity(
            [[maybe_unused]]
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript)
        -> void
    {
        InHandle.Remove<MarkedDirtyBy>();

        const auto& EntityScript = InEntityScript.Get_Script().Get();

        CK_ENSURE_IF_NOT(ck::IsValid(EntityScript),
            TEXT("EntityScript is INVALID for [{}] when attempting to invoke ContinueConstruction on it"), InHandle)
        { return; }

        CK_CALLSTACK_RECORD(ck::FFragment_EntityScript, InHandle);
        EntityScript->ContinueConstruction(InHandle);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_Replicate::
        ForEachEntity(
            const TimeType&,
            HandleType InHandle,
            const FRequest_EntityScript_Replicate& InRequest)
        -> void
    {
        const auto& EntityScript = InRequest.Get_Script();

        CK_ENSURE_IF_NOT(ck::IsValid(EntityScript),
            TEXT("EntityScript is INVALID for [{}] when attempting to invoke ContinueConstruction on it"), InHandle)
        {
            InHandle.Remove<MarkedDirtyBy>();
            return;
        }

        auto ReplicatedOwner = InRequest.Get_Owner();
        const auto IsSelfOwned = ReplicatedOwner == InHandle;

        ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] ReplicateProcessor — Handle=[{}] Owner=[{}] IsSelfOwned=[{}]"),
            InHandle, ReplicatedOwner, IsSelfOwned);

        // Defer (retry next frame, KEEP the dirty marker) until the ReplicationDriver exists: WithActor entities
        // acquire theirs via OwningActor::Add slightly after this request lands.
        if (NOT InHandle.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>())
        {
            ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] ReplicateProcessor — Handle [{}] does NOT have ReplicationDriver yet, deferring"), InHandle);
            return;
        }

        // NOTE: Copy on purpose otherwise the data doesn't make its way over correctly
        const auto SpawnParamsCopy = InRequest.Get_SpawnParams();

        ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] ReplicateProcessor — Calling Request_ReplicateEntityScript for [{}] with owner [{}]"), InHandle, ReplicatedOwner);

        UCk_Utils_EntityScript_UE::Request_ReplicateEntityScript(InHandle, ReplicatedOwner, EntityScript.Get(), SpawnParamsCopy,
            InRequest.Get_ContextOwnerOverride());

        InHandle.Remove<MarkedDirtyBy>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_FinishConstruction::
        ForEachEntity(
            const TimeType&,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript)
        -> void
    {
        InHandle.Remove<MarkedDirtyBy>();

        CK_CALLSTACK_RECORD_MSG(ck::FFragment_EntityScript, InHandle,
            TEXT("Construction finished, ready for BeginPlay"));
        InHandle.Add<FTag_EntityScript_BeginPlay>();

        // Mark "composed this frame" so the FGroup_DeferredApply dispatchers defer their apply until this frame's
        // pump / next frame — the feature Setups this construction just queued have not drained yet.
        InHandle.AddOrGet<FTag_EntityScript_ConstructedThisFrame>();

        UUtils_Signal_OnConstructed::Broadcast(InHandle, ck::MakePayload(InHandle));

        if (ck::Is_NOT_Valid(InEntityScript.Get_Script()))
        { return; }

        auto WasConsumed = false;
        auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);

        if (ck::IsValid(LifetimeOwner) && LifetimeOwner.Has<FFragment_PendingReplication>())
        {
            auto& PendingFragment = LifetimeOwner.AddOrGet<FFragment_PendingReplication>();
            auto* Cdo = InEntityScript.Get_Script()->GetClass()->GetDefaultObject<UCk_EntityScript_UE>();
            auto PendingEntity = PendingFragment.ConsumeFirst(
                InEntityScript.Get_Script()->GetClass(), Cdo, InHandle);

            if (ck::IsValid(PendingEntity))
            {
                UUtils_Signal_OnConstructed::Broadcast(PendingEntity, ck::MakePayload(InHandle));
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(PendingEntity);
                WasConsumed = true;
            }
        }

        if (NOT WasConsumed
            && InEntityScript.Get_Script()->Get_EffectiveReplication() == ECk_Replication::Replicates
            && ck::IsValid(LifetimeOwner)
            && UCk_Utils_Net_UE::Get_EntityNetMode(LifetimeOwner) == ECk_Net_NetModeType::Client)
        {
            const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
            // WALL time: this backoff is a kernel watchdog that has to keep running while a snapshot load holds
            // game time frozen — one that freezes with the world it is retrying against is a wedge, not a retry.
            const auto TimeParams = FCk_Utils_Time_GetWorldTime_Params{World}
                .Set_TimeType(ECk_Time_WorldTimeType::RealTime);
            const auto CurrentTime = UCk_Utils_Time_UE::Get_WorldTime(TimeParams).Get_WorldTime().Get_Time();

            InHandle.Add<FTag_EntityScript_PendingReplicationRetry>();
            InHandle.Add<FFragment_EntityScript_PendingReplicationRetryTimestamp>(CurrentTime);
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

#if UE_BUILD_SHIPPING
    constexpr auto PendingReplicationRetryTimeoutSeconds = 2.0;
#else
    constexpr auto PendingReplicationRetryTimeoutSeconds = 10.0;
#endif

    auto
        FProcessor_EntityScript_PendingReplicationRetry::
        ForEachEntity(
            const TimeType&,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript,
            const FFragment_EntityScript_PendingReplicationRetryTimestamp& InTimestamp)
        -> void
    {
        auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);

        if (ck::IsValid(LifetimeOwner) && LifetimeOwner.Has<FFragment_PendingReplication>())
        {
            auto& PendingFragment = LifetimeOwner.AddOrGet<FFragment_PendingReplication>();
            auto* Cdo = InEntityScript.Get_Script()->GetClass()->GetDefaultObject<UCk_EntityScript_UE>();
            auto PendingEntity = PendingFragment.ConsumeFirst(
                InEntityScript.Get_Script()->GetClass(), Cdo, InHandle);

            if (ck::IsValid(PendingEntity))
            {
                UUtils_Signal_OnConstructed::Broadcast(PendingEntity, ck::MakePayload(InHandle));
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(PendingEntity);

                InHandle.Remove<FTag_EntityScript_PendingReplicationRetry>();
                InHandle.Remove<FFragment_EntityScript_PendingReplicationRetryTimestamp>();
                return;
            }
        }

        const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        // WALL time, matching the stamp above: a load freezes game time, so a game-timed elapsed would read zero
        // for the whole hold and this timeout could never fire inside the window it exists to bound.
        const auto TimeParams = FCk_Utils_Time_GetWorldTime_Params{World}
            .Set_TimeType(ECk_Time_WorldTimeType::RealTime);
        const auto CurrentTime = UCk_Utils_Time_UE::Get_WorldTime(TimeParams).Get_WorldTime().Get_Time();
        const auto ElapsedSeconds = (CurrentTime - InTimestamp.Get_TaggedAt()).Get_Seconds();

        if (ElapsedSeconds >= PendingReplicationRetryTimeoutSeconds)
        {
            ck::ecs::Warning(
                TEXT("PendingReplicationRetry timed out after [{}]s for entity [{}] with script [{}]. "
                     "No matching PendingEntity was found on the client."),
                ElapsedSeconds,
                InHandle,
                InEntityScript.Get_Script()->GetClass()->GetName());

            InHandle.Remove<FTag_EntityScript_PendingReplicationRetry>();
            InHandle.Remove<FFragment_EntityScript_PendingReplicationRetryTimestamp>();
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_BeginPlay::
        ForEachEntity(
            const TimeType&,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript)
        -> void
    {
        InHandle.Remove<MarkedDirtyBy>();

        const auto& EntityScript = InEntityScript.Get_Script().Get();

        CK_ENSURE_IF_NOT(ck::IsValid(EntityScript), TEXT("EntityScript is INVALID for [{}] when attempting to invoke BeginPlay on it"), InHandle)
        { return; }

        CK_CALLSTACK_RECORD(ck::FFragment_EntityScript, InHandle);
        EntityScript->BeginPlay();

        InHandle.Add<FTag_EntityScript_HasBegunPlay>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_EntityScript_EndPlay::
        ForEachEntity(
            const TimeType&,
            HandleType InHandle,
            FFragment_EntityScript& InEntityScript)
        -> void
    {
        auto* EntityScript = InEntityScript.Get_Script().Get();

        CK_ENSURE_IF_NOT(ck::IsValid(EntityScript), TEXT("EntityScript is INVALID for [{}] when attempting to invoke EndPlay on it"), InHandle)
        { return; }

        CK_CALLSTACK_RECORD(ck::FFragment_EntityScript, InHandle);
        EntityScript->EndPlay();
        InHandle.Add<FTag_EntityScript_HasEndedPlay>();

        // release the instance (Poolable recycles, force-new unpins; no-op for CDO/snapshot scripts).
        // Clear the weak ptr so a same-frame re-issue is unobservable through this dead entity
        UCk_Utils_Object_UE::TryReleaseToPool(EntityScript);
        InEntityScript._Script.Reset();
    }
}

// --------------------------------------------------------------------------------------------------------------------

