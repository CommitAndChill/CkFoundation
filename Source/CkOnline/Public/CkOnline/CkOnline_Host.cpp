#include "CkOnline_Host.h"

#include "CkOnline/CkOnline_Fragment.h"
#include "CkOnline/CkOnline_Processor.h"
#include "CkOnline/CkOnline_Utils.h"
#include "CkOnline_Log.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Processor.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorGraph.h"
#include "CkEcs/Scheduler/CkProcessorScheduler.h"
#include "CkEcs/Scheduler/CkProcessorTraits.inl.h"
#include "CkEcs/World/CkEcsWorld.h"

#include <Misc/ScopeLock.h>

namespace ck
{
    struct FOnlineServiceCallback
    {
        FGuid _Ticket;
        ECk_OnlineService_Result _Result = ECk_OnlineService_Result::Failed;
    };

    class FOnlineServiceInbox
    {
    public:
        auto Push(FGuid InTicket, ECk_OnlineService_Result InResult) -> void
        {
            const auto Lock = FScopeLock{&_Mutex};
            if (_Closed)
            { return; }
            if (_Values.Num() >= 256)
            {
                _Overflowed = true;
                return;
            }
            _Values.Add({InTicket, InResult});
        }

        auto Drain(TArray<FOnlineServiceCallback>& OutValues) -> bool
        {
            const auto Lock = FScopeLock{&_Mutex};
            OutValues = MoveTemp(_Values);
            _Values.Reset();
            const auto Overflowed = _Overflowed;
            _Overflowed = false;
            return Overflowed;
        }

        auto Close() -> void
        {
            const auto Lock = FScopeLock{&_Mutex};
            _Closed = true;
            _Values.Reset();
        }

    private:
        FCriticalSection _Mutex;
        TArray<FOnlineServiceCallback> _Values;
        bool _Closed = false;
        bool _Overflowed = false;
    };

    struct FOnlineServiceDispatch
    {
        FCk_Request_Base _Request;
        FCk_OnlineService_Completion _Completion;
        ECk_Request_OperationResult _GenericResult = ECk_Request_OperationResult::Failed;
        bool _EmitSignal = false;
    };

