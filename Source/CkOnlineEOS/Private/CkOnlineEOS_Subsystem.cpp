#include "CkOnlineEOS/CkOnlineEOS_Subsystem.h"

#include "CkCore/Macros/CkMacros.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServicesEngineUtils.h"
#include "Online/Sessions.h"
#include "IEOSSDKManager.h"
#include "eos_connect.h"
#include "eos_sdk.h"

using namespace UE::Online;

namespace
{
    struct FAuthOwnershipRecord
    {
        TWeakPtr<IOnlineServices> Services;
        TWeakPtr<FCk_OnlineEOS_State> Owner;
        uint64 Generation = 0;
        FAccountId NativeAccountId;
        TMap<FName, FAccountId> CleanupSessions;
        bool NativeOwnershipUnknown = false;
    };

    TMap<IOnlineServices*, FAuthOwnershipRecord> AuthOwners;
    uint64 NextAuthGeneration = 0;
    enum class ESessionPhase : uint8 { Creating, Ready, Destroying, CleanupFailed };

    struct FSessionOperation
    {
        ESessionPhase Phase = ESessionPhase::Creating;
        bool Cancelled = false;
        FAccountId AccountId;
        FCk_Delegate_OnlineEOS_SessionComplete CreateCallback;
        TArray<FCk_Delegate_OnlineEOS_SessionComplete> DestroyCallbacks;
    };
}

struct FCk_OnlineEOS_State : TSharedFromThis<FCk_OnlineEOS_State>
{
    TWeakObjectPtr<UCk_OnlineEOS_Subsystem_UE> Owner;
    IOnlineServicesPtr Services;
    FAccountId AccountId;
    FAccountId NativeAccountId;
    TMap<FName, FSessionOperation> Sessions;
    FCk_Delegate_OnlineEOS_LoginComplete LoginCallback;
    FOnlineEventDelegateHandle ExpirationHandle;
    FOnlineEventDelegateHandle StatusHandle;
    FString DisplayName;
    bool Active = true;
    bool LoggingIn = false;
    bool UseDeviceId = false;
    bool LoggingOut = false;
    bool ReconcileRequired = false;
    bool NativeOwnershipUnknown = false;
    uint64 AuthGeneration = 0;

    ~FCk_OnlineEOS_State()
    {
        if (NOT OwnsAuthLease()) { return; }
        auto& Record = AuthOwners.FindChecked(Services.Get());
        Record.NativeAccountId = NativeAccountId;
        Record.NativeOwnershipUnknown = NativeOwnershipUnknown;
        Record.CleanupSessions.Reset();
        for (const auto& Pair : Sessions) { Record.CleanupSessions.Add(Pair.Key, Pair.Value.AccountId); }
    }

    auto OwnsAuthLease() const -> bool
    {
        const auto* Lease = AuthOwners.Find(Services.Get());
        return Lease != nullptr && AuthGeneration != 0 && Lease->Generation == AuthGeneration &&
            Lease->Services.Pin().Get() == Services.Get();
    }

    auto ClaimAuthLease() -> bool
    {
        for (auto It = AuthOwners.CreateIterator(); It; ++It)
        {
            if (It.Key() != nullptr && NOT It.Value().Services.IsValid()) { It.RemoveCurrent(); }
        }
        if (auto* Lease = AuthOwners.Find(Services.Get()))
        {
            if (Lease->Services.Pin().Get() != Services.Get())
            { AuthOwners.Remove(Services.Get()); }
            else
            {
                auto Existing = Lease->Owner.Pin();
                if (Existing.IsValid() && Existing.Get() != this)
                {
                    if (NOT Existing->Active) { Existing->RetryCleanup(); }
                    return false;
                }
                if (Existing.Get() == this) { return true; }
                NativeAccountId = Lease->NativeAccountId;
                NativeOwnershipUnknown = Lease->NativeOwnershipUnknown;
                for (const auto& Pair : Lease->CleanupSessions)
                {
                    auto& Session = Sessions.Add(Pair.Key);
                    Session.AccountId = Pair.Value;
                    Session.Cancelled = true;
                    Session.Phase = ESessionPhase::CleanupFailed;
                }
                ReconcileRequired = NativeOwnershipUnknown || NativeAccountId.IsValid() || NOT Sessions.IsEmpty();
            }
        }
        AuthGeneration = ++NextAuthGeneration;
        auto& Record = AuthOwners.FindOrAdd(Services.Get());
        Record.Services = Services;
        Record.Owner = AsShared();
        Record.Generation = AuthGeneration;
        Record.NativeAccountId = NativeAccountId;
        Record.NativeOwnershipUnknown = NativeOwnershipUnknown;
        Record.CleanupSessions.Reset();
        return true;
    }

