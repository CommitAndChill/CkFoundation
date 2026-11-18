#include "CkOnlineEOS/CkOnlineEOS_NetDriver.h"

#include "CkOnlineEOS/CkOnlineEOS_Subsystem.h"
#include "CkCore/Macros/CkMacros.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EOSSharedTypes.h"

namespace
{
    enum class EListenRoute : uint8 { IP, EOS, Reject };
    enum class EConnectRoute : uint8 { IP, EOS, Reject };

    auto SelectConnectRoute(const FString& InHost) -> EConnectRoute
    {
        if (NOT InHost.StartsWith(EOS_CONNECTION_URL_PREFIX, ESearchCase::IgnoreCase)) { return EConnectRoute::IP; }
        const auto Address = FString::Printf(TEXT("[%s]"), *InHost);
        return GetDefault<UCk_OnlineEOS_Subsystem_UE>()->Get_IsValidConnectString(Address) ? EConnectRoute::EOS : EConnectRoute::Reject;
    }

    auto SelectListenRoute(bool InExplicitIP, bool InPIE, bool InRequestedOnline, bool InAuthenticated) -> EListenRoute
    {
        if (InRequestedOnline && (InPIE || NOT InAuthenticated || InExplicitIP)) { return EListenRoute::Reject; }
        return InRequestedOnline ? EListenRoute::EOS : EListenRoute::IP;
    }
}

UCk_OnlineEOS_NetDriver_UE::UCk_OnlineEOS_NetDriver_UE(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Native EOS connections implement IP passthrough for both local and remote connections.
    NetConnectionClassName = TEXT("/Script/SocketSubsystemEOS.NetConnectionEOS");
    bIsPassthrough = true;
}

auto
    UCk_OnlineEOS_NetDriver_UE::
    IsAvailable() const
    -> bool
{
    // Keep selection inside CK so a missing EOS backend cannot silently turn explicit online intent into IP.
    return true;
}

auto
    UCk_OnlineEOS_NetDriver_UE::
    GetSocketSubsystem()
    -> ISocketSubsystem*
{
    if (NOT bIsPassthrough && NOT Super::IsAvailable()) { return nullptr; }
    return Super::GetSocketSubsystem();
}

auto
    UCk_OnlineEOS_NetDriver_UE::
    Shutdown()
    -> void
{
    // Native shutdown resets metrics even after rejection before InitBase created them.
    // Complete that local prerequisite without starting sockets, listeners or authentication.
    if (GetMetrics() == nullptr) { SetupNetworkMetrics(); }
    Super::Shutdown();
}

auto
    UCk_OnlineEOS_NetDriver_UE::
    InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
    -> bool
{
    const auto Route = SelectConnectRoute(ConnectURL.Host);
    if (Route == EConnectRoute::Reject)
    {
        Error = TEXT("Malformed reserved EOS connection address");
        return false;
    }
    if (Route == EConnectRoute::EOS)
    {
        auto* OwningWorld = FindWorld();
        auto* Instance = OwningWorld != nullptr ? OwningWorld->GetGameInstance() : nullptr;
        auto* Online = Instance != nullptr ? Instance->GetSubsystem<UCk_OnlineEOS_Subsystem_UE>() : nullptr;
        if (Online == nullptr || NOT Online->Get_IsLoggedIn() || NOT Super::IsAvailable())
        {
            Error = TEXT("CK EOS connection requires authenticated identity and valid P2P address");
            return false;
        }
    }
    bIsPassthrough = Route == EConnectRoute::IP;
    return Super::InitConnect(InNotify, ConnectURL, Error);
}

