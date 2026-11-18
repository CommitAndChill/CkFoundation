#include "CkOnlineLobby/CkOnlineLobby_Subsystem.h"

#include "CkCore/Validation/CkIsValid.h"

#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemModule.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#if CK_WITH_STEAM_LOBBY
#include "SteamSharedModule.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
    TWeakObjectPtr<UCk_OnlineLobby_Subsystem_UE> GSteamLobbyOwner;

    enum class EOnlineLobbyIdentity { Original, Unavailable, Lost };

    struct FOnlineLobbyCleanupPolicy
    {
        bool Invalidated = false;
        bool CleanupRevoked = false;

        auto Observe(EOnlineLobbyIdentity InIdentity, bool InInterfacesAvailable, bool InOverflowed) -> bool
        {
            if (InIdentity == EOnlineLobbyIdentity::Lost)
            { CleanupRevoked = true; }
            if (NOT Invalidated && (InIdentity != EOnlineLobbyIdentity::Original || NOT InInterfacesAvailable || InOverflowed))
            {
                Invalidated = true;
                return true;
            }
            return false;
        }

        auto CanCleanup(EOnlineLobbyIdentity InIdentity, bool InInterfacesAvailable) const -> bool
        {
            return NOT CleanupRevoked && InIdentity == EOnlineLobbyIdentity::Original && InInterfacesAvailable;
        }

        auto CanAdmit(EOnlineLobbyIdentity InIdentity, bool InInterfacesAvailable) const -> bool
        {
            return NOT Invalidated && CanCleanup(InIdentity, InInterfacesAvailable);
        }

        auto TakeOwnedLobby(EOnlineLobbyIdentity InIdentity, bool InInterfacesAvailable, uint64& InOwnedLobby) const -> uint64
        {
            if (NOT CanCleanup(InIdentity, InInterfacesAvailable))
            { return 0; }
            const auto Lobby = InOwnedLobby;
            InOwnedLobby = 0;
            return Lobby;
        }

        auto GetLateLobbyToLeave(EOnlineLobbyIdentity InIdentity, bool InInterfacesAvailable,
            bool InSuccess, uint64 InLobby, uint64 InProtectedLobby) const -> uint64
        {
            return CanCleanup(InIdentity, InInterfacesAvailable) && InSuccess && InLobby != InProtectedLobby ? InLobby : 0;
        }
    };

    auto ParseId(const FString& InValue) -> uint64
    {
        if (InValue.IsEmpty() || InValue.Len() > 20)
        { return 0; }
        uint64 Value = 0;
        for (const auto Character : InValue)
        {
            if (Character < TEXT('0') || Character > TEXT('9'))
            { return 0; }
            const auto Digit = static_cast<uint64>(Character - TEXT('0'));
            if (Value > (MAX_uint64 - Digit) / 10)
            { return 0; }
            Value = Value * 10 + Digit;
        }
        if (LexToString(Value) != InValue)
        { return 0; }
        return Value;
    }

    auto ParseLaunchLobbyId(const TCHAR* InCommandLine) -> FString
    {
        auto* Cursor = InCommandLine;
        FString Token;
        while (FParse::Token(Cursor, Token, false))
        {
            if (Token != TEXT("+connect_lobby"))
            { continue; }
            FString Id;
            if (FParse::Token(Cursor, Id, false) && ParseId(Id) != 0)
            { return Id; }
            return {};
        }
        return {};
    }
}

// Callback receiver and queue have process lifetime. Unregister is not an in-flight
// callback barrier. Ingress touches only copied values and process-lifetime data;
// live owners/orphan operations retain SDK handles, released at normal module shutdown.
#if CK_WITH_STEAM_LOBBY
struct FCk_OnlineLobby_CallbackReceiver
{
    struct FEvent
    {
        enum class EKind { Roster, Data, Invite, Avatar };
        EKind Kind;
        uint64 Lobby = 0;
        uint64 User = 0;
        uint64 ChangedBy = 0;
        int32 Value = 0;
        uint64 Generation = 0;
        uint64 Account = 0;
        uint32 App = 0;
    };

    TQueue<FEvent, EQueueMode::Mpsc> Events;
    FThreadSafeCounter PendingEvents;
    TAtomic<uint64> Generation{0};
    TAtomic<uint64> Account{0};
    TAtomic<uint32> App{0};
    TAtomic<uint64> OverflowGeneration{0};
    STEAM_CALLBACK_MANUAL(FCk_OnlineLobby_CallbackReceiver, OnRoster, LobbyChatUpdate_t, RosterCallback);
    STEAM_CALLBACK_MANUAL(FCk_OnlineLobby_CallbackReceiver, OnData, LobbyDataUpdate_t, DataCallback);
    STEAM_CALLBACK_MANUAL(FCk_OnlineLobby_CallbackReceiver, OnInvite, GameLobbyJoinRequested_t, InviteCallback);
    STEAM_CALLBACK_MANUAL(FCk_OnlineLobby_CallbackReceiver, OnAvatar, AvatarImageLoaded_t, AvatarCallback);