    auto ReleaseAuthLease() -> void
    {
        if (OwnsAuthLease()) { AuthOwners.Remove(Services.Get()); }
    }

    auto CompleteLogout(bool InSuccess) -> void
    {
        LoggingOut = false;
        if (NOT InSuccess) { ReconcileRequired = true; return; }
        NativeAccountId = {};
        ReconcileRequired = false;
        ReleaseAuthLease();
    }

    auto RetryCleanup() -> void
    {
        if (LoggingIn || LoggingOut) { return; }
        auto Names = TArray<FName>{};
        Sessions.GetKeys(Names);
        for (auto Name : Names)
        {
            if (Sessions.FindChecked(Name).Phase == ESessionPhase::CleanupFailed) { DestroySession(Name); }
        }
        FinishShutdown();
    }

    auto FinishShutdown() -> void
    {
        if ((Active && NOT ReconcileRequired) || NOT Sessions.IsEmpty() || LoggingIn || LoggingOut) { return; }
        if (NOT OwnsAuthLease()) { return; }
        if (NativeOwnershipUnknown)
        {
            const auto NativeUser = Services->GetAuthInterface()->GetLocalOnlineUserByPlatformUserId({FPlatformMisc::GetPlatformUserForUserIndex(0)});
            if (NativeUser.IsOk()) { NativeAccountId = NativeUser.GetOkValue().AccountInfo->AccountId; }
            else if (NativeUser.GetErrorValue() != Errors::InvalidUser()) { return; }
            NativeOwnershipUnknown = false;
        }
        if (NOT NativeAccountId.IsValid())
        {
            ReconcileRequired = false;
            ReleaseAuthLease();
            return;
        }
        auto Auth = Services->GetAuthInterface();
        auto Params = FAuthLogout::Params{};
        Params.LocalAccountId = NativeAccountId;
        AccountId = {};
        LoggingOut = true;
        Auth->Logout(MoveTemp(Params)).OnComplete([Self = AsShared()](const TOnlineResult<FAuthLogout>& Result)
        {
            const auto NativeAccountAbsent = Result.IsError() && Result.GetErrorValue() == Errors::InvalidUser();
            Self->CompleteLogout(Result.IsOk() || NativeAccountAbsent);
        });
    }

    auto InvalidateAccount() -> void
    {
        AccountId = {};
        auto Names = TArray<FName>{};
        Sessions.GetKeys(Names);
        for (auto Name : Names)
        {
            auto& Operation = Sessions.FindChecked(Name);
            Operation.Cancelled = true;
            if (Operation.Phase != ESessionPhase::Creating) { DestroySession(Name); }
        }
    }

    auto CompleteLogin(bool InSuccess, const FString& InError) -> void
    {
        LoggingIn = false;
        if (InSuccess) { ReconcileRequired = false; }
        if (NOT InSuccess)
        {
            ReconcileRequired = true;
            InvalidateAccount();
        }
        auto Callback = LoginCallback;
        LoginCallback.Unbind();
        FinishShutdown();
        if (NOT Active) { return; }
        Callback.ExecuteIfBound(InSuccess, InError);
        if (auto* Subsystem = Owner.Get())
        { Subsystem->OnLoginStatusChanged.Broadcast(InSuccess, InError); }
    }

    auto DestroySession(FName InName) -> void
    {
        auto* Operation = Sessions.Find(InName);
        if (Operation == nullptr || Operation->Phase == ESessionPhase::Destroying) { return; }
        Operation->Phase = ESessionPhase::Destroying;
        auto Params = FLeaveSession::Params{};
        Params.LocalAccountId = Operation->AccountId;
        Params.SessionName = InName;
        Params.bDestroySession = true;
        Services->GetSessionsInterface()->LeaveSession(MoveTemp(Params)).OnComplete(
            [Self = AsShared(), InName](const TOnlineResult<FLeaveSession>& Result)
            {
                auto* Pending = Self->Sessions.Find(InName);
                if (Pending == nullptr) { return; }
                auto Callbacks = MoveTemp(Pending->DestroyCallbacks);
                if (Result.IsOk()) { Self->Sessions.Remove(InName); }
                else { Pending->Phase = ESessionPhase::CleanupFailed; }
                Self->FinishShutdown();
                if (NOT Self->Active) { return; }
                for (auto& Callback : Callbacks)
                { if (Self->Active) { Callback.ExecuteIfBound(Result.IsOk(), InName); } }
            });
    }

