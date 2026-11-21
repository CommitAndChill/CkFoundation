#include "CkEntityReplicationDriver_Fragment.h"

#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Payload/CkPayload.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/EntityConstructionScript/CkEntity_ConstructionScript.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/EntityScript/CkEntityScript.h"

#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include "Net/Core/PushModel/PushModel.h"

#include <Engine/World.h>
#include <Engine/LocalPlayer.h>

#include <Net/UnrealNetwork.h>

// --------------------------------------------------------------------------------------------------------------------

UCk_Fragment_EntityReplicationDriver_Rep::
    UCk_Fragment_EntityReplicationDriver_Rep(
        const FObjectInitializer& InObjInitializer)
    : Super(InObjInitializer)
{
    _Fragments._OwningDriver = this;

    if (IsTemplate())
    { return; }

    auto World = UObject::GetWorld();

    if (ck::Is_NOT_Valid(World))
    {
        // ROOT FAILURE, loud on purpose: _AssociatedEntity is created ONLY here, so a driver built before
        // its outer Actor had a UWorld stays permanently invalid and every child driver that names it as
        // owner parks forever in a queue nothing ever drains. See CkEcs/CLAUDE.md.
        ck::ecs::Warning(TEXT("EntityReplicationDriver constructed with NO valid UWorld (Outer=[{}]). Its associated "
            "entity can never be created, permanently stranding any dependent child drivers on the client."), GetOuter());
        return;
    }

    // Creating via registry since we want 'Request_SetupEntityWithLifetimeOwner' to execute during the OnRep_XYZ
    auto TransientHandle = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);
    auto RegistryView = TransientHandle.Get_RegistryView();
    _AssociatedEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(RegistryView);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Get_IsReplicationCompleteOnAllDependents() const
        -> bool
{
    if (const auto Entity = Get_AssociatedEntity();
        UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(Entity))
    { return true; }

    return Get_ExpectedNumberOfDependentReplicationDrivers() == _NumSyncedDependentReplicationDrivers;
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const
    -> void
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    constexpr auto Params = FDoRepLifetimeParams{COND_Custom, REPNOTIFY_OnChanged, true};

    DOREPLIFETIME_WITH_PARAMS_FAST(ThisType, _ReplicationData, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisType, _ReplicationData_EntityScript, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisType, _ReplicationData_Ability, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisType, _ExpectedNumberOfDependentReplicationDrivers, Params);

    constexpr auto FragmentParams = FDoRepLifetimeParams{COND_None, REPNOTIFY_Always, true};
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisType, _Fragments, FragmentParams);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    PostLink()
    -> void
{
    auto Entity = Get_AssociatedEntity();
    if (ck::Is_NOT_Valid(Entity))
    { return; }

    // Link is pure bookkeeping: FProcessor_ReplicatedFragments_Dispatch applies these entries later
    // in the frame, after OnConstructed-driven composition has run.
    for (auto& Entry : _Fragments._Items)
    {
        const auto* Handler = FCk_PersistenceHandlerRegistry::Resolve(Entry.Data.GetScriptStruct());
        if (Handler == nullptr || NOT Handler->NetApply)
        { continue; }

        Entry._PendingApply = true;
        Entry._PendingSinceRealTimeSeconds = 0.0;
        Entity.AddOrGet<ck::FTag_RepFragments_PendingApply>();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    GetReplicatedCustomConditionState(
        FCustomPropertyConditionState& OutActiveState) const
    -> void
{
    Super::GetReplicatedCustomConditionState(OutActiveState);

    DOREPCUSTOMCONDITION_ACTIVE_FAST(ThisType, _ReplicationData, ck::IsValid(Get_AssociatedEntity()));
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    OnRep_ReplicationData()
    -> void
{
    if (_ReplicationDataKind != ECk_EntityReplicationDriver_DataKind::None)
    {
        DoReparent_ToOwningEntityDriver();
        return;
    }

    if ([[maybe_unused]] const auto ShouldSkipIfAllObjectsAreNotYetResolved =
        AnyOf(_ReplicationData.Get_ReplicatedObjectsData().Get_Objects(), ck::algo::Is_NOT_Valid{}))
    { return; }

    // --------------------------------------------------------------------------------------------------------------------

    const auto& OwningEntityDriver = _ReplicationData.Get_OwningEntityDriver();

    if (ck::Is_NOT_Valid(OwningEntityDriver))
    {
        DoReport_UnresolvedOwningEntityDriver();
        return;
    }

    const auto IsSelfReferencing = OwningEntityDriver == this;
    const auto OwningEntity = IsSelfReferencing
        ? UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(GetWorld())
        : OwningEntityDriver->Get_AssociatedEntity();

    if (ck::Is_NOT_Valid(OwningEntity))
    {
        // NOT a wait: a client driver's _AssociatedEntity is only ever set in its ctor, so this child can
        // never be set up and the queue below is never drained. Root cause is the ctor no-world warning.
        ck::ecs::Warning(TEXT("Replicated child driver [{}] cannot construct: owner driver [{}] has no valid associated "
            "entity and will never recover. Child parked in a queue that is never drained."), this, OwningEntityDriver);
        OwningEntityDriver->_PendingChildEntityConstructions.Emplace(this);
        return;
    }

    // --------------------------------------------------------------------------------------------------------------------

    const auto& ConstructionInfos = _ReplicationData.Get_ConstructionInfos();

    CK_ENSURE_IF_NOT(NOT ConstructionInfos.IsEmpty(),
        TEXT("ConstructionInfos is empty. Unable to proceed with replicating the Entity.{}"), ck::Context(this))
    { return; }

    for (const auto& ConstructionInfo : ConstructionInfos)
    {
        CK_ENSURE_IF_NOT(ck::IsValid(ConstructionInfo.Get_ConstructionScript()),
            TEXT("ConstructionScript is [{}]. Unable to proceed with replicating the Entity"),
            ConstructionInfo.Get_ConstructionScript())
        { return; }
    }

    // --------------------------------------------------------------------------------------------------------------------

    DoCommit_Construction(ECk_EntityReplicationDriver_DataKind::Entity);

    _AssociatedEntity._ReplicationDriver = this;

    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(_AssociatedEntity, OwningEntity);

    UCk_Utils_Net_UE::Add(_AssociatedEntity, FCk_Net_ConnectionSettings
    {
        ECk_Replication::Replicates,
        ECk_Net_NetModeType::Client,
        ECk_Net_EntityNetRole::Proxy
    });

    for (const auto& ConstructionInfo : ConstructionInfos)
    { ck::entity_replication_driver::Construct_FromInfo(ConstructionInfo, _AssociatedEntity); }

    UCk_Utils_ReplicatedObjects_UE::Add(_AssociatedEntity, FCk_ReplicatedObjects{}.
        Set_ReplicatedObjects(FCk_ReplicatedObjects::ToStrong(_ReplicationData.Get_ReplicatedObjectsData().Get_Objects())));

    // --------------------------------------------------------------------------------------------------------------------

    // Make sure to call this on "self" since the # of dependent rep driver include "self" as well
    DoAdd_SyncedDependentReplicationDriver();

    // A Replicated Entity built from inside the OwningEntity's ConstructionScript is a dependent of it
    if (_ReplicationData.Get_IsOwningEntityDriverDependentOnThis() && NOT IsSelfReferencing)
    {
        DoAdd_SyncedDependentOf(OwningEntityDriver);
    }
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    OnRep_ReplicationData_EntityScript() -> void
{
    if (_ReplicationDataKind != ECk_EntityReplicationDriver_DataKind::None)
    {
        DoReparent_ToOwningEntityDriver();
        return;
    }

    if ([[maybe_unused]] const auto ShouldSkipIfAllObjectsAreNotYetResolved =
        AnyOf(_ReplicationData_EntityScript.Get_ReplicatedObjectsData().Get_Objects(), ck::algo::Is_NOT_Valid{}))
    {
        return;
    }
    const auto EntityScriptClass = _ReplicationData_EntityScript.Get_EntityScriptClass();
    const auto SpawnParams = _ReplicationData_EntityScript.Get_SpawnParams();
    const auto& OwningEntityDriver = _ReplicationData_EntityScript.Get_OwningEntityDriver();
    const auto IsSelfReferencing = OwningEntityDriver == this;

    auto* EntityScriptDefault = UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_EntityScript_UE>(EntityScriptClass);
    const auto IsActorBound = ck::IsValid(EntityScriptDefault) && EntityScriptDefault->Get_IsActorBound();

    auto* Bridge = static_cast<UCk_EntityBridge_ActorComponent_UE*>(nullptr);

    if (IsActorBound)
    {
        auto* BoundActor = _ReplicationData_EntityScript.Get_BoundActor().Get();

        // Not resolved on this client yet. The RepNotify fires again when the reference maps, the same
        // contract the replicated-objects guard above relies on.
        if (ck::Is_NOT_Valid(BoundActor))
        { return; }

        Bridge = UCk_Utils_EntityBridge_UE::TryGet_Bridge(BoundActor);

        CK_ENSURE_IF_NOT(ck::IsValid(Bridge),
            TEXT("Replicated actor-bound EntityScript [{}] arrived for Actor [{}], which has no Ck Entity Bridge. "
                 "Its Entity cannot be constructed on this client."),
            EntityScriptClass, BoundActor)
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_AssociatedEntity);
            return;
        }
    }

    if (NOT IsSelfReferencing)
    {
        if (ck::Is_NOT_Valid(OwningEntityDriver))
        {
            DoReport_UnresolvedOwningEntityDriver();
            return;
        }

        const auto OwningEntity = OwningEntityDriver->Get_AssociatedEntity();

        if (ck::Is_NOT_Valid(OwningEntity))
        {
            // NOT a wait: a client driver's _AssociatedEntity never recovers, so this child entity-script
            // can never be set up and the queue below is never drained. See the ctor no-world warning.
            ck::ecs::Warning(TEXT("Replicated child entity-script driver [{}] cannot construct: owner driver [{}] has no "
                "valid associated entity and will never recover. Child parked in a queue that is never drained."),
                this, OwningEntityDriver);
            OwningEntityDriver->_PendingChildEntityConstructions.Emplace(this);
            return;
        }

        DoCommit_Construction(ECk_EntityReplicationDriver_DataKind::EntityScript);
        UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(_AssociatedEntity, OwningEntity);
    }
    else
    {
        DoCommit_Construction(ECk_EntityReplicationDriver_DataKind::EntityScript);
        const auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(GetWorld());
        UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(_AssociatedEntity, TransientEntity);
    }

    // Re-home the ContextOwner: the setup call above inherited it from the lifetime owner. An unset
    // override means the authority resolved the entity as its own ContextOwner, so map it back to self.
    if (const auto& ContextOwnerOverride = _ReplicationData_EntityScript.Get_ContextOwnerOverride();
        ck::IsValid(ContextOwnerOverride))
    { UCk_Utils_ContextOwner_UE::Request_Override(_AssociatedEntity, ContextOwnerOverride, {}); }
    else
    { UCk_Utils_ContextOwner_UE::Request_OverrideToSelf(_AssociatedEntity, {}); }

    // The ownership chain may not resolve to a World yet on clients, so Get_WorldForEntity is given
    // one directly rather than having to walk it.
    _AssociatedEntity.AddOrGet<TWeakObjectPtr<UWorld>>(GetWorld());

    auto ThisAsWeakPtr = TWeakObjectPtr<ThisType>{this};
    const auto OnConstructionFinished = [ThisAsWeakPtr](FCk_Handle InHandle)
    {
        if (ck::Is_NOT_Valid(ThisAsWeakPtr))
        { return; }

        UCk_Utils_ReplicatedObjects_UE::Add(ThisAsWeakPtr->_AssociatedEntity, FCk_ReplicatedObjects{}.
            Set_ReplicatedObjects(FCk_ReplicatedObjects::ToStrong(ThisAsWeakPtr->_ReplicationData_EntityScript.Get_ReplicatedObjectsData().Get_Objects())));

        // --------------------------------------------------------------------------------------------------------------------
        // Make sure to call this on "self" since the # of dependent rep driver include "self" as well
        ThisAsWeakPtr->DoAdd_SyncedDependentReplicationDriver();

        if (UCk_Utils_Net_UE::Get_Replication(InHandle) == ECk_Replication::Replicates)
        {
            if (UCk_Utils_EntityReplicationDriver_UE::Get_IsReplicationCompleteAllDependents(InHandle))
            {
                InHandle.Add<ck::FTag_EntityReplicationDriver_FireOnDependentReplicationComplete>();
            }
        }

        // A Replicated Entity built from inside the OwningEntity's ConstructionScript is a dependent of it
        if (NOT ThisAsWeakPtr->_ReplicationData_EntityScript.Get_IsOwningEntityDriverDependentOnThis())
        { return; }

        const auto& CurrentOwningEntityDriver = ThisAsWeakPtr->_ReplicationData_EntityScript.Get_OwningEntityDriver();

        // Its destroy can reach this client before the rewrite that clears the flag: no count is left to sync
        if (ck::Is_NOT_Valid(CurrentOwningEntityDriver))
        { return; }

        ThisAsWeakPtr->DoAdd_SyncedDependentOf(CurrentOwningEntityDriver);
    };

    const auto ReplicaConnectionSettings = FCk_Net_ConnectionSettings
    {
        ECk_Replication::Replicates,
        ECk_Net_NetModeType::Client,
        ECk_Net_EntityNetRole::Proxy
    };

    if (IsActorBound)
    {
        UCk_Utils_Net_UE::Add(_AssociatedEntity, ReplicaConnectionSettings);
        UCk_Utils_EntityScript_UE::Add_ForBridge(_AssociatedEntity, EntityScriptDefault, SpawnParams, Bridge, OnConstructionFinished);
        _AssociatedEntity._ReplicationDriver = this;
        return;
    }

    UCk_Utils_EntityScript_UE::Add(_AssociatedEntity, EntityScriptClass, SpawnParams, OnConstructionFinished);

    _AssociatedEntity._ReplicationDriver = this;

    UCk_Utils_Net_UE::Add(_AssociatedEntity, ReplicaConnectionSettings);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    OnRep_ReplicationData_Ability()
    -> void
{
    if (_ReplicationDataKind != ECk_EntityReplicationDriver_DataKind::None)
    {
        DoReparent_ToOwningEntityDriver();
        return;
    }

    // wait for the data to be fully replicated
    if (ck::Is_NOT_Valid(_ReplicationData_Ability.Get_AbilityScriptClass()))
    { return; }

    if ([[maybe_unused]] const auto ShouldSkipIfAllObjectsAreNotYetResolved =
        AnyOf(_ReplicationData_Ability.Get_ReplicatedObjectsData().Get_Objects(), ck::algo::Is_NOT_Valid{}))
    { return; }

    // --------------------------------------------------------------------------------------------------------------------

    const auto& OwningEntityDriver = _ReplicationData_Ability.Get_OwningEntityDriver();

    if (ck::Is_NOT_Valid(OwningEntityDriver))
    {
        DoReport_UnresolvedOwningEntityDriver();
        return;
    }

    const auto OwningEntity = OwningEntityDriver == this
        ? UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(GetWorld())
        : OwningEntityDriver->Get_AssociatedEntity();

    if (ck::Is_NOT_Valid(OwningEntity))
    {
        // NOT a wait: a client driver's _AssociatedEntity never recovers, so this child ability can never
        // be set up and the queue below is never drained. See the ctor no-world warning.
        ck::ecs::Warning(TEXT("Replicated child ability driver [{}] cannot construct: owner driver [{}] has no valid "
            "associated entity and will never recover. Child parked in a queue that is never drained."),
            this, OwningEntityDriver);
        OwningEntityDriver->_PendingChildAbilityEntityConstructions.Emplace(this);
        return;
    }

    ck::ecs::Verbose(TEXT("Adding Ability [{}] to [{}] with Owning Entity [{}] on Client [{}].{}"),
        _ReplicationData_Ability.Get_AbilityScriptClass(), Get_AssociatedEntity(), OwningEntity,
        GetWorld()->GetFirstLocalPlayerFromController(), this);

    // --------------------------------------------------------------------------------------------------------------------

    DoCommit_Construction(ECk_EntityReplicationDriver_DataKind::Ability);

    _AssociatedEntity._ReplicationDriver = this;

    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(_AssociatedEntity, OwningEntity);

    UCk_Utils_Net_UE::Add(_AssociatedEntity, FCk_Net_ConnectionSettings
    {
        ECk_Replication::Replicates,
        ECk_Net_NetModeType::Client,
        ECk_Net_EntityNetRole::Proxy
    });

    // Handed to the Ability Processor for construction, which removes it once the Entity is built
    _AssociatedEntity.Add<FCk_EntityReplicationDriver_AbilityData>(_ReplicationData_Ability);

    UCk_Utils_ReplicatedObjects_UE::Add(_AssociatedEntity, FCk_ReplicatedObjects{}.
        Set_ReplicatedObjects(FCk_ReplicatedObjects::ToStrong(_ReplicationData_Ability.Get_ReplicatedObjectsData().Get_Objects())));

    // --------------------------------------------------------------------------------------------------------------------

    // The #SyncedDrivers count is deliberately NOT incremented here — FProcessor_AbilityOwner_HandleRequests
    // does it once the replicated ability exists. Incrementing now fires ReplicationComplete before an
    // extension-added ability's features exist, and manipulating them then fails.
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    OnRep_ExpectedNumberOfDependentReplicationDrivers() const
    -> void
{
    if (_NumSyncedDependentReplicationDrivers == Get_ExpectedNumberOfDependentReplicationDrivers())
    {
        auto AssociatedEntity = Get_AssociatedEntity();
        AssociatedEntity.AddOrGet<ck::FTag_EntityReplicationDriver_FireOnDependentReplicationComplete>();
    }
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoAdd_SyncedDependentReplicationDriver()
    -> void
{
    ++_NumSyncedDependentReplicationDrivers;

    CK_ENSURE_IF_NOT(_NumSyncedDependentReplicationDrivers <= _ExpectedNumberOfDependentReplicationDrivers,
        TEXT("The number of Synced Dependent Replication Drivers [{}] is greater than the expected amount of [{}] on Associated Entity [{}]. This is likely due to faulty logic."),
        _NumSyncedDependentReplicationDrivers,
        _ExpectedNumberOfDependentReplicationDrivers,
        Get_AssociatedEntity())
    { return; }

    if (_ExpectedNumberOfDependentReplicationDrivers == _NumSyncedDependentReplicationDrivers)
    {
        auto AssociatedEntity = Get_AssociatedEntity();
        AssociatedEntity.AddOrGet<ck::FTag_EntityReplicationDriver_FireOnDependentReplicationComplete>();
    }
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoRemove_SyncedDependentReplicationDriver()
    -> void
{
    CK_ENSURE_IF_NOT(_NumSyncedDependentReplicationDrivers > 0,
        TEXT("Removing a Synced Dependent Replication Driver from Associated Entity [{}] which has none. This is likely "
            "due to faulty logic."), Get_AssociatedEntity())
    { return; }

    --_NumSyncedDependentReplicationDrivers;
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoAdd_SyncedDependentOf(
        UCk_Fragment_EntityReplicationDriver_Rep* InOwningEntityDriver)
    -> void
{
    InOwningEntityDriver->DoAdd_SyncedDependentReplicationDriver();
    _SyncedOwningEntityDriver = InOwningEntityDriver;
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoCommit_Construction(
        ECk_EntityReplicationDriver_DataKind InDataKind)
    -> void
{
    _ReplicationDataKind = InDataKind;
    _AssociatedEntity.Add<ck::FFragment_EntityReplicationDriver_Replica>(TWeakObjectPtr<ThisType>{this});
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoReport_UnresolvedOwningEntityDriver()
    -> void
{
    // A plain wait: Iris notifies again when an unresolved object reference maps, and an owner hosted on another
    // root may arrive after this driver.
    if (_HasReportedUnresolvedOwningEntityDriver)
    { return; }

    _HasReportedUnresolvedOwningEntityDriver = true;
    ck::ecs::Log(TEXT("Replicated driver [{}] is waiting for its OwningEntityDriver to resolve before constructing "
        "Entity [{}]."), this, _AssociatedEntity);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoGet_IsDependentOfOwningEntityDriver() const
    -> bool
{
    switch (_ReplicationDataKind)
    {
        case ECk_EntityReplicationDriver_DataKind::Entity:
        { return _ReplicationData.Get_IsOwningEntityDriverDependentOnThis(); }
        case ECk_EntityReplicationDriver_DataKind::EntityScript:
        { return _ReplicationData_EntityScript.Get_IsOwningEntityDriverDependentOnThis(); }
        case ECk_EntityReplicationDriver_DataKind::None:
        case ECk_EntityReplicationDriver_DataKind::Ability:
        { return false; }
    }

    return false;
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoReconcile_SyncedOwningEntityDriver()
    -> void
{
    if (_SyncedOwningEntityDriver.IsExplicitlyNull())
    { return; }

    auto* SyncedOwningEntityDriver = _SyncedOwningEntityDriver.Get();

    if (ck::IsValid(SyncedOwningEntityDriver)
        && DoGet_IsDependentOfOwningEntityDriver()
        && Get_OwningEntityDriver() == SyncedOwningEntityDriver)
    { return; }

    // The server lowered that owner's expected count when it published the change, coalesced or not
    if (ck::IsValid(SyncedOwningEntityDriver))
    { SyncedOwningEntityDriver->DoRemove_SyncedDependentReplicationDriver(); }

    _SyncedOwningEntityDriver.Reset();
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    DoReparent_ToOwningEntityDriver()
    -> void
{
    DoReconcile_SyncedOwningEntityDriver();

    auto* OwningEntityDriver = Get_OwningEntityDriver();

    // Null on a client is unresolved, never the world: the entity keeps its current owner
    if (ck::Is_NOT_Valid(OwningEntityDriver))
    { return; }

    if (ck::Is_NOT_Valid(_AssociatedEntity))
    { return; }

    CK_ENSURE_IF_NOT(_AssociatedEntity.Has<ck::FFragment_LifetimeOwner>(),
        TEXT("Replicated Entity [{}] was constructed by its ReplicationDriver but has NO LifetimeOwner. Unable to "
            "follow the published owner.{}"), _AssociatedEntity, ck::Context(this))
    { return; }

    const auto NewLifetimeOwner = OwningEntityDriver == this
        ? UCk_Utils_EntityLifetime_UE::Get_TransientEntity(_AssociatedEntity)
        : OwningEntityDriver->Get_AssociatedEntity();

    if (ck::Is_NOT_Valid(NewLifetimeOwner) || NewLifetimeOwner.Has<ck::FTag_DestroyEntity_Initiate>())
    { return; }

    if (NewLifetimeOwner == UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(_AssociatedEntity, ECk_PendingKill_Policy::IncludePendingKill))
    { return; }

    UCk_Utils_EntityLifetime_UE::Request_TransferLifetimeOwner(_AssociatedEntity, NewLifetimeOwner);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Get_OwningEntityDriver() const
    -> UCk_Fragment_EntityReplicationDriver_Rep*
{
    switch (_ReplicationDataKind)
    {
        case ECk_EntityReplicationDriver_DataKind::None:
        { return {}; }
        case ECk_EntityReplicationDriver_DataKind::Entity:
        { return _ReplicationData.Get_OwningEntityDriver().Get(); }
        case ECk_EntityReplicationDriver_DataKind::EntityScript:
        { return _ReplicationData_EntityScript.Get_OwningEntityDriver().Get(); }
        case ECk_EntityReplicationDriver_DataKind::Ability:
        { return _ReplicationData_Ability.Get_OwningEntityDriver().Get(); }
    }

    return {};
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Request_PublishOwningEntityDriver(
        UCk_Fragment_EntityReplicationDriver_Rep* InOwningEntityDriver)
    -> void
{
    if (_ReplicationDataKind == ECk_EntityReplicationDriver_DataKind::None)
    { return; }

    CK_ENSURE_IF_NOT(ck::IsValid(InOwningEntityDriver),
        TEXT("Cannot publish an invalid OwningEntityDriver for Associated Entity [{}].{}"), Get_AssociatedEntity(), ck::Context(this))
    { return; }

    auto* PreviousOwningEntityDriver = Get_OwningEntityDriver();

    if (PreviousOwningEntityDriver == InOwningEntityDriver)
    { return; }

    // Counted by the owner's establishment walk only once that owner published; by the EntityScript bump always
    auto IsCountedByPreviousOwningEntityDriver = false;

    switch (_ReplicationDataKind)
    {
        case ECk_EntityReplicationDriver_DataKind::None:
        { return; }
        case ECk_EntityReplicationDriver_DataKind::Entity:
        {
            IsCountedByPreviousOwningEntityDriver = _ReplicationData.Get_IsOwningEntityDriverDependentOnThis()
                && ck::IsValid(PreviousOwningEntityDriver)
                && PreviousOwningEntityDriver->_ReplicationDataKind != ECk_EntityReplicationDriver_DataKind::None;
            _ReplicationData.Set_OwningEntityDriver(InOwningEntityDriver).Set_IsOwningEntityDriverDependentOnThis(false);
            MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData, this);
            break;
        }
        case ECk_EntityReplicationDriver_DataKind::EntityScript:
        {
            IsCountedByPreviousOwningEntityDriver = _ReplicationData_EntityScript.Get_IsOwningEntityDriverDependentOnThis();
            _ReplicationData_EntityScript.Set_OwningEntityDriver(InOwningEntityDriver).Set_IsOwningEntityDriverDependentOnThis(false);
            MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData_EntityScript, this);
            break;
        }
        case ECk_EntityReplicationDriver_DataKind::Ability:
        {
            _ReplicationData_Ability.Set_OwningEntityDriver(InOwningEntityDriver);
            MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData_Ability, this);
            break;
        }
    }

    if (NOT IsCountedByPreviousOwningEntityDriver)
    { return; }

    if (ck::Is_NOT_Valid(PreviousOwningEntityDriver))
    { return; }

    const auto PreviousExpected = PreviousOwningEntityDriver->Get_ExpectedNumberOfDependentReplicationDrivers();

    CK_ENSURE_IF_NOT(PreviousExpected > 0,
        TEXT("Associated Entity [{}] was counted by [{}], whose expected number of Dependent Replication Drivers is "
            "already [{}]. This is likely due to faulty logic."),
        Get_AssociatedEntity(), PreviousOwningEntityDriver->Get_AssociatedEntity(), PreviousExpected)
    { return; }

    PreviousOwningEntityDriver->Set_ExpectedNumberOfDependentReplicationDrivers(PreviousExpected - 1);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Set_ReplicationData(
        const FCk_EntityReplicationDriver_ReplicationData& InReplicationData)
    -> void
{
    _ReplicationData = InReplicationData;
    _ReplicationDataKind = ECk_EntityReplicationDriver_DataKind::Entity;
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData, this);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Set_ReplicationData_EntityScript(
        const FCk_EntityReplicationDriver_ReplicationData_EntityScript& InReplicationData)
    -> void
{
    _ReplicationData_EntityScript = InReplicationData;
    _ReplicationDataKind = ECk_EntityReplicationDriver_DataKind::EntityScript;
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData_EntityScript, this);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Set_ReplicationData_Ability(
        const FCk_EntityReplicationDriver_AbilityData& InReplicationData)
    -> void
{
    _ReplicationData_Ability = InReplicationData;
    _ReplicationDataKind = ECk_EntityReplicationDriver_DataKind::Ability;
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ReplicationData_Ability, this);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    Set_ExpectedNumberOfDependentReplicationDrivers(
        int32 InNumOfDependents)
    -> void
{
    CK_ENSURE_IF_NOT(GetWorld()->GetNetMode() != NM_Client,
        TEXT("Setting the ExpectedNumberOfDependentReplicationDrivers is only allowed on the Server.{}"), ck::Context(this))
    { return; }

    _ExpectedNumberOfDependentReplicationDrivers = InNumOfDependents;
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _ExpectedNumberOfDependentReplicationDrivers, this);
}

// --------------------------------------------------------------------------------------------------------------------
// Generic Fragment Container

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    MarkFragmentDirty(
        FCk_ReplicatedFragmentEntry& InEntry)
    -> void
{
    _Fragments.MarkItemDirty(InEntry);
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisType, _Fragments, this);
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    FindEntry(
        const UScriptStruct* InType)
    -> FCk_ReplicatedFragmentEntry*
{
    return _Fragments._Items.FindByPredicate([InType](const FCk_ReplicatedFragmentEntry& InEntry)
    {
        return InEntry.Data.GetScriptStruct() == InType;
    });
}

auto
    UCk_Fragment_EntityReplicationDriver_Rep::
    SetFragmentData_Runtime(
        const FInstancedStruct& InData)
    -> int32
{
    if (auto* Entry = FindEntry(InData.GetScriptStruct()))
    {
        Entry->Data = InData;
        MarkFragmentDirty(*Entry);
        return _Fragments._Items.IndexOfByPredicate([Entry](const FCk_ReplicatedFragmentEntry& InEntry)
        {
            return &InEntry == Entry;
        });
    }

    auto& NewEntry = _Fragments._Items.AddDefaulted_GetRef();
    NewEntry.Data = InData;
    MarkFragmentDirty(NewEntry);

    return _Fragments._Items.Num() - 1;
}

// --------------------------------------------------------------------------------------------------------------------
