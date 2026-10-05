#include "CkOnline/Connection/CkOnlineConnection_Transport.h"

#include "CkOnline/Direct/CkOnlineDirect_Subsystem.h"
#include "CkOnline/Direct/CkOnlineDirect_Utils.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IPAddress.h"
#include "IpConnection.h"
#include "IpNetDriver.h"
#include "Misc/PackageName.h"

namespace ck::connection
{
    class FIpTransport final : public ITransport, public TSharedFromThis<FIpTransport>
    {
    public:
        explicit FIpTransport(UGameInstance* InInstance) : _Instance(InInstance), _Engine(GEngine) {}
        ~FIpTransport() override { Shutdown(); }

        auto Attach() -> bool
        {
            auto* Engine = _Engine.Get();
            if (NOT IsInGameThread() || NOT IsValid(Engine) || NOT _Instance.IsValid()) { return false; }
            _NetworkFailure = Engine->OnNetworkFailure().AddSP(AsShared(), &FIpTransport::OnNetworkFailure);
            _TravelFailure = Engine->OnTravelFailure().AddSP(AsShared(), &FIpTransport::OnTravelFailure);
            return true;
        }

        auto CanEnter(const FEntryIntent& InIntent) const -> bool override
        {
            if (NOT IsIdle() || NOT ValidateIntent(InIntent)) { return false; }
            auto* Instance = _Instance.Get();
            auto* Direct = Instance->GetSubsystem<UCk_OnlineDirect_Subsystem_UE>();
            if (NOT IsValid(Direct)) { return false; }
            const auto Service = Direct->Get_Service();
            if (ck::Is_NOT_Valid(Service) || Service != InIntent.DirectService) { return false; }
            const auto* Context = GetContext();
            auto* World = Context != nullptr ? Context->World() : nullptr;
            if (NOT IsValid(World)) { return false; }
            if (NOT InIntent.Hosting && NOT IsValid(Instance->GetFirstLocalPlayerController())) { return false; }
            auto* Engine = _Engine.Get();
            const auto* Definition = Engine->NetDriverDefinitions.FindByPredicate(
                [](const FNetDriverDefinition& InDefinition) { return InDefinition.DefName == NAME_GameNetDriver; });
            if (Definition == nullptr || Definition->DriverClassName.GetStringLength() > 512) { return false; }
            const auto DriverPath = Definition->DriverClassName.ToString();
            const auto* DriverClass = FindObject<UClass>(nullptr, *DriverPath);
            return IsValid(DriverClass) && DriverClass == UIpNetDriver::StaticClass() &&
                NOT DriverClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
        }

        auto IsIdle() const -> bool override
        {
            if (NOT IsInGameThread() || _Phase != EPhase::Idle) { return false; }
            const auto* Context = GetContext();
            auto* World = Context != nullptr ? Context->World() : nullptr;
            return IsValid(World) && World->IsGameWorld() && World->HasBegunPlay() &&
                World->GetNetMode() == NM_Standalone && World->GetNetDriver() == nullptr &&
                Context->PendingNetGame == nullptr && Context->TravelURL.IsEmpty() && World->NextURL.IsEmpty();
        }

        auto BeginEntry(const FEntryIntent& InIntent) -> bool override
        {
            if (NOT CanEnter(InIntent)) { return false; }
            const auto* Context = GetContext();
            auto* World = Context->World();
            auto* Controller = _Instance->GetFirstLocalPlayerController();
            if (NOT InIntent.Hosting && NOT IsValid(Controller)) { return false; }
            _Intent = InIntent;
            _ContextHandle = Context->ContextHandle;
            _Departure = World;
            _EstablishedWorld.Reset();
            _EstablishedDriver.Reset();
            _Pending.Reset();
            _PendingDriver.Reset();
            _CapturedPending = false;
            _CapturedPendingDriver = false;
            _FailureLatched = false;
            _Error.Empty();
            _Phase = EPhase::Entering;
            auto* Engine = _Engine.Get();
            if (InIntent.Hosting) { Engine->SetClientTravel(World, *InIntent.EntryUrl, TRAVEL_Absolute); }
            else { Controller->ClientTravel(InIntent.EntryUrl, TRAVEL_Absolute); }
            return true;
        }