    FCk_OnlineLobby_CallbackReceiver()
    {
        RosterCallback.Register(this, &FCk_OnlineLobby_CallbackReceiver::OnRoster);
        DataCallback.Register(this, &FCk_OnlineLobby_CallbackReceiver::OnData);
        InviteCallback.Register(this, &FCk_OnlineLobby_CallbackReceiver::OnInvite);
        AvatarCallback.Register(this, &FCk_OnlineLobby_CallbackReceiver::OnAvatar);
    }

    auto StopCallbacks() -> void
    {
        Generation.Store(0);
        RosterCallback.Unregister();
        DataCallback.Unregister();
        InviteCallback.Unregister();
        AvatarCallback.Unregister();
    }

    auto Enqueue(FEvent InEvent, uint64 InGeneration) -> void
    {
        if (InGeneration == 0)
        { return; }
        if (PendingEvents.Increment() > 1024)
        {
            PendingEvents.Decrement();
            OverflowGeneration.Store(InGeneration);
            return;
        }
        InEvent.Generation = InGeneration;
        InEvent.Account = Account.Load();
        InEvent.App = App.Load();
        Events.Enqueue(InEvent);
    }
};

namespace
{
    // Intentionally retained until process exit. Shutdown does not free DLL code in
    // the pinned engine (ModuleManager.cpp, UnloadModule bIsShutdown branch).
    FCk_OnlineLobby_CallbackReceiver* GCallbackReceiver = nullptr;
    uint64 GNextLobbyGeneration = 1;
    FCk_OnlineLobby_Native* GActiveLobbyNative = nullptr;
}
#endif

struct FCk_OnlineLobby_Native
{
#if CK_WITH_STEAM_LOBBY
    struct FOperation
    {
        SteamAPICall_t Call = k_uAPICallInvalid;
        bool Create = false;
        bool Abandoned = false;
        uint64 Lobby = 0;
        double Deadline = 0;
    };

    TSharedPtr<FSteamClientInstanceHandler> ClientHandle;
    IOnlineSubsystem* Provider = nullptr;
    TArray<FOperation> Operations;
    uint64 CurrentLobby = 0;
    uint64 Generation = 0;
    uint64 Account = 0;
    uint32 App = 0;
    FOnlineLobbyCleanupPolicy Cleanup;

    FCk_OnlineLobby_Native(TSharedPtr<FSteamClientInstanceHandler> InHandle, IOnlineSubsystem* InProvider)
        : ClientHandle(MoveTemp(InHandle)), Provider(InProvider)
    {
        if (NOT GCallbackReceiver)
        { GCallbackReceiver = new FCk_OnlineLobby_CallbackReceiver(); }
        Generation = GNextLobbyGeneration++;
        Account = SteamUser()->GetSteamID().ConvertToUint64();
        App = SteamUtils()->GetAppID();
        GCallbackReceiver->Account.Store(Account);
        GCallbackReceiver->App.Store(App);
        GCallbackReceiver->OverflowGeneration.Store(0);
        GCallbackReceiver->Generation.Store(Generation);
        GActiveLobbyNative = this;
    }

    auto GetIdentity() const -> EOnlineLobbyIdentity
    {
        auto* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
        const FName SteamName(TEXT("Steam"));
        const auto SameProvider = Module && Module->DoesInstanceExist(SteamName) &&
            Module->GetOnlineSubsystem(SteamName) == Provider;
        if (NOT SameProvider)
        { return EOnlineLobbyIdentity::Lost; }
        if (NOT ClientHandle.IsValid() || NOT SteamUser() || NOT SteamUtils())
        { return EOnlineLobbyIdentity::Unavailable; }
        return SteamUser()->GetSteamID().ConvertToUint64() == Account && SteamUtils()->GetAppID() == App
            ? EOnlineLobbyIdentity::Original : EOnlineLobbyIdentity::Lost;
    }

    auto IsOriginalAccount() const -> bool
    {
        return Cleanup.CanCleanup(GetIdentity(), true);
    }

    auto LeaveOwnedLobby() -> void
    {
        const auto Identity = GetIdentity();
        const auto Available = Identity == EOnlineLobbyIdentity::Original && SteamMatchmaking();
        Cleanup.Observe(Identity, Available, false);
        const auto Lobby = Cleanup.TakeOwnedLobby(Identity, Available, CurrentLobby);
        if (Lobby != 0)
        { SteamMatchmaking()->LeaveLobby(CSteamID(Lobby)); }
    }

    ~FCk_OnlineLobby_Native()
    {
        if (GActiveLobbyNative == this)
        { GActiveLobbyNative = nullptr; }
        LeaveOwnedLobby();
    }
#endif
};

namespace
{
    TArray<TUniquePtr<FCk_OnlineLobby_Native>> GOrphans;
    FTSTicker::FDelegateHandle GOrphanTicker;

