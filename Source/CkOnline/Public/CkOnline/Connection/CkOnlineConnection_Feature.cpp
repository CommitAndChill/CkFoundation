#include "CkOnlineConnection_Feature.h"

#include "CkOnline/Connection/CkOnlineConnection_Processor.h"
#include "CkOnline/CkOnline_Utils.h"
#include "CkOnline/CkOnline_Fragment.h"
#include "CkOnline/Direct/CkOnlineDirect_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Scheduler/CkProcessorGraph.h"
#include "CkEcs/Scheduler/CkProcessorTraits.inl.h"

namespace ck::connection
{
    auto FFeature::TryCreate(TSharedRef<ITransport> InTransport) -> TSharedPtr<FFeature>
    {
        if (NOT IsInGameThread()) { return {}; }
        return MakeShared<FFeature>(MoveTemp(InTransport));
    }

    FFeature::FFeature(TSharedRef<ITransport> InTransport) : _Transport(MoveTemp(InTransport)) {}

    auto FFeature::TryAttach(FCk_Handle_OnlineService InService) -> bool
    {
        if (_Closed || ck::IsValid(_Handle) || NOT IsInGameThread() ||
            NOT UCk_Utils_OnlineService_UE::Has(InService) || InService.Has<FFragment_OnlineConnection>()) { return false; }
        auto& Fragment = InService.Add<FFragment_OnlineConnection>();
        Fragment._Feature = AsShared();
        Fragment._Snapshot.Set_State(ECk_OnlineConnection_State::Idle);
        _Handle = ck::StaticCast<FCk_Handle_OnlineConnection>(InService);
        return true;
    }

