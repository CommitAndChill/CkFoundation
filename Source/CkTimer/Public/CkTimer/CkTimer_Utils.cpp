#include "CkTimer_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkRecord/RecordEntry/CkRecordEntry_Fragment.h"

#include "Engine/World.h"

#include "CkEcsExt/EntityHolder/CkEntityHolder_Utils.h"

#include "CkProfile/Stats/CkStats.h"

#include "CkTimer/CkTimer_Fragment.h"
#include "CkTimer/CkTimer_Log.h"

#include <NativeGameplayTags.h>

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(Tag_Timer_CategoryName, TEXT("Timer"))

// --------------------------------------------------------------------------------------------------------------------

DECLARE_STATS_GROUP(TEXT("CkTimer"), STATGROUP_CkTimer_Details, STATCAT_Advanced);

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::MakeStatId(
        const FCk_Handle_Timer& InTimer)
    -> TStatId
{
    const auto& StatString = ck::Format_UE(TEXT("Timer Broadcast Event [{}]"), UCk_Utils_Timer_UE::Get_Name(InTimer));
    return CK_CREATE_DYNAMIC_STAT_ID(STATGROUP_CkTimer_Details, StatString);
}

auto
    UCk_Utils_Timer_UE::
    Add(
        FCk_Handle& InHandle,
        const FCk_Timer_Spec& InParams)
    -> FCk_Handle_Timer
{
    const auto IsTimerOwnerValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(IsTimerOwnerValid,
        TEXT("Timer Add rejected invalid owner [{}]."), InHandle)
    { return {}; }
    if (NOT IsTimerOwnerValid)
    { return {}; }

    if (NOT UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InHandle))
    { return {}; }

    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InHandle, [&](FCk_Handle InNewEntity)
    {
        if (InParams.Get_TimerName().IsValid())
        {
            UCk_Utils_GameplayLabel_UE::Add(InNewEntity, InParams.Get_TimerName());
        }
#if NOT CK_DISABLE_ECS_HANDLE_DEBUGGING
        else
        {
            UCk_Utils_Handle_UE::Set_DebugName(InNewEntity, "Timer: No Name Specified");
        }
#endif

        InNewEntity.Add<ck::FFragment_Timer_Params>(InParams.Get_Behavior());
        InNewEntity.Add<ck::FFragment_Timer>(FCk_Chrono{InParams.Get_Duration()});

        InNewEntity.Add<ck::FTag_Timer_NeedsSetup>();

        if (InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountDown)
        { InNewEntity.Add<ck::FTag_Timer_Countdown>(); }

        if (InParams.Get_StartingState() == ECk_Timer_State::Running)
        {
            InNewEntity.Add<ck::FTag_Timer_NeedsUpdate>();
        }
    });

    const auto IsNewTimerEntityValid = ck::IsValid(NewEntity);
    CK_ENSURE_IF_NOT(IsNewTimerEntityValid,
        TEXT("Timer Add could not create a timer entity for owner [{}]."), InHandle)
    { return {}; }
    if (NOT IsNewTimerEntityValid)
    { return {}; }

    auto NewTimerEntity = UCk_Utils_Timer_UE::CastChecked(NewEntity);

    RecordOfTimers_Utils::AddIfMissing(InHandle, ECk_Record_EntryHandlingPolicy::Default);
    RecordOfTimers_Utils::Request_Connect(InHandle, NewTimerEntity, ECk_Record_LabelRequirementPolicy::Optional);

    return NewTimerEntity;
}

namespace ck_timer_capture
{
    auto HasCurrentOwnerContext(const FCk_Handle& InOwner) -> bool
    {
        if (NOT IsInGameThread() || InOwner == FCk_Handle{})
        { return false; }

        const auto Registry = InOwner.Get_RegistryView();
        const auto RegistryHandle = Registry.Get_RegistryHandle();
        if (ck::registry_table::Get_IsInParallelRegion(RegistryHandle) ||
            NOT ck::IsValid(InOwner) ||
            InOwner.Has_Any<ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>())
        { return false; }

        const auto WorldRoot = FCk_Handle{Registry.Get_TransientEntity(), RegistryHandle};
        if (NOT ck::IsValid(WorldRoot) ||
            WorldRoot.Has_Any<ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>() ||
            NOT WorldRoot.Has<TWeakObjectPtr<UWorld>>())
        { return false; }

        // A valid handle in the registry is not necessarily attached to this world.
        // Follow only live direct owners and reject malformed or circular chains.
        auto Current = InOwner;
        TArray<FCk_Handle> Visited;
        while (Current != WorldRoot)
        {
            if (Current == FCk_Handle{} ||
                Current.Get_RegistryView().Get_RegistryHandle() != RegistryHandle ||
                NOT ck::IsValid(Current) ||
                Current.Has_Any<ck::FTag_DestroyEntity_Initiate,
                    ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                    ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>() ||
                NOT Current.Has<ck::FFragment_LifetimeOwner>() ||
                Visited.Contains(Current))
            { return false; }
            Visited.Add(Current);
            Current = Current.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        }

        auto* const World = WorldRoot.Get<TWeakObjectPtr<UWorld>>().Get();
        if (NOT ck::IsValid(World) ||
            World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            World->bIsTearingDown)
        { return false; }

        auto* const EcsWorld = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (NOT ck::IsValid(EcsWorld) ||
            EcsWorld->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            EcsWorld->Get_LoadHold() == ECk_EcsWorld_LoadHold::Teardown ||
            UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World) != WorldRoot)
        { return false; }