        auto Observe() -> FObservation override
        {
            auto Result = FObservation{};
            if (NOT IsInGameThread() || _Phase == EPhase::Closed || _Phase == EPhase::Idle) { return Result; }
            auto* Context = GetContext();
            if (Context == nullptr) { RecordFailure(TEXT("The owned gameplay travel context was lost.")); }
            if (Context != nullptr && _Phase == EPhase::Entering) { CapturePending(*Context); }
            auto* World = Context != nullptr ? Context->World() : nullptr;
            if (NOT _FailureLatched && IsValid(World) && World->IsGameWorld() && World->HasBegunPlay())
            {
                if (_Phase == EPhase::Returning)
                {
                    if (World != _ReturnDeparture.Get() && Context->PendingNetGame == nullptr &&
                        Context->TravelURL.IsEmpty() && World->NextURL.IsEmpty() &&
                        World->GetNetDriver() == nullptr && World->GetNetMode() == NM_Standalone)
                    {
                        if (MatchesWorldTarget(World, _Intent.ReturnMap, _Intent.ReturnMode, true))
                        {
                            Result.ReturnEstablished = true;
                            _Phase = EPhase::Idle;
                            _Departure.Reset();
                            _ReturnDeparture.Reset();
                            _EstablishedWorld.Reset();
                            _EstablishedDriver.Reset();
                            _Pending.Reset();
                            _PendingDriver.Reset();
                        }
                        else { RecordFailure(TEXT("Return travel reached a different map or game mode.")); }
                    }
                }
                else if (_Phase == EPhase::Established)
                {
                    if (World != _EstablishedWorld.Get() || World->GetNetDriver() != _EstablishedDriver.Get() ||
                        NOT ReadTransport(World))
                    { RecordFailure(TEXT("The established gameplay transport was lost or replaced.")); }
                    else { Result.TransportEstablished = true; }
                }
                else if (World != _Departure.Get() && Context->PendingNetGame == nullptr &&
                    Context->TravelURL.IsEmpty() && World->NextURL.IsEmpty())
                {
                    if (ReadTransport(World))
                    {
                        _EstablishedWorld = World;
                        _EstablishedDriver = World->GetNetDriver();
                        _Phase = EPhase::Established;
                        Result.TransportEstablished = true;
                    }
                    else { RecordFailure(TEXT("Gameplay travel did not establish the requested stock IP transport and target.")); }
                }
            }
            else if (_Phase == EPhase::Established && NOT _FailureLatched)
            { RecordFailure(TEXT("The established gameplay world is no longer live.")); }
            Result.TransportLost = _Phase == EPhase::Established && _FailureLatched;
            Result.Error = MoveTemp(_Error);
            _Error.Empty();
            return Result;
        }

        auto BeginReturn(const FEntryIntent& InIntent) -> EReturnAdmission override
        {
            if (NOT IsInGameThread() || (_Phase != EPhase::Entering && _Phase != EPhase::Established) ||
                NOT SameIntent(InIntent) || NOT ValidateTarget(InIntent.ReturnMap, InIntent.ReturnMode))
            { return EReturnAdmission::Rejected; }
            auto* Context = GetContext();
            auto* World = Context != nullptr ? Context->World() : nullptr;
            if (Context == nullptr) { return EReturnAdmission::Rejected; }
            if (NOT IsValid(World) || NOT World->IsGameWorld() || NOT World->HasBegunPlay())
            { return EReturnAdmission::Busy; }
            if (NOT World->NextURL.IsEmpty()) { return EReturnAdmission::Busy; }
            const bool OwnQueuedEntry = _Phase == EPhase::Entering && World == _Departure.Get() &&
                Context->TravelURL == _Intent.EntryUrl && Context->TravelType == TRAVEL_Absolute;
            if (NOT Context->TravelURL.IsEmpty() && NOT OwnQueuedEntry) { return EReturnAdmission::Busy; }
            CapturePending(*Context);
            if (Context->PendingNetGame != nullptr &&
                (NOT _CapturedPending || Context->PendingNetGame.Get() != _Pending.Get() ||
                 Context->PendingNetGame->NetDriver.Get() != _PendingDriver.Get()))
            { return EReturnAdmission::Rejected; }
            auto ReturnUrl = FURL{nullptr, *InIntent.ReturnMap, TRAVEL_Absolute};
            const auto GameOption = FString::Printf(TEXT("game=%s"), *InIntent.ReturnMode);
            ReturnUrl.AddOption(*GameOption);
            const auto Url = ReturnUrl.ToString();
            const auto RoundTrip = FURL{nullptr, *Url, TRAVEL_Absolute};
            if (NOT ReturnUrl.Valid || NOT RoundTrip.Valid || NOT RoundTrip.IsLocalInternal() ||
                RoundTrip.Map != InIntent.ReturnMap || NOT RoundTrip.Portal.IsEmpty() ||
                NOT RoundTrip.RedirectURL.IsEmpty() || RoundTrip.Op.Num() != 1 ||
                RoundTrip.Op[0] != GameOption || RoundTrip.ToString() != Url) { return EReturnAdmission::Rejected; }
            _ReturnDeparture = World;
            _Phase = EPhase::Returning;
            _FailureLatched = false;
            _Error.Empty();
            auto* Engine = _Engine.Get();
            if (Context->PendingNetGame != nullptr) { Engine->CancelPending(World); }
            // CancelPending may notify other owners synchronously. Never overwrite their new travel.
            Context = GetContext();
            if (_Phase == EPhase::Closed || _FailureLatched || Context == nullptr || Context->World() != World ||
                Context->PendingNetGame != nullptr || NOT World->NextURL.IsEmpty() ||
                (NOT Context->TravelURL.IsEmpty() && NOT
                 (OwnQueuedEntry && Context->TravelURL == _Intent.EntryUrl && Context->TravelType == TRAVEL_Absolute)))
            {
                if (_Phase != EPhase::Closed && NOT _FailureLatched)
                { RecordFailure(TEXT("Return travel context changed during owned pending cancellation.")); }
                return EReturnAdmission::Rejected;
            }
            Engine->SetClientTravel(World, *Url, TRAVEL_Absolute);
            return EReturnAdmission::Started;
        }

