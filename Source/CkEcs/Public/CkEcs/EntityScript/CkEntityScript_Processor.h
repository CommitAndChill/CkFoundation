#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment_Data.h"

#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Processor/CkProcessor_NetModePolicy.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck { class FProcessor_EntityLifetime_DestructionPhase_Endplay; }

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // One entry an _InOrder processor collected from its view in the pass under way.
    template <typename T_HandleType>
    struct TEntityScript_OrderedEntry
    {
        FFragment_EntityScript_OrderingSequence _Ordering;
        T_HandleType _Handle;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_SpawnEntity_HandleRequests : public TProcessor<
            FProcessor_EntityScript_SpawnEntity_HandleRequests,
            ck::TReadOnly<FFragment_EntityScript_RequestSpawnEntity>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using MarkedDirtyBy = FFragment_EntityScript_RequestSpawnEntity;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity& InRequestFragment) -> void;

    private:
        friend class FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder;

        // The spawn step for one request, shared with FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder: the
        // liveness cancel, the mid-construction defer, the spawn and the request's destruction. A deferred request keeps
        // its marker (T_RequestFragment); every other outcome removes it. Defined in the .cpp, which is its only caller.
        template <typename T_RequestFragment>
        static auto
        DoProcess_Request(
            HandleType InHandle,
            const T_RequestFragment& InRequestFragment) -> void;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FCk_Request_EntityScript_SpawnEntity& InRequest) -> void;

        static auto
        DoCancel_NewEntity(
            const FCk_Request_EntityScript_SpawnEntity& InRequest) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The spawn step for ordered scripts (FFragment_EntityScript_RequestSpawnEntity_InOrder). Ordered requests that reach
    // the step in one pass go through it in (group, sequence) order, after the pass's unordered requests; a deferred one
    // reaches it in a later pass and nothing waits for it. Liveness is evaluated at each entry's turn.
    class CKECS_API FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder : public TProcessor<
            FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder,
            ck::TReadOnly<FFragment_EntityScript_RequestSpawnEntity_InOrder>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_SpawnEntity_HandleRequests>;
        using MarkedDirtyBy = FFragment_EntityScript_RequestSpawnEntity_InOrder;
        // Every pass of this processor's custom DoTick works only on entities carrying its marker, so the scheduler may
        // skip it while none exists (CkProcessorDescriptor.h, MainPassRequiredFragments).
        using MainPassRequiredFragments = entt::type_list<FFragment_EntityScript_RequestSpawnEntity_InOrder>;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        auto
        DoTick(
            TimeType InDeltaT) -> void;

        auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity_InOrder& InRequestFragment) -> void;

    private:
        TArray<TEntityScript_OrderedEntry<HandleType>> _Collected;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // A request entity torn down before its spawn request was ever drained (world teardown, or its script entity
    // destroyed on the same stack that enqueued it) leaves a caller awaiting completion hanging forever. This
    // completes those with Failed_Cancelled. The drain view deliberately keeps CK_IGNORE_PENDING_KILL WITHOUT
    // excluding FTag_DestroyEntity_Initiate: the drain's own liveness branch cancels a spawn whose owner, request
    // or pending entity died, and excluding the tag would suppress that completion.
    class CKECS_API FProcessor_EntityScript_SpawnEntity_CancelPendingRequests : public TProcessor<
            FProcessor_EntityScript_SpawnEntity_CancelPendingRequests,
            ck::TReadOnly<FFragment_EntityScript_RequestSpawnEntity>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity& InRequestFragment) -> void;
    };

    // The same cancellation for an ordered script's request (FFragment_EntityScript_RequestSpawnEntity_InOrder).
    class CKECS_API FProcessor_EntityScript_SpawnEntity_CancelPendingOrderedRequests : public TProcessor<
            FProcessor_EntityScript_SpawnEntity_CancelPendingOrderedRequests,
            ck::TReadOnly<FFragment_EntityScript_RequestSpawnEntity_InOrder>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript_RequestSpawnEntity_InOrder& InRequestFragment) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_ContinueConstruction : public ck_exp::TProcessor<
            FProcessor_EntityScript_ContinueConstruction,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            FTag_EntityScript_ContinueConstruction,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        // After both spawn processors, so an ordered script built this pass goes on through the pipeline this pass.
        using RunAfter = TDepList<FProcessor_EntityScript_SpawnEntity_HandleRequests, FProcessor_EntityScript_SpawnEntity_HandleRequests_InOrder>;
        using MarkedDirtyBy = FTag_EntityScript_ContinueConstruction;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_Replicate : public ck_exp::TProcessor<
            FProcessor_EntityScript_Replicate,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FRequest_EntityScript_Replicate>,
            FTag_EntityScript_FinishConstruction,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_ContinueConstruction>;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;
        static constexpr auto WorldTypeRequirement = ECk_ProcessorWorldTypeRequirement::RuntimeOnly;
        // Either half of the view wakes it: a Continue-flow script finishes after a pass already saw its request with
        // no finish tag, and only the tag then moves.
        using MarkedDirtyByAnyOf = TDepList<FRequest_EntityScript_Replicate, FTag_EntityScript_FinishConstruction>;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        // Replayed by the settle barrier (not a trigger): a spawn the barrier executes is replicated in the same pass,
        // before FinishConstruction consumes the tag this view requires.
        using LocalSettleAfter = FGroup_Gameplay_Script;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FRequest_EntityScript_Replicate& InRequest) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_FinishConstruction : public ck_exp::TProcessor<
            FProcessor_EntityScript_FinishConstruction,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            FTag_EntityScript_FinishConstruction,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_Replicate>;
        using MarkedDirtyBy = FTag_EntityScript_FinishConstruction;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript&) -> void;

    private:
        friend class FProcessor_EntityScript_FinishConstruction_InOrder;

        // The finish step for one script, shared with FProcessor_EntityScript_FinishConstruction_InOrder. The caller has
        // already removed FTag_EntityScript_FinishConstruction.
        static auto
        DoFinish(
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The finish step for ordered scripts, which FinishConstruction parks (FTag_EntityScript_FinishConstruction_InOrder).
    // Ordered scripts parked in one pass finish in (group, sequence) order, after the pass's unordered scripts.
    // Liveness is evaluated at each entry's turn, with the test this step's view applies.
    class CKECS_API FProcessor_EntityScript_FinishConstruction_InOrder : public ck_exp::TProcessor<
            FProcessor_EntityScript_FinishConstruction_InOrder,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            ck::TReadOnly<FFragment_EntityScript_OrderingSequence>,
            FTag_EntityScript_FinishConstruction_InOrder,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_FinishConstruction>;
        using MarkedDirtyBy = FTag_EntityScript_FinishConstruction_InOrder;
        // Every pass of this processor's custom DoTick works only on entities carrying its marker, so the scheduler may
        // skip it while none exists (CkProcessorDescriptor.h, MainPassRequiredFragments).
        using MainPassRequiredFragments = entt::type_list<FTag_EntityScript_FinishConstruction_InOrder>;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        auto
        DoTick(
            TimeType InDeltaT) -> void;

        auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript,
            const FFragment_EntityScript_OrderingSequence& InOrdering) -> void;

    private:
        TArray<TEntityScript_OrderedEntry<HandleType>> _Collected;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_PendingReplicationRetry : public ck_exp::TProcessor<
            FProcessor_EntityScript_PendingReplicationRetry,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            FTag_EntityScript_PendingReplicationRetry,
            ck::TReadOnly<FFragment_EntityScript_PendingReplicationRetryTimestamp>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_FinishConstruction, FProcessor_EntityScript_FinishConstruction_InOrder>;
        static constexpr auto WorldTypeRequirement = ECk_ProcessorWorldTypeRequirement::RuntimeOnly;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript,
            const FFragment_EntityScript_PendingReplicationRetryTimestamp& InTimestamp) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_BeginPlay : public ck_exp::TProcessor<
            FProcessor_EntityScript_BeginPlay,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            FTag_EntityScript_BeginPlay,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        // After both finish processors, so an ordered script finished this pass begins play this pass.
        using RunAfter = TDepList<FProcessor_EntityScript_FinishConstruction, FProcessor_EntityScript_FinishConstruction_InOrder>;
        static constexpr auto WorldTypeRequirement = ECk_ProcessorWorldTypeRequirement::RuntimeOnly;
        using MarkedDirtyBy = FTag_EntityScript_BeginPlay;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript) -> void;

    private:
        friend class FProcessor_EntityScript_BeginPlay_InOrder;

        // The BeginPlay step for one script, shared with FProcessor_EntityScript_BeginPlay_InOrder. The caller has
        // already removed FTag_EntityScript_BeginPlay.
        static auto
        DoBeginPlay(
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // BeginPlay for ordered scripts, which BeginPlay parks (FTag_EntityScript_BeginPlay_InOrder). Ordered scripts parked
    // in one pass begin play in (group, sequence) order, after the pass's unordered scripts. Liveness is evaluated at
    // each entry's turn, with the test this step's view applies.
    class CKECS_API FProcessor_EntityScript_BeginPlay_InOrder : public ck_exp::TProcessor<
            FProcessor_EntityScript_BeginPlay_InOrder,
            FCk_Handle_EntityScript,
            ck::TReadOnly<FFragment_EntityScript>,
            ck::TReadOnly<FFragment_EntityScript_OrderingSequence>,
            FTag_EntityScript_BeginPlay_InOrder,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Script;
        using RunAfter = TDepList<FProcessor_EntityScript_BeginPlay>;
        static constexpr auto WorldTypeRequirement = ECk_ProcessorWorldTypeRequirement::RuntimeOnly;
        using MarkedDirtyBy = FTag_EntityScript_BeginPlay_InOrder;
        // Every pass of this processor's custom DoTick works only on entities carrying its marker, so the scheduler may
        // skip it while none exists (CkProcessorDescriptor.h, MainPassRequiredFragments).
        using MainPassRequiredFragments = entt::type_list<FTag_EntityScript_BeginPlay_InOrder>;
        static constexpr auto LoadPolicy = ECk_ProcessorLoadPolicy::RunsDuringLoad; // load-gate kernel
        static constexpr auto HydrationQuarantinePolicy = ECk_ProcessorHydrationQuarantine::Exempt; // load-gate kernel
        using LocalSettleAfter = FGroup_Gameplay_Script;
        static constexpr auto LocalSettleTrigger = true;

    public:
        using TProcessor::TProcessor;

    public:
        auto
        DoTick(
            TimeType InDeltaT) -> void;

        auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            const FFragment_EntityScript& InEntityScript,
            const FFragment_EntityScript_OrderingSequence& InOrdering) -> void;

    private:
        TArray<TEntityScript_OrderedEntry<HandleType>> _Collected;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECS_API FProcessor_EntityScript_EndPlay : public ck_exp::TProcessor<
            FProcessor_EntityScript_EndPlay,
            FCk_Handle_EntityScript,
            ck::TReadWrite<FFragment_EntityScript>,
            FTag_EntityScript_HasBegunPlay,
            CK_IF_END_PLAY>
    {
    public:
        // EntityScript_EndPlay lives alongside the other feature *_EndPlay processors in FGroup_EndPlay.
        // The cross-group RunAfter on DestructionPhase_Endplay is superseded by the group-level ordering
        // (FGroup_EndPlay::RunAfter = FGroup_EntityLifecycle).
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            const TimeType& InDeltaT,
            HandleType InHandle,
            FFragment_EntityScript& InEntityScript) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
