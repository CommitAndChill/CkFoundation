#include "CkEntityReplicationDriver_Utils.h"
#include "CkEcs/EntityConstructionScript/CkEntity_ConstructionScript.h"
#include "CkEcs/Persistence/CkRuntimeArchetype_Registry.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"

#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Fragment.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_BuildRecipe.h" // FFragment_BuildRecipe retention (definition-built entity recipe)
#include "CkEcs/Net/ReplicatedFragmentContainer/CkReplicatedFragmentContainer.h" // FTag_RepFragments_PendingApply
#include "CkEcs/Persistence/CkPersistenceHydration.h" // FFragment_PendingHydration
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h" // Get_WorldForEntity (recipe holder outer)
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h" // FTag_DefinitionBuild_InProgress (construction-window stamp for labeled children)

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Algorithms/CkAlgorithms.h" // ck::algo::AnyOf (undrained-dependents recursion)

#include "Misc/ScopeExit.h" // ON_SCOPE_EXIT (DefinitionBuild construction-window tag removal)

#if UE_WITH_IRIS
#include <Iris/ReplicationSystem/ReplicationSystem.h>
#include <Net/Iris/ReplicationSystem/EngineReplicationBridge.h>
#endif

#include <Engine/PackageMapClient.h>
#include <GameFramework/Actor.h>
#include <Engine/NetDriver.h>
#include <numeric>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_entity_replication_driver_utils
{
    // A runtime-minted archetype is RF_Transient, so it is not net-addressable: the object reference in the
    // ConstructionInfo arrives NULL on every client, which then builds from the class default and comes back
    // holding a husk the server does not have. The PATH is the identity a registered runtime-archetype provider
    // re-mints from on the far side, so every builder stamps it rather than only the loader that first needed
    // it. An ASSET archetype is skipped: a NetGUID already carries that reference, so the path is pure wire cost.
    auto
        DoStamp_ArchetypeIdentity(
            TArray<FCk_EntityReplicationDriver_Spec> InConstructionInfos)
        -> TArray<FCk_EntityReplicationDriver_Spec>
    {
        for (auto& ConstructionInfo : InConstructionInfos)
        {
            const auto* Archetype = ConstructionInfo.Get_ConstructionScriptArchetype().Get();

            if (ck::Is_NOT_Valid(Archetype) || Archetype->IsAsset())
            { continue; }

            if (NOT ConstructionInfo.Get_ArchetypeIdentityPath().IsEmpty())
            { continue; }

            ConstructionInfo.Set_ArchetypeIdentityPath(Archetype->GetPathName());
        }

        return InConstructionInfos;
    }

    auto
        Get_IsInDestruction(
            const FCk_Handle& InHandle)
        -> bool
    {
        return InHandle.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>();
    }

    auto
        Get_IsOwningEntityDriverDependentOn(
            UCk_Fragment_EntityReplicationDriver_Rep* InOwningEntityDriver,
            UCk_Fragment_EntityReplicationDriver_Rep* InDriver)
        -> bool
    {
        return ck::IsValid(InOwningEntityDriver)
            && InOwningEntityDriver != InDriver
            && InOwningEntityDriver->Get_AssociatedEntity().Has<ck::FTag_EntityJustCreated>();
    }

    auto
        DoPublish_LifetimeOwner(
            const FCk_Handle& InEntity,
            TSet<FCk_Entity>& InOutVisited)
        -> void
    {
        auto AlreadyVisited = false;
        InOutVisited.Add(InEntity.Get_Entity(), &AlreadyVisited);

        CK_ENSURE_IF_NOT(NOT AlreadyVisited,
            TEXT("Cyclic LifetimeDependents detected at Entity [{}] while publishing lifetime owners. The lifetime-ownership "
                "graph must be a tree. Truncating to avoid infinite recursion."), InEntity)
        { return; }

        if (NOT UCk_Utils_EntityReplicationDriver_UE::Has(InEntity))
        {
            for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InEntity))
            { DoPublish_LifetimeOwner(Dependent, InOutVisited); }

            return;
        }

        if (Get_IsInDestruction(InEntity))
        { return; }

        const auto& Driver = InEntity.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();

        CK_ENSURE_IF_NOT(ck::IsValid(Driver),
            TEXT("Entity [{}] has an invalid ReplicationDriver. Unable to publish its lifetime owner."), InEntity)
        { return; }

        Driver->Request_PublishOwningEntityDriver(UCk_Utils_EntityReplicationDriver_UE::Get_PublishedOwningEntityDriver(InEntity));
    }
}