auto
    UCk_OnlineEOS_NetDriver_UE::
    InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool InReuseAddressAndPort, FString& Error)
    -> bool
{
    auto* OwningWorld = FindWorld();
    auto* Instance = OwningWorld != nullptr ? OwningWorld->GetGameInstance() : nullptr;
    auto* Online = Instance != nullptr ? Instance->GetSubsystem<UCk_OnlineEOS_Subsystem_UE>() : nullptr;
    const auto IsPIE = OwningWorld != nullptr && OwningWorld->WorldType == EWorldType::PIE;
    const auto Authenticated = Online != nullptr && Online->Get_IsLoggedIn();
    const auto ExplicitIP = LocalURL.HasOption(TEXT("bUseIPSockets")) || LocalURL.HasOption(TEXT("bIsLanMatch"));
    const auto RequestedOnline = LocalURL.HasOption(TEXT("CkOnlineP2P"));
    const auto Route = SelectListenRoute(ExplicitIP, IsPIE, RequestedOnline, Authenticated);
    if (Route == EListenRoute::Reject)
    {
        Error = TEXT("CK EOS online hosting requires authenticated non-PIE identity and compatible P2P options");
        return false;
    }
    if (Route == EListenRoute::EOS && NOT Super::IsAvailable())
    {
        Error = TEXT("CK EOS P2P sockets unavailable");
        return false;
    }
    if (Route == EListenRoute::IP) { LocalURL.AddOption(TEXT("bUseIPSockets")); }
    bIsPassthrough = Route == EListenRoute::IP;
    return Super::InitListen(InNotify, LocalURL, InReuseAddressAndPort, Error);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSListenRouteTest,
    "Ck.OnlineEOS.ListenRoutePreservesOfflineAndPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSListenRouteTest::RunTest(const FString&)
{
    TestTrue(TEXT("Plain offline listen uses IP"), SelectListenRoute(false, false, false, false) == EListenRoute::IP);
    TestTrue(TEXT("Plain authenticated listen remains IP"), SelectListenRoute(false, false, false, true) == EListenRoute::IP);
    TestTrue(TEXT("PIE listen uses IP"), SelectListenRoute(false, true, false, false) == EListenRoute::IP);
    TestTrue(TEXT("Explicit IP remains IP"), SelectListenRoute(true, false, false, true) == EListenRoute::IP);
    TestTrue(TEXT("Intentional authenticated CK host uses EOS"), SelectListenRoute(false, false, true, true) == EListenRoute::EOS);
    TestTrue(TEXT("Lost authentication rejects intended online hosting"), SelectListenRoute(false, false, true, false) == EListenRoute::Reject);
    TestTrue(TEXT("Contradictory IP/online intent rejects"), SelectListenRoute(true, false, true, true) == EListenRoute::Reject);
    TestTrue(TEXT("Online PIE is rejected without changing local PIE routing"), SelectListenRoute(false, true, true, true) == EListenRoute::Reject);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineEOSConnectRouteTest,
    "Ck.OnlineEOS.ConnectRouteRejectsMalformedNativePrefixes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOnlineEOSConnectRouteTest::RunTest(const FString&)
{
    const auto Cases = TArray<TPair<FString, EConnectRoute>>{
        {TEXT("eos-test"), EConnectRoute::Reject},
        {TEXT("eos.example.com"), EConnectRoute::Reject},
        {TEXT("EOS"), EConnectRoute::Reject},
        {TEXT("EOS:"), EConnectRoute::Reject},
        {TEXT("eos:01234567890123456789012345678901"), EConnectRoute::Reject},
        {TEXT("EOS:01234567890123456789012345678901?listen"), EConnectRoute::Reject},
        {TEXT("EOS:zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"), EConnectRoute::Reject},
        {TEXT("localhost"), EConnectRoute::IP},
        {TEXT("127.0.0.1"), EConnectRoute::IP},
        {TEXT("192.168.1.2"), EConnectRoute::IP},
        {TEXT("::1"), EConnectRoute::IP},
        {TEXT("game.example.com"), EConnectRoute::IP},
        {TEXT("EOS:01234567890123456789012345678901"), EConnectRoute::EOS}
    };
    for (const auto& Case : Cases)
    { TestTrue(*FString::Printf(TEXT("Native prefix classification: %s"), *Case.Key), SelectConnectRoute(Case.Key) == Case.Value); }

    auto* Driver = NewObject<UCk_OnlineEOS_NetDriver_UE>();
    TestTrue(TEXT("Pre-init driver has not started metrics lifecycle"), Driver->GetMetrics() == nullptr);
    TestTrue(TEXT("Pre-init driver uses safe IP accessor"), Driver->bIsPassthrough);
    TestNotNull(TEXT("Pre-init socket lookup uses ordinary platform subsystem"), Driver->GetSocketSubsystem());
    for (const auto& Case : Cases)
    {
        if (Case.Value != EConnectRoute::Reject) { continue; }
        auto URL = FURL{};
        URL.Host = Case.Key;
        auto Error = FString{};
        TestFalse(TEXT("Malformed native prefix rejected before native init"), Driver->InitConnect(nullptr, URL, Error));
        TestFalse(TEXT("Rejected travel reports a reason"), Error.IsEmpty());
        TestTrue(TEXT("Rejected travel preserves safe pre-init passthrough"), Driver->bIsPassthrough);
    }
    TestTrue(TEXT("Malformed rejection never enters native InitBase"), Driver->GetMetrics() == nullptr);
    Driver->Shutdown();
    TestTrue(TEXT("Rejected-driver teardown satisfies native metrics prerequisite"), Driver->GetMetrics() != nullptr);
    return true;
}
#endif
