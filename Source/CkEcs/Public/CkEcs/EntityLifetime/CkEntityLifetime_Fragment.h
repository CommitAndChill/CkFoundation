#pragma once

#include "CkEcs/Tag/CkTag.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment_Data.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#include "CkEcs/Signal/CkSignal_Macros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // The FTag_DestroyEntity_* and FTag_EntityJustCreated tags are defined in CkHandle.h to avoid a circular
    // dependency, since they are needed there for debugging purposes.

    // 'Initialize' phase NOT part of Pending Kill as all regular Processors should still be able to complete their work
    // before the end of the frame
#define CK_IGNORE_PENDING_KILL                     \
    ck::TExclude<ck::FTag_DestroyEntity_EndPlay>,  \
    ck::TExclude<ck::FTag_DestroyEntity_Teardown>, \
    ck::TExclude<ck::FTag_DestroyEntity_Await>,    \
    ck::TExclude<ck::FTag_DestroyEntity_Finalize>

#define CK_IF_END_PLAY                             \
    ck::FTag_DestroyEntity_EndPlay,                \
    ck::TExclude<ck::FTag_DestroyEntity_Teardown>, \
    ck::TExclude<ck::FTag_DestroyEntity_Await>,    \
    ck::TExclude<ck::FTag_DestroyEntity_Finalize>

#define CK_IF_TEARING_DOWN                         \
    ck::FTag_DestroyEntity_Teardown, ck::TExclude<ck::FTag_DestroyEntity_Await>, ck::TExclude<ck::FTag_DestroyEntity_Finalize>

    // --------------------------------------------------------------------------------------------------------------------

    // Provenance marker stamped on an entity created while its lifetime owner is still constructing: such a child
    // is re-created by the owner's replayed construction on load, so the save ADOPTS it by identity (owner + label)
    // rather than respawning a recipe. The tag is read LIVE at capture and recorded as entity-table metadata; it
    // must never round-trip through a save (no tag does).
    CK_DEFINE_ECS_TAG(FTag_ConstructSpawned);

    // Construction-window marker for a definition-built entity, which carries NO FFragment_EntityScript and
    // so would be missed by the ConstructSpawned stamp above. Held for the SYNCHRONOUS span of ConstructionInfo
    // execution only. A live construction marker is not a persistent property and is never captured.
    CK_DEFINE_ECS_TAG(FTag_DefinitionBuild_InProgress);

    // --------------------------------------------------------------------------------------------------------------------

    /**
     * Head of the owner's dependents list. The list itself is intrusive: every same-registry dependent carries its
     * sibling links in its own FFragment_LifetimeOwner, so attaching, detaching, and transferring a dependent are
     * O(1) and allocation-free regardless of how many dependents the owner has or has ever had, and a dependent
     * leaves the list the moment its FFragment_LifetimeOwner is destroyed. Iterate through
     * UCk_Utils_EntityLifetime_UE (ForEach_LifetimeDependent and friends); the links are not a public contract.
     *
     * A dependent stays linked until it is finally destroyed, so the list can name entities that are already
     * pending kill — the traversal utilities still visit those, as the former array did, and a caller that must
     * not see them filters with ck::IsValid.
     */
    struct CKECS_API FFragment_LifetimeDependents
    {
        CK_GENERATED_BODY(FFragment_LifetimeDependents);

        friend class UCk_Utils_EntityLifetime_UE;
        friend struct FFragment_LifetimeOwner;

    public:
        // TODO: Use FCk_DebuggableEntity when available [OBS-845]
        using EntityType = FCk_Handle;

    private:
        EntityType _FirstDependent;
        EntityType _LastDependent;
        int32 _LinkedCount = 0;

    public:
        CK_PROPERTY_GET(_FirstDependent);
        CK_PROPERTY_GET(_LastDependent);
        CK_PROPERTY_GET(_LinkedCount);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API FFragment_LifetimeOwner
    {
        CK_GENERATED_BODY(FFragment_LifetimeOwner);

        friend class UCk_Utils_EntityLifetime_UE;

    public:
        // TODO: Use FCk_DebuggableEntity when available [OBS-845]
        using EntityType = FCk_Handle;

    public:
        // EnTT customization point (the name is EnTT's, not house style): a component type that declares a static
        // on_destroy(Registry&, entity) has it connected to its storage's destruction signal automatically, so the
        // dependent unlinks itself from its owner's list whenever this fragment goes away — entity destruction,
        // registry clear, or an explicit remove — without any registry having to opt in.
        static auto
        on_destroy(
            ck::registry_table::EnttRegistryType& InRegistry,
            FCk_Entity::IdType InEntity) -> void;

    private:
        // The unlink works on the raw registry, never through a handle: it runs inside EnTT's destroy loop, where
        // constructing an FCk_Handle on the game thread would attach handle-debug state to an entity whose pools
        // are being emptied — a structural mutation mid-destroy.
        static auto
        DoUnlink(
            ck::registry_table::EnttRegistryType& InRegistry,
            FCk_Entity::IdType InDependent) -> void;

        template <typename T_Fragment>
        [[nodiscard]]
        static auto
        DoTryGet_Linked(
            ck::registry_table::EnttRegistryType& InRegistry,
            const EntityType& InLinked) -> T_Fragment*;

    private:
        EntityType _Entity;
        EntityType _PrevSibling;
        EntityType _NextSibling;
        bool _IsReverseLinked = false;

    public:
        CK_PROPERTY_GET(_Entity);
        CK_PROPERTY_GET(_PrevSibling);
        CK_PROPERTY_GET(_NextSibling);
        CK_PROPERTY_GET(_IsReverseLinked);

        CK_DEFINE_CONSTRUCTORS(FFragment_LifetimeOwner, _Entity);
    };

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKECS_API, OnEntityBeginDestroy, FCk_Delegate_OnBeginDestroy, FCk_Handle);
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::algo
{
    struct CKECS_API IsDestructionPhase
    {
    public:
        auto operator()(const FCk_Handle& InHandle) const -> bool;

    private:
        ECk_EntityLifetime_DestructionPhase _DestructionPhase = ECk_EntityLifetime_DestructionPhase::BeginDestroy;

    public:
        CK_DEFINE_CONSTRUCTOR(IsDestructionPhase, _DestructionPhase)
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API Is_NOT_DestructionPhase
    {
    public:
        auto operator()(const FCk_Handle& InHandle) const -> bool;

    private:
        ECk_EntityLifetime_DestructionPhase _DestructionPhase = ECk_EntityLifetime_DestructionPhase::BeginDestroy;

    public:
        CK_DEFINE_CONSTRUCTOR(Is_NOT_DestructionPhase, _DestructionPhase)
    };
}

// --------------------------------------------------------------------------------------------------------------------