    auto LoginWithToken(FName InType, FString InToken) -> void
    {
        if (NOT Active) { CompleteLogin(false, FString{}); return; }
        auto Auth = Services->GetAuthInterface();
        auto Params = FAuthLogin::Params{};
        Params.PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
        Params.CredentialsType = LoginCredentialsType::ExternalAuth;
        Params.CredentialsToken = FCredentialsToken(TInPlaceType<UE::Online::FExternalAuthToken>(), UE::Online::FExternalAuthToken{InType, MoveTemp(InToken)});
        Auth->Login(MoveTemp(Params)).OnComplete([Self = AsShared()](const TOnlineResult<FAuthLogin>& Result)
        {
            if (NOT Result.IsOk())
            {
                Self->CompleteLogin(false, TEXT("EOS authentication failed"));
                return;
            }
            Self->AccountId = Result.GetOkValue().AccountInfo->AccountId;
            Self->NativeAccountId = Self->AccountId;
            Self->CompleteLogin(true, FString{});
        });
    }

    auto BeginSteamLogin() -> void
    {
        auto* Steam = IOnlineSubsystem::Get(TEXT("STEAM"));
        auto Identity = Steam != nullptr ? Steam->GetIdentityInterface() : IOnlineIdentityPtr{};
        if (NOT Identity.IsValid() || Identity->GetLoginStatus(0) != ::ELoginStatus::LoggedIn)
        {
            CompleteLogin(false, TEXT("Steam authentication unavailable"));
            return;
        }
        DisplayName = Identity->GetPlayerNickname(0);
        Identity->GetLinkedAccountAuthToken(0, TEXT("WebAPI:epiconlineservices"),
            IOnlineIdentity::FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda(
                [Self = AsShared()](int32, bool InSuccess, const ::FExternalAuthToken& InToken)
                {
                    if (NOT Self->Active) { Self->CompleteLogin(false, FString{}); return; }
                    if (NOT InSuccess || NOT InToken.IsValid())
                    {
                        Self->CompleteLogin(false, TEXT("Could not obtain Steam authentication ticket"));
                        return;
                    }
                    auto Token = InToken.TokenString;
                    if (InToken.HasTokenData())
                    { Token = BytesToHex(InToken.TokenData.GetData(), InToken.TokenData.Num()); }
                    Self->LoginWithToken(ExternalLoginType::SteamSessionTicket, MoveTemp(Token));
                }));
    }
};