// --------------------------------------------------------------------------------------------------------------------
auto
    UCk_Utils_EntityReplicationDriver_UE::
    TryAdd(
        FCk_Handle& InHandle)
    -> ECk_AddedOrNot
{
    const auto HasNetParams = UCk_Utils_Net_UE::Has(InHandle);
    const auto EntityReplication = HasNetParams ? UCk_Utils_Net_UE::Get_EntityReplication(InHandle) : ECk_Replication::DoesNotReplicate;
    const auto IsHost = UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(InHandle);
    const auto HasOwningActor = UCk_Utils_EntityBridge_UE::TryGet_BridgedEntity_InOwnershipChain(InHandle);

    ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] ReplicationDriver::Add — Handle=[{}] HasNetParams=[{}] EntityReplication=[{}] IsHost=[{}] HasOwningActorInChain=[{}]"),
        InHandle, HasNetParams, EntityReplication, IsHost, ck::IsValid(HasOwningActor));

    const auto AddedOrNot = UCk_Utils_Net_UE::TryAddReplicatedFragment<UCk_Fragment_EntityReplicationDriver_Rep>(InHandle);

    ck::ecs::VeryVerbose(TEXT("[REP_DEBUG] ReplicationDriver::Add — Result=[{}]"), AddedOrNot);

    if (AddedOrNot == ECk_AddedOrNot::Added)
    {
        InHandle._ReplicationDriver = InHandle.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
        InHandle.Add<TWeakObjectPtr<UCk_Ecs_ReplicatedObject_UE>>(InHandle._ReplicationDriver);
    }
    return AddedOrNot;
}

// Cycle-safe DFS over the lifetime-dependents subtree. Lifetime ownership is a TREE, so a revisit means
// the graph is corrupt: truncating there keeps the recursion from overflowing the stack (which kills the
// process with NO crash dump) and the ensure names the entity. An acyclic graph never hits the visited set.
static auto
    DoCount_ReplicationDriversIncludingDependents(
        const FCk_Handle& InHandle,
        TSet<FCk_Entity>& InOutVisited)
    -> int32
{
    if (NOT UCk_Utils_EntityReplicationDriver_UE::Has(InHandle))
    { return 0; }

    auto AlreadyVisited = false;
    InOutVisited.Add(InHandle.Get_Entity(), &AlreadyVisited);

    if (AlreadyVisited)
    {
        CK_TRIGGER_ENSURE(TEXT("Cyclic LifetimeDependents detected at Entity [{}] while counting replication "
            "drivers — the lifetime-ownership graph must be a tree. Truncating to avoid infinite recursion."),
            InHandle);
        return 0;
    }

    const auto& Dependents = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InHandle);

    return std::accumulate(Dependents.begin(), Dependents.end(), 1, [&](const int32 Value, const FCk_Handle& Dependent)
    {
        return Value + DoCount_ReplicationDriversIncludingDependents(Dependent, InOutVisited);
    });
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_NumOfReplicationDriversIncludingDependents(
        const FCk_Handle& InHandle)
    -> int32
{
    auto Visited = TSet<FCk_Entity>{};
    return DoCount_ReplicationDriversIncludingDependents(InHandle, Visited);
}

// Cycle-safe DFS mirroring DoCount above, except it does NOT prune at non-driver entities: hydration runs
// on every net mode, so a non-replicated dependent's queued hydration must still block the aggregate fire.
static auto
    DoGet_HasUndrainedReplicatedFragments_Recursive(
        const FCk_Handle& InHandle,
        TSet<FCk_Entity>& InOutVisited)
    -> bool
{
    auto AlreadyVisited = false;
    InOutVisited.Add(InHandle.Get_Entity(), &AlreadyVisited);

    if (AlreadyVisited)
    {
        CK_TRIGGER_ENSURE(TEXT("Cyclic LifetimeDependents detected at Entity [{}] while checking for undrained "
            "replicated fragments — the lifetime-ownership graph must be a tree. Truncating to avoid infinite recursion."),
            InHandle);
        return false;
    }

    if (InHandle.Has<ck::FTag_RepFragments_PendingApply>() ||
        InHandle.Has<ck::FFragment_PendingHydration>())
    { return true; }

    return ck::algo::AnyOf(UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InHandle),
        [&](const auto& InDependent) { return DoGet_HasUndrainedReplicatedFragments_Recursive(InDependent, InOutVisited); });
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_HasUndrainedReplicatedFragments_IncludingDependents(
        const FCk_Handle& InHandle)
    -> bool
{
    auto Visited = TSet<FCk_Entity>{};
    return DoGet_HasUndrainedReplicatedFragments_Recursive(InHandle, Visited);
}