        auto Shutdown() -> void override
        {
            if (NOT IsInGameThread() || _Phase == EPhase::Closed) { return; }
            _Phase = EPhase::Closed;
            auto* Engine = _Engine.Get();
            if (IsValid(Engine))
            {
                Engine->OnNetworkFailure().Remove(_NetworkFailure);
                Engine->OnTravelFailure().Remove(_TravelFailure);
            }
            _NetworkFailure.Reset();
            _TravelFailure.Reset();
            _Error.Empty();
            _Instance.Reset();
            _Engine.Reset();
            _Departure.Reset();
            _ReturnDeparture.Reset();
            _EstablishedWorld.Reset();
            _EstablishedDriver.Reset();
            _Pending.Reset();
            _PendingDriver.Reset();
        }

    private:
        enum class EPhase : uint8 { Idle, Entering, Established, Returning, Closed };

        static auto ValidateTarget(const FString& InMap, const FString& InMode) -> bool
        {
            if (InMap.IsEmpty() || InMap.Len() > 512 || InMode.IsEmpty() || InMode.Len() > 512 ||
                NOT FPackageName::IsValidLongPackageName(InMap) || NOT FPackageName::IsValidObjectPath(InMode))
            { return false; }
            const auto* Mode = FindObject<UClass>(nullptr, *InMode);
            return IsValid(Mode) && Mode->IsChildOf(AGameModeBase::StaticClass()) &&
                NOT Mode->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
                Mode->GetPathName() == InMode;
        }

        static auto ValidateIntent(const FEntryIntent& InIntent) -> bool
        {
            if (NOT ValidateTarget(InIntent.GameplayMap, InIntent.GameplayMode) ||
                NOT ValidateTarget(InIntent.ReturnMap, InIntent.ReturnMode) ||
                InIntent.Endpoint.Len() > 21 || InIntent.EntryUrl.Len() > 1100) { return false; }
            auto Endpoint = FString{};
            if (NOT UCk_Utils_OnlineDirect_UE::TryParseGameplayEndpoint(InIntent.Endpoint, Endpoint) ||
                Endpoint != InIntent.Endpoint) { return false; }
            if (NOT InIntent.Hosting) { return InIntent.EntryUrl == Endpoint; }
            if (InIntent.ListenPort < 1 || InIntent.ListenPort > 65535) { return false; }
            const auto ExpectedPort = FString::Printf(TEXT(":%d"), InIntent.ListenPort);
            if (NOT Endpoint.EndsWith(ExpectedPort)) { return false; }
            const auto Url = FURL{nullptr, *InIntent.EntryUrl, TRAVEL_Absolute};
            const auto GameOption = FString::Printf(TEXT("game=%s"), *InIntent.GameplayMode);
            return Url.Valid && Url.IsLocalInternal() && Url.Map == InIntent.GameplayMap &&
                Url.Port == InIntent.ListenPort && Url.Portal.IsEmpty() && Url.RedirectURL.IsEmpty() &&
                Url.Op.Num() == 2 && Url.Op[0] == GameOption && Url.Op[1] == TEXT("listen") &&
                Url.ToString() == InIntent.EntryUrl;
        }