auto
    UCk_OnlineEOS_Subsystem_UE::
    Initialize(FSubsystemCollectionBase& Collection)
    -> void
{
    Super::Initialize(Collection);
    _State = MakeShared<FCk_OnlineEOS_State>();
    _State->Owner = this;
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Deinitialize()
    -> void
{
    if (_State.IsValid())
    {
        auto State = _State;
        State->Active = false;
        State->Owner.Reset();
        State->LoginCallback.Unbind();
        State->ExpirationHandle = {};
        State->StatusHandle = {};
        auto Names = TArray<FName>{};
        State->Sessions.GetKeys(Names);
        for (auto Name : Names)
        {
            auto& Operation = State->Sessions.FindChecked(Name);
            Operation.Cancelled = true;
            Operation.CreateCallback.Unbind();
            Operation.DestroyCallbacks.Reset();
            if (Operation.Phase != ESessionPhase::Creating) { State->DestroySession(Name); }
        }
        State->FinishShutdown();
        _State.Reset();
    }
    Super::Deinitialize();
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Get_IsSteamAvailable() const
    -> bool
{
    auto* Steam = IOnlineSubsystem::Get(TEXT("STEAM"));
    auto Identity = Steam != nullptr ? Steam->GetIdentityInterface() : IOnlineIdentityPtr{};
    return Identity.IsValid() && Identity->GetLoginStatus(0) == ::ELoginStatus::LoggedIn;
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Get_IsLoggedIn() const
    -> bool
{
    return _State.IsValid() && _State->Active && _State->AccountId.IsValid() &&
        _State->Services.IsValid() && _State->Services->GetAuthInterface()->IsLoggedIn(_State->AccountId);
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Get_DisplayName() const
    -> FString
{
    return _State.IsValid() ? _State->DisplayName : FString{};
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Request_Login(bool InUseDeviceId, FCk_Delegate_OnlineEOS_LoginComplete InCallback)
    -> void
{
    if (NOT _State.IsValid() || NOT _State->Active || _State->LoggingIn || _State->LoggingOut)
    { InCallback.ExecuteIfBound(false, TEXT("Authentication request unavailable")); return; }
    if (Get_IsLoggedIn()) { InCallback.ExecuteIfBound(true, FString{}); return; }
    auto State = _State;
    if (NOT State->Services.IsValid()) { State->Services = GetServices(GetWorld(), EOnlineServices::Epic); }
    if (NOT State->Services.IsValid() || NOT State->Services->GetAuthInterface().IsValid())
    { InCallback.ExecuteIfBound(false, TEXT("EOS services unavailable")); return; }
    if (NOT State->ClaimAuthLease())
    { InCallback.ExecuteIfBound(false, TEXT("Previous EOS authentication cleanup pending")); return; }
    if (State->ReconcileRequired)
    {
        State->RetryCleanup();
        InCallback.ExecuteIfBound(false, TEXT("EOS authentication cleanup pending; retry after cleanup"));
        return;
    }
    const auto NativeUser = State->Services->GetAuthInterface()->GetLocalOnlineUserByPlatformUserId({FPlatformMisc::GetPlatformUserForUserIndex(0)});
    if (NativeUser.IsError() && NativeUser.GetErrorValue() != Errors::InvalidUser())
    {
        State->NativeOwnershipUnknown = true;
        State->ReconcileRequired = true;
        InCallback.ExecuteIfBound(false, TEXT("EOS native identity lookup unavailable"));
        return;
    }
    State->NativeAccountId = NativeUser.IsOk() ? NativeUser.GetOkValue().AccountInfo->AccountId : FAccountId{};
    State->LoggingIn = true;
    State->UseDeviceId = InUseDeviceId;
    State->LoginCallback = InCallback;
    auto WeakState = TWeakPtr<FCk_OnlineEOS_State>(State);
    State->ExpirationHandle = State->Services->GetAuthInterface()->OnPendingAuthExpiration().Add(
        [WeakState](const FAuthPendingAuthExpiration& Event)
        {
            auto Self = WeakState.Pin();
            if (NOT Self.IsValid() || NOT Self->Active || Self->LoggingIn || Event.AccountInfo->AccountId != Self->AccountId) { return; }
            Self->LoggingIn = true;
            if (Self->UseDeviceId) { Self->LoginWithToken(ExternalLoginType::DeviceIdAccessToken, FString{}); }
            else { Self->BeginSteamLogin(); }
        });
    State->StatusHandle = State->Services->GetAuthInterface()->OnLoginStatusChanged().Add(
        [WeakState](const FAuthLoginStatusChanged& Event)
        {
            auto Self = WeakState.Pin();
            if (NOT Self.IsValid() || NOT Self->Active || Event.AccountInfo->AccountId != Self->NativeAccountId || Event.LoginStatus == UE::Online::ELoginStatus::LoggedIn) { return; }
            Self->NativeAccountId = {};
            Self->ReconcileRequired = true;
            Self->InvalidateAccount();
            Self->FinishShutdown();
            if (auto* Owner = Self->Owner.Get())
            { Owner->OnLoginStatusChanged.Broadcast(false, TEXT("EOS authentication lost")); }
        });
    if (NOT InUseDeviceId) { State->BeginSteamLogin(); return; }
    State->DisplayName = TEXT("PlatformUser");
    auto* Manager = IEOSSDKManager::Get();
    if (Manager == nullptr) { State->CompleteLogin(false, TEXT("EOS SDK unavailable")); return; }
    auto PlatformConfigName = State->Services->GetInstanceConfigName().ToString();
    if (State->Services->GetInstanceConfigName().IsNone())
    {
        PlatformConfigName.Reset();
        GConfig->GetString(TEXT("OnlineServices.EOS"), TEXT("PlatformConfigName"), PlatformConfigName, GEngineIni);
        if (PlatformConfigName.IsEmpty())
        {
            auto LegacyProductId = FString{};
            GConfig->GetString(TEXT("OnlineServices.EOS"), TEXT("ProductId"), LegacyProductId, GEngineIni);
            if (NOT LegacyProductId.IsEmpty())
            { State->CompleteLogin(false, TEXT("EOS requires shared named platform configuration")); return; }
            PlatformConfigName = Manager->GetDefaultPlatformConfigName();
        }
    }
    // Match the native factory's named configuration and world instance. EOSShared caches this pair.
    // Legacy inline platform credentials are intentionally unsupported: they cannot identify a shared platform.
    auto Platform = Manager->CreatePlatform(PlatformConfigName, State->Services->GetInstanceName());
    if (NOT Platform.IsValid()) { State->CompleteLogin(false, TEXT("EOS platform unavailable")); return; }
    auto Options = EOS_Connect_CreateDeviceIdOptions{};
    Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    const auto DeviceModel = FPlatformMisc::GetDeviceMakeAndModel();
    const auto Utf8DeviceModel = FTCHARToUTF8(*DeviceModel);
    Options.DeviceModel = Utf8DeviceModel.Get();
    auto* Context = new TSharedPtr<FCk_OnlineEOS_State>(State);
    EOS_Connect_CreateDeviceId(EOS_Platform_GetConnectInterface(*Platform), &Options, Context,
        [](const EOS_Connect_CreateDeviceIdCallbackInfo* Info)
        {
            auto* Context = static_cast<TSharedPtr<FCk_OnlineEOS_State>*>(Info->ClientData);
            auto Self = *Context;
            delete Context;
            if (NOT Self->Active) { Self->CompleteLogin(false, FString{}); return; }
            if (Info->ResultCode != EOS_EResult::EOS_Success && Info->ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
            { Self->CompleteLogin(false, TEXT("Could not initialize EOS device identity")); return; }
            Self->LoginWithToken(ExternalLoginType::DeviceIdAccessToken, FString{});
        });
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Request_CreateSession(FName InSessionName, int32 InPublicConnections, FCk_Delegate_OnlineEOS_SessionComplete InCallback)
    -> void
{
    if (NOT Get_IsLoggedIn() || InSessionName.IsNone() || InPublicConnections < 1 || _State->Sessions.Contains(InSessionName) || NOT _State->Services->GetSessionsInterface().IsValid())
    { InCallback.ExecuteIfBound(false, InSessionName); return; }
    auto& Pending = _State->Sessions.Add(InSessionName);
    Pending.CreateCallback = InCallback;
    Pending.AccountId = _State->AccountId;
    auto Params = FCreateSession::Params{};
    Params.LocalAccountId = _State->AccountId;
    Params.SessionName = InSessionName;
    Params.SessionSettings.SchemaName = TEXT("GameSession");
    Params.SessionSettings.NumMaxConnections = static_cast<uint32>(InPublicConnections);
    _State->Services->GetSessionsInterface()->CreateSession(MoveTemp(Params)).OnComplete(
        [State = _State, InSessionName](const TOnlineResult<FCreateSession>& Result)
        {
            auto* Pending = State->Sessions.Find(InSessionName);
            if (Pending == nullptr) { return; }
            auto Callback = Pending->CreateCallback;
            Pending->CreateCallback.Unbind();
            auto Cancelled = Pending->Cancelled || NOT State->Active;
            if (Result.IsOk())
            {
                Pending->Phase = ESessionPhase::Ready;
                if (State->Active) { Callback.ExecuteIfBound(NOT Cancelled, InSessionName); }
                if (Cancelled) { State->DestroySession(InSessionName); }
                return;
            }
            else
            {
                auto DestroyCallbacks = MoveTemp(Pending->DestroyCallbacks);
                State->Sessions.Remove(InSessionName);
                State->FinishShutdown();
                if (State->Active) { Callback.ExecuteIfBound(false, InSessionName); }
                if (State->Active)
                {
                    for (auto& DestroyCallback : DestroyCallbacks)
                    { if (State->Active) { DestroyCallback.ExecuteIfBound(true, InSessionName); } }
                }
            }
        });
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Request_DestroySession(FName InSessionName, FCk_Delegate_OnlineEOS_SessionComplete InCallback)
    -> void
{
    if (NOT _State.IsValid() || NOT _State->Active) { InCallback.ExecuteIfBound(false, InSessionName); return; }
    auto* Pending = _State->Sessions.Find(InSessionName);
    if (Pending == nullptr) { InCallback.ExecuteIfBound(true, InSessionName); return; }
    Pending->Cancelled = true;
    Pending->DestroyCallbacks.Add(InCallback);
    if (Pending->Phase != ESessionPhase::Creating) { _State->DestroySession(InSessionName); }
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Get_ResolvedConnectString(FName InSessionName) const
    -> FString
{
    if (NOT Get_IsLoggedIn()) { return {}; }
    const auto* Pending = _State->Sessions.Find(InSessionName);
    if (Pending == nullptr || Pending->Cancelled || Pending->Phase != ESessionPhase::Ready) { return {}; }
    auto Session = _State->Services->GetSessionsInterface()->GetSessionByName({InSessionName});
    if (NOT Session.IsOk()) { return {}; }
    auto Params = FGetResolvedConnectString::Params{};
    Params.LocalAccountId = _State->AccountId;
    Params.SessionId = Session.GetOkValue().Session->GetSessionId();
    Params.PortType = FName{TEXT("GamePort")};
    auto Result = _State->Services->GetResolvedConnectString(MoveTemp(Params));
    if (NOT Result.IsOk()) { return {}; }
    const auto& Address = Result.GetOkValue().ResolvedConnectString;
    return Get_IsValidConnectString(Address) ? Address : FString{};
}

auto
    UCk_OnlineEOS_Subsystem_UE::
    Get_IsValidConnectString(const FString& InAddress) const
    -> bool
{
    constexpr auto ExpectedLength = 38;
    if (InAddress.Len() != ExpectedLength || NOT InAddress.StartsWith(TEXT("[EOS:"), ESearchCase::CaseSensitive) || InAddress[ExpectedLength - 1] != TEXT(']'))
    { return false; }
    const auto ProductUserId = InAddress.Mid(5, 32);
    for (auto Character : ProductUserId)
    { if (NOT FChar::IsHexDigit(Character)) { return false; } }
    const auto Utf8ProductUserId = FTCHARToUTF8(*ProductUserId);
    return EOS_ProductUserId_IsValid(EOS_ProductUserId_FromString(Utf8ProductUserId.Get())) == EOS_TRUE;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSDefaultStateTest,
    "Ck.OnlineEOS.DefaultStateRejectsOperations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSDefaultStateTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* Subsystem = NewObject<UCk_OnlineEOS_Subsystem_UE>(Instance);
    TestFalse(TEXT("Default subsystem has no identity"), Subsystem->Get_IsLoggedIn());
    TestTrue(TEXT("Default display name is empty"), Subsystem->Get_DisplayName().IsEmpty());
    Subsystem->Request_Login(true, {});
    Subsystem->Request_CreateSession(NAME_None, 0, {});
    Subsystem->Request_DestroySession(NAME_None, {});
    TestTrue(TEXT("Rejected operations cannot publish an address"), Subsystem->Get_ResolvedConnectString(FName{TEXT("GameSession")}).IsEmpty());
    for (const auto& Address : TArray<FString>{TEXT(""), TEXT("127.0.0.1"), TEXT("[EOS:]"),
        TEXT("[EOS:00000000000000000000000000000000]?listen"), TEXT("[EOS:zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz]"),
        TEXT("[EOS:01234567890123456789012345678901]/map")})
    { TestFalse(TEXT("Malformed address rejected"), Subsystem->Get_IsValidConnectString(Address)); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSPendingCreateLossTest,
    "Ck.OnlineEOS.AccountLossCancelsPendingCreate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSPendingCreateLossTest::RunTest(const FString&)
{
    auto State = MakeShared<FCk_OnlineEOS_State>();
    State->Sessions.Add(FName{TEXT("GameSession")});
    State->InvalidateAccount();
    TestTrue(TEXT("Pending create is canceled on account loss"), State->Sessions.FindChecked(FName{TEXT("GameSession")}).Cancelled);
    TestTrue(TEXT("Name remains reserved until native create completes"), State->Sessions.Contains(FName{TEXT("GameSession")}));
    TestTrue(TEXT("Cancellation cannot begin destroy before create completes"),
        State->Sessions.FindChecked(FName{TEXT("GameSession")}).Phase == ESessionPhase::Creating);
    State->Active = false;
    State->FinishShutdown();
    TestTrue(TEXT("Shutdown preserves pending cleanup state"), State->Sessions.Contains(FName{TEXT("GameSession")}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSRenewalShutdownTest,
    "Ck.OnlineEOS.RenewalShutdownRetainsNativeOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSRenewalShutdownTest::RunTest(const FString&)
{
    auto State = MakeShared<FCk_OnlineEOS_State>();
    const auto NativeId = FAccountId{EOnlineServices::Epic, 1};
    State->AccountId = NativeId;
    State->NativeAccountId = NativeId;
    State->LoggingIn = true;
    State->Active = false;
    State->Sessions.Add(FName{TEXT("GameSession")});
    TestTrue(TEXT("Initial state claims native ownership"), State->ClaimAuthLease());
    State->CompleteLogin(false, FString{});
    TestFalse(TEXT("Abandoned Steam-ticket wait settles LoggingIn"), State->LoggingIn);
    TestFalse(TEXT("Rejected refresh cannot expose usable CK identity"), State->AccountId.IsValid());
    TestTrue(TEXT("Native identity survives refresh rejection until reconciliation"), State->NativeAccountId == NativeId);
    TestTrue(TEXT("Pending session canceled while preserving owner"), State->Sessions.FindChecked(FName{TEXT("GameSession")}).Cancelled);
    TestTrue(TEXT("Session cleanup retains native ownership lease"), State->OwnsAuthLease());
    auto Replacement = MakeShared<FCk_OnlineEOS_State>();
    TestFalse(TEXT("Replacement cannot claim provider while cleanup is pending"), Replacement->ClaimAuthLease());
    State->Sessions.Reset();
    State->NativeAccountId = {};
    State->FinishShutdown();
    TestTrue(TEXT("Authoritative absent native identity permits replacement claim"), Replacement->ClaimAuthLease());
    State->ReleaseAuthLease();
    TestTrue(TEXT("Old completion cannot release replacement ownership"), Replacement->OwnsAuthLease());
    Replacement->ReleaseAuthLease();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSLogoutLeaseTest,
    "Ck.OnlineEOS.LogoutCompletionControlsOwnershipLease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSLogoutLeaseTest::RunTest(const FString&)
{
    TSharedPtr<FCk_OnlineEOS_State> State = MakeShared<FCk_OnlineEOS_State>();
    const auto NativeId = FAccountId{EOnlineServices::Epic, 1};
    State->NativeAccountId = NativeId;
    State->LoggingOut = true;
    TestTrue(TEXT("Logout begins under provider lease"), State->ClaimAuthLease());
    State->CompleteLogout(false);
    TestTrue(TEXT("Failed logout retains backend identity"), State->NativeAccountId == NativeId);
    TestTrue(TEXT("Failed logout retains ownership for explicit retry"), State->OwnsAuthLease());
    TestTrue(TEXT("Failed logout requires reconciliation"), State->ReconcileRequired);
    State->LoggingOut = true;
    State->CompleteLogout(true);
    TestFalse(TEXT("Observed successful logout clears backend identity"), State->NativeAccountId.IsValid());
    TestFalse(TEXT("Observed successful logout releases lease"), State->OwnsAuthLease());
    State->NativeAccountId = NativeId;
    State->Active = false;
    State->ReconcileRequired = true;
    auto& Pending = State->Sessions.Add(FName{TEXT("GameSession")});
    Pending.AccountId = NativeId;
    Pending.Phase = ESessionPhase::CleanupFailed;
    State->NativeOwnershipUnknown = true;
    TestTrue(TEXT("Failed terminal cleanup records exact ownership"), State->ClaimAuthLease());
    auto PreviousOwner = TWeakPtr<FCk_OnlineEOS_State>(State);
    State.Reset();
    TestFalse(TEXT("Cleanup record does not retain full state"), PreviousOwner.IsValid());
    auto Replacement = MakeShared<FCk_OnlineEOS_State>();
    TestTrue(TEXT("Replacement claims cleanup record before authentication"), Replacement->ClaimAuthLease());
    TestTrue(TEXT("Replacement must reconcile old native identity"), Replacement->ReconcileRequired && Replacement->NativeAccountId == NativeId);
    TestTrue(TEXT("Unknown native outcome survives ownership transfer"), Replacement->NativeOwnershipUnknown);
    TestTrue(TEXT("Recorded session cleanup retains its original account"), Replacement->Sessions.FindChecked(FName{TEXT("GameSession")}).AccountId == NativeId);
    Replacement->ReleaseAuthLease();
    return true;
}
#endif