        return UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InOwner);
    }

    auto HasValidSpec(const FCk_Timer_Spec& InParams) -> bool
    {
        return FMath::IsFinite(InParams.Get_Duration().Get_Seconds()) &&
            (InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountUp ||
             InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountDown) &&
            (InParams.Get_Behavior() == ECk_Timer_Behavior::StopOnDone ||
             InParams.Get_Behavior() == ECk_Timer_Behavior::ResetOnDone ||
             InParams.Get_Behavior() == ECk_Timer_Behavior::PauseOnDone) &&
            InParams.Get_StartingState() == ECk_Timer_State::Paused;
    }

    auto HasFrozenContext(const FCk_Handle& InOwner, const FCk_Handle& InWorldRoot,
        const UWorld* InWorld) -> bool
    {
        if (NOT HasCurrentOwnerContext(InOwner))
        { return false; }
        const auto Registry = InOwner.Get_RegistryView();
        const auto CurrentRoot = FCk_Handle{Registry.Get_TransientEntity(), Registry.Get_RegistryHandle()};
        return CurrentRoot == InWorldRoot &&
            CurrentRoot.Get<TWeakObjectPtr<UWorld>>().Get() == InWorld;
    }
}

auto
    UCk_Utils_Timer_UE::
    TryAddCapturedPaused(
        const FCk_Handle& InOwner,
        const FCk_Timer_Spec& InParams)
    -> FCk_Timer_AddCapturedResult
{
    auto Result = FCk_Timer_AddCapturedResult{};
    if (NOT ck_timer_capture::HasCurrentOwnerContext(InOwner) ||
        NOT ck_timer_capture::HasValidSpec(InParams) ||
        InOwner.Has<ck::FFragment_RecordOfTimers>() ||
        InOwner.Has<ck::FFragment_RecordOfEntityExtensions>())
    { return Result; }

    const auto OwnerRegistry = InOwner.Get_RegistryView();
    const auto FrozenWorldRoot = FCk_Handle{OwnerRegistry.Get_TransientEntity(),
        OwnerRegistry.Get_RegistryHandle()};
    auto* const FrozenWorld = FrozenWorldRoot.Get<TWeakObjectPtr<UWorld>>().Get();

    // The allocator stamps lifetime ownership before invoking this callback. The first
    // callback action retains its raw return, before label/fragment/record composition.
    auto Owner = InOwner;
    auto Composed = false;
    Result.bAllocationAttempted = true;
    const auto Returned = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner,
        [&](FCk_Handle InNewEntity)
    {
        Result.RawEntity = InNewEntity;
        if (NOT ck_timer_capture::HasFrozenContext(Owner, FrozenWorldRoot, FrozenWorld) ||
            InNewEntity == FCk_Handle{} ||
            InNewEntity.Get_RegistryView().Get_RegistryHandle() !=
                Owner.Get_RegistryView().Get_RegistryHandle() ||
            NOT ck::IsValid(InNewEntity) ||
            InNewEntity.Has_Any<ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>() ||
            InNewEntity.Has_Any<ck::FFragment_Timer, ck::FFragment_Timer_Params,
                ck::FTag_Timer_NeedsSetup, ck::FTag_Timer_NeedsUpdate,
                ck::FTag_Timer_Countdown, ck::FFragment_RecordEntry>() ||
            NOT InNewEntity.Has<ck::FFragment_LifetimeOwner>() ||
            InNewEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != Owner)
        { return; }

        if (InParams.Get_TimerName().IsValid())
        { UCk_Utils_GameplayLabel_UE::Add(InNewEntity, InParams.Get_TimerName()); }
#if NOT CK_DISABLE_ECS_HANDLE_DEBUGGING
        else
        { UCk_Utils_Handle_UE::Set_DebugName(InNewEntity, "Timer: No Name Specified"); }
#endif
        InNewEntity.Add<ck::FFragment_Timer_Params>(InParams.Get_Behavior());
        InNewEntity.Add<ck::FFragment_Timer>(FCk_Chrono{InParams.Get_Duration()});
        InNewEntity.Add<ck::FTag_Timer_NeedsSetup>();
        if (InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountDown)
        { InNewEntity.Add<ck::FTag_Timer_Countdown>(); }
        Composed = true;
    });

    if (NOT Composed || Returned != Result.RawEntity ||
        NOT ck_timer_capture::HasFrozenContext(Owner, FrozenWorldRoot, FrozenWorld) ||
        Result.RawEntity == FCk_Handle{} ||
        Result.RawEntity.Get_RegistryView().Get_RegistryHandle() !=
            Owner.Get_RegistryView().Get_RegistryHandle() ||
        NOT ck::IsValid(Result.RawEntity) ||
        Result.RawEntity.Has_Any<ck::FTag_DestroyEntity_Initiate,
            ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
            ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>())
    { return Result; }

    auto& Raw = Result.RawEntity;
    if (NOT Raw.Has<ck::FFragment_LifetimeOwner>() ||
        Raw.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != Owner ||
        NOT Raw.Has<ck::FFragment_Timer_Params>() ||
        NOT Raw.Has<ck::FFragment_Timer>() ||
        NOT Raw.Has<ck::FTag_Timer_NeedsSetup>() ||
        Raw.Has<ck::FTag_Timer_Countdown>() !=
            (InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountDown) ||
        Raw.Has<ck::FTag_Timer_NeedsUpdate>() ||
        Raw.Get<ck::FFragment_Timer_Params>().Get_Behavior() != InParams.Get_Behavior())
    { return Result; }

    auto Timer = ck::StaticCast<FCk_Handle_Timer>(Raw);
    // This captured API owns first creation of the timer record. An existing
    // record's private policy cannot be safely inspected from CkTimer.
    if (Owner.Has<ck::FFragment_RecordOfTimers>() ||
        Owner.Has<ck::FFragment_RecordOfEntityExtensions>())
    { return Result; }
    RecordOfTimers_Utils::AddIfMissing(Owner, ECk_Record_EntryHandlingPolicy::Default);
    if (NOT ck_timer_capture::HasFrozenContext(Owner, FrozenWorldRoot, FrozenWorld) ||
        Raw.Get_RegistryView().Get_RegistryHandle() !=
            Owner.Get_RegistryView().Get_RegistryHandle() ||
        NOT ck::IsValid(Raw) ||
        NOT Owner.Has<ck::FFragment_RecordOfTimers>() ||
        Owner.Has<ck::FFragment_RecordOfEntityExtensions>())
    { return Result; }
    RecordOfTimers_Utils::Request_Connect(Owner, Timer,
        ECk_Record_LabelRequirementPolicy::Optional);
    if (NOT ck_timer_capture::HasFrozenContext(Owner, FrozenWorldRoot, FrozenWorld) ||
        Raw.Get_RegistryView().Get_RegistryHandle() !=
            Owner.Get_RegistryView().Get_RegistryHandle() ||
        NOT ck::IsValid(Raw) || NOT Raw.Has<ck::FFragment_RecordEntry>() ||
        NOT Owner.Has<ck::FFragment_RecordOfTimers>() ||
        Owner.Has<ck::FFragment_RecordOfEntityExtensions>() ||
        NOT RecordOfTimers_Utils::Get_ContainsEntry(Owner, Timer))
    { return Result; }

    // Request_Connect appends its forward entry and RecordEntry backlink in the
    // same synchronous call, with no caller delegate here. The reverse private
    // field has no public read API; this does not claim independent backlink proof.
    Result.Timer = Timer;
    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Timer_UE::
    AddOrReplace(
        FCk_Handle& InTimerOwnerEntity,
        const FCk_Timer_Spec& InParams)
    -> FCk_Handle_Timer
{
    auto MaybeExistingTimerEntity = TryGet_Timer(InTimerOwnerEntity, InParams.Get_TimerName());

    if (ck::Is_NOT_Valid(MaybeExistingTimerEntity))
    { return Add(InTimerOwnerEntity, InParams); }

    // Replace re-applies the WHOLE spec: fragments re-seed and every spec-driven tag is set or
    // cleared, exactly as a fresh Add would leave them. NeedsSetup re-runs the Setup processor so
    // a countdown chrono is Completed to its GoalValue again.
    MaybeExistingTimerEntity.Replace<ck::FFragment_Timer_Params>(InParams.Get_Behavior());
    MaybeExistingTimerEntity.Replace<ck::FFragment_Timer>(FCk_Chrono{InParams.Get_Duration()});

    MaybeExistingTimerEntity.AddOrGet<ck::FTag_Timer_NeedsSetup>();

    if (InParams.Get_CountDirection() == ECk_Timer_CountDirection::CountDown)
    { MaybeExistingTimerEntity.AddOrGet<ck::FTag_Timer_Countdown>(); }
    else
    { MaybeExistingTimerEntity.Try_Remove<ck::FTag_Timer_Countdown>(); }

    if (InParams.Get_StartingState() == ECk_Timer_State::Running)
    { MaybeExistingTimerEntity.AddOrGet<ck::FTag_Timer_NeedsUpdate>(); }
    else
    { MaybeExistingTimerEntity.Try_Remove<ck::FTag_Timer_NeedsUpdate>(); }

    return MaybeExistingTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    AddMultiple(
        FCk_Handle& InHandle,
        const FCk_MultipleTimer_Spec& InParams)
    -> TArray<FCk_Handle_Timer>
{
    return ck::algo::Transform<TArray<FCk_Handle_Timer>>(InParams.Get_TimerParams(),
    [&](const FCk_Timer_Spec& InTimerParams)
    {
        return Add(InHandle, InTimerParams);
    });
}

auto
    UCk_Utils_Timer_UE::
    Has_Any(
        const FCk_Handle& InAttributeOwnerEntity)
    -> bool
{
    return RecordOfTimers_Utils::Has(InAttributeOwnerEntity);
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_Timer_UE, FCk_Handle_Timer, ck::FFragment_Timer);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Timer_UE::
    TryGet_Timer(
        const FCk_Handle& InTimerOwnerEntity,
        FGameplayTag InTimerName)
    -> FCk_Handle_Timer
{
    return RecordOfTimers_Utils::Get_ValidEntry_ByTag(InTimerOwnerEntity, InTimerName);
}

auto
    UCk_Utils_Timer_UE::
    Get_Name(
        const FCk_Handle_Timer& InTimerEntity)
    -> FGameplayTag
{
    // Unnamed timers are a designed state (Add only labels a named timer) — return an invalid tag for them
    // instead of tripping the label ensure on every read.
    if (NOT UCk_Utils_GameplayLabel_UE::Has(InTimerEntity))
    { return {}; }

    return UCk_Utils_GameplayLabel_UE::Get_Label(InTimerEntity);
}

auto
    UCk_Utils_Timer_UE::
    Get_CurrentState(
        const FCk_Handle_Timer& InTimerEntity)
    -> ECk_Timer_State
{
    return InTimerEntity.Has<ck::FTag_Timer_NeedsUpdate>() ? ECk_Timer_State::Running : ECk_Timer_State::Paused;
}

auto
    UCk_Utils_Timer_UE::
    Get_CountDirection(
        const FCk_Handle_Timer& InTimerEntity)
    -> ECk_Timer_CountDirection
{
    return InTimerEntity.Has<ck::FTag_Timer_Countdown>() ?
        ECk_Timer_CountDirection::CountDown :
        ECk_Timer_CountDirection::CountUp;
}

auto
    UCk_Utils_Timer_UE::
    Get_Behavior(
        const FCk_Handle_Timer& InTimerEntity)
    -> ECk_Timer_Behavior
{
    return InTimerEntity.Get<ck::FFragment_Timer_Params>().Get_Behavior();
}

auto
    UCk_Utils_Timer_UE::
    Get_CurrentTimerValue(
        const FCk_Handle_Timer& InTimerEntity)
    -> FCk_Chrono
{
    return InTimerEntity.Get<ck::FFragment_Timer>().Get_Chrono();
}

auto
    UCk_Utils_Timer_UE::
    ForEach_Timer(
        const FCk_Handle& InTimerOwnerEntity,
        const FInstancedStruct& InOptionalPayload,
        const FCk_Lambda_InHandle& InDelegate)
    -> TArray<FCk_Handle_Timer>
{
    auto Timers = TArray<FCk_Handle_Timer>{};

    ForEach_Timer(InTimerOwnerEntity, [&](FCk_Handle_Timer InTimer)
    {
        if (InDelegate.IsBound())
        { InDelegate.Execute(InTimer, InOptionalPayload); }
        else
        { Timers.Emplace(InTimer); }
    });

    return Timers;
}

auto
    UCk_Utils_Timer_UE::
    ForEach_Timer(
        const FCk_Handle& InTimerOwnerEntity,
        const TFunction<void(FCk_Handle_Timer)>& InFunc)
    -> void
{
    RecordOfTimers_Utils::ForEach_ValidEntry(InTimerOwnerEntity, InFunc);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Timer_UE::
    Request_Reset(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    const auto Request = FCk_Request_Timer_Manipulate{ECk_Timer_Manipulate::Reset};

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(Request);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Complete(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    const auto Request = FCk_Request_Timer_Manipulate{ECk_Timer_Manipulate::Complete};

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(Request);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Stop(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    const auto Request = FCk_Request_Timer_Manipulate{ECk_Timer_Manipulate::Stop};

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(Request);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Pause(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    const auto Request = FCk_Request_Timer_Manipulate{ECk_Timer_Manipulate::Pause};

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(Request);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Resume(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    const auto Request = FCk_Request_Timer_Manipulate{ECk_Timer_Manipulate::Resume};

    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(Request);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Jump(
        FCk_Handle_Timer& InTimerEntity,
        FCk_Request_Timer_Jump InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD_MSG(ck::FFragment_Timer_Requests, InTimerEntity,
        TEXT("Jump: Duration [{}]"), InRequest.Get_JumpDuration().Get_Seconds());

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(InRequest);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_Consume(
        FCk_Handle_Timer& InTimerEntity,
        FCk_Request_Timer_Consume InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD_MSG(ck::FFragment_Timer_Requests, InTimerEntity,
        TEXT("Consume: Duration [{}]"), InRequest.Get_ConsumeDuration().Get_Seconds());

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InTimerEntity.AddOrGet<ck::FFragment_Timer_Requests>()._Requests.Emplace(InRequest);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_ChangeCountDirection(
        FCk_Handle_Timer& InTimerEntity,
        ECk_Timer_CountDirection InCountDirection,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD_MSG(ck::FFragment_Timer_Requests, InTimerEntity,
        TEXT("ChangeCountDirection: [{}]"),
        StaticEnum<ECk_Timer_CountDirection>()->GetNameStringByValue(static_cast<int64>(InCountDirection)));

    switch(InCountDirection)
    {
        case ECk_Timer_CountDirection::CountUp:
        {
            InTimerEntity.Try_Remove<ck::FTag_Timer_Countdown>();
            break;
        }
        case ECk_Timer_CountDirection::CountDown:
        {
            InTimerEntity.AddOrGet<ck::FTag_Timer_Countdown>();
            break;
        }
    }

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InTimerEntity, ECk_Request_OperationResult::Succeeded);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    Request_ReverseDirection(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Timer
{
    CK_CALLSTACK_RECORD(ck::FFragment_Timer_Requests, InTimerEntity);

    if (InTimerEntity.Has<ck::FTag_Timer_Countdown>())
    { InTimerEntity.Remove<ck::FTag_Timer_Countdown>(); }
    else
    { InTimerEntity.Add<ck::FTag_Timer_Countdown>(); }

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InTimerEntity, ECk_Request_OperationResult::Succeeded);

    return InTimerEntity;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Timer_UE::
    BindTo_OnReset(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerReset::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnStop(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerStop::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnPause(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerPause::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnResume(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerResume::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnDone(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerDone::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnDepleted(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerDepleted::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnUpdate(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate,
        ECk_Signal_BindingPolicy  InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerUpdate::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    BindTo_OnJump(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer_Jump& InDelegate,
        ECk_Signal_BindingPolicy  InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerJump::Bind(InTimerEntity, InDelegate, InBindingPolicy);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnReset(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerReset::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnStop(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerStop::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnPause(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerPause::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnResume(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerResume::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnDone(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerDone::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnDepleted(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerDepleted::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnUpdate(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerUpdate::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

auto
    UCk_Utils_Timer_UE::
    UnbindFrom_OnJump(
        FCk_Handle_Timer& InTimerEntity,
        const FCk_Delegate_Timer_Jump& InDelegate)
    -> FCk_Handle_Timer
{
    ck::UUtils_Signal_OnTimerJump::Unbind(InTimerEntity, InDelegate);

    return InTimerEntity;
}

//--------------------------------------------------------------------------------------------------------------------
