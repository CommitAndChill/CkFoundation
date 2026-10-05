#include "CkOnline/Steam/CkOnlineSteam_Session.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

namespace ck::steam
{
    namespace
    {
        class FSessionTransport final : public ITransport, public TSharedFromThis<FSessionTransport>
        {
        public:
            FSessionTransport(const FCk_OnlineSteam_Spec& InSpec, TSharedRef<ISessionProvider> InProvider)
                : _Spec(InSpec), _Provider(MoveTemp(InProvider))
            {
                RefreshIdentity(0.0);
            }

            auto Execute(const FCk_Request_OnlineSteam& InRequest, FGuid InTicket, double InNow) -> void override
            {
                if (!IsInGameThread() || !InTicket.IsValid()) { return; }
                RefreshIdentity(InNow);
                if (_Closed || _Remote.IsSet() || _CleanupOwed)
                { Emit(InTicket, ECk_OnlineSteam_Result::Rejected, TEXT("Steam session ownership is busy or awaiting cleanup.")); return; }
                if (!_Identity.Available)
                { Emit(InTicket, ECk_OnlineSteam_Result::ProviderUnavailable, _Identity.Detail); return; }
                if (!_Identity.LoggedIn || _Identity.AccountId.IsEmpty())
                { Emit(InTicket, ECk_OnlineSteam_Result::NotLoggedIn, TEXT("Sign in through the Steam client before selecting this profile.")); return; }
                const auto Action = InRequest.Get_Action();
                if (Action == ECk_OnlineSteam_Action::Login)
                { Emit(InTicket, ECk_OnlineSteam_Result::Succeeded, TEXT("The selected Steam account is signed in.")); return; }
                const auto HasSession = _Provider->HasSession();
                if ((Action == ECk_OnlineSteam_Action::CreateLobby || Action == ECk_OnlineSteam_Action::FindLobbies ||
                    Action == ECk_OnlineSteam_Action::JoinLobby) && HasSession)
                { Emit(InTicket, ECk_OnlineSteam_Result::Rejected, TEXT("Leave the current Steam lobby first.")); return; }
                if (Action == ECk_OnlineSteam_Action::LeaveLobby && !HasSession)
                { Emit(InTicket, ECk_OnlineSteam_Result::Rejected, TEXT("There is no owned Steam lobby to leave.")); return; }
                if (Action == ECk_OnlineSteam_Action::JoinLobby &&
                    (InRequest.Get_SearchGeneration() != _Snapshot.Get_SearchGeneration() ||
                     !InRequest.Get_SearchGeneration().IsValid() ||
                     !_Snapshot.Get_SearchResults().IsValidIndex(InRequest.Get_LobbyIndex()) ||
                     _Snapshot.Get_SearchResults()[InRequest.Get_LobbyIndex()].Get_OpenSlots() <= 0))
                { Emit(InTicket, ECk_OnlineSteam_Result::Rejected, TEXT("The selected lobby is stale, invalid, or full. Find again.")); return; }
                if (Action != ECk_OnlineSteam_Action::CreateLobby && Action != ECk_OnlineSteam_Action::FindLobbies &&
                    Action != ECk_OnlineSteam_Action::JoinLobby && Action != ECk_OnlineSteam_Action::LeaveLobby)
                { Emit(InTicket, ECk_OnlineSteam_Result::Rejected, TEXT("Unsupported Steam session action.")); return; }

                // Canonical lease exists before provider work, including synchronous failure callbacks.
                _Remote = FRemote{InTicket, Action, _Snapshot.Get_AccountGeneration(), false};
                _Snapshot.Set_LastError({}).Set_State(Action == ECk_OnlineSteam_Action::CreateLobby
                    ? ECk_OnlineSteam_State::Creating : Action == ECk_OnlineSteam_Action::FindLobbies
                    ? ECk_OnlineSteam_State::Searching : Action == ECk_OnlineSteam_Action::JoinLobby
                    ? ECk_OnlineSteam_State::Joining : ECk_OnlineSteam_State::Leaving);
                if (Action != ECk_OnlineSteam_Action::JoinLobby) { ClearSearch(); }
                Touch();
                _Provider->Execute(InRequest, InTicket, _Spec);
                if (Action == ECk_OnlineSteam_Action::JoinLobby)
                { _Snapshot.Set_SearchGeneration({}).Set_SearchResults({}); }
            }