    auto TickOrphans(float InDeltaSeconds) -> bool
    {
#if CK_WITH_STEAM_LOBBY
        for (int32 OwnerIndex = GOrphans.Num() - 1; OwnerIndex >= 0; --OwnerIndex)
        {
            auto& Native = *GOrphans[OwnerIndex];
            const auto Identity = Native.GetIdentity();
            const auto Available = Identity == EOnlineLobbyIdentity::Original && SteamUtils() && SteamMatchmaking();
            Native.Cleanup.Observe(Identity, Available, false);
            if (Native.Cleanup.CleanupRevoked)
            {
                Native.CurrentLobby = 0;
                Native.Operations.Empty();
            }
            else
            {
                Native.LeaveOwnedLobby();
                if (NOT Native.Cleanup.CanCleanup(Identity, Available))
                { continue; }
            }
            for (int32 Index = Native.Operations.Num() - 1; Index >= 0; --Index)
            {
                const auto& Operation = Native.Operations[Index];
                bool Failed = false;
                if (NOT SteamUtils() || NOT SteamMatchmaking())
                { continue; }
                if (NOT SteamUtils()->IsAPICallCompleted(Operation.Call, &Failed))
                { continue; }
                uint64 Lobby = 0;
                bool Success = false;
                if (Operation.Create)
                {
                    LobbyCreated_t Result{};
                    const auto Received = SteamUtils()->GetAPICallResult(Operation.Call, &Result, sizeof(Result),
                        LobbyCreated_t::k_iCallback, &Failed);
                    Lobby = Result.m_ulSteamIDLobby;
                    Success = Received && NOT Failed && Result.m_eResult == k_EResultOK;
                }
                else
                {
                    LobbyEnter_t Result{};
                    const auto Received = SteamUtils()->GetAPICallResult(Operation.Call, &Result, sizeof(Result),
                        LobbyEnter_t::k_iCallback, &Failed);
                    Lobby = Result.m_ulSteamIDLobby;
                    Success = Received && NOT Failed && Result.m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess;
                }
                const auto* CurrentOwner = GSteamLobbyOwner.Get();
                const auto ProtectedLobby = CurrentOwner ? ParseId(CurrentOwner->Get_CurrentLobbyId()) : 0;
                const auto CleanupLobby = Native.Cleanup.GetLateLobbyToLeave(Native.GetIdentity(),
                    SteamMatchmaking() != nullptr, Success, Lobby, ProtectedLobby);
                if (CleanupLobby != 0)
                { SteamMatchmaking()->LeaveLobby(CSteamID(CleanupLobby)); }
                Native.Operations.RemoveAtSwap(Index);
            }
            if (Native.CurrentLobby == 0 && Native.Operations.IsEmpty())
            { GOrphans.RemoveAtSwap(OwnerIndex); }
        }
#endif
        if (GOrphans.IsEmpty())
        {
            GOrphanTicker.Reset();
            return false;
        }
        return true;
    }
}

auto CkOnlineLobby_ReleaseOrphans() -> void
{
    FTSTicker::GetCoreTicker().RemoveTicker(GOrphanTicker);
    GOrphanTicker.Reset();
#if CK_WITH_STEAM_LOBBY
    if (GCallbackReceiver)
    { GCallbackReceiver->StopCallbacks(); }
#endif
    GOrphans.Empty();
#if CK_WITH_STEAM_LOBBY
    if (GActiveLobbyNative)
    {
        GActiveLobbyNative->LeaveOwnedLobby();
        GActiveLobbyNative->Cleanup.Invalidated = true;
        GActiveLobbyNative->CurrentLobby = 0;
        GActiveLobbyNative->Operations.Empty();
        GActiveLobbyNative->ClientHandle.Reset();
    }
#endif
}

#if CK_WITH_STEAM_LOBBY
auto FCk_OnlineLobby_CallbackReceiver::OnRoster(LobbyChatUpdate_t* InEvent) -> void
{
    const auto Token = Generation.Load();
    Enqueue({FEvent::EKind::Roster, InEvent->m_ulSteamIDLobby, InEvent->m_ulSteamIDUserChanged,
        InEvent->m_ulSteamIDMakingChange, static_cast<int32>(InEvent->m_rgfChatMemberStateChange)}, Token);
}

auto FCk_OnlineLobby_CallbackReceiver::OnData(LobbyDataUpdate_t* InEvent) -> void
{
    const auto Token = Generation.Load();
    Enqueue({FEvent::EKind::Data, InEvent->m_ulSteamIDLobby, InEvent->m_ulSteamIDMember, 0,
        InEvent->m_bSuccess != 0 ? 1 : 0}, Token);
}

auto FCk_OnlineLobby_CallbackReceiver::OnInvite(GameLobbyJoinRequested_t* InEvent) -> void
{
    const auto Token = Generation.Load();
    Enqueue({FEvent::EKind::Invite, InEvent->m_steamIDLobby.ConvertToUint64(),
        InEvent->m_steamIDFriend.ConvertToUint64()}, Token);
}

auto FCk_OnlineLobby_CallbackReceiver::OnAvatar(AvatarImageLoaded_t* InEvent) -> void
{
    const auto Token = Generation.Load();
    Enqueue({FEvent::EKind::Avatar, 0, InEvent->m_steamID.ConvertToUint64()}, Token);
}
#endif

