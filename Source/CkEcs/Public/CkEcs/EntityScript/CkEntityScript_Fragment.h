#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"

#include "CkEcs/EntityScript/CkEntityScript_Fragment_Data.h"
#include "CkEcs/Tag/CkTag.h"

#include "CkEcs/Signal/CkSignal_Macros.h"
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_EntityScript_UE;
class UCk_Utils_EntityScript_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_EntityScript_ContinueConstruction);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_FinishConstruction);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_BeginPlay);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_HasBegunPlay);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_HasEndedPlay);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_PendingReplicationRetry);

    // Stamped by FinishConstruction the frame an entity finishes construction; the hydration dispatchers skip
    // entities carrying it (their feature Setups drain in the pump AFTER FGroup_DeferredApply, so an apply this
    // main-pass would be stomped) and the last dispatcher clears it registry-wide. Transient — never snapshotted.
    CK_DEFINE_ECS_TAG(FTag_EntityScript_ConstructedThisFrame);

    // Stamped by FinishConstruction / BeginPlay on an ordered script instead of running the step on it; the step's
    // _InOrder processor consumes it and runs the step in sequence order. Transient — never snapshotted.
    CK_DEFINE_ECS_TAG(FTag_EntityScript_FinishConstruction_InOrder);
    CK_DEFINE_ECS_TAG(FTag_EntityScript_BeginPlay_InOrder);

    // --------------------------------------------------------------------------------------------------------------------

    // An ordered script's place in its ordering group: the group (the empty tag is the world's default group) and the
    // sequence its spawn was given when it was requested on this machine. Taken by UCk_Utils_EntityScript_UE::DoAdd and
    // carried by the ordered request; written once on the script entity, by the spawn step, once the script object
    // exists (a spawn that fails before then leaves none). Transient — never snapshotted.
    struct FFragment_EntityScript_OrderingSequence
    {
    public:
        CK_GENERATED_BODY(FFragment_EntityScript_OrderingSequence);

    private:
        FGameplayTag _Group;
        int64 _Sequence = 0;

    public:
        CK_PROPERTY_GET(_Group);
        CK_PROPERTY_GET(_Sequence);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_EntityScript_OrderingSequence, _Group, _Sequence);
    };

    // The next sequence of each ordering group of one world, keyed by group (the empty tag is the default group).
    // Lives in the registry's ctx; monotonic and never reset.
    struct FCtx_EntityScript_OrderingSequence
    {
        TMap<FGameplayTag, int64> _NextByGroup;
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct FFragment_EntityScript_PendingReplicationRetryTimestamp
    {
    public:
        CK_GENERATED_BODY(FFragment_EntityScript_PendingReplicationRetryTimestamp);

    private:
        FCk_Time _TaggedAt;

    public:
        CK_PROPERTY_GET(_TaggedAt);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_EntityScript_PendingReplicationRetryTimestamp, _TaggedAt);
    };

    // --------------------------------------------------------------------------------------------------------------------

    using FFragment_EntityScript_RequestSpawnEntity = FCk_Request_EntityScript_SpawnEntity;

    // An ordered script's spawn request: a distinct type, so FProcessor_EntityScript_SpawnEntity_HandleRequests never
    // sees it and FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder runs it in sequence order. It carries the
    // FFragment_EntityScript_OrderingSequence the spawn step publishes to the script entity.
    struct CKECS_API FFragment_EntityScript_RequestSpawnEntity_InOrder : public FCk_Request_EntityScript_SpawnEntity
    {
    public:
        CK_GENERATED_BODY(FFragment_EntityScript_RequestSpawnEntity_InOrder);

    public:
        FFragment_EntityScript_RequestSpawnEntity_InOrder() = default;

        FFragment_EntityScript_RequestSpawnEntity_InOrder(
            const FCk_Request_EntityScript_SpawnEntity& InRequest,
            const FFragment_EntityScript_OrderingSequence& InOrdering);

    private:
        FFragment_EntityScript_OrderingSequence _Ordering;

    public:
        CK_PROPERTY_GET(_Ordering);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API FFragment_EntityScript
    {
    public:
        CK_GENERATED_BODY(FFragment_EntityScript);

    public:
        friend class UCk_Utils_EntityScript_UE;
        friend class FProcessor_EntityScript_EndPlay;

    public:
        FFragment_EntityScript() = default;

        explicit
        FFragment_EntityScript(
            UCk_EntityScript_UE* InScript);

    private:
        // WEAK — lifetime owned by the CkCore ObjectPooling subsystem (NotInstanced = the CDO)
        TWeakObjectPtr<UCk_EntityScript_UE> _Script;

    public:
        CK_PROPERTY_GET(_Script);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API FRequest_EntityScript_Replicate
    {
    public:
        CK_GENERATED_BODY(FRequest_EntityScript_Replicate);

    public:
        FRequest_EntityScript_Replicate() = default;
        FRequest_EntityScript_Replicate(
            const FCk_Handle& InOwner,
            const TOptional<FCk_Handle>& InContextOwnerOverride,
            const FInstancedStruct& InSpawnParams,
            UCk_EntityScript_UE* InScript);

    private:
        FCk_Handle _Owner;
        TOptional<FCk_Handle> _ContextOwnerOverride;
        FInstancedStruct _SpawnParams;
        TWeakObjectPtr<UCk_EntityScript_UE> _Script;

    public:
        CK_PROPERTY_GET(_Owner);
        CK_PROPERTY_GET(_ContextOwnerOverride);
        CK_PROPERTY_GET(_SpawnParams);
        CK_PROPERTY_GET(_Script);
    };

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
        CKECS_API,
        OnConstructed,
        FCk_Delegate_EntityScript_Constructed,
        FCk_Handle_EntityScript);

    // --------------------------------------------------------------------------------------------------------------------

    CK_ECS_DEFINE_CALLSTACK_FRAGMENT_FOR(FFragment_EntityScript);

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API FFragment_PendingReplication
    {
    public:
        CK_GENERATED_BODY(FFragment_PendingReplication);

    public:
        auto
        Add(
            UClass* InEntityScriptClass,
            FCk_Handle InPendingEntity,
            FInstancedStruct InSpawnParams) -> void;

        auto
        ConsumeFirst(
            UClass* InEntityScriptClass,
            const UCk_EntityScript_UE* InCDO,
            const FCk_Handle& InConstructedEntity) -> FCk_Handle;

        auto
        CleanupRemaining() -> void;

    private:
        struct FPendingEntry
        {
            FCk_Handle _Entity;
            FInstancedStruct _SpawnParams;
        };

        TMap<TObjectKey<UClass>, TArray<FPendingEntry>> _PendingByClass;
    };
}

// --------------------------------------------------------------------------------------------------------------------