            auto Poll(double InNow) -> void override
            {
                if (!IsInGameThread() || !FMath::IsFinite(InNow)) { return; }
                RefreshIdentity(InNow);
                const auto Events = _Provider->DrainCompletions();
                for (const auto& Event : Events)
                {
                    if (!_Remote.IsSet() || Event.Ticket != _Remote->Ticket || Event.Action != _Remote->Action) { continue; }
                    const auto Remote = _Remote.GetValue();
                    _Remote.Reset();
                    if (Remote.Abandoned || _Closed || Remote.AccountGeneration != _Snapshot.Get_AccountGeneration())
                    {
                        ClearSearch();
                        if (!_Identity.Available)
                        { Unknown(TEXT("Steam provider authority disappeared before cleanup could be confirmed.")); }
                        else if (_Provider->HasSession())
                        {
                            if (Remote.Action == ECk_OnlineSteam_Action::LeaveLobby)
                            { Unknown(TEXT("Steam did not confirm removal of the owned session.")); }
                            else { BeginCleanup(InNow); }
                        }
                        else { SettleCleanup(); }
                        continue;
                    }

                    auto Result = Event.Result;
                    auto Detail = Event.Detail;
                    if (Result == ECk_OnlineSteam_Result::Succeeded &&
                        (Remote.Action == ECk_OnlineSteam_Action::CreateLobby || Remote.Action == ECk_OnlineSteam_Action::JoinLobby) &&
                        (!_Provider->HasSession() || _Provider->Get_LobbyId().IsEmpty()))
                    { Result = ECk_OnlineSteam_Result::Failed; Detail = TEXT("Steam completion did not establish an owned lobby session."); }
                    if (Result == ECk_OnlineSteam_Result::Succeeded && Remote.Action == ECk_OnlineSteam_Action::LeaveLobby &&
                        _Provider->HasSession())
                    { Result = ECk_OnlineSteam_Result::Failed; Detail = TEXT("Steam completion did not remove the owned lobby session."); }

                    if (Remote.Action == ECk_OnlineSteam_Action::FindLobbies)
                    {
                        auto ValidRows = Event.SearchResults.Num() <= _Spec.Get_MaxSearchResults();
                        auto Ids = TSet<FString>{};
                        for (const auto& Row : Event.SearchResults)
                        {
                            if (Row.Get_Id().IsEmpty() || Row.Get_Id().Len() > 128 || Row.Get_OwnerName().Len() > 128 ||
                                Row.Get_Label().Len() > 64 || Row.Get_MaxMembers() < 1 || Row.Get_MaxMembers() > 16 ||
                                Row.Get_OpenSlots() < 0 || Row.Get_OpenSlots() > Row.Get_MaxMembers() || Ids.Contains(Row.Get_Id()))
                            { ValidRows = false; break; }
                            Ids.Add(Row.Get_Id());
                        }
                        if (Result == ECk_OnlineSteam_Result::Succeeded && ValidRows)
                        { _Snapshot.Set_SearchResults(Event.SearchResults).Set_SearchGeneration(Remote.Ticket); }
                        else
                        {
                            ClearSearch();
                            if (Result == ECk_OnlineSteam_Result::Succeeded)
                            { Result = ECk_OnlineSteam_Result::Failed; Detail = TEXT("Steam returned an invalid lobby selection set."); }
                        }
                    }
                    else { ClearSearch(); }

                    _Snapshot.Set_LobbyId(_Provider->Get_LobbyId());
                    if (Result == ECk_OnlineSteam_Result::Succeeded)
                    {
                        _Snapshot.Set_State(_Provider->HasSession() ? ECk_OnlineSteam_State::Joined : BaseState()).Set_LastError({});
                    }
                    else
                    {
                        _Snapshot.Set_LastError(Detail).Set_State(_Provider->HasSession()
                            ? ECk_OnlineSteam_State::CleanupUnknown : ECk_OnlineSteam_State::Failed);
                    }
                    Emit(Remote.Ticket, Result, Detail);
                    if (Result != ECk_OnlineSteam_Result::Succeeded && _Provider->HasSession())
                    {
                        if (Remote.Action == ECk_OnlineSteam_Action::LeaveLobby) { Unknown(Detail); }
                        else { BeginCleanup(InNow); }
                    }
                    Touch();
                }
                if (_CleanupOwed && !_Remote.IsSet() && _Identity.Available && !_Provider->HasSession())
                { SettleCleanup(); }
                if (_Closed && _CleanupOwed && !_Identity.Available)
                {
                    // The captured OSS instance has ended. Stop observing its interfaces,
                    // but preserve the unknown outcome instead of reporting a clean leave.
                    _Remote.Reset();
                    _Snapshot.Set_State(ECk_OnlineSteam_State::CleanupUnknown)
                        .Set_LastError(TEXT("Steam provider ended before remote cleanup could be confirmed."));
                    _CleanupOwed = false;
                    Touch();
                }
                if (_CleanupOwed && InNow >= _CleanupDeadline && _Snapshot.Get_State() != ECk_OnlineSteam_State::CleanupUnknown)
                { Unknown(TEXT("Steam cleanup is not confirmed. This provider remains reserved until it settles.")); }
            }