auto UCk_OnlineLobby_Subsystem_UE::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
    _Released = false;
    _Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::DoTick));
}

auto UCk_OnlineLobby_Subsystem_UE::Deinitialize() -> void
{
    DoRelease();
    Super::Deinitialize();
}

auto UCk_OnlineLobby_Subsystem_UE::BeginDestroy() -> void
{
    DoRelease();
    Super::BeginDestroy();
}

auto UCk_OnlineLobby_Subsystem_UE::DoRelease() -> void
{
    _Released = true;
    FTSTicker::GetCoreTicker().RemoveTicker(_Ticker);
    _Ticker.Reset();
    if (_Native)
    {
#if CK_WITH_STEAM_LOBBY
        if (GCallbackReceiver && GCallbackReceiver->Generation.Load() == _Native->Generation)
        { GCallbackReceiver->Generation.Store(0); }
        _Native->LeaveOwnedLobby();
        if (_Native->Cleanup.CleanupRevoked)
        {
            _Native->CurrentLobby = 0;
            _Native->Operations.Empty();
        }
        if (_Native->CurrentLobby != 0 || NOT _Native->Operations.IsEmpty())
        {
            GOrphans.Emplace(_Native);
            if (NOT GOrphanTicker.IsValid())
            { GOrphanTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickOrphans)); }
        }
        else
#endif
        { delete _Native; }
    }
    _Native = nullptr;
    _Avatars.Empty();
    if (GSteamLobbyOwner.Get() == this)
    { GSteamLobbyOwner.Reset(); }
}

auto UCk_OnlineLobby_Subsystem_UE::DoTick(float InDeltaSeconds) -> bool
{
    if (_Released)
    { return false; }
#if CK_WITH_STEAM_LOBBY
    const auto* World = GetWorld();
    if (FApp::IsUnattended() || NOT World || World->WorldType != EWorldType::Game)
    { return true; }
    DoTryInitializeNative();
    if (NOT _Native)
    { return true; }
    const auto Identity = _Native->GetIdentity();
    const auto InterfacesAvailable = Identity == EOnlineLobbyIdentity::Original && SteamUtils() && SteamMatchmaking();
    const auto Overflowed = GCallbackReceiver->OverflowGeneration.Load() == _Native->Generation;
    const auto NotifyInvalidated = _Native->Cleanup.Observe(Identity, InterfacesAvailable, Overflowed);
    if (_Native->Cleanup.Invalidated)
    {
        GCallbackReceiver->Generation.Store(0);
        _Native->LeaveOwnedLobby();
        TArray<FCk_OnlineLobby_Native::FOperation> NotifyPending;
        for (auto& Operation : _Native->Operations)
        {
            if (NOT Operation.Abandoned)
            { NotifyPending.Add(Operation); }
            Operation.Abandoned = true;
        }
        if (_Native->Cleanup.CleanupRevoked)
        {
            // There is no authority to act on a replacement provider/account, even if
            // an old numeric lobby ID or API handle happens to be valid there.
            _Native->Operations.Empty();
            _Native->CurrentLobby = 0;
        }
        FCk_OnlineLobby_CallbackReceiver::FEvent Discarded;
        while (GCallbackReceiver->Events.Dequeue(Discarded))
        { GCallbackReceiver->PendingEvents.Decrement(); }
        if (NotifyInvalidated)
        {
            if (Overflowed)
            { UE_LOG(LogTemp, Error, TEXT("CK online lobby callback queue exceeded 1024 events; service invalidated, original-account native cleanup retained.")); }
            for (const auto& Operation : NotifyPending)
            {
                if (Operation.Create)
                { OnLobbyCreated.Broadcast(FString{}, false); }
                else
                { OnLobbyEnter.Broadcast(LexToString(Operation.Lobby), false); }
                if (_Released)
                { return false; }
            }
            OnLobbyInvalidated.Broadcast();
            if (_Released)
            { return false; }
        }
    }
    if (NOT _Native->Cleanup.CanCleanup(_Native->GetIdentity(), InterfacesAvailable))
    { return true; }

    // Polling native calls avoids per-call callbacks holding a game-instance UObject.
    // A timed-out/canceled handle remains tracked until its eventual result can be cleaned up.
    struct FCompletion { bool Create; uint64 Lobby; bool Locked; int32 Result; bool Abandoned; };
    TArray<FCompletion> Completions;
    for (int32 Index = _Native->Operations.Num() - 1; Index >= 0; --Index)
    {
        auto& Operation = _Native->Operations[Index];
        bool Failed = false;
        if (SteamUtils()->IsAPICallCompleted(Operation.Call, &Failed))
        {
            FCompletion Completion{Operation.Create, Operation.Lobby, false, 0, Operation.Abandoned};
            if (Operation.Create)
            {
                LobbyCreated_t Result{};
                const auto Received = SteamUtils()->GetAPICallResult(Operation.Call, &Result, sizeof(Result),
                    LobbyCreated_t::k_iCallback, &Failed);
                Completion.Lobby = Result.m_ulSteamIDLobby;
                Completion.Result = Received && NOT Failed ? static_cast<int32>(Result.m_eResult) : 2;
            }
            else
            {
                LobbyEnter_t Result{};
                const auto Received = SteamUtils()->GetAPICallResult(Operation.Call, &Result, sizeof(Result),
                    LobbyEnter_t::k_iCallback, &Failed);
                Completion.Lobby = Result.m_ulSteamIDLobby;
                Completion.Locked = Result.m_bLocked;
                Completion.Result = Received && NOT Failed ? static_cast<int32>(Result.m_EChatRoomEnterResponse) : 2;
            }
            Completions.Add(Completion);
            _Native->Operations.RemoveAtSwap(Index);
        }
        else if (NOT Operation.Abandoned && FPlatformTime::Seconds() >= Operation.Deadline)
        {
            Operation.Abandoned = true;
            Completions.Add({Operation.Create, Operation.Lobby, false, 2, false});
        }
    }

    for (const auto& Completion : Completions)
    {
        if (Completion.Abandoned)
        {
            const auto CleanupLobby = _Native->Cleanup.GetLateLobbyToLeave(_Native->GetIdentity(),
                SteamMatchmaking() != nullptr, Completion.Result == 1, Completion.Lobby, _Native->CurrentLobby);
            if (CleanupLobby != 0)
            { SteamMatchmaking()->LeaveLobby(CSteamID(CleanupLobby)); }
            continue;
        }
        if (Completion.Result == 1)
        { _Native->CurrentLobby = Completion.Lobby; }
        if (Completion.Create)
        { OnLobbyCreated.Broadcast(LexToString(Completion.Lobby), Completion.Result == 1); }
        if (_Released)
        { return false; }
        // Steam auto-enters successfully created lobbies. Deliver the same entry contract as joins.
        if (NOT Completion.Create || (Completion.Result == 1 && _Native->CurrentLobby == Completion.Lobby))
        { OnLobbyEnter.Broadcast(LexToString(Completion.Lobby), Completion.Result == 1); }
        if (_Released)
        { return false; }
    }

    if (_Native->Cleanup.Invalidated)
    { return true; }
    FCk_OnlineLobby_CallbackReceiver::FEvent Event;
    while (GCallbackReceiver->Events.Dequeue(Event))
    {
        GCallbackReceiver->PendingEvents.Decrement();
        if (Event.Generation != _Native->Generation || Event.Account != _Native->Account || Event.App != _Native->App)
        { continue; }
        using EKind = FCk_OnlineLobby_CallbackReceiver::FEvent::EKind;
        if (Event.Kind == EKind::Invite)
        { OnGameLobbyJoinRequested.Broadcast(LexToString(Event.Lobby)); }
        else if (Event.Kind == EKind::Avatar)
        { _Avatars.Remove(LexToString(Event.User)); }
        else if (Event.Lobby == _Native->CurrentLobby)
        {
            if (Event.Kind == EKind::Roster)
            { OnLobbyRosterChanged.Broadcast(LexToString(Event.Lobby)); }
            else
            { OnLobbyDataUpdate.Broadcast(LexToString(Event.Lobby), Event.Value != 0); }
        }
        if (_Released)
        { return false; }
    }
#endif
    return true;
}