    auto FFeature::Get_LocalProcessors() const -> TArray<FProcessorDescriptor>
    {
        return {
            BuildDescriptor<FProcessor_OnlineConnection_HandleRequests>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineConnection_HandleRequests{InRegistry}; }),
            BuildDescriptor<FProcessor_OnlineConnection_Update>([](const FCk_Registry& InRegistry)
                -> concepts::FTickableType { return FProcessor_OnlineConnection_Update{InRegistry}; })
        };
    }

    auto FFeature::CanDispatchWork() const -> bool
    {
        if (_Closed || NOT IsInGameThread() || ck::Is_NOT_Valid(_Handle) ||
            _Handle.Has<FTag_DestroyEntity_Initiate>()) { return false; }
        const auto Host = _Handle.Get<FFragment_OnlineService>().Get_Host().Pin();
        return Host && Host->Get_IsAcceptingRequests();
    }

    auto FFeature::MatchesControl(const FEntryIntent& InIntent) const -> bool
    {
        if (NOT UCk_Utils_OnlineDirect_UE::Has(InIntent.DirectService) ||
            InIntent.DirectService.Has<FTag_DestroyEntity_Initiate>()) { return false; }
        const auto Host = InIntent.DirectService.Get<FFragment_OnlineService>().Get_Host().Pin();
        if (NOT Host || NOT Host->Get_IsAcceptingRequests()) { return false; }
        return InIntent.ExpectedMembers == InIntent.Binding.Get_Members().Num() &&
            InIntent.Binding.Get_Hosting() == InIntent.Hosting &&
            InIntent.Binding.Matches(UCk_Utils_OnlineDirect_UE::Get_Snapshot(InIntent.DirectService));
    }

    auto FFeature::TryPrepare(const FCk_Request_OnlineConnection& InRequest, FEntryIntent& OutIntent) const -> bool
    {
        OutIntent = {};
        const auto& Spec = InRequest.Get_Spec();
        const auto Action = InRequest.Get_Action();
        const bool Hosting = Action == ECk_OnlineConnection_Action::Host;
        if ((NOT Hosting && Action != ECk_OnlineConnection_Action::Join) ||
            Spec.Get_ExpectedMembers() < 1 || Spec.Get_ExpectedMembers() > 8 ||
            NOT FMath::IsFinite(Spec.Get_EntryTimeoutSeconds()) || Spec.Get_EntryTimeoutSeconds() < 0.1 ||
            Spec.Get_EntryTimeoutSeconds() > 120.0 || NOT FMath::IsFinite(Spec.Get_ReturnTimeoutSeconds()) ||
            Spec.Get_ReturnTimeoutSeconds() < 0.1 || Spec.Get_ReturnTimeoutSeconds() > 120.0 ||
            Spec.Get_Endpoint().Len() > 21 || NOT Spec.Get_Binding().Get_IsValid()) { return false; }
        auto Intent = FEntryIntent{};
        Intent.DirectService = Spec.Get_DirectService();
        Intent.Binding = Spec.Get_Binding();
        Intent.ExpectedMembers = Spec.Get_ExpectedMembers();
        Intent.Hosting = Hosting;
        Intent.EntryTimeoutSeconds = Spec.Get_EntryTimeoutSeconds();
        Intent.ReturnTimeoutSeconds = Spec.Get_ReturnTimeoutSeconds();
        if (NOT MatchesControl(Intent)) { return false; }
        auto Target = FCk_OnlineDirect_ListenTarget{};
        auto ReturnTarget = FCk_OnlineDirect_ListenTarget{};
        if (NOT UCk_Utils_OnlineDirect_UE::TryBuildListenTarget(Spec.Get_GameplayMap(), Spec.Get_GameplayMode(),
                Intent.Binding.Get_HostAddress(), Hosting ? Spec.Get_ListenPort() : 1, Target) ||
            NOT UCk_Utils_OnlineDirect_UE::TryBuildListenTarget(Spec.Get_ReturnMap(), Spec.Get_ReturnMode(),
                TEXT("127.0.0.1"), 1, ReturnTarget)) { return false; }
        Intent.GameplayMap = Spec.Get_GameplayMap().ToSoftObjectPath().GetLongPackageName();
        Intent.GameplayMode = Spec.Get_GameplayMode().Get()->GetPathName();
        Intent.ReturnMap = Spec.Get_ReturnMap().ToSoftObjectPath().GetLongPackageName();
        Intent.ReturnMode = Spec.Get_ReturnMode().Get()->GetPathName();
        if (Hosting)
        {
            Intent.ListenPort = Spec.Get_ListenPort();
            Intent.EntryUrl = Target.Get_TravelUrl();
            Intent.Endpoint = Target.Get_GameplayEndpoint();
        }
        else
        {
            if (NOT UCk_Utils_OnlineDirect_UE::TryParseReadyLobbyEndpoint(Spec.Get_Endpoint(),
                Intent.Binding, Intent.Endpoint)) { return false; }
            Intent.EntryUrl = Intent.Endpoint;
        }
        if (NOT _Transport->CanEnter(Intent) || NOT CanDispatchWork()) { return false; }
        OutIntent = MoveTemp(Intent);
        return true;
    }

    auto FFeature::IsIdle() const -> bool
    {
        if (NOT CanDispatchWork()) { return false; }
        const auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        return Fragment._Snapshot.Get_State() == ECk_OnlineConnection_State::Idle &&
            Fragment._Requests.IsEmpty() && _Transport->IsIdle();
    }

    auto FFeature::TryQueue(const FCk_Request_OnlineConnection& InRequest,
        FCk_OnlineConnection_Operation& OutOperation) -> bool
    {
        const auto Target = InRequest.Get_Target();
        OutOperation = {};
        if (NOT CanDispatchWork()) { return false; }
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        if (Fragment._Requests.Num() >= 8) { return false; }
        const auto State = Fragment._Snapshot.Get_State();
        const auto SameKey = [](const auto& InLeft, const auto& InRight)
        {
            return InLeft.Get_Id().IsValid() && InLeft.Get_Generation().IsValid() &&
                InLeft.Get_Id() == InRight.Get_Id() && InLeft.Get_Generation() == InRight.Get_Generation();
        };
        auto Intent = FEntryIntent{};
        switch (InRequest.Get_Action())
        {
            case ECk_OnlineConnection_Action::Host:
            case ECk_OnlineConnection_Action::Join:
                if (State != ECk_OnlineConnection_State::Idle || NOT Fragment._Requests.IsEmpty() ||
                    NOT TryPrepare(InRequest, Intent)) { return false; }
                break;
            case ECk_OnlineConnection_Action::ConfirmGameReady:
                if (State != ECk_OnlineConnection_State::AwaitingGameReady ||
                    NOT SameKey(Target, Fragment._Snapshot.Get_Operation())) { return false; }
                break;
            case ECk_OnlineConnection_Action::Cancel:
                if ((State != ECk_OnlineConnection_State::Entering && State != ECk_OnlineConnection_State::AwaitingGameReady) ||
                    NOT SameKey(Target, Fragment._Snapshot.Get_Operation())) { return false; }
                break;
            case ECk_OnlineConnection_Action::Return:
                if ((State != ECk_OnlineConnection_State::Entering && State != ECk_OnlineConnection_State::AwaitingGameReady &&
                        State != ECk_OnlineConnection_State::Active) ||
                    NOT SameKey(Target, State == ECk_OnlineConnection_State::Active ?
                        Fragment._Snapshot.Get_Session() : Fragment._Snapshot.Get_Operation())) { return false; }
                break;
            default: return false;
        }
        const auto Host = _Handle.Get<FFragment_OnlineService>().Get_Host().Pin();
        if (NOT Host || NOT CanDispatchWork()) { return false; }
        auto Command = FCommand{};
        static_cast<FCk_Request_Base&>(Command.Request) = static_cast<const FCk_Request_Base&>(InRequest);
        Command.Request.Set_Action(InRequest.Get_Action()).Set_Target(Target).Set_Spec({});
        Command.Operation.Set_Id(FGuid::NewGuid()).Set_Generation(Host->Get_Diagnostics().Get_Generation());
        Command.Intent = MoveTemp(Intent);
        const auto Now = Host->Get_MonotonicTime().Get_Seconds();
        if (Command.Request.Get_Action() == ECk_OnlineConnection_Action::Host ||
            Command.Request.Get_Action() == ECk_OnlineConnection_Action::Join)
        {
            const auto Deadline = Now + Command.Intent.EntryTimeoutSeconds;
            if (NOT FMath::IsFinite(Deadline) || Deadline <= Now) { return false; }
        }
        OutOperation = Command.Operation;
        Fragment._Requests.Add(MoveTemp(Command));
        return true;
    }

    auto FFeature::SetState(ECk_OnlineConnection_State InState, const FString& InDetail) -> void
    {
        auto& Snapshot = _Handle.Get<FFragment_OnlineConnection>()._Snapshot;
        Snapshot.Set_State(InState);
        if (NOT InDetail.IsEmpty()) { Snapshot.Set_LastError(InDetail.Left(512)); }
        if (Snapshot.Get_Revision() < MAX_int32) { Snapshot.Set_Revision(Snapshot.Get_Revision() + 1); }
        _Changed = true;
    }

    auto FFeature::Complete(FCommand InCommand, ECk_OnlineConnection_Result InResult, const FString& InDetail) -> void
    {
        auto Completion = FCk_OnlineConnection_Completion{};
        Completion.Set_Operation(InCommand.Operation).Set_Target(InCommand.Request.Get_Target()).Set_Action(InCommand.Request.Get_Action())
            .Set_Result(InResult).Set_Detail(InDetail.Left(512));
        if (InCommand.Request.Get_Action() == ECk_OnlineConnection_Action::Return ||
            InCommand.Request.Get_Action() == ECk_OnlineConnection_Action::Cancel)
        { Completion.Set_RecoveryCause(_Handle.Get<FFragment_OnlineConnection>()._Snapshot.Get_RecoveryCause()); }
        _Dispatch.Add({MoveTemp(InCommand.Request), MoveTemp(Completion)});
    }

    auto FFeature::CompleteActive(ECk_OnlineConnection_Result InResult, const FString& InDetail) -> void
    {
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        if (NOT Fragment._Active.IsSet()) { return; }
        auto Command = MoveTemp(Fragment._Active.GetValue());
        Fragment._Active.Reset();
        Fragment._Snapshot.Set_Operation({});
        Complete(MoveTemp(Command), InResult, InDetail);
    }

    auto FFeature::StartReturn(FCommand InCommand, double InNow) -> void
    {
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        Fragment._Deadline = InNow + Fragment._SessionIntent.ReturnTimeoutSeconds;
        Fragment._Snapshot.Set_Operation(InCommand.Operation);
        Fragment._Active = MoveTemp(InCommand);
        SetState(ECk_OnlineConnection_State::AwaitingReturn);
        if (NOT FMath::IsFinite(Fragment._Deadline) || Fragment._Deadline <= InNow)
        {
            CompleteActive(ECk_OnlineConnection_Result::ReturnFailed, TEXT("Return deadline cannot advance."));
            Fragment._Snapshot.Set_Session({});
            SetState(ECk_OnlineConnection_State::Failed, TEXT("Return deadline cannot advance."));
        }
    }

    auto FFeature::Recover(ECk_OnlineConnection_Result InResult, const FString& InDetail, double InNow) -> void
    {
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        const auto Target = Fragment._Snapshot.Get_State() == ECk_OnlineConnection_State::Active ?
            Fragment._Snapshot.Get_Session() : Fragment._Snapshot.Get_Operation();
        CompleteActive(InResult, InDetail);
        Fragment._Snapshot.Set_LastError(InDetail.Left(512)).Set_RecoveryCause(InResult);
        auto Recovery = FCommand{};
        Recovery.Request.Set_Action(ECk_OnlineConnection_Action::Return).Set_Target(Target);
        const auto Host = _Handle.Get<FFragment_OnlineService>().Get_Host().Pin();
        Recovery.Operation.Set_Id(FGuid::NewGuid()).Set_Generation(Host->Get_Diagnostics().Get_Generation());
        StartReturn(MoveTemp(Recovery), InNow);
    }

    auto FFeature::HandleRequests(double InNow) -> void
    {
        if (NOT CanDispatchWork()) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        auto Requests = MoveTemp(Fragment._Requests);
        Fragment._Requests.Reset();
        for (auto& Command : Requests)
        {
            if (NOT CanDispatchWork())
            { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::Cancelled, TEXT("Connection owner stopped.")); continue; }
            const auto State = Fragment._Snapshot.Get_State();
            const auto Target = Command.Request.Get_Target();
            const auto CurrentKey = State == ECk_OnlineConnection_State::Active ?
                Fragment._Snapshot.Get_Session() : Fragment._Snapshot.Get_Operation();
            const bool Matches = Target.Get_Id().IsValid() && Target.Get_Id() == CurrentKey.Get_Id() &&
                Target.Get_Generation() == CurrentKey.Get_Generation();
            switch (Command.Request.Get_Action())
            {
                case ECk_OnlineConnection_Action::Host:
                case ECk_OnlineConnection_Action::Join:
                {
                    const auto Deadline = InNow + Command.Intent.EntryTimeoutSeconds;
                    if (State != ECk_OnlineConnection_State::Idle || NOT FMath::IsFinite(Deadline) || Deadline <= InNow ||
                        NOT MatchesControl(Command.Intent))
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Entry prerequisites changed before execution.")); break; }
                    const auto CanEnter = _Transport->CanEnter(Command.Intent);
                    if (NOT CanDispatchWork())
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::Cancelled, TEXT("Connection owner stopped during preflight.")); break; }
                    if (NOT CanEnter)
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Entry context changed before execution.")); break; }
                    Fragment._SessionIntent = MoveTemp(Command.Intent);
                    Fragment._Deadline = Deadline;
                    Fragment._Snapshot.Set_Operation(Command.Operation).Set_Session({}).Set_Hosting(Fragment._SessionIntent.Hosting)
                        .Set_Endpoint(Fragment._SessionIntent.Endpoint).Set_LastError({}).Set_RecoveryCause(ECk_OnlineConnection_Result::InvalidState);
                    Fragment._Active = MoveTemp(Command);
                    SetState(ECk_OnlineConnection_State::Entering);
                    const auto Submitted = _Transport->BeginEntry(Fragment._SessionIntent);
                    if (NOT CanDispatchWork()) { break; }
                    if (NOT Submitted)
                    {
                        CompleteActive(ECk_OnlineConnection_Result::InvalidState, TEXT("Entry rejected before travel."));
                        Fragment._SessionIntent = {};
                        SetState(ECk_OnlineConnection_State::Idle, TEXT("Entry rejected before travel."));
                    }
                    break;
                }
                case ECk_OnlineConnection_Action::ConfirmGameReady:
                {
                    if (State != ECk_OnlineConnection_State::AwaitingGameReady || NOT Matches ||
                        InNow >= Fragment._Deadline || NOT MatchesControl(Fragment._SessionIntent))
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Readiness confirmation is stale, expired or premature.")); break; }
                    const auto Observation = _Transport->Observe();
                    if (NOT CanDispatchWork())
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::Cancelled, TEXT("Connection owner stopped during observation.")); break; }
                    if (NOT Observation.TransportEstablished || Observation.TransportLost || NOT Observation.Error.IsEmpty())
                    {
                        Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Readiness confirmation has no live transport."));
                        Recover(ECk_OnlineConnection_Result::TransportError,
                            Observation.Error.IsEmpty() ? TEXT("Transport was lost before gameplay readiness.") : Observation.Error, InNow);
                        break;
                    }
                    Fragment._Snapshot.Set_Session(Fragment._Snapshot.Get_Operation());
                    CompleteActive(ECk_OnlineConnection_Result::Succeeded, TEXT("Transport and caller gameplay readiness confirmed."));
                    SetState(ECk_OnlineConnection_State::Active);
                    Complete(MoveTemp(Command), ECk_OnlineConnection_Result::Succeeded, TEXT("Gameplay readiness accepted."));
                    break;
                }
                case ECk_OnlineConnection_Action::Return:
                case ECk_OnlineConnection_Action::Cancel:
                    if (NOT Matches || (State != ECk_OnlineConnection_State::Entering &&
                        State != ECk_OnlineConnection_State::AwaitingGameReady && State != ECk_OnlineConnection_State::Active) ||
                        (Command.Request.Get_Action() == ECk_OnlineConnection_Action::Cancel && State == ECk_OnlineConnection_State::Active))
                    { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Return target no longer owns an active entry or session.")); break; }
                    CompleteActive(ECk_OnlineConnection_Result::Cancelled, TEXT("Entry cancelled by a configured return."));
                    StartReturn(MoveTemp(Command), InNow);
                    break;
                default:
                    Complete(MoveTemp(Command), ECk_OnlineConnection_Result::InvalidState, TEXT("Unsupported connection action."));
                    break;
            }
        }
    }

    auto FFeature::Update(double InNow) -> void
    {
        if (NOT CanDispatchWork()) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        const auto State = Fragment._Snapshot.Get_State();
        if (State == ECk_OnlineConnection_State::Idle || State == ECk_OnlineConnection_State::Failed) { return; }
        if (State == ECk_OnlineConnection_State::AwaitingReturn || State == ECk_OnlineConnection_State::Returning)
        {
            if (InNow >= Fragment._Deadline)
            {
                CompleteActive(ECk_OnlineConnection_Result::ReturnFailed, TEXT("Configured return timed out."));
                Fragment._Snapshot.Set_Session({});
                SetState(ECk_OnlineConnection_State::Failed, TEXT("Configured return timed out."));
                return;
            }
            if (State == ECk_OnlineConnection_State::AwaitingReturn)
            {
                const auto Admission = _Transport->BeginReturn(Fragment._SessionIntent);
                if (NOT CanDispatchWork()) { return; }
                if (Admission == EReturnAdmission::Started) { SetState(ECk_OnlineConnection_State::Returning); }
                else if (Admission == EReturnAdmission::Rejected)
                {
                    CompleteActive(ECk_OnlineConnection_Result::ReturnFailed, TEXT("Configured return rejected its context or target."));
                    Fragment._Snapshot.Set_Session({});
                    SetState(ECk_OnlineConnection_State::Failed, TEXT("Configured return rejected its context or target."));
                }
                return;
            }
            const auto Observation = _Transport->Observe();
            if (NOT CanDispatchWork()) { return; }
            if (NOT Observation.Error.IsEmpty())
            {
                CompleteActive(ECk_OnlineConnection_Result::ReturnFailed, Observation.Error);
                Fragment._Snapshot.Set_Session({});
                SetState(ECk_OnlineConnection_State::Failed, Observation.Error);
            }
            else if (Observation.ReturnEstablished)
            {
                CompleteActive(ECk_OnlineConnection_Result::Succeeded, TEXT("New configured idle Lobby world observed."));
                Fragment._Snapshot.Set_Session({});
                Fragment._SessionIntent = {};
                SetState(ECk_OnlineConnection_State::Idle);
            }
            return;
        }
        if ((State == ECk_OnlineConnection_State::Entering || State == ECk_OnlineConnection_State::AwaitingGameReady) &&
            InNow >= Fragment._Deadline)
        { Recover(ECk_OnlineConnection_Result::TimedOut, TEXT("Gameplay entry deadline expired before caller readiness."), InNow); return; }
        if (NOT MatchesControl(Fragment._SessionIntent))
        { Recover(ECk_OnlineConnection_Result::ControlLost, TEXT("Ready control binding or profile was lost."), InNow); return; }
        const auto Observation = _Transport->Observe();
        if (NOT CanDispatchWork()) { return; }
        if (Observation.TransportLost || NOT Observation.Error.IsEmpty())
        { Recover(ECk_OnlineConnection_Result::TransportError, Observation.Error.IsEmpty() ? TEXT("Established gameplay transport was lost.") : Observation.Error, InNow); return; }
        if (State == ECk_OnlineConnection_State::Entering && Observation.TransportEstablished)
        { SetState(ECk_OnlineConnection_State::AwaitingGameReady); }
    }

    auto FFeature::Dispatch() -> void
    {
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{}))
        { _Dispatch.Reset(); _Changed = false; return; }
        const auto Values = MoveTemp(_Dispatch);
        _Dispatch.Reset();
        const auto Snapshot = _Handle.Get<FFragment_OnlineConnection>()._Snapshot;
        const auto Changed = _Changed;
        _Changed = false;
        for (const auto& Value : Values)
        {
            auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
            Fragment._LastCompletion = Value.Completion;
            if (Fragment._CompletedCount < MAX_int32) { ++Fragment._CompletedCount; }
            const auto Result = Value.Completion.Get_Result() == ECk_OnlineConnection_Result::Succeeded ?
                ECk_Request_OperationResult::Succeeded : Value.Completion.Get_Result() == ECk_OnlineConnection_Result::Cancelled ?
                ECk_Request_OperationResult::Failed_Cancelled : ECk_Request_OperationResult::Failed;
            Value.Request.TryFireCompletion(_Handle, Result);
            UUtils_Signal_OnOnlineConnectionCompleted::Broadcast(_Handle, MakePayload(_Handle, Value.Completion));
        }
        if (Changed) { UUtils_Signal_OnOnlineConnectionChanged::Broadcast(_Handle, MakePayload(_Handle, Snapshot)); }
    }

    auto FFeature::BeginShutdown() -> void
    {
        if (_Closed) { return; }
        _Closed = true;
        _Transport->Shutdown();
        if (ck::Is_NOT_Valid(_Handle, ck::IsValid_Policy_IncludePendingKill{})) { return; }
        auto& Fragment = _Handle.Get<FFragment_OnlineConnection>();
        const auto Requests = MoveTemp(Fragment._Requests);
        Fragment._Requests.Reset();
        for (auto Command : Requests)
        { Complete(MoveTemp(Command), ECk_OnlineConnection_Result::Cancelled, TEXT("Connection owner stopped.")); }
        CompleteActive(ECk_OnlineConnection_Result::Cancelled, TEXT("Connection owner stopped."));
        Fragment._Snapshot.Set_Session({});
        Fragment._SessionIntent = {};
        SetState(ECk_OnlineConnection_State::Stopped);
    }
}