    namespace
    {
        template <typename TProcessorType>
        auto AddLocalDescriptor(TArray<FProcessorDescriptor>& InOutDescriptors) -> void
        {
            auto Descriptor = BuildDescriptor<TProcessorType>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return TProcessorType{InRegistry}; });
            // These are local copies; gameplay registration and its Actor/replication edges stay intact.
            Descriptor._GroupName = NAME_None;
            Descriptor._RunAfter.Reset();
            Descriptor._RunBefore.Reset();
            Descriptor._TickGroupMode = ECk_TickGroupMode::Explicit;
            Descriptor._TickGroupValue = TG_PrePhysics;
            if (NOT InOutDescriptors.IsEmpty())
            { Descriptor._RunAfter.Add(InOutDescriptors.Last()._Name); }
            InOutDescriptors.Add(MoveTemp(Descriptor));
        }

        auto ToGenericResult(ECk_OnlineService_Result InResult) -> ECk_Request_OperationResult
        {
            switch (InResult)
            {
                case ECk_OnlineService_Result::Succeeded: return ECk_Request_OperationResult::Succeeded;
                case ECk_OnlineService_Result::Cancelled: return ECk_Request_OperationResult::Failed_Cancelled;
                default: return ECk_Request_OperationResult::Failed;
            }
        }
    }

    FOnlineServiceHost::FOnlineServiceHost(TSharedRef<IOnlineServiceAdapter> InAdapter, FCk_Time InMonotonicTime)
        : FOnlineServiceHost(InAdapter, {}, InMonotonicTime)
    {
    }

    FOnlineServiceHost::FOnlineServiceHost(TSharedPtr<IOnlineServiceAdapter> InAdapter,
        TSharedPtr<IOnlineServiceFeature> InFeature, FCk_Time InMonotonicTime)
        : _Adapter(MoveTemp(InAdapter)), _Feature(MoveTemp(InFeature)),
          _Inbox(MakeShared<FOnlineServiceInbox, ESPMode::ThreadSafe>()), _Now(InMonotonicTime)
    {
    }

    FOnlineServiceHost::~FOnlineServiceHost()
    {
        Request_Shutdown();
    }

    auto FOnlineServiceHost::TryCreate(TSharedRef<IOnlineServiceAdapter> InAdapter, FCk_Time InMonotonicTime)
        -> TSharedPtr<FOnlineServiceHost>
    {
        const auto ValidTime = IsInGameThread() && FMath::IsFinite(InMonotonicTime.Get_Seconds()) &&
            InMonotonicTime.Get_Seconds() >= 0.0;
        CK_ENSURE_IF_NOT(ValidTime, TEXT("Online service creation requires the owning thread and a finite clock.")) {}
        if (NOT ValidTime)
        { return {}; }
        auto Host = MakeShared<FOnlineServiceHost>(MoveTemp(InAdapter), InMonotonicTime);
        if (NOT Host->DoInitialize())
        { return {}; }
        return Host;
    }

    auto FOnlineServiceHost::TryCreateFeature(TSharedRef<IOnlineServiceFeature> InFeature, FCk_Time InMonotonicTime)
        -> TSharedPtr<FOnlineServiceHost>
    {
        const auto ValidTime = IsInGameThread() && FMath::IsFinite(InMonotonicTime.Get_Seconds()) &&
            InMonotonicTime.Get_Seconds() >= 0.0;
        if (!ValidTime) { return {}; }
        auto Host = MakeShared<FOnlineServiceHost>(TSharedPtr<IOnlineServiceAdapter>{}, InFeature, InMonotonicTime);
        if (!Host->DoInitialize()) { return {}; }
        return Host;
    }

    auto FOnlineServiceHost::DoInitialize() -> bool
    {
        _World = MakeUnique<FEcsWorld>();
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_World->Get_Registry());
        if (ck::Is_NOT_Valid(Entity))
        { return false; }
        auto Diagnostics = FCk_OnlineService_Diagnostics{};
        Diagnostics.Set_Generation(FGuid::NewGuid()).Set_State(ECk_OnlineService_State::Running);
        Entity.Add<FFragment_OnlineService>(TWeakPtr<FOnlineServiceHost>{AsShared()}, Diagnostics);
        Entity.Add<FFragment_OnlineService_Requests>();
        _Handle = UCk_Utils_OnlineService_UE::CastChecked(Entity);

        if (_Feature && !_Feature->TryAttach(_Handle))
        { return false; }

        auto Descriptors = TArray<FProcessorDescriptor>{};
        AddLocalDescriptor<FProcessor_OnlineService_HandleRequests>(Descriptors);
        AddLocalDescriptor<FProcessor_OnlineService_Update>(Descriptors);
        if (_Feature)
        {
            auto FeatureDescriptors = _Feature->Get_LocalProcessors();
            if (FeatureDescriptors.IsEmpty() || FeatureDescriptors.Num() > 16) { return false; }
            for (auto& Descriptor : FeatureDescriptors)
            {
                if (Descriptor._Name.IsNone() || !Descriptor._Factory || Descriptors.ContainsByPredicate(
                    [&](const FProcessorDescriptor& InExisting) { return InExisting._Name == Descriptor._Name; }))
                { return false; }
                Descriptor._GroupName = NAME_None;
                Descriptor._LocalSettleAfterGroupName = NAME_None;
                Descriptor._IsLocalSettleTrigger = false;
                Descriptor._RunAfter.Reset();
                Descriptor._RunBefore.Reset();
                Descriptor._RunAfterTags.Reset();
                Descriptor._RunBeforeTags.Reset();
                Descriptor._TickGroupMode = ECk_TickGroupMode::Explicit;
                Descriptor._TickGroupValue = TG_PrePhysics;
                Descriptor._RunAfter.Add(Descriptors.Last()._Name);
                Descriptors.Add(MoveTemp(Descriptor));
            }
        }
        AddLocalDescriptor<FProcessor_EntityLifetime_EntityJustCreated>(Descriptors);
        AddLocalDescriptor<FProcessor_EntityLifetime_DestroyEntity>(Descriptors);
        AddLocalDescriptor<FProcessor_EntityLifetime_DestructionPhase_Endplay>(Descriptors);
        AddLocalDescriptor<FProcessor_EntityLifetime_DestructionPhase_Teardown>(Descriptors);
        AddLocalDescriptor<FProcessor_EntityLifetime_DestructionPhase_Finalize>(Descriptors);
        AddLocalDescriptor<FProcessor_EntityLifetime_DestructionPhase_Await>(Descriptors);

        // Validate the complete graph before publishing; features cannot shadow lifecycle processors.
        auto Names = TSet<FName>{};
        for (const auto& Descriptor : Descriptors)
        {
            if (Names.Contains(Descriptor._Name)) { return false; }
            Names.Add(Descriptor._Name);
        }

        auto Graph = FProcessorGraphBuilder{}.Build(Descriptors, _World->Get_Registry(),
            UCk_Utils_EntityLifetime_UE::Get_TransientEntity(_World->Get_Registry()), ECk_UnresolvedRefPolicy::Strict);
        const auto CompleteGraph = Graph._Partitions.Num() == 1 &&
            Graph._Partitions.Contains(TG_PrePhysics) &&
            Graph._Partitions[TG_PrePhysics]._Nodes.Num() == Descriptors.Num() &&
            Graph._Partitions[TG_PrePhysics]._ExecutionOrder.Num() == Descriptors.Num();
        CK_ENSURE_IF_NOT(CompleteGraph, TEXT("Online service graph must contain every required processor exactly once.")) {}
        if (NOT CompleteGraph)
        { return false; }

        _Scheduler = MakeUnique<FProcessorScheduler>(MoveTemp(Graph._Partitions[TG_PrePhysics]));
        _Accepting = true;
        online::Log(TEXT("Service [{}] started with [{}] local processors."), Diagnostics.Get_Generation(), Descriptors.Num());
        return true;
    }

    auto FOnlineServiceHost::Tick(FCk_Time InMonotonicTime) -> void
    {
        const auto ValidTime = IsInGameThread() && FMath::IsFinite(InMonotonicTime.Get_Seconds()) &&
            InMonotonicTime.Get_Seconds() >= _Now.Get_Seconds();
        CK_ENSURE_IF_NOT(ValidTime, TEXT("Online service tick requires an owning-thread monotonic clock.")) {}
        if (NOT ValidTime || _Active || NOT _World || NOT _Scheduler)
        { return; }
        const auto KeepAlive = AsShared();
        const auto DeltaTime = FCk_Time{InMonotonicTime.Get_Seconds() - _Now.Get_Seconds()};
        _Now = InMonotonicTime;
        if (_Handle.Has<FTag_DestroyEntity_Initiate>())
        { _Accepting = false; }
        if (NOT _Accepting)
        {
            DoShutdown();
            return;
        }
        _Active = true;
        auto& Diagnostics = _Handle.Get<FFragment_OnlineService>()._Diagnostics;
        Diagnostics.Set_TickCount(Diagnostics.Get_TickCount() + 1);
        _Scheduler->Tick(DeltaTime, _World->Get_Registry());
        DoDispatch();
        _Active = false;
        if (NOT _Accepting)
        { DoShutdown(); }
    }

    auto FOnlineServiceHost::DoHandleRequests() -> void
    {
        if (_Destroying)
        { return; }
        auto& State = _Handle.Get<FFragment_OnlineService>();
        auto& Queue = _Handle.Get<FFragment_OnlineService_Requests>()._Requests;
        const auto Requests = MoveTemp(Queue);
        Queue.Reset();
        for (const auto& Request : Requests)
        {
            if (std::holds_alternative<FCk_Request_OnlineService_Execute>(Request))
            {
                const auto& Execute = std::get<FCk_Request_OnlineService_Execute>(Request);
                const auto Ticket = FGuid::NewGuid();
                const auto Deadline = _Now.Get_Seconds() + Execute.Get_Timeout().Get_Seconds();
                const auto ValidDeadline = FMath::IsFinite(Deadline) && Deadline > _Now.Get_Seconds();
                State._Pending.Add(Ticket, FOnlineServicePending{Execute,
                    ValidDeadline ? FCk_Time{Deadline} : _Now});
                if (NOT ValidDeadline || !_Adapter)
                {
                    DoComplete(Ticket, ECk_OnlineService_Result::Failed);
                    continue;
                }
                if (NOT _Accepting)
                {
                    DoComplete(Ticket, ECk_OnlineService_Result::Cancelled);
                    continue;
                }
                // A fresh ticket prevents a late callback from completing a reused public operation ID.
                _Adapter->Execute(Execute.Get_OperationId(), [Inbox = _Inbox, Ticket](ECk_OnlineService_Result InResult)
                { Inbox->Push(Ticket, InResult); });
            }
            else
            {
                const auto& Cancel = std::get<FCk_Request_OnlineService_Cancel>(Request);
                auto Ticket = FGuid{};
                for (const auto& Entry : State._Pending)
                {
                    if (Entry.Value.Get_Request().Get_OperationId() == Cancel.Get_OperationId())
                    { Ticket = Entry.Key; break; }
                }
                if (Ticket.IsValid())
                {
                    DoComplete(Ticket, ECk_OnlineService_Result::Cancelled);
                    if (_Adapter) { _Adapter->Cancel(Cancel.Get_OperationId()); }
                }
                _Dispatch.Add({Cancel, {}, Ticket.IsValid() ? ECk_Request_OperationResult::Succeeded :
                    ECk_Request_OperationResult::Failed, false});
            }
        }
    }

    auto FOnlineServiceHost::DoUpdate() -> void
    {
        if (_Destroying)
        { return; }
        auto& State = _Handle.Get<FFragment_OnlineService>();
        auto Expired = TArray<FGuid>{};
        for (const auto& Entry : State._Pending)
        {
            if (NOT _Accepting || _Now.Get_Seconds() >= Entry.Value.Get_Deadline().Get_Seconds())
            { Expired.Add(Entry.Key); }
        }
        for (const auto& Ticket : Expired)
        {
            const auto OperationId = State._Pending[Ticket].Get_Request().Get_OperationId();
            DoComplete(Ticket, _Accepting ? ECk_OnlineService_Result::TimedOut : ECk_OnlineService_Result::Cancelled);
            if (_Adapter) { _Adapter->Cancel(OperationId); }
        }

        auto Values = TArray<FOnlineServiceCallback>{};
        const auto Overflowed = _Inbox->Drain(Values);
        if (Overflowed)
        {
            auto Tickets = TArray<FGuid>{};
            State._Pending.GetKeys(Tickets);
            for (const auto& Ticket : Tickets)
            {
                const auto OperationId = State._Pending[Ticket].Get_Request().Get_OperationId();
                DoComplete(Ticket, ECk_OnlineService_Result::Failed);
                if (_Adapter) { _Adapter->Cancel(OperationId); }
            }
        }
        for (const auto& Value : Values)
        {
            if (NOT State._Pending.Contains(Value._Ticket))
            {
                State._Diagnostics.Set_IgnoredCallbacks(State._Diagnostics.Get_IgnoredCallbacks() + 1);
                continue;
            }
            const auto Result = Value._Result == ECk_OnlineService_Result::Succeeded ?
                ECk_OnlineService_Result::Succeeded : ECk_OnlineService_Result::Failed;
            DoComplete(Value._Ticket, Result);
        }
    }

    auto FOnlineServiceHost::DoComplete(FGuid InTicket, ECk_OnlineService_Result InResult) -> void
    {
        auto& State = _Handle.Get<FFragment_OnlineService>();
        auto Pending = FOnlineServicePending{};
        if (NOT State._Pending.RemoveAndCopyValue(InTicket, Pending))
        { return; }
        auto& Diagnostics = State._Diagnostics;
        Diagnostics.Set_Completed(Diagnostics.Get_Completed() + 1);
        if (InResult == ECk_OnlineService_Result::Cancelled)
        { Diagnostics.Set_Cancelled(Diagnostics.Get_Cancelled() + 1); }
        if (InResult == ECk_OnlineService_Result::TimedOut)
        { Diagnostics.Set_TimedOut(Diagnostics.Get_TimedOut() + 1); }
        _Dispatch.Add({Pending.Get_Request(),
            FCk_OnlineService_Completion{Pending.Get_Request().Get_OperationId(), InResult}, ToGenericResult(InResult), true});
    }

    auto FOnlineServiceHost::DoDispatch() -> void
    {
        const auto Dispatch = MoveTemp(_Dispatch);
        _Dispatch.Reset();
        for (const auto& Item : Dispatch)
        {
            if (Item._EmitSignal)
            {
                UUtils_Signal_OnOnlineServiceCompleted::Broadcast(_Handle, MakePayload(_Handle, Item._Completion));
            }
            Item._Request.TryFireCompletion(_Handle, Item._GenericResult);
        }
        if (_Feature) { _Feature->Dispatch(); }
    }

    auto FOnlineServiceHost::Request_Shutdown() -> void
    {
        const auto OwningThread = IsInGameThread();
        CK_ENSURE_IF_NOT(OwningThread, TEXT("Online service shutdown requires the owning thread.")) {}
        if (NOT OwningThread)
        { return; }
        // A listener may release the caller's last owner while shutdown is dispatching.
        // Unreal forbids AsShared/AsWeak after final release, including in the destructor.
        auto KeepAlive = TSharedPtr<FOnlineServiceHost>{};
        if (DoesSharedInstanceExist())
        { KeepAlive = AsShared(); }
        _Accepting = false;
        if (NOT _Active)
        { DoShutdown(); }
    }

    auto FOnlineServiceHost::DoShutdown() -> void
    {
        if (_ShuttingDown || NOT _World)
        { return; }
        _ShuttingDown = true;
        _Active = true;
        if (_Scheduler && ck::IsValid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        {
            _Handle.Get<FFragment_OnlineService>()._Diagnostics.Set_State(ECk_OnlineService_State::Stopping);
            // The last native owner may be destructing, so processors cannot rely on pinning its weak reference.
            DoHandleRequests();
            DoUpdate();
            DoDispatch();
            _FinalDiagnostics = UCk_Utils_OnlineService_UE::Get_Diagnostics(_Handle);
        }
        _Inbox->Close();
        if (_Feature)
        {
            _Feature->BeginShutdown();
            _Feature->Dispatch();
        }
        if (_Adapter) { _Adapter->Shutdown(); }
        if (_Scheduler && ck::IsValid(_Handle))
        {
            _Destroying = true;
            auto Entity = FCk_Handle{_Handle};
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Entity);
            // EndPlay/Teardown/Await, then Finalize, then physical destroy occupy three ordered passes.
            constexpr auto DestructionPasses = 3;
            for (auto Pass = 0; Pass < DestructionPasses; ++Pass)
            { _Scheduler->Tick(FCk_Time{0.0}, _World->Get_Registry()); }
            const auto Destroyed = ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{});
            CK_ENSURE_IF_NOT(Destroyed, TEXT("Online service lifecycle did not finish its bounded destruction drain.")) {}
            if (NOT Destroyed)
            { online::Error(TEXT("Online service is closed; abandoning an incomplete lifecycle registry.")); }
        }
        _FinalDiagnostics.Set_State(ECk_OnlineService_State::Stopped).Set_Queued(0).Set_Pending(0);
        _Scheduler.Reset();
        _World.Reset();
        _Active = false;
    }

    auto FOnlineServiceHost::Get_Handle() const -> FCk_Handle_OnlineService
    { return _Handle; }

    auto FOnlineServiceHost::Get_Diagnostics() const -> FCk_OnlineService_Diagnostics
    {
        if (NOT _World)
        { return _FinalDiagnostics; }
        auto Diagnostics = UCk_Utils_OnlineService_UE::Get_Diagnostics(_Handle);
        if (NOT _Accepting)
        { Diagnostics.Set_State(ECk_OnlineService_State::Stopping); }
        return Diagnostics;
    }

    auto FOnlineServiceHost::Get_IsAcceptingRequests() const -> bool
    { return _Accepting; }

    auto FOnlineServiceHost::WithDispatchScope(TFunctionRef<void()> InNotify) -> void
    {
        const auto KeepAlive = AsShared();
        const auto AlreadyActive = _Active;
        _Active = true;
        InNotify();
        _Active = AlreadyActive;
        if (NOT _Active && NOT _Accepting)
        { DoShutdown(); }
    }
}