            auto Cancel(FGuid InTicket) -> void override
            {
                if (!IsInGameThread()) { return; }
                if (_Remote.IsSet() && _Remote->Ticket == InTicket)
                {
                    _Remote->Abandoned = true;
                    OweCleanup(_LastNow);
                    ClearSearch();
                    return;
                }
                // A completion can arrive in Poll at the same tick as the local deadline.
                // Its local ownership still belongs to the feature until DrainCompletions.
                const auto Removed = _Completions.RemoveAll([&](const FTransportCompletion& Value) { return Value.Ticket == InTicket; });
                if (Removed > 0)
                {
                    ClearSearch();
                    if (_Provider->HasSession()) { BeginCleanup(_LastNow); }
                }
            }

            auto Shutdown() -> void override
            {
                if (_Closed || !IsInGameThread()) { return; }
                _Closed = true;
                _Completions.Reset();
                ClearSearch();
                if (_Remote.IsSet())
                { _Remote->Abandoned = true; OweCleanup(_LastNow); }
                else if (_CleanupOwed)
                { /* Preserve an unconfirmed cleanup outcome; stopping is not a retry. */ }
                else if (_Provider->HasSession()) { BeginCleanup(_LastNow); }
                else { SettleCleanup(); }
                if (_CleanupOwed)
                {
                    // One bounded provider lease survives Ck registry destruction. No retry,
                    // second search, or early DestroySession is issued while OSS is pending.
                    const auto Self = AsShared();
                    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
                    {
                        Self->Poll(FPlatformTime::Seconds());
                        return Self->_CleanupOwed;
                    }));
                }
            }

            auto DrainCompletions() -> TArray<FTransportCompletion> override
            { auto Values = MoveTemp(_Completions); _Completions.Reset(); return Values; }
            auto Get_Snapshot() const -> FCk_OnlineSteam_Snapshot override { return _Snapshot; }

