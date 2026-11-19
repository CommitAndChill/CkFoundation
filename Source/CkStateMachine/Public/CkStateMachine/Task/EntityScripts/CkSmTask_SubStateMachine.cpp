#include "CkSmTask_SubStateMachine.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Snapshot/CkSnapshot_RestoreMarker.h" // FTag_Snapshot_SaveTransient
#include "CkStateMachine/Net/CkStateMachine_NetContextUtils.h"
#include "CkStateMachine/Net/CkStateMachine_RepData.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkStateMachine/CkStateMachine_Log.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Utils.h"
#include "CkStateMachine/Task/CkSmTask_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_sm_task_sub_state_machine
{
    // The task's index among its hosting state's sub-SM tasks, in definition order: the task record is
    // filled in DefineState order, which the DefineState fingerprint already requires to match on every
    // machine. Read from each task's script class, not its runtime fragment, so the result does not depend
    // on which sibling finished constructing first.
    auto
    DoGet_SubSmTaskOrdinal(
        const FCk_Handle_SmState& InHostingState,
        const FCk_Handle_SmTask& InTask) -> TOptional<int32>
    {
        auto Ordinal = 0;
        auto Found = false;

        UCk_Utils_StateMachine_UE::RecordOfSmTasks_Utils::ForEach_ValidEntry(InHostingState,
        [&](FCk_Handle_SmTask InEntry) -> ECk_Record_ForEachIterationResult
        {
            if (InEntry == InTask)
            {
                Found = true;
                return ECk_Record_ForEachIterationResult::Break;
            }

            if (const auto TaskClass = UCk_Utils_SmTask_UE::Get_ScriptClass(InEntry);
                ck::IsValid(TaskClass) && TaskClass->IsChildOf(UCk_SmTask_SubStateMachine::StaticClass()))
            { ++Ordinal; }

            return ECk_Record_ForEachIterationResult::Continue;
        });

        if (NOT Found)
        { return {}; }

        return Ordinal;
    }

    // The live sub-SM under InSubSm's root that already holds InSubSm's address; invalid when there is none.
    auto
    DoFind_LiveSubSmSharingAddress(
        const FCk_Handle_StateMachine& InSubSm) -> FCk_Handle_StateMachine
    {
        return UCk_Utils_StateMachine_UE::TryFind_ActiveSubSm_ByAddress(
            UCk_Utils_StateMachine_UE::Get_RootStateMachine(InSubSm),
            UCk_Utils_StateMachine_UE::Get_SubSmAddress(InSubSm));
    }

    // An address names a slot, not an instance: a hosting state left and re-entered builds a new sub-SM at the same
    // address, whose seqs restart at 1 while the root's WithHistory ring still holds the previous instance's relayed
    // events under that address. An observer's new sub-SM (watermark 0) would replay that dead history and then drop
    // the new instance's first events as stale. So when the host builds a sub-SM, the previous instance's events
    // leave the ring before the new instance publishes any; one ring delivery then carries the root's re-entry
    // without them. No-op off the host, on a root without a WithHistory container, and on a first entry.
    auto
    DoRemove_PreviousInstanceEventsFromRootRing(
        const FCk_Handle_StateMachine& InSubSm) -> void
    {
        auto Root = UCk_Utils_StateMachine_UE::Get_RootStateMachine(InSubSm);

        if (NOT UCk_Utils_Net_UE::Get_IsEntityNetMode_Host(Root))
        { return; }

        const auto* RepData = UCk_Utils_Net_UE::TryGetContainerFragmentData<FCk_RepData_StateMachine_WithHistory>(Root);
        if (RepData == nullptr)
        { return; }

        const auto Address = UCk_Utils_StateMachine_UE::Get_SubSmAddress(InSubSm);
        const auto IsAtAddress = [&](const FCk_Sm_TransitionEvent& InEvent) -> bool
        { return ck::statemachine::FSubSmAddress::Make(InEvent) == Address; };

        if (NOT RepData->Get_History().ContainsByPredicate(IsAtAddress))
        { return; }

        UCk_Utils_Net_UE::TryUpdateContainerFragment<FCk_RepData_StateMachine_WithHistory>(Root,
            [&](FCk_RepData_StateMachine_WithHistory& InRepData) -> void
            {
                InRepData.Get_History().RemoveAll(IsAtAddress);
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_SmTask_SubStateMachine::
    Construct(
        FCk_Handle& InHandle,
        const FInstancedStruct& InSpawnParams)
    -> ECk_EntityScript_ConstructionFlow
{
    // Schema-only add so siblings (e.g. UCk_SmCondition_SubSmFinished) can discover this task by
    // fragment during their own Construct/BeginPlay; the handle payload is filled at EnterTask.
    InHandle.AddOrGet<ck::FFragment_SmTask_SubStateMachine>();

    return Super::Construct(InHandle, InSpawnParams);
}

auto
    UCk_SmTask_SubStateMachine::
    EnterTask(
        FCk_Handle_SmTask InHandle,
        ECk_Sm_NetContext InNetContext)
    -> void
{
    CK_ENSURE_IF_NOT(ck::IsValid(_InitialStateClass),
        TEXT("SubStateMachine task [{}] could not resolve sub-SM class.{}"),
        InHandle, ck::Context(this))
    {
        Mark_Result(ECk_SmTaskResult::Failed);
        return;
    }

    auto ScriptEntity = DoGet_ScriptEntity();
    auto TypeUnsafeSubSmHandle = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(ScriptEntity);

    // Save-transient: the parent's task recreates the sub-SM fresh when the parent redrives on load.
    TypeUnsafeSubSmHandle.Add<ck::FTag_Snapshot_SaveTransient>();
    auto SubSmParams = FCk_StateMachine_Spec{_InitialStateClass};
    SubSmParams.Set_AutoStart(ECk_SmAutoStart::Disabled);
    _SubSmHandle = UCk_Utils_StateMachine_UE::Add(TypeUnsafeSubSmHandle, SubSmParams);

    CK_ENSURE_IF_NOT(ck::IsValid(_SubSmHandle),
        TEXT("Failed to create sub-StateMachine for task [{}]"), InHandle)
    {
        Mark_Result(ECk_SmTaskResult::Failed);
        return;
    }

    // Derived graph state, recreated right here on redrive — so the v3 loader never respawns it as a
    // top-level entity.
    _SubSmHandle.Add<ck::FTag_Sm_IsSubMachine>();

    if (const auto OwningStateMachine = ck::TUtils_Sm_OwningStateMachine::Get_StoredEntity(InHandle);
        ck::IsValid(OwningStateMachine))
    {
        // Lets consumers ask "is this a sub-SM? who owns it?" without walking lifetime -> task ->
        // owning-SM.
        ck::TUtils_Sm_OwningStateMachine::AddOrReplace(_SubSmHandle, OwningStateMachine);

        _SubSmHandle.Add<ck::FFragment_Sm_NetIdentity>(
            UCk_Utils_StateMachine_UE::Get_EffectiveAuthorityModel(OwningStateMachine),
            ck::statemachine::ComputeNetContext(OwningStateMachine));

        if (OwningStateMachine.Has<ck::FFragment_Sm_StateOverrides>())
        {
            const auto& ParentStateMachineStateOverrides = OwningStateMachine.Get<ck::FFragment_Sm_StateOverrides>();
            _SubSmHandle.Add<ck::FFragment_Sm_StateOverrides>(ParentStateMachineStateOverrides);
        }
    }

    if (ck::TUtils_Sm_ParentState::Has(ScriptEntity))
    {
        if (auto HostingState = ck::TUtils_Sm_ParentState::Get_StoredEntity(ScriptEntity);
            HostingState.Has<ck::FFragment_SmState_Hierarchy>())
        {
            DoAdd_Address(InHandle, HostingState);

            // By construction no two live sub-SMs under one root share an address; this guards a regression in how
            // the address is built (or a task entered twice). The new sub-SM is not reachable from the root yet (the
            // task fragment is filled below), so any match is another sub-SM.
            CK_ENSURE_IF_NOT(ck::Is_NOT_Valid(ck_sm_task_sub_state_machine::DoFind_LiveSubSmSharingAddress(_SubSmHandle)),
                TEXT("Sub-SM [{}] of task [{}] has network address {}, already held by the live sub-SM [{}] under root [{}]. "
                     "A relayed event for that address would reach only one of them, so this one is not started.{}"),
                _SubSmHandle, InHandle, UCk_Utils_StateMachine_UE::Get_SubSmAddress(_SubSmHandle).ToString(),
                ck_sm_task_sub_state_machine::DoFind_LiveSubSmSharingAddress(_SubSmHandle),
                UCk_Utils_StateMachine_UE::Get_RootStateMachine(_SubSmHandle), ck::Context(this))
            {
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_SubSmHandle);
                _SubSmHandle = {};
                Mark_Result(ECk_SmTaskResult::Failed);
                return;
            }

            ck_sm_task_sub_state_machine::DoRemove_PreviousInstanceEventsFromRootRing(_SubSmHandle);
        }
    }

    auto& SubSmFragment = ScriptEntity.Get<ck::FFragment_SmTask_SubStateMachine>();
    SubSmFragment._SubStateMachineHandle = _SubSmHandle;

    // Promise signal for late subscribers — e.g. sibling conditions whose EnterCondition ran first.
    ck::UUtils_Signal_OnSubSmConstructed::Broadcast(InHandle,
        ck::MakePayload(InHandle, FCk_Sm_Payload_OnSubSmConstructed{_SubSmHandle}));

    if (_CompletionBehavior == ECk_SmTask_SubSm_CompletionBehavior::SucceedOnStop)
    {
        auto Delegate = FCk_Delegate_Sm_OnStopped{};
        Delegate.BindDynamic(this, &ThisType::OnSubSmStopped);
        UCk_Utils_StateMachine_UE::BindTo_OnStopped(
            _SubSmHandle,
            Delegate,
            ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
            ECk_Signal_PostFireBehavior::DoNothing);
    }

    UCk_Utils_StateMachine_UE::Request_Start(_SubSmHandle, {});

    ck::sm::Verbose(TEXT("[SubStateMachine] Started sub-SM with initial state [{}]"),
        _InitialStateClass->GetName());

    Super::EnterTask(InHandle, InNetContext);
}

auto
    UCk_SmTask_SubStateMachine::
    ExitTask(
        FCk_Handle_SmTask InHandle,
        ECk_Sm_NetContext InNetContext)
    -> void
{
    if (ck::IsValid(_SubSmHandle))
    {
        if (_CompletionBehavior == ECk_SmTask_SubSm_CompletionBehavior::SucceedOnStop)
        {
            auto Delegate = FCk_Delegate_Sm_OnStopped{};
            Delegate.BindDynamic(this, &ThisType::OnSubSmStopped);
            UCk_Utils_StateMachine_UE::UnbindFrom_OnStopped(_SubSmHandle, Delegate);
        }

        // Synchronously exit the sub-SM's active state chain so its tasks/conditions fire their
        // DoExitTask/DoExitCondition (delegate unbinds, etc.) before the cascading destruction —
        // otherwise sub-SM children only get FProcessor_Sm_EndPlay at end-of-frame.
        ck::sm::VeryVerbose(TEXT("[SM Lifecycle] SubStateMachine ExitTask -> recursing into sub-SM [{}]"),
            _SubSmHandle);
        UCk_Utils_StateMachine_UE::Request_ExitStateMachine(_SubSmHandle);
    }
    else
    {
        ck::sm::VeryVerbose(TEXT("[SM Lifecycle] SubStateMachine ExitTask on [{}] — no sub-SM to recurse into"),
            InHandle);
    }

    _SubSmHandle = {};
    Super::ExitTask(InHandle, InNetContext);
}

auto
    UCk_SmTask_SubStateMachine::
    DoAdd_Address(
        const FCk_Handle_SmTask& InTask,
        const FCk_Handle_SmState& InHostingState)
    -> void
{
    const auto& HostingHierarchy = InHostingState.Get<ck::FFragment_SmState_Hierarchy>().Get_Hierarchy();

    // Nested levels compose: the owning SM's own ordinals (empty for a root), then this task's.
    auto Ordinals = TArray<uint8>{};
    if (const auto OwningStateMachine = ck::TUtils_Sm_OwningStateMachine::Get_StoredEntity(InTask);
        ck::IsValid(OwningStateMachine))
    { Ordinals = UCk_Utils_StateMachine_UE::Get_SubSmAddress(OwningStateMachine).Get_Ordinals(); }

    auto MaybeOrdinal = ck_sm_task_sub_state_machine::DoGet_SubSmTaskOrdinal(InHostingState, InTask);

    CK_ENSURE_IF_NOT(MaybeOrdinal.IsSet(),
        TEXT("SubStateMachine task [{}] is not in its hosting state [{}]'s task record; addressing its sub-SM as the state's first.{}"),
        InTask, InHostingState, ck::Context(this))
    { MaybeOrdinal = 0; }

    auto Ordinal = *MaybeOrdinal;

    CK_ENSURE_IF_NOT(Ordinal <= static_cast<int32>(MAX_uint8),
        TEXT("SubStateMachine task [{}] is sub-SM task [{}] of state [{}]; an address level holds at most [{}].{}"),
        InTask, Ordinal, InHostingState, static_cast<int32>(MAX_uint8), ck::Context(this))
    { Ordinal = MAX_uint8; }

    Ordinals.Add(static_cast<uint8>(Ordinal));

    _SubSmHandle.Add<ck::FFragment_Sm_ParentHierarchy>(HostingHierarchy, Ordinals);
}

// --------------------------------------------------------------------------------------------------------------------

void
    UCk_SmTask_SubStateMachine::
    OnSubSmStopped(
        FCk_Handle_StateMachine InHandle,
        FCk_Sm_Payload_OnStopped InPayload)
{
    Mark_Result(ECk_SmTaskResult::Succeeded);
}

// --------------------------------------------------------------------------------------------------------------------