        auto GetContext() const -> FWorldContext*
        {
            auto* Instance = _Instance.Get();
            auto* Engine = _Engine.Get();
            if (_Phase == EPhase::Closed || NOT IsValid(Instance) || NOT IsValid(Engine) || Engine != GEngine)
            { return nullptr; }
            auto* Context = Instance->GetWorldContext();
            if (Context == nullptr || Context->OwningGameInstance != Instance ||
                (NOT _ContextHandle.IsNone() && Context->ContextHandle != _ContextHandle)) { return nullptr; }
            auto* World = Context->World();
            if (World != nullptr && (NOT IsValid(World) || World->GetGameInstance() != Instance ||
                Engine->GetWorldContextFromWorld(World) != Context)) { return nullptr; }
            return Context;
        }

        auto SameIntent(const FEntryIntent& InIntent) const -> bool
        {
            return InIntent.Hosting == _Intent.Hosting && InIntent.EntryUrl == _Intent.EntryUrl &&
                InIntent.Endpoint == _Intent.Endpoint && InIntent.GameplayMap == _Intent.GameplayMap &&
                InIntent.GameplayMode == _Intent.GameplayMode && InIntent.ReturnMap == _Intent.ReturnMap &&
                InIntent.ReturnMode == _Intent.ReturnMode;
        }

        auto CapturePending(const FWorldContext& InContext) -> void
        {
            if (_Phase != EPhase::Entering || _Intent.Hosting ||
                NOT IsValid(InContext.PendingNetGame)) { return; }
            const auto* Pending = InContext.PendingNetGame.Get();
            if (_CapturedPending)
            {
                if (Pending != _Pending.Get()) { return; }
            }
            else
            {
                if (Pending->URL.Host.Len() > 15 || Pending->URL.Port < 1 || Pending->URL.Port > 65535) { return; }
                const auto Endpoint = FString::Printf(TEXT("%s:%d"), *Pending->URL.Host, Pending->URL.Port);
                if (Endpoint != _Intent.Endpoint) { return; }
                _Pending = InContext.PendingNetGame.Get();
                _CapturedPending = true;
            }
            if (NOT _CapturedPendingDriver && IsValid(Pending->NetDriver))
            {
                _PendingDriver = Pending->NetDriver.Get();
                _CapturedPendingDriver = true;
            }
        }

        static auto MatchesWorldTarget(UWorld* InWorld, const FString& InMap, const FString& InMode,
            bool InRequireMode) -> bool
        {
            if (NOT IsValid(InWorld) || NOT IsValid(InWorld->GetPackage()) ||
                UWorld::RemovePIEPrefix(InWorld->GetPackage()->GetName()) != InMap) { return false; }
            const auto* Mode = InWorld->GetAuthGameMode();
            if (IsValid(Mode)) { return Mode->GetClass()->GetPathName() == InMode; }
            if (InRequireMode) { return false; }
            const auto* ModeOption = InWorld->URL.GetOption(TEXT("game="), nullptr);
            return ModeOption == nullptr || InMode == ModeOption;
        }

        auto ReadTransport(UWorld* InWorld) const -> bool
        {
            if (NOT MatchesWorldTarget(InWorld, _Intent.GameplayMap, _Intent.GameplayMode, _Intent.Hosting))
            { return false; }
            auto* Driver = InWorld->GetNetDriver();
            if (NOT IsValid(Driver) || Driver->GetClass() != UIpNetDriver::StaticClass() ||
                Driver->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || Driver->GetWorld() != InWorld)
            { return false; }
            auto* IpDriver = static_cast<UIpNetDriver*>(Driver);
            if (_Intent.Hosting)
            {
                if (InWorld->GetNetMode() != NM_ListenServer || Driver->ServerConnection != nullptr) { return false; }
                if (NOT IpDriver->IsNetResourceValid()) { return false; }
                const auto Address = IpDriver->GetLocalAddr();
                return Address.IsValid() && Address->IsValid() && Address->GetPort() == _Intent.ListenPort;
            }
            if (InWorld->GetNetMode() != NM_Client) { return false; }
            auto* Connection = Driver->ServerConnection.Get();
            if (NOT IsValid(Connection) || Connection->GetClass() != UIpConnection::StaticClass() ||
                Connection->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
                Connection->GetDriver() != Driver || Connection->GetConnectionState() != USOCK_Open)
            { return false; }
            if (NOT IpDriver->IsNetResourceValid()) { return false; }
            const auto Address = Connection->GetRemoteAddr();
            return Address.IsValid() && Address->IsValid() && Address->ToString(true) == _Intent.Endpoint;
        }

        auto RecordFailure(const FString& InError) -> void
        {
            if (_FailureLatched || _Phase == EPhase::Closed) { return; }
            _FailureLatched = true;
            _Error = InError.IsEmpty() ? TEXT("Gameplay transport failed.") : InError.Left(512);
        }