        private:
            struct FRemote
            {
                FGuid Ticket;
                ECk_OnlineSteam_Action Action;
                FGuid AccountGeneration;
                bool Abandoned;
            };
            auto BaseState() const -> ECk_OnlineSteam_State
            {
                return _Closed ? ECk_OnlineSteam_State::Stopped : !_Identity.Available ? ECk_OnlineSteam_State::Unavailable
                    : !_Identity.LoggedIn ? ECk_OnlineSteam_State::LoggedOut : ECk_OnlineSteam_State::Ready;
            }
            auto Touch() -> void
            { if (_Snapshot.Get_Revision() < MAX_int32) { _Snapshot.Set_Revision(_Snapshot.Get_Revision() + 1); } }
            auto Emit(FGuid InTicket, ECk_OnlineSteam_Result InResult, const FString& InDetail) -> void
            { if (!_Closed && _Completions.Num() < 32) { _Completions.Add({InTicket, InResult, InDetail}); } }
            auto ClearSearch() -> void
            { _Snapshot.Set_SearchResults({}).Set_SearchGeneration({}); _Provider->InvalidateSearch(); }
            auto OweCleanup(double InNow) -> void
            {
                if (!_CleanupOwed) { _CleanupDeadline = InNow + _Spec.Get_CleanupGraceSeconds(); }
                _CleanupOwed = true;
                _Snapshot.Set_State(ECk_OnlineSteam_State::CleaningUp).Set_LastError(TEXT("Waiting for Steam to settle the abandoned session operation."));
                Touch();
            }
            auto Unknown(const FString& InDetail) -> void
            {
                _CleanupOwed = true;
                _Snapshot.Set_State(ECk_OnlineSteam_State::CleanupUnknown).Set_LastError(InDetail);
                Touch();
            }
            auto SettleCleanup() -> void
            {
                _CleanupOwed = false;
                _Snapshot.Set_LobbyId({}).Set_State(BaseState()).Set_LastError(_Identity.Detail);
                Touch();
            }
            auto BeginCleanup(double InNow) -> void
            {
                OweCleanup(InNow);
                _Snapshot.Set_LobbyId(_Provider->Get_LobbyId());
                if (_Remote.IsSet()) { _Remote->Abandoned = true; return; }
                if (!_Provider->HasSession()) { SettleCleanup(); return; }
                const auto Ticket = FGuid::NewGuid();
                _Remote = FRemote{Ticket, ECk_OnlineSteam_Action::LeaveLobby, _Snapshot.Get_AccountGeneration(), true};
                auto Request = FCk_Request_OnlineSteam{};
                Request.Set_OperationId(Ticket).Set_Action(ECk_OnlineSteam_Action::LeaveLobby);
                _Provider->Execute(Request, Ticket, _Spec);
            }
            auto RefreshIdentity(double InNow) -> void
            {
                _LastNow = InNow;
                const auto Identity = _Provider->ObserveIdentity();
                const auto Changed = Identity.Available != _Identity.Available || Identity.LoggedIn != _Identity.LoggedIn ||
                    Identity.AccountId != _Identity.AccountId;
                const auto DisplayChanged = Identity.DisplayName != _Identity.DisplayName || Identity.Detail != _Identity.Detail;
                _Identity = Identity;
                if (Changed || !_Snapshot.Get_AccountGeneration().IsValid())
                {
                    _Snapshot.Set_AccountGeneration(FGuid::NewGuid());
                    ClearSearch();
                    for (auto& Completion : _Completions)
                    {
                        Completion.Result = ECk_OnlineSteam_Result::IdentityChanged;
                        Completion.Detail = TEXT("The selected Steam account changed before completion was consumed.");
                    }
                    if (_Remote.IsSet())
                    {
                        if (!_Remote->Abandoned)
                        { Emit(_Remote->Ticket, ECk_OnlineSteam_Result::IdentityChanged, TEXT("The selected Steam account changed during the operation.")); }
                        _Remote->Abandoned = true;
                        OweCleanup(InNow);
                    }
                    else if (_Provider->HasSession()) { BeginCleanup(InNow); }
                    else if (!_CleanupOwed) { _Snapshot.Set_State(BaseState()); }
                }
                _Snapshot.Set_AccountId(Identity.LoggedIn ? Identity.AccountId : FString{}).Set_DisplayName(Identity.DisplayName);
                if ((Changed || DisplayChanged) && !_Remote.IsSet() && !_CleanupOwed && !_Provider->HasSession())
                { _Snapshot.Set_State(BaseState()).Set_LastError(Identity.Detail); }
                if (Changed || DisplayChanged) { Touch(); }
            }

            FCk_OnlineSteam_Spec _Spec;
            TSharedRef<ISessionProvider> _Provider;
            FSessionIdentity _Identity;
            FCk_OnlineSteam_Snapshot _Snapshot;
            TOptional<FRemote> _Remote;
            TArray<FTransportCompletion> _Completions;
            double _LastNow = 0.0;
            double _CleanupDeadline = 0.0;
            bool _CleanupOwed = false;
            bool _Closed = false;
        };
    }

    auto IsValidSpec(const FCk_OnlineSteam_Spec& InSpec) -> bool
    {
        const auto Label = InSpec.Get_LobbyLabel().TrimStartAndEnd();
        return IsInGameThread() && InSpec.Get_LocalUserIndex() == 0 &&
            InSpec.Get_MaxLobbyMembers() >= 1 && InSpec.Get_MaxLobbyMembers() <= 16 &&
            InSpec.Get_MaxSearchResults() >= 1 && InSpec.Get_MaxSearchResults() <= 50 &&
            !Label.IsEmpty() && InSpec.Get_LobbyLabel().Len() <= 64 && !Label.Contains(TEXT("\r")) && !Label.Contains(TEXT("\n")) &&
            FMath::IsFinite(InSpec.Get_CleanupGraceSeconds()) && InSpec.Get_CleanupGraceSeconds() >= 1.0 &&
            InSpec.Get_CleanupGraceSeconds() <= 120.0;
    }

    auto TryCreateSessionTransport(const FCk_OnlineSteam_Spec& InSpec,
        TSharedRef<ISessionProvider> InProvider) -> TSharedPtr<ITransport>
    {
        if (!IsValidSpec(InSpec)) { return {}; }
        return MakeShared<FSessionTransport>(InSpec, MoveTemp(InProvider));
    }
}
