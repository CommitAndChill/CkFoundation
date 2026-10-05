#include "CkOnlineSteam_OssProvider.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/ScopeLock.h"

namespace ck::steam
{
    namespace
    {
        const FName SteamName(TEXT("Steam"));
        const FName ProtocolKey(TEXT("CkOnlineProtocol"));
        const FName LabelKey(TEXT("CkLobbyLabel"));
        const FString ProtocolValue(TEXT("CkSteamLobby1"));

        // A lease is held by the provider, and therefore by the transport, until retirement.
        FCriticalSection LeaseLock;
        TWeakPtr<uint8> ActiveLease;

        auto AcquireLease() -> TSharedPtr<uint8>
        {
            FScopeLock Lock(&LeaseLock);
            if (ActiveLease.IsValid())
            {
                return nullptr;
            }
            TSharedPtr<uint8> Lease = MakeShared<uint8>(0);
            ActiveLease = Lease;
            return Lease;
        }

        class FSteamSessionProvider final : public ISessionProvider,
            public TSharedFromThis<FSteamSessionProvider>
        {
        public:
            FSteamSessionProvider(TSharedPtr<uint8> InLease, IOnlineIdentityPtr InIdentity,
                IOnlineSessionPtr InSession, FString InUnavailableReason)
                : Lease(MoveTemp(InLease))
                , Identity(MoveTemp(InIdentity))
                , Session(MoveTemp(InSession))
                , UnavailableReason(MoveTemp(InUnavailableReason))
                , SessionName(*FString::Printf(TEXT("CK_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)))
            {
            }

            ~FSteamSessionProvider() override
            {
                ClearDelegate();
                // The OSS owns native state. In particular, a late Create/Join must not be
                // destroyed from this destructor while its completion remains outstanding.
            }

            auto ObserveIdentity() const -> FSessionIdentity override
            {
                FSessionIdentity Result;
                Result.Available = IsCurrent();
                if (!Result.Available)
                {
                    Result.Detail = UnavailableReason.IsEmpty()
                        ? TEXT("Steam session interface is unavailable") : UnavailableReason;
                    return Result;
                }
                Result.LoggedIn = Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
                if (!Result.LoggedIn)
                {
                    Result.Detail = TEXT("Steam local user is not logged in");
                    return Result;
                }
                const FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
                if (!UserId.IsValid() || !UserId->IsValid())
                {
                    Result.LoggedIn = false;
                    Result.Detail = TEXT("Steam local user identity is unavailable");
                    return Result;
                }
                Result.AccountId = TEXT("Steam:") + UserId->ToString();
                Result.DisplayName = Identity->GetPlayerNickname(0).Left(128);
                return Result;
            }

            auto Execute(const FCk_Request_OnlineSteam& InRequest, FGuid InTicket,
                const FCk_OnlineSteam_Spec& InSpec) -> void override
            {
                const ECk_OnlineSteam_Action Action = InRequest.Get_Action();
                if (Pending.bActive)
                {
                    Queue(InTicket, Action, ECk_OnlineSteam_Result::Rejected,
                        TEXT("A Steam session operation is already pending"));
                    return;
                }
                const FSessionIdentity Current = ObserveIdentity();
                if (!Current.Available)
                {
                    Queue(InTicket, Action, ECk_OnlineSteam_Result::ProviderUnavailable, Current.Detail);
                    return;
                }
                if (!Current.LoggedIn && Action != ECk_OnlineSteam_Action::LeaveLobby)
                {
                    Queue(InTicket, Action, ECk_OnlineSteam_Result::NotLoggedIn, Current.Detail);
                    return;
                }
                switch (Action)
                {
                case ECk_OnlineSteam_Action::Login:
                    Queue(InTicket, Action, ECk_OnlineSteam_Result::Succeeded, FString());
                    return;
                case ECk_OnlineSteam_Action::CreateLobby:
                    Create(InTicket, InSpec);
                    return;
                case ECk_OnlineSteam_Action::FindLobbies:
                    Find(InTicket, InSpec);
                    return;
                case ECk_OnlineSteam_Action::JoinLobby:
                    Join(InTicket, InRequest.Get_LobbyIndex());
                    return;
                case ECk_OnlineSteam_Action::LeaveLobby:
                    Leave(InTicket);
                    return;
                default:
                    Queue(InTicket, Action, ECk_OnlineSteam_Result::Rejected,
                        TEXT("Unsupported Steam session operation"));
                    return;
                }
            }

            auto DrainCompletions() -> TArray<FSessionCompletion> override
            {
                TArray<FSessionCompletion> Result = MoveTemp(Completions);
                Completions.Reset();
                return Result;
            }

            auto HasSession() const -> bool override
            {
                return IsCurrent() && Session->GetNamedSession(SessionName) != nullptr;
            }

            auto Get_LobbyId() const -> FString override
            {
                if (!IsCurrent())
                {
                    return FString();
                }
                const FNamedOnlineSession* Named = Session->GetNamedSession(SessionName);
                if (!Named || !Named->SessionInfo.IsValid() || !Named->SessionInfo->IsValid())
                {
                    return FString();
                }
                return Named->GetSessionIdStr();
            }

            auto InvalidateSearch() -> void override
            {
                RetainedResults.Reset();
            }

        private:
            struct FPending
            {
                bool bActive = false;
                FGuid Ticket;
                ECk_OnlineSteam_Action Action = ECk_OnlineSteam_Action::Invalid;
                FDelegateHandle Handle;
            } Pending;

            TSharedPtr<uint8> Lease;
            IOnlineIdentityPtr Identity;
            IOnlineSessionPtr Session;
            FString UnavailableReason;
            FName SessionName;
            TSharedPtr<FOnlineSessionSearch> ActiveSearch;
            TArray<FOnlineSessionSearchResult> RetainedResults;
            TArray<FSessionCompletion> Completions;

            auto IsCurrent() const -> bool
            {
                if (!Lease.IsValid() || !Identity.IsValid() || !Session.IsValid() ||
                    !FModuleManager::Get().IsModuleLoaded(TEXT("OnlineSubsystem")))
                {
                    return false;
                }
                FOnlineSubsystemModule* Module =
                    FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
                if (!Module || !Module->DoesInstanceExist(SteamName))
                {
                    return false;
                }
                IOnlineSubsystem* Steam = Module->GetOnlineSubsystem(SteamName);
                return Steam && Steam->GetIdentityInterface() == Identity &&
                    Steam->GetSessionInterface() == Session;
            }

            auto Queue(FGuid InTicket, ECk_OnlineSteam_Action InAction,
                ECk_OnlineSteam_Result InResult, FString InDetail,
                TArray<FCk_OnlineSteam_Lobby> InResults = {}) -> void
            {
                FSessionCompletion Completion;
                Completion.Ticket = InTicket;
                Completion.Action = InAction;
                Completion.Result = InResult;
                Completion.Detail = MoveTemp(InDetail);
                Completion.SearchResults = MoveTemp(InResults);
                Completions.Add(MoveTemp(Completion));
            }

            auto Begin(FGuid InTicket, ECk_OnlineSteam_Action InAction) -> void
            {
                Pending.bActive = true;
                Pending.Ticket = InTicket;
                Pending.Action = InAction;
            }

            auto ClearDelegate() -> void
            {
                if (!Pending.Handle.IsValid())
                {
                    return;
                }
                if (!IsCurrent())
                {
                    Pending.Handle.Reset();
                    return;
                }
                switch (Pending.Action)
                {
                case ECk_OnlineSteam_Action::CreateLobby:
                    Session->ClearOnCreateSessionCompleteDelegate_Handle(Pending.Handle);
                    break;
                case ECk_OnlineSteam_Action::FindLobbies:
                    Session->ClearOnFindSessionsCompleteDelegate_Handle(Pending.Handle);
                    break;
                case ECk_OnlineSteam_Action::JoinLobby:
                    Session->ClearOnJoinSessionCompleteDelegate_Handle(Pending.Handle);
                    break;
                default:
                    break;
                }
                Pending.Handle.Reset();
            }

            auto Complete(FGuid InTicket, ECk_OnlineSteam_Action InAction,
                ECk_OnlineSteam_Result InResult, FString InDetail,
                TArray<FCk_OnlineSteam_Lobby> InResults = {}) -> void
            {
                if (!Pending.bActive || Pending.Ticket != InTicket || Pending.Action != InAction)
                {
                    return;
                }
                ClearDelegate();
                Pending = FPending();
                ActiveSearch.Reset();
                Queue(InTicket, InAction, InResult, MoveTemp(InDetail), MoveTemp(InResults));
            }

            auto Create(FGuid InTicket, const FCk_OnlineSteam_Spec& InSpec) -> void
            {
                if (Session->GetNumSessions() != 0)
                {
                    Queue(InTicket, ECk_OnlineSteam_Action::CreateLobby,
                        ECk_OnlineSteam_Result::ProviderUnavailable,
                        TEXT("Another Steam session already owns the OSS session interface"));
                    return;
                }
                FOnlineSessionSettings Settings;
                Settings.NumPublicConnections = InSpec.Get_MaxLobbyMembers();
                Settings.NumPrivateConnections = 0;
                Settings.bIsLANMatch = false;
                Settings.bShouldAdvertise = true;
                Settings.bAllowInvites = true;
                Settings.bAllowJoinInProgress = true;
                Settings.bUsesPresence = true;
                Settings.bAllowJoinViaPresence = true;
                Settings.bUseLobbiesIfAvailable = true;
                Settings.Set(ProtocolKey, ProtocolValue, EOnlineDataAdvertisementType::ViaOnlineService);
                Settings.Set(LabelKey, InSpec.Get_LobbyLabel(), EOnlineDataAdvertisementType::ViaOnlineService);

                Begin(InTicket, ECk_OnlineSteam_Action::CreateLobby);
                const TWeakPtr<FSteamSessionProvider> Weak = AsShared();
                Pending.Handle = Session->AddOnCreateSessionCompleteDelegate_Handle(
                    FOnCreateSessionCompleteDelegate::CreateLambda(
                        [Weak, InTicket, Name = SessionName](FName CompletedName, bool bSuccess)
                        {
                            if (CompletedName != Name) return;
                            if (const TSharedPtr<FSteamSessionProvider> Self = Weak.Pin())
                            {
                                Self->Complete(InTicket, ECk_OnlineSteam_Action::CreateLobby,
                                    bSuccess ? ECk_OnlineSteam_Result::Succeeded : ECk_OnlineSteam_Result::Failed,
                                    bSuccess ? FString() : TEXT("Steam lobby creation failed"));
                            }
                        }));
                if (!Session->CreateSession(0, SessionName, Settings))
                {
                    Complete(InTicket, ECk_OnlineSteam_Action::CreateLobby,
                        ECk_OnlineSteam_Result::Rejected, TEXT("Steam rejected lobby creation"));
                }
            }

            auto Find(FGuid InTicket, const FCk_OnlineSteam_Spec& InSpec) -> void
            {
                RetainedResults.Reset();
                ActiveSearch = MakeShared<FOnlineSessionSearch>();
                ActiveSearch->bIsLanQuery = false;
                ActiveSearch->MaxSearchResults = InSpec.Get_MaxSearchResults();
                ActiveSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
                ActiveSearch->QuerySettings.Set(ProtocolKey, ProtocolValue, EOnlineComparisonOp::Equals);
                Begin(InTicket, ECk_OnlineSteam_Action::FindLobbies);
                const TWeakPtr<FSteamSessionProvider> Weak = AsShared();
                Pending.Handle = Session->AddOnFindSessionsCompleteDelegate_Handle(
                    FOnFindSessionsCompleteDelegate::CreateLambda([Weak, InTicket](bool bSuccess)
                    {
                        if (const TSharedPtr<FSteamSessionProvider> Self = Weak.Pin())
                        {
                            Self->OnFindComplete(InTicket, bSuccess);
                        }
                    }));
                if (!Session->FindSessions(0, ActiveSearch.ToSharedRef()))
                {
                    Complete(InTicket, ECk_OnlineSteam_Action::FindLobbies,
                        ECk_OnlineSteam_Result::Rejected, TEXT("Steam rejected lobby search"));
                }
            }

            auto OnFindComplete(FGuid InTicket, bool bSuccess) -> void
            {
                if (!Pending.bActive || Pending.Ticket != InTicket ||
                    Pending.Action != ECk_OnlineSteam_Action::FindLobbies || !ActiveSearch.IsValid() ||
                    ActiveSearch->SearchState == EOnlineAsyncTaskState::InProgress)
                {
                    return;
                }
                if (!bSuccess || ActiveSearch->SearchState != EOnlineAsyncTaskState::Done)
                {
                    Complete(InTicket, ECk_OnlineSteam_Action::FindLobbies,
                        ECk_OnlineSteam_Result::Failed, TEXT("Steam lobby search failed"));
                    return;
                }
                TArray<FOnlineSessionSearchResult> Candidates;
                TArray<FCk_OnlineSteam_Lobby> Rows;
                const int32 Limit = ActiveSearch->MaxSearchResults;
                for (const FOnlineSessionSearchResult& Found : ActiveSearch->SearchResults)
                {
                    if (Candidates.Num() >= Limit) break;
                    FString Protocol;
                    if (!Found.IsValid() ||
                        !Found.Session.SessionSettings.bUseLobbiesIfAvailable ||
                        !Found.Session.SessionSettings.bUsesPresence ||
                        !Found.Session.SessionSettings.Get(ProtocolKey, Protocol) ||
                        Protocol != ProtocolValue || Found.GetSessionIdStr().IsEmpty() ||
                        Found.GetSessionIdStr() == TEXT("InvalidSession"))
                    {
                        continue;
                    }
                    FString Label;
                    Found.Session.SessionSettings.Get(LabelKey, Label);
                    FCk_OnlineSteam_Lobby Row;
                    Row.Set_Id(Found.GetSessionIdStr())
                        .Set_OwnerName(Found.Session.OwningUserName)
                        .Set_Label(Label)
                        .Set_OpenSlots(FMath::Max(0, Found.Session.NumOpenPublicConnections))
                        .Set_MaxMembers(FMath::Max(0, Found.Session.SessionSettings.NumPublicConnections));
                    Candidates.Add(Found);
                    Rows.Add(MoveTemp(Row));
                }
                RetainedResults = MoveTemp(Candidates);
                Complete(InTicket, ECk_OnlineSteam_Action::FindLobbies,
                    ECk_OnlineSteam_Result::Succeeded, FString(), MoveTemp(Rows));
            }

            auto Join(FGuid InTicket, int32 InIndex) -> void
            {
                if (!RetainedResults.IsValidIndex(InIndex) || !RetainedResults[InIndex].IsValid())
                {
                    Queue(InTicket, ECk_OnlineSteam_Action::JoinLobby,
                        ECk_OnlineSteam_Result::Rejected, TEXT("Lobby search result is no longer valid"));
                    return;
                }
                if (Session->GetNumSessions() != 0)
                {
                    Queue(InTicket, ECk_OnlineSteam_Action::JoinLobby,
                        ECk_OnlineSteam_Result::ProviderUnavailable,
                        TEXT("Another Steam session already owns the OSS session interface"));
                    return;
                }
                Begin(InTicket, ECk_OnlineSteam_Action::JoinLobby);
                const TWeakPtr<FSteamSessionProvider> Weak = AsShared();
                Pending.Handle = Session->AddOnJoinSessionCompleteDelegate_Handle(
                    FOnJoinSessionCompleteDelegate::CreateLambda(
                        [Weak, InTicket, Name = SessionName](FName CompletedName,
                            EOnJoinSessionCompleteResult::Type Result)
                        {
                            if (CompletedName != Name) return;
                            if (const TSharedPtr<FSteamSessionProvider> Self = Weak.Pin())
                            {
                                const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success;
                                Self->Complete(InTicket, ECk_OnlineSteam_Action::JoinLobby,
                                    bSuccess ? ECk_OnlineSteam_Result::Succeeded : ECk_OnlineSteam_Result::Failed,
                                    bSuccess ? FString() : TEXT("Steam lobby join failed"));
                            }
                        }));
                if (!Session->JoinSession(0, SessionName, RetainedResults[InIndex]))
                {
                    Complete(InTicket, ECk_OnlineSteam_Action::JoinLobby,
                        ECk_OnlineSteam_Result::Rejected, TEXT("Steam rejected lobby join"));
                }
            }

            auto Leave(FGuid InTicket) -> void
            {
                if (!HasSession())
                {
                    Queue(InTicket, ECk_OnlineSteam_Action::LeaveLobby,
                        ECk_OnlineSteam_Result::Succeeded, FString());
                    return;
                }
                Begin(InTicket, ECk_OnlineSteam_Action::LeaveLobby);
                const TWeakPtr<FSteamSessionProvider> Weak = AsShared();
                const FOnDestroySessionCompleteDelegate Done = FOnDestroySessionCompleteDelegate::CreateLambda(
                    [Weak, InTicket, Name = SessionName](FName CompletedName, bool bSuccess)
                    {
                        if (CompletedName != Name) return;
                        if (const TSharedPtr<FSteamSessionProvider> Self = Weak.Pin())
                        {
                            Self->Complete(InTicket, ECk_OnlineSteam_Action::LeaveLobby,
                                bSuccess ? ECk_OnlineSteam_Result::Succeeded : ECk_OnlineSteam_Result::Failed,
                                bSuccess ? FString() : TEXT("Steam lobby leave failed"));
                        }
                    });
                if (!Session->DestroySession(SessionName, Done))
                {
                    Complete(InTicket, ECk_OnlineSteam_Action::LeaveLobby,
                        ECk_OnlineSteam_Result::Rejected, TEXT("Steam rejected lobby leave"));
                }
            }
        };
    }

    auto CreateSteamTransport(const FCk_OnlineSteam_Spec& InSpec) -> TSharedPtr<ITransport>
    {
        if (!IsValidSpec(InSpec))
        {
            return nullptr;
        }

        TSharedPtr<uint8> Lease = AcquireLease();
        IOnlineIdentityPtr Identity;
        IOnlineSessionPtr Session;
        FString UnavailableReason;
        if (!Lease.IsValid())
        {
            UnavailableReason = TEXT("Another Steam session adapter is active");
        }
        else if (!FModuleManager::Get().IsModuleLoaded(TEXT("OnlineSubsystem")))
        {
            UnavailableReason = TEXT("OnlineSubsystem module is not loaded");
        }
        else if (FOnlineSubsystemModule* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem")))
        {
            if (!Module->DoesInstanceExist(SteamName))
            {
                UnavailableReason = TEXT("Steam subsystem instance is not active");
            }
            else if (IOnlineSubsystem* Steam = Module->GetOnlineSubsystem(SteamName))
            {
                Identity = Steam->GetIdentityInterface();
                Session = Steam->GetSessionInterface();
                if (!Identity.IsValid() || !Session.IsValid())
                {
                    UnavailableReason = TEXT("Steam identity or session interface is unavailable");
                }
                else if (Session->GetNumSessions() > 0)
                {
                    Identity.Reset();
                    Session.Reset();
                    UnavailableReason = TEXT("Another adapter owns an active Steam session");
                }
            }
            else
            {
                UnavailableReason = TEXT("Steam subsystem instance is unavailable");
            }
        }
        else
        {
            UnavailableReason = TEXT("OnlineSubsystem module is unavailable");
        }
        TSharedRef<ISessionProvider> Provider = MakeShared<FSteamSessionProvider>(
            MoveTemp(Lease), MoveTemp(Identity), MoveTemp(Session), MoveTemp(UnavailableReason));
        return TryCreateSessionTransport(InSpec, Provider);
    }
}