        auto OnNetworkFailure(UWorld* InWorld, UNetDriver* InDriver, ENetworkFailure::Type InType,
            const FString& InError) -> void
        {
            if (NOT IsInGameThread() || (_Phase != EPhase::Entering && _Phase != EPhase::Established) ||
                _FailureLatched) { return; }
            auto* Context = GetContext();
            if (Context == nullptr) { return; }
            CapturePending(*Context);
            auto* World = Context->World();
            const bool CurrentWorld = IsValid(World) && InWorld == World;
            const bool HostingReplacement = _Phase == EPhase::Entering && _Intent.Hosting && CurrentWorld &&
                World != _Departure.Get() && MatchesWorldTarget(World, _Intent.GameplayMap, _Intent.GameplayMode, false);
            const bool CurrentDriver = IsValid(InDriver) && CurrentWorld && World->GetNetDriver() == InDriver &&
                ((_Phase == EPhase::Established && World == _EstablishedWorld.Get() && InDriver == _EstablishedDriver.Get()) ||
                 HostingReplacement);
            const bool PendingDriver = _Phase == EPhase::Entering && NOT _Intent.Hosting &&
                _CapturedPending && IsValid(Context->PendingNetGame) && Context->PendingNetGame.Get() == _Pending.Get() &&
                IsValid(InDriver) && InDriver == _PendingDriver.Get() && Context->PendingNetGame->NetDriver == InDriver &&
                (InWorld == nullptr || CurrentWorld);
            const bool MissingHostDriver = _Phase == EPhase::Entering && _Intent.Hosting && InDriver == nullptr &&
                InType == ENetworkFailure::NetDriverCreateFailure && HostingReplacement &&
                World->GetNetDriver() == nullptr;
            const bool MissingPendingDriver = _Phase == EPhase::Entering && NOT _Intent.Hosting &&
                InType == ENetworkFailure::PendingConnectionFailure && InWorld == nullptr && InDriver == nullptr &&
                _CapturedPending && IsValid(Context->PendingNetGame) && Context->PendingNetGame.Get() == _Pending.Get() &&
                Context->PendingNetGame->NetDriver == nullptr && NOT Context->PendingNetGame->ConnectionError.IsEmpty();
            if (CurrentDriver || PendingDriver || MissingHostDriver || MissingPendingDriver) { RecordFailure(InError); }
        }

        auto OnTravelFailure(UWorld* InWorld, ETravelFailure::Type InType, const FString& InError) -> void
        {
            if (NOT IsInGameThread() || (_Phase != EPhase::Entering && _Phase != EPhase::Returning) ||
                _FailureLatched) { return; }
            const auto* Context = GetContext();
            if (Context == nullptr || NOT IsValid(InWorld) || InWorld != Context->World()) { return; }
            if (_Phase == EPhase::Returning)
            {
                if (InWorld == _ReturnDeparture.Get() ||
                    MatchesWorldTarget(InWorld, _Intent.ReturnMap, _Intent.ReturnMode, false))
                { RecordFailure(InError); }
                return;
            }
            // The same context may still own the departure world when Browse fails before replacement.
            if (InWorld == _Departure.Get() ||
                MatchesWorldTarget(InWorld, _Intent.GameplayMap, _Intent.GameplayMode, false))
            { RecordFailure(InError); }
        }

        TWeakObjectPtr<UGameInstance> _Instance;
        TWeakObjectPtr<UEngine> _Engine;
        TWeakObjectPtr<UWorld> _Departure;
        TWeakObjectPtr<UWorld> _ReturnDeparture;
        TWeakObjectPtr<UWorld> _EstablishedWorld;
        TWeakObjectPtr<UNetDriver> _EstablishedDriver;
        TWeakObjectPtr<UPendingNetGame> _Pending;
        TWeakObjectPtr<UNetDriver> _PendingDriver;
        FDelegateHandle _NetworkFailure;
        FDelegateHandle _TravelFailure;
        FName _ContextHandle;
        FEntryIntent _Intent;
        FString _Error;
        EPhase _Phase = EPhase::Idle;
        bool _CapturedPending = false;
        bool _CapturedPendingDriver = false;
        bool _FailureLatched = false;
    };

    auto TryCreateIpTransport(UGameInstance* InInstance) -> TSharedPtr<ITransport>
    {
        if (NOT IsInGameThread() || NOT IsValid(InInstance) || NOT IsValid(GEngine)) { return {}; }
        const auto Transport = MakeShared<FIpTransport>(InInstance);
        if (NOT Transport->Attach()) { return {}; }
        return Transport;
    }
}