// Diagnostic twin of the DFS above: returns the FIRST offender instead of a bool. Cycle truncation is
// silent here because the bool twin already ensures on cycles.
static auto
    DoGet_FirstUndrainedEntity_Recursive(
        const FCk_Handle& InHandle,
        TSet<FCk_Entity>& InOutVisited)
    -> FCk_Handle
{
    auto AlreadyVisited = false;
    InOutVisited.Add(InHandle.Get_Entity(), &AlreadyVisited);

    if (AlreadyVisited)
    { return {}; }

    if (InHandle.Has<ck::FTag_RepFragments_PendingApply>() ||
        InHandle.Has<ck::FFragment_PendingHydration>())
    { return InHandle; }

    for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InHandle))
    {
        if (const auto Found = DoGet_FirstUndrainedEntity_Recursive(Dependent, InOutVisited);
            ck::IsValid(Found))
        { return Found; }
    }

    return {};
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    TryGet_FirstUndrainedReplicatedFragmentsEntity_IncludingDependents(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    auto Visited = TSet<FCk_Entity>{};
    return DoGet_FirstUndrainedEntity_Recursive(InHandle, Visited);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_BuildAndReplicate(
        FCk_Handle& InHandle,
        const FCk_EntityReplicationDriver_Spec& InConstructionInfo)
    -> FCk_Handle
{
    return Request_BuildAndReplicate_Multiple(InHandle, { InConstructionInfo });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_BuildAndReplicate_Multiple(
        FCk_Handle& InHandle,
        const TArray<FCk_EntityReplicationDriver_Spec>& InConstructionInfos)
    -> FCk_Handle
{
    return Request_TryBuildAndReplicate(InHandle, InConstructionInfos, [](FCk_Handle){});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_TryBuildAndReplicate(
        FCk_Handle& InHandle,
        const FCk_EntityReplicationDriver_Spec& InConstructionInfo,
        const std::function<void(FCk_Handle)>& InFunc_OnCreateEntityBeforeBuild)
    -> FCk_Handle
{
    return Request_TryBuildAndReplicate(InHandle, TArray<FCk_EntityReplicationDriver_Spec>{ InConstructionInfo }, InFunc_OnCreateEntityBeforeBuild);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_TryBuildAndReplicate(
        FCk_Handle& InHandle,
        const TArray<FCk_EntityReplicationDriver_Spec>& InConstructionInfos,
        const std::function<void(FCk_Handle)>& InFunc_OnCreateEntityBeforeBuild)
    -> FCk_Handle
{
    const auto OwnerIsValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(OwnerIsValid,
        TEXT("Unable to BuildAndReplicate using invalid owner [{}]"), InHandle)
    { return {}; }

    const auto OwnerHasNetParams = UCk_Utils_Net_UE::Has(InHandle);
    CK_ENSURE_IF_NOT(OwnerHasNetParams,
        TEXT("Entity [{}] does NOT have Network Info. Unable to BuildAndReplicate."), InHandle)
    { return {}; }

    if (NOT UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(InHandle))
    { return {}; }

    CK_ENSURE_IF_NOT(NOT InConstructionInfos.IsEmpty(),
        TEXT("Unable to ReplicateEntity as ConstructionInfos is empty"))
    { return {}; }

    for (const auto& ConstructionInfo : InConstructionInfos)
    {
        CK_ENSURE_IF_NOT(ck::IsValid(ConstructionInfo.Get_ConstructionScript()),
            TEXT("Unable to ReplicateEntity as ConstructionScript is [{}]"),
            ConstructionInfo.Get_ConstructionScript())
        { return {}; }
    }

    const auto EntityReplication = UCk_Utils_Net_UE::Get_EntityReplication(InHandle);
    const auto HasRequiredReplicationDriver = EntityReplication == ECk_Replication::DoesNotReplicate
        || InHandle.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
    CK_ENSURE_IF_NOT(HasRequiredReplicationDriver,
        TEXT("Entity [{}] does NOT have a ReplicationDriver. Unable to proceed with Replication."),
        InHandle)
    { return {}; }

    const auto ConstructionInfos = ck_entity_replication_driver_utils::DoStamp_ArchetypeIdentity(InConstructionInfos);

    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InHandle, InFunc_OnCreateEntityBeforeBuild);

    UCk_Utils_Net_UE::Copy(InHandle, NewEntity);
    TryAdd(NewEntity);

    // The tag marks the SYNCHRONOUS construction window: children composed inline by the scripts below get
    // stamped ConstructSpawned (and so persist), while any child composed later at runtime must NOT be. A
    // definition-built entity has no EntityScript fragment, so without this its labeled children lose their state.
    {
        NewEntity.Add<ck::FTag_DefinitionBuild_InProgress>();
        ON_SCOPE_EXIT { NewEntity.Remove<ck::FTag_DefinitionBuild_InProgress>(); };

        for (const auto& ConstructionInfo : ConstructionInfos)
        { ck::entity_replication_driver::Construct_FromInfo(ConstructionInfo, NewEntity); }
    }

    // Construct consumes the ConstructionInfos, so retain them for save capture. Stamped BEFORE the
    // DoesNotReplicate early-return so non-replicated definition-built entities are captured too.
    if (const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(NewEntity);
        ck::IsValid(World))
    {
        auto* Recipe = NewObject<UCk_BuildRecipe_UE>(World);
        Recipe->Populate(ConstructionInfos);
        NewEntity.Add<ck::FFragment_BuildRecipe>(TStrongObjectPtr<UCk_BuildRecipe_UE>{Recipe});
    }

    // Non-replicated owners: entity was built successfully, skip replication setup.
    if (EntityReplication == ECk_Replication::DoesNotReplicate)
    { return NewEntity; }

    switch (const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(InHandle))
    {
        case ECk_Net_NetModeType::Host:
        case ECk_Net_NetModeType::ClientAndHost:
        {
            const auto& RepDriver = NewEntity.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
            const auto& ReplicatedObjects = UCk_Utils_ReplicatedObjects_UE::Get_ReplicatedObjects(NewEntity);
            auto* OwningEntityDriver = Get_PublishedOwningEntityDriver(NewEntity);
            const auto IsOwningEntityDriverDependentOnThis =
                ck_entity_replication_driver_utils::Get_IsOwningEntityDriverDependentOn(OwningEntityDriver, RepDriver);

            const auto& DependentRepDriversAddedDuringConstruction = RepDriver->Get_ExpectedNumberOfDependentReplicationDrivers();

            RepDriver->Set_ExpectedNumberOfDependentReplicationDrivers(Get_NumOfReplicationDriversIncludingDependents(NewEntity) + DependentRepDriversAddedDuringConstruction);

            RepDriver->Set_ReplicationData
            (
                FCk_EntityReplicationDriver_ReplicationData
                {
                    ConstructionInfos,
                    FCk_EntityReplicationDriver_ReplicateObjects_Data{FCk_ReplicatedObjects::ToWeak(ReplicatedObjects.Get_ReplicatedObjects())}
                }
                .Set_OwningEntityDriver(OwningEntityDriver)
                .Set_IsOwningEntityDriverDependentOnThis(IsOwningEntityDriverDependentOnThis)
            );
            ck::UUtils_Signal_OnReplicationComplete::Broadcast(NewEntity, ck::MakePayload(NewEntity));
            ck::UUtils_Signal_OnDependentsReplicationComplete::Broadcast(NewEntity, ck::MakePayload(NewEntity));

            break;
        }
        case ECk_Net_NetModeType::Unknown:
        default:
        {
            CK_INVALID_ENUM(NetMode);
            break;
        }
    }

    return NewEntity;
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_TryReplicateExisting(
        FCk_Handle& InExistingEntity,
        FCk_Handle& InReplicatedOwner,
        const TArray<FCk_EntityReplicationDriver_Spec>& InConstructionInfos)
    -> void
{
    if (NOT UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(InReplicatedOwner))
    { return; }

    CK_ENSURE_IF_NOT(NOT InConstructionInfos.IsEmpty(),
        TEXT("Unable to ReplicateExisting [{}] as ConstructionInfos is empty"), InExistingEntity)
    { return; }

    for (const auto& ConstructionInfo : InConstructionInfos)
    {
        CK_ENSURE_IF_NOT(ck::IsValid(ConstructionInfo.Get_ConstructionScript()),
            TEXT("Unable to ReplicateExisting [{}] as ConstructionScript is [{}]"),
            InExistingEntity, ConstructionInfo.Get_ConstructionScript())
        { return; }
    }

    UCk_Utils_Net_UE::Copy(InReplicatedOwner, InExistingEntity);
    TryAdd(InExistingEntity);

    if (UCk_Utils_Net_UE::Get_EntityReplication(InReplicatedOwner) == ECk_Replication::DoesNotReplicate)
    { return; }

    CK_ENSURE_IF_NOT(InReplicatedOwner.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>(),
        TEXT("Owner [{}] does NOT have a ReplicationDriver. Unable to ReplicateExisting [{}]."),
        InReplicatedOwner, InExistingEntity)
    { return; }

    const auto ConstructionInfos = ck_entity_replication_driver_utils::DoStamp_ArchetypeIdentity(InConstructionInfos);

    switch (const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(InReplicatedOwner))
    {
        case ECk_Net_NetModeType::Host:
        case ECk_Net_NetModeType::ClientAndHost:
        {
            const auto& RepDriver = InExistingEntity.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
            const auto& ReplicatedObjects = UCk_Utils_ReplicatedObjects_UE::Get_ReplicatedObjects(InExistingEntity);
            auto* OwningEntityDriver = Get_PublishedOwningEntityDriver(InExistingEntity);
            const auto IsOwningEntityDriverDependentOnThis =
                ck_entity_replication_driver_utils::Get_IsOwningEntityDriverDependentOn(OwningEntityDriver, RepDriver);

            const auto& DependentRepDriversAddedDuringConstruction = RepDriver->Get_ExpectedNumberOfDependentReplicationDrivers();

            RepDriver->Set_ExpectedNumberOfDependentReplicationDrivers(
                Get_NumOfReplicationDriversIncludingDependents(InExistingEntity) +
                DependentRepDriversAddedDuringConstruction);

            RepDriver->Set_ReplicationData
            (
                FCk_EntityReplicationDriver_ReplicationData
                {
                    ConstructionInfos,
                    FCk_EntityReplicationDriver_ReplicateObjects_Data{FCk_ReplicatedObjects::ToWeak(ReplicatedObjects.Get_ReplicatedObjects())}
                }
                .Set_OwningEntityDriver(OwningEntityDriver)
                .Set_IsOwningEntityDriverDependentOnThis(IsOwningEntityDriverDependentOnThis)
            );
            ck::UUtils_Signal_OnReplicationComplete::Broadcast(InExistingEntity, ck::MakePayload(InExistingEntity));
            ck::UUtils_Signal_OnDependentsReplicationComplete::Broadcast(InExistingEntity, ck::MakePayload(InExistingEntity));

            break;
        }
        case ECk_Net_NetModeType::Unknown:
        default:
        {
            CK_INVALID_ENUM(NetMode);
            break;
        }
    }
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_Replicate(
        FCk_Handle& InHandleToReplicate,
        FCk_Handle InReplicatedOwner,
        TSubclassOf<UCk_EntityScript_UE> InEntityScript,
        const FInstancedStruct& InSpawnParams,
        const TOptional<FCk_Handle>& InReplicatedContextOwnerOverride)
    -> void
{
    if (const auto ScriptStruct = InSpawnParams.GetScriptStruct();
        ck::IsValid(ScriptStruct))
    {
        CK_ENSURE_IF_NOT(ck::IsValid(ScriptStruct) && ScriptStruct->IsNameStableForNetworking(),
            TEXT("SpawnParams ScriptStruct [{}] does NOT have a stable name for networking. Is this a runtime defined struct? "
            "OR is this a Struct defined in a scripting language that does not override IsNameStableForNetworking? "
            "Handle [{}] CANNOT be replicated correctly."), ScriptStruct, InHandleToReplicate)
        { return; }
    }

    CK_ENSURE_IF_NOT(InHandleToReplicate.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>(),
        TEXT("Handle [{}] does NOT have a ReplicationDriver. Unable to proceed with Replication of the handle."),
        InHandleToReplicate)
    { return; }

    CK_ENSURE_IF_NOT(InReplicatedOwner.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>(),
        TEXT("Owner [{}] does NOT have a ReplicationDriver. Unable to proceed with Replication of Entity with EntityScript [{}]"),
        InReplicatedOwner,
        InEntityScript)
    { return; }

    switch(const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(InReplicatedOwner))
    {
        case ECk_Net_NetModeType::Host:
        case ECk_Net_NetModeType::ClientAndHost:
        {
            const auto& RepDriver = InHandleToReplicate.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
            const auto& ReplicatedObjects = UCk_Utils_ReplicatedObjects_UE::Get_ReplicatedObjects(InHandleToReplicate);
            auto* OwningEntityDriver = Get_PublishedOwningEntityDriver(InHandleToReplicate);

            // Unlike the other variants, a world-published EntityScript counts itself twice when it is still
            // JustCreated: Request_ReplicateEntityScript added the matching one to its expected count.
            const auto IsOwningEntityDriverDependentOnThis = ck::IsValid(OwningEntityDriver)
                && OwningEntityDriver->Get_AssociatedEntity().Has<ck::FTag_EntityJustCreated>();

            const auto& DependentRepDriversAddedDuringConstruction = RepDriver->Get_ExpectedNumberOfDependentReplicationDrivers();

            RepDriver->Set_ExpectedNumberOfDependentReplicationDrivers(
                Get_NumOfReplicationDriversIncludingDependents(InHandleToReplicate) +
                DependentRepDriversAddedDuringConstruction);

            RepDriver->Set_ReplicationData_EntityScript
            (
                FCk_EntityReplicationDriver_ReplicationData_EntityScript
                {
                    InEntityScript,
                    InSpawnParams,
                    OwningEntityDriver,
                    FCk_EntityReplicationDriver_ReplicateObjects_Data{FCk_ReplicatedObjects::ToWeak(ReplicatedObjects.Get_ReplicatedObjects())}
                }
                .Set_IsOwningEntityDriverDependentOnThis(IsOwningEntityDriverDependentOnThis)
                .Set_ContextOwnerOverride(InReplicatedContextOwnerOverride.Get(FCk_Handle{}))
                .Set_BoundActor(UCk_Utils_EntityBridge_UE::TryGet_Actor(InHandleToReplicate))
            );

            break;
        }
        case ECk_Net_NetModeType::Unknown:
        default:
            CK_INVALID_ENUM(NetMode);
            break;
    }
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_PublishedOwningEntityDriver(
        const FCk_Handle& InHandle)
    -> UCk_Fragment_EntityReplicationDriver_Rep*
{
    CK_ENSURE_IF_NOT(Has(InHandle),
        TEXT("Handle [{}] does NOT have a ReplicationDriver, so there is no owner to publish for it."), InHandle)
    { return {}; }

    auto* OwnDriver = InHandle.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>().Get();

    if (NOT InHandle.Has<ck::FFragment_LifetimeOwner>())
    { return OwnDriver; }

    const auto NearestReplicatedAncestor = UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_If(
        UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle, ECk_PendingKill_Policy::IncludePendingKill),
        [](const FCk_Handle& InAncestor) { return UCk_Utils_EntityReplicationDriver_UE::Has(InAncestor); },
        ECk_PendingKill_Policy::IncludePendingKill);

    if (ck::Is_NOT_Valid(NearestReplicatedAncestor, ck::IsValid_Policy_IncludePendingKill{}))
    { return OwnDriver; }

    return NearestReplicatedAncestor.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>().Get();
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_PublishedOwningEntity(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    auto* OwningEntityDriver = Get_PublishedOwningEntityDriver(InHandle);

    CK_ENSURE_IF_NOT(ck::IsValid(OwningEntityDriver),
        TEXT("Handle [{}] has no valid published OwningEntityDriver."), InHandle)
    { return {}; }

    if (OwningEntityDriver == InHandle.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>())
    { return InHandle; }

    return OwningEntityDriver->Get_AssociatedEntity();
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Request_PublishLifetimeOwner(
        FCk_Handle& InEntity,
        const FCk_Handle& InPreviousLifetimeOwner,
        const FCk_Handle& InNewLifetimeOwner)
    -> void
{
    if (ck_entity_replication_driver_utils::Get_IsInDestruction(InEntity) ||
        ck_entity_replication_driver_utils::Get_IsInDestruction(InNewLifetimeOwner))
    { return; }

    if (NOT UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(InEntity))
    { return; }

    ck::ecs::VeryVerbose(TEXT("Publishing the lifetime owner of [{}] after its transfer from [{}] to [{}]"),
        InEntity, InPreviousLifetimeOwner, InNewLifetimeOwner);

    auto Visited = TSet<FCk_Entity>{};
    ck_entity_replication_driver_utils::DoPublish_LifetimeOwner(InEntity, Visited);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Has(
        const FCk_Handle& InEntity)
    -> bool
{
    return InEntity.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Ensure(
        const FCk_Handle& InEntity)
    -> bool
{
    CK_ENSURE_IF_NOT(Has(InEntity),
        TEXT("Handle [{}] does NOT have [{}]"),
        InEntity, ck::TypeToString<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>)
    { return false; }

    return true;
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_IsReplicationComplete(
        const FCk_Handle& InHandle)
    -> bool
{
    CK_ENSURE_IF_NOT(Has(InHandle),
        TEXT("Handle [{}] does NOT have an EntityReplicationDriver OR it is still new/setting-up"), InHandle)
    { return false; }

    return ck::UUtils_Signal_OnReplicationComplete_PostFireUnbind::HasFiredAtLeastOnce(InHandle);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_IsReplicationCompleteAllDependents(
        const FCk_Handle& InHandle)
    -> bool
{
    CK_ENSURE_IF_NOT(Has(InHandle),
        TEXT("Handle [{}] does NOT have an EntityReplicationDriver OR it is still new/setting-up"), InHandle)
    { return false; }

    return ck::UUtils_Signal_OnDependentsReplicationComplete_PostFireUnbind::HasFiredAtLeastOnce(InHandle);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Promise_OnReplicationComplete(
        FCk_Handle& InEntity,
        const FCk_Delegate_EntityReplicationDriver_OnReplicationComplete& InDelegate)
    -> void
{
    CK_SIGNAL_BIND_PROMISE(ck::UUtils_Signal_OnReplicationComplete, InEntity, InDelegate);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Promise_OnReplicationCompleteAllDependents(
        FCk_Handle& InEntity,
        const FCk_Delegate_EntityReplicationDriver_OnReplicationComplete& InDelegate)
    -> void
{
    CK_SIGNAL_BIND_PROMISE(ck::UUtils_Signal_OnDependentsReplicationComplete, InEntity, InDelegate);
}

auto
    UCk_Utils_EntityReplicationDriver_UE::
    Get_ReplicatedHandleForWorld(
        const FCk_Handle& InHandle,
        const UWorld* InHandleOwningWorld,
        const UWorld* InTargetWorld)
    -> FCk_Handle
{
    QUICK_SCOPE_CYCLE_COUNTER(Get_ReplicatedHandleForWorld)

    CK_ENSURE_IF_NOT(ck::IsValid(InHandle), TEXT("Handle input is NOT valid!"))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InHandleOwningWorld), TEXT("Handle owning world input is NOT valid!"))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InTargetWorld), TEXT("Target world input is NOT valid!"))
    { return {}; }

    if (InHandleOwningWorld == InTargetWorld)
    { return InHandle; }

    const auto NetDriver = InHandleOwningWorld->GetNetDriver();
    if (ck::Is_NOT_Valid(NetDriver))
    { return {}; }

    if (NOT InHandle.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>())
    { return {}; }

    const auto& ReplicationDriver = InHandle.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>();

    if (ck::Is_NOT_Valid(ReplicationDriver))
    { return {}; }

    if (NetDriver->IsUsingIrisReplication())
    {
#if UE_WITH_IRIS
        const auto SelectedNetDriver = InTargetWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(SelectedNetDriver))
        { return {}; }

        const auto ReplicationSystem = NetDriver->GetReplicationSystem();
        if (ck::Is_NOT_Valid(ReplicationSystem))
        { return {}; }

        const auto SelectedReplicationSystem = SelectedNetDriver->GetReplicationSystem();
        if (ck::Is_NOT_Valid(SelectedReplicationSystem))
        { return {}; }

        const auto WorldRepBridge = ReplicationSystem->GetReplicationBridgeAs<UEngineReplicationBridge>();
        if (ck::Is_NOT_Valid(WorldRepBridge))
        { return {}; }

        const auto SelectedWorldRepBridge = SelectedReplicationSystem->GetReplicationBridgeAs<UEngineReplicationBridge>();
        if (ck::Is_NOT_Valid(SelectedWorldRepBridge))
        { return {}; }

        const auto RefHandle = WorldRepBridge->GetReplicatedRefHandle(ReplicationDriver.Get());
        const auto SelectedWorldRepDriver = Cast<UCk_Ecs_ReplicatedObject_UE>(SelectedWorldRepBridge->GetReplicatedObject(RefHandle));

        if (ck::Is_NOT_Valid(SelectedWorldRepDriver))
        { return {}; }

        return SelectedWorldRepDriver->Get_AssociatedEntity();

#else
        CK_ENSURE_IF_NOT(false, TEXT("Net Driver has Iris enabled without UE_WITH_IRIS enabled, this should NOT be possible!"))
        { return {}; }
#endif
    }
    else
    {
        if (ck::Is_NOT_Valid(InHandleOwningWorld))
        { return {}; }

        const auto& WorldNetDriver = InHandleOwningWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(WorldNetDriver))
        { return {}; }

        const auto& WorldGuidCache = WorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(WorldGuidCache))
        { return {}; }

        const auto WorldNetGuid = WorldGuidCache->GetNetGUID(ReplicationDriver.Get());
        if (ck::Is_NOT_Valid(WorldNetGuid))
        { return {}; }

        const auto SelectedWorldNetDriver = InTargetWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(SelectedWorldNetDriver))
        { return {}; }

        const auto& SelectedWorldGuidCache = SelectedWorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(SelectedWorldGuidCache))
        { return {}; }

        constexpr auto IgnoreMustBeMapped = true;
        const auto SelectedWorldRepDriver = Cast<UCk_Ecs_ReplicatedObject_UE>(SelectedWorldGuidCache->GetObjectFromNetGUID(WorldNetGuid, IgnoreMustBeMapped));
        if (ck::Is_NOT_Valid(SelectedWorldRepDriver))
        { return {}; }

        return SelectedWorldRepDriver->Get_AssociatedEntity();
    }
}

// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::entity_replication_driver::
    Construct_FromInfo(
        const FCk_EntityReplicationDriver_Spec& InInfo,
        FCk_Handle& InEntity)
    -> void
{
    if (const auto* Archetype = InInfo.Get_ConstructionScriptArchetype().Get();
        ck::IsValid(Archetype))
    {
        Archetype->Construct(InEntity);
        return;
    }

    // A runtime-minted archetype has no path this process can load, and the info carries its identity anyway so
    // the archetype survives without the object reference. A client reaches this with a path its own catalog CAN
    // mint from, which is why the retry lives here rather than only in the loader: the authority resolved it
    // once, and the client has to resolve it for itself or come back holding a husk the server does not have.
    if (const auto& IdentityPath = InInfo.Get_ArchetypeIdentityPath();
        NOT IdentityPath.IsEmpty())
    {
        if (auto* Minted = ck::FCk_RuntimeArchetypeRegistry::TryResolve(FSoftObjectPath{IdentityPath});
            ck::IsValid(Minted))
        {
            Minted->Construct(InEntity);
            return;
        }
    }

    InInfo.Get_ConstructionScript()->GetDefaultObject<UCk_Entity_ConstructionScript_PDA>()->Construct(InEntity);
}

auto
    ck::entity_replication_driver::
    Get_IsReplicaWithLiveDriver(
        const FCk_Handle& InEntity)
    -> bool
{
    if (NOT InEntity.Has<FFragment_EntityReplicationDriver_Replica>())
    { return false; }

    auto* Driver = InEntity.Get<FFragment_EntityReplicationDriver_Replica>().Get_Driver().Get();

    if (ck::Is_NOT_Valid(Driver) || Driver->Get_AssociatedEntity() != InEntity)
    { return false; }

    // A client-local EndPlay of the host actor (streaming out, tear-off) leaves its child drivers alive with no
    // destroy coming, and each replica strong-roots its own driver: sparing one then would leak it.
    auto* HostActor = Driver->GetTypedOuter<AActor>();

    return ck::IsValid(HostActor) && NOT HostActor->IsActorBeingDestroyed() && HostActor->HasActorBegunPlay();
}

auto
    ck::entity_replication_driver::
    Get_LifetimeOwnerForOrphanedReplica(
        const FCk_Handle& InReplica)
    -> FCk_Handle
{
    const auto TransientEntity = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InReplica.Get_RegistryView());

    if (NOT InReplica.Has<FFragment_EntityReplicationDriver_Replica>())
    { return TransientEntity; }

    auto* Driver = InReplica.Get<FFragment_EntityReplicationDriver_Replica>().Get_Driver().Get();

    if (ck::Is_NOT_Valid(Driver))
    { return TransientEntity; }

    auto* OwningEntityDriver = Driver->Get_OwningEntityDriver();

    if (ck::Is_NOT_Valid(OwningEntityDriver) || OwningEntityDriver == Driver)
    { return TransientEntity; }

    const auto& OwningEntity = OwningEntityDriver->Get_AssociatedEntity();

    if (ck::Is_NOT_Valid(OwningEntity) || OwningEntity.Has<FTag_DestroyEntity_Initiate>())
    { return TransientEntity; }

    return OwningEntity;
}

// --------------------------------------------------------------------------------------------------------------------