auto UCk_OnlineLobby_Subsystem_UE::DoTryInitializeNative() -> void
{
#if CK_WITH_STEAM_LOBBY
    if (_Released || NOT IsInGameThread() || FApp::IsUnattended())
    { return; }
    const auto* World = GetWorld();
    if (NOT World || World->WorldType != EWorldType::Game || IsRunningDedicatedServer())
    { return; }
    if (NOT _Native)
    {
        if (GSteamLobbyOwner.IsValid() && GSteamLobbyOwner.Get() != this)
        { return; }
        auto* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
        const FName SteamName(TEXT("Steam"));
        if (NOT Module || NOT Module->DoesInstanceExist(SteamName))
        { return; }
        auto* Provider = Module->GetOnlineSubsystem(SteamName);
        if (NOT Provider || NOT SteamUser() ||
            NOT SteamMatchmaking() || NOT SteamFriends() || NOT SteamUtils() ||
            NOT SteamUser()->GetSteamID().IsValid())
        { return; }
        if (NOT FSteamSharedModule::IsAvailable())
        { return; }
        auto ClientHandle = FSteamSharedModule::Get().ObtainSteamClientInstanceHandle();
        if (NOT ClientHandle.IsValid() || NOT ClientHandle->IsInitialized())
        { return; }
        _Native = new FCk_OnlineLobby_Native(MoveTemp(ClientHandle), Provider);
        GSteamLobbyOwner = this;
    }

#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_IsAvailable() const -> bool
{
    const auto* World = GetWorld();
    if (NOT IsInGameThread() || FApp::IsUnattended() || NOT World || World->WorldType != EWorldType::Game)
    { return false; }
    const_cast<UCk_OnlineLobby_Subsystem_UE*>(this)->DoTryInitializeNative();
#if CK_WITH_STEAM_LOBBY
    if (_Released || NOT _Native) { return false; }
    const auto Identity = _Native->GetIdentity();
    auto* User = Identity == EOnlineLobbyIdentity::Original ? SteamUser() : nullptr;
    const auto InterfacesAvailable = User && User->BLoggedOn() && SteamMatchmaking() && SteamFriends() && SteamUtils();
    return _Native->Cleanup.CanAdmit(Identity, InterfacesAvailable);
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_IsAdmissionPending() const -> bool
{
#if CK_WITH_STEAM_LOBBY
    return NOT GOrphans.IsEmpty() || (_Native && NOT _Native->Operations.IsEmpty());
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_IsOverlayEnabled() const -> bool
{
#if CK_WITH_STEAM_LOBBY
    return Get_IsAvailable() && SteamUtils()->IsOverlayEnabled();
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_IsSteamRunningOnSteamDeck() const -> bool
{
#if CK_WITH_STEAM_LOBBY
    if (_Released || NOT IsInGameThread() || NOT FSteamSharedModule::IsAvailable() ||
        NOT FSteamSharedModule::Get().AreSteamDllsLoaded())
    { return false; }
    auto* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
    const FName SteamName(TEXT("Steam"));
    if (NOT Module || NOT Module->DoesInstanceExist(SteamName))
    { return false; }
    return SteamUtils() && SteamUtils()->IsSteamRunningOnSteamDeck();
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_LocalUserId() const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (Get_IsAvailable())
    { return LexToString(SteamUser()->GetSteamID().ConvertToUint64()); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_CurrentLobbyId() const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (_Native && NOT _Native->Cleanup.Invalidated && _Native->CurrentLobby != 0)
    { return LexToString(_Native->CurrentLobby); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_LocalPersonaName() const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (Get_IsAvailable())
    { return UTF8_TO_TCHAR(SteamFriends()->GetPersonaName()); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_FriendPersonaName(const FString& InUserId) const -> FString
{
#if CK_WITH_STEAM_LOBBY
    const auto Id = CSteamID(ParseId(InUserId));
    if (Get_IsAvailable() && Id.IsValid() && Id.BIndividualAccount())
    { return UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(Id)); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_MediumAvatar(const FString& InUserId) -> UTexture2D*
{
#if CK_WITH_STEAM_LOBBY
    const auto Id = CSteamID(ParseId(InUserId));
    if (NOT Get_IsAvailable() || NOT Id.IsValid() || NOT Id.BIndividualAccount())
    { return nullptr; }
    if (const auto* Existing = _Avatars.Find(InUserId))
    { return Existing->Get(); }
    const auto Image = SteamFriends()->GetMediumFriendAvatar(Id);
    uint32 Width = 0;
    uint32 Height = 0;
    if (Image <= 0 || NOT SteamUtils()->GetImageSize(Image, &Width, &Height) ||
        Width == 0 || Height == 0 || Width > 512 || Height > 512)
    { return nullptr; }
    TArray<uint8> Pixels;
    Pixels.SetNumUninitialized(Width * Height * 4);
    if (NOT SteamUtils()->GetImageRGBA(Image, Pixels.GetData(), Pixels.Num()))
    { return nullptr; }
    auto* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
    if (ck::Is_NOT_Valid(Texture))
    { return nullptr; }
    auto& Mip = Texture->GetPlatformData()->Mips[0];
    auto* Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Destination, Pixels.GetData(), Pixels.Num());
    Mip.BulkData.Unlock();
    Texture->UpdateResource();
    _Avatars.Add(InUserId, Texture);
    return Texture;
#else
    return nullptr;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Request_CreateFriendsOnlyLobby(int32 InMaxMembers) -> bool
{
#if CK_WITH_STEAM_LOBBY
    if (NOT Get_IsAvailable() || InMaxMembers < 1 || InMaxMembers > 250 || NOT _Native->Operations.IsEmpty() ||
        _Native->CurrentLobby != 0 || NOT GOrphans.IsEmpty())
    { return false; }
    const auto Call = SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, InMaxMembers);
    if (Call == k_uAPICallInvalid)
    { return false; }
    _Native->Operations.Add({Call, true, false, 0, FPlatformTime::Seconds() + 30.0});
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Request_JoinLobby(const FString& InLobbyId) -> bool
{
#if CK_WITH_STEAM_LOBBY
    if (NOT Get_IsAvailable() || NOT Get_IsValidLobbyId(InLobbyId) || NOT _Native->Operations.IsEmpty() ||
        _Native->CurrentLobby != 0 || NOT GOrphans.IsEmpty())
    { return false; }
    const auto Lobby = ParseId(InLobbyId);
    const auto Call = SteamMatchmaking()->JoinLobby(CSteamID(Lobby));
    if (Call == k_uAPICallInvalid)
    { return false; }
    _Native->Operations.Add({Call, false, false, Lobby, FPlatformTime::Seconds() + 30.0});
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Request_LeaveLobby(const FString& InLobbyId) -> void
{
#if CK_WITH_STEAM_LOBBY
    if (NOT _Native || NOT _Native->IsOriginalAccount() ||
        (NOT InLobbyId.IsEmpty() && NOT Get_IsValidLobbyId(InLobbyId)))
    { return; }
    const auto Lobby = InLobbyId.IsEmpty() ? _Native->CurrentLobby : ParseId(InLobbyId);
    if (NOT InLobbyId.IsEmpty() && Lobby != _Native->CurrentLobby &&
        NOT _Native->Operations.ContainsByPredicate([Lobby](const FCk_OnlineLobby_Native::FOperation& InOperation)
        { return InOperation.Lobby == Lobby; }))
    { return; }
    for (auto& Operation : _Native->Operations)
    { Operation.Abandoned = true; }
    if (SteamMatchmaking() && Lobby != 0)
    { SteamMatchmaking()->LeaveLobby(CSteamID(Lobby)); }
    if (Lobby == _Native->CurrentLobby)
    { _Native->CurrentLobby = 0; }
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Request_OpenInviteDialog(const FString& InLobbyId) -> bool
{
#if CK_WITH_STEAM_LOBBY
    if (NOT Get_IsOverlayEnabled() || NOT Get_IsValidLobbyId(InLobbyId) || ParseId(InLobbyId) != _Native->CurrentLobby)
    { return false; }
    SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(ParseId(InLobbyId)));
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_LobbyOwner(const FString& InLobbyId) const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (Get_IsAvailable() && Get_IsValidLobbyId(InLobbyId))
    { return LexToString(SteamMatchmaking()->GetLobbyOwner(CSteamID(ParseId(InLobbyId))).ConvertToUint64()); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_NumLobbyMembers(const FString& InLobbyId) const -> int32
{
#if CK_WITH_STEAM_LOBBY
    if (Get_IsAvailable() && Get_IsValidLobbyId(InLobbyId))
    { return SteamMatchmaking()->GetNumLobbyMembers(CSteamID(ParseId(InLobbyId))); }
#endif
    return 0;
}

auto UCk_OnlineLobby_Subsystem_UE::Get_LobbyMemberByIndex(const FString& InLobbyId, int32 InIndex) const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (InIndex >= 0 && InIndex < Get_NumLobbyMembers(InLobbyId))
    { return LexToString(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(ParseId(InLobbyId)), InIndex).ConvertToUint64()); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Get_LobbyData(const FString& InLobbyId, const FString& InKey) const -> FString
{
#if CK_WITH_STEAM_LOBBY
    if (Get_IsAvailable() && Get_IsValidLobbyId(InLobbyId) && NOT InKey.IsEmpty())
    { return UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyData(CSteamID(ParseId(InLobbyId)), TCHAR_TO_UTF8(*InKey))); }
#endif
    return {};
}

auto UCk_OnlineLobby_Subsystem_UE::Request_SetLobbyData(const FString& InLobbyId, const FString& InKey, const FString& InValue) -> bool
{
#if CK_WITH_STEAM_LOBBY
    if (NOT Get_IsAvailable() || NOT Get_IsValidLobbyId(InLobbyId) || InKey.IsEmpty() ||
        ParseId(InLobbyId) != _Native->CurrentLobby || Get_LobbyOwner(InLobbyId) != Get_LocalUserId())
    { return false; }
    return SteamMatchmaking()->SetLobbyData(CSteamID(ParseId(InLobbyId)), TCHAR_TO_UTF8(*InKey), TCHAR_TO_UTF8(*InValue));
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_IsValidLobbyId(const FString& InLobbyId) const -> bool
{
#if CK_WITH_STEAM_LOBBY
    const auto Id = CSteamID(ParseId(InLobbyId));
    return Id.IsValid() && Id.IsLobby();
#else
    return false;
#endif
}

auto UCk_OnlineLobby_Subsystem_UE::Get_CommandLineLobbyId() const -> FString
{
    const auto Id = ParseLaunchLobbyId(FCommandLine::Get());
    return Get_IsValidLobbyId(Id) ? Id : FString{};
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCk_OnlineLobby_InvalidInput,
    "Ck.Online.Lobby.InvalidInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCk_OnlineLobby_InvalidInput::RunTest(const FString& InParameters) -> bool
{
    for (const auto* Invalid : {TEXT(""), TEXT("-1"), TEXT("+1"), TEXT(" 1"), TEXT("01"),
        TEXT("1x"), TEXT("18446744073709551616"), TEXT("99999999999999999999999")})
    { TestEqual(FString::Printf(TEXT("Reject malformed ID '%s'"), Invalid), ParseId(Invalid), uint64{0}); }
    TestEqual(TEXT("Preserve full unsigned ID precision"), ParseId(TEXT("18446744073709551615")), MAX_uint64);
    TestEqual(TEXT("Read cold launch invite"), ParseLaunchLobbyId(TEXT("-windowed +connect_lobby 109775241012345678")),
        FString(TEXT("109775241012345678")));
    TestEqual(TEXT("Read quoted launch invite"), ParseLaunchLobbyId(TEXT("+connect_lobby \"109775241012345678\"")),
        FString(TEXT("109775241012345678")));
    TestTrue(TEXT("Reject missing launch target"), ParseLaunchLobbyId(TEXT("+connect_lobby")).IsEmpty());
    TestTrue(TEXT("Reject malformed launch target"), ParseLaunchLobbyId(TEXT("+connect_lobby 1x")).IsEmpty());

    auto* Instance = NewObject<UGameInstance>();
    auto* Lobby = NewObject<UCk_OnlineLobby_Subsystem_UE>(Instance);
    TestFalse(TEXT("Worldless instance has no provider"), Lobby->Get_IsAvailable());
    TestFalse(TEXT("Malformed join rejected"), Lobby->Request_JoinLobby(TEXT("not-a-lobby")));
    TestFalse(TEXT("Malformed metadata rejected"), Lobby->Request_SetLobbyData(TEXT("not-a-lobby"), TEXT("key"), TEXT("value")));
    TestFalse(TEXT("Malformed invite rejected"), Lobby->Request_OpenInviteDialog(TEXT("not-a-lobby")));
    TestFalse(TEXT("Invalid capacity rejected"), Lobby->Request_CreateFriendsOnlyLobby(0));
    Lobby->Request_LeaveLobby(TEXT("not-a-lobby"));
    TestTrue(TEXT("Rejected operations publish no membership"), Lobby->Get_CurrentLobbyId().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCk_OnlineLobby_InvalidationCleanup,
    "Ck.Online.Lobby.InvalidationCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCk_OnlineLobby_InvalidationCleanup::RunTest(const FString& InParameters) -> bool
{
    FOnlineLobbyCleanupPolicy Overflow;
    uint64 Owned = 42;
    TestTrue(TEXT("Overflow delivers one terminal invalidation"), Overflow.Observe(EOnlineLobbyIdentity::Original, true, true));
    TestFalse(TEXT("Invalidated service refuses new admission"), Overflow.CanAdmit(EOnlineLobbyIdentity::Original, true));
    TestTrue(TEXT("Overflow retains original native cleanup authority"), Overflow.CanCleanup(EOnlineLobbyIdentity::Original, true));
    TestEqual(TEXT("Owned lobby is released before notifying reentrant consumers"),
        Overflow.TakeOwnedLobby(EOnlineLobbyIdentity::Original, true, Owned), uint64{42});
    TestEqual(TEXT("Reentrant release or later destructor cannot leave it twice"),
        Overflow.TakeOwnedLobby(EOnlineLobbyIdentity::Original, true, Owned), uint64{0});
    TestEqual(TEXT("Abandoned admission late success is still cleaned"),
        Overflow.GetLateLobbyToLeave(EOnlineLobbyIdentity::Original, true, true, 84, 0), uint64{84});
    TestEqual(TEXT("Late cleanup never leaves a protected current membership"),
        Overflow.GetLateLobbyToLeave(EOnlineLobbyIdentity::Original, true, true, 84, 84), uint64{0});
    TestFalse(TEXT("Repeated overflow cannot notify twice"), Overflow.Observe(EOnlineLobbyIdentity::Original, true, true));

    FOnlineLobbyCleanupPolicy MissingInterface;
    Owned = 42;
    TestTrue(TEXT("Unavailable native interfaces still invalidate immediately"),
        MissingInterface.Observe(EOnlineLobbyIdentity::Unavailable, false, false));
    TestEqual(TEXT("Missing interface retains owned membership for later cleanup"),
        MissingInterface.TakeOwnedLobby(EOnlineLobbyIdentity::Unavailable, false, Owned), uint64{0});
    TestEqual(TEXT("Owned cleanup ID survives temporary loss"), Owned, uint64{42});
    TestFalse(TEXT("Interface recovery does not emit a second terminal event"),
        MissingInterface.Observe(EOnlineLobbyIdentity::Original, true, false));
    TestEqual(TEXT("Recovered original interface completes owned cleanup"),
        MissingInterface.TakeOwnedLobby(EOnlineLobbyIdentity::Original, true, Owned), uint64{42});
    TestFalse(TEXT("Interface recovery cannot silently reopen admission"),
        MissingInterface.CanAdmit(EOnlineLobbyIdentity::Original, true));

    FOnlineLobbyCleanupPolicy Replacement;
    Owned = 99;
    TestTrue(TEXT("Provider/account replacement notifies once"), Replacement.Observe(EOnlineLobbyIdentity::Lost, false, false));
    TestFalse(TEXT("Replacement revokes native cleanup even if numeric identity later matches"),
        Replacement.CanCleanup(EOnlineLobbyIdentity::Original, true));
    TestEqual(TEXT("No owned leave is sent to a replacement account"),
        Replacement.TakeOwnedLobby(EOnlineLobbyIdentity::Original, true, Owned), uint64{0});
    TestEqual(TEXT("No late admission cleanup is sent to a replacement account"),
        Replacement.GetLateLobbyToLeave(EOnlineLobbyIdentity::Original, true, true, 99, 0), uint64{0});
    TestFalse(TEXT("Repeated identity loss does not notify twice"), Replacement.Observe(EOnlineLobbyIdentity::Lost, false, false));
    return true;
}

#endif
