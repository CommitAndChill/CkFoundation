#include "CkOnlineAchievements/CkOnlineAchievements_Subsystem.h"
#include "CkOnlineAchievements_OperationOwner.h"

#include <Containers/Ticker.h>
#include <Engine/World.h>
#include <Interfaces/OnlineIdentityInterface.h>
#include <Misc/App.h>
#include <Modules/ModuleManager.h>
#include <OnlineSubsystem.h>
#include <OnlineSubsystemModule.h>

#if WITH_CK_ACHIEVEMENTS_STEAM
#include <SteamSharedModule.h>
THIRD_PARTY_INCLUDES_START
#include <steam/steam_api.h>
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
    using namespace ck::online_achievements;

#if WITH_CK_ACHIEVEMENTS_STEAM
    // One receiver and native ingress stay allocated until OS exit. Unregister is not a
    // dispatch barrier; an OSS OnlineThread callback may still be inside Receive at shutdown.
    class FSteamStatsReceiver
    {
    public:
        FCallbackIngress Ingress;

        FSteamStatsReceiver()
        {
            Callback.Register(this, &FSteamStatsReceiver::OnStored);
        }

        auto Unregister() -> void { Callback.Unregister(); }

    private:
        auto OnStored(UserStatsStored_t* InResult) -> void
        {
            if (InResult)
            {
                Ingress.Receive(InResult->m_nGameID, static_cast<int32>(InResult->m_eResult));
            }
        }
        CCallbackManual<FSteamStatsReceiver, UserStatsStored_t> Callback;
    };

    class FSteamStatsBridge
    {
    public:
        TSharedPtr<FSteamClientInstanceHandler> Instance;
        TWeakObjectPtr<UCk_OnlineAchievements_Subsystem_UE> Owner;
        IOnlineSubsystem* Provider = nullptr;
        TWeakPtr<IOnlineIdentity, ESPMode::ThreadSafe> ProviderIdentity;
        uint64 ProviderEpoch = 0;
        FOperationOwner Operations;
        TMap<FString, int32> StagedStats;
        TSet<FString> StagedAchievements;

        FSteamStatsBridge(TSharedPtr<FSteamClientInstanceHandler> InInstance, FCallbackIngress& InIngress)
            : Instance(MoveTemp(InInstance)), Operations(InIngress)
        {
        }

        auto Tick() -> void
        {
            if (!Owner.IsValid())
            {
                Operations.Detach();
            }
            auto Current = Operations.GetIdentity();
            auto* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
            const FName SteamName(TEXT("Steam"));
            const auto SameProvider = Module && Module->DoesInstanceExist(SteamName) &&
                Module->GetOnlineSubsystem(SteamName) == Provider && ProviderIdentity.IsValid() &&
                Provider->GetIdentityInterface() == ProviderIdentity.Pin();
            Current.Account = SameProvider && SteamUser() ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
            Current.App = SameProvider && SteamUtils() ? SteamUtils()->GetAppID() : 0;
            if (!SameProvider)
            {
                Current.Provider = 0;
            }
            Operations.Pump(Current, FPlatformTime::Seconds(), [this](FCompletion Completion)
            {
                if (!Owner.IsValid())
                {
                    return;
                }
                auto Kind = ECk_OnlineAchievements_Operation::Store;
                if (Completion.Operation == EOperation::Progress)
                {
                    Kind = ECk_OnlineAchievements_Operation::Progress;
                }
                else if (Completion.Operation == EOperation::Reset)
                {
                    Kind = ECk_OnlineAchievements_Operation::Reset;
                }
                if (Completion.Result == 1 && Completion.Operation == EOperation::Store)
                {
                    StagedStats.Empty();
                    StagedAchievements.Empty();
                }
                Owner->OnStatsStored.Broadcast(static_cast<int64>(Completion.GameId), Completion.Result, Completion.Id, Kind);
            });
        }
    };

    FSteamStatsReceiver* Receiver = nullptr;
    TUniquePtr<FSteamStatsBridge> Bridge;
    bool ShuttingDown = false;

    auto GetStats(UCk_OnlineAchievements_Subsystem_UE* InOwner) -> ISteamUserStats*
    {
        if (!IsInGameThread() || ShuttingDown || FApp::IsUnattended() || IsRunningDedicatedServer())
        {
            return nullptr;
        }
        auto* World = InOwner ? InOwner->GetWorld() : nullptr;
        if (!World || World->WorldType != EWorldType::Game)
        {
            return nullptr;
        }
        auto* Module = FModuleManager::GetModulePtr<FOnlineSubsystemModule>(TEXT("OnlineSubsystem"));
        const FName SteamName(TEXT("Steam"));
        if (!Module || !Module->DoesInstanceExist(SteamName))
        {
            return nullptr;
        }
        auto* Oss = Module->GetOnlineSubsystem(SteamName);
        const auto Identity = Oss ? Oss->GetIdentityInterface() : nullptr;
        if (!Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn ||
            !FSteamSharedModule::IsAvailable() || !FSteamSharedModule::Get().AreSteamDllsLoaded())
        {
            return nullptr;
        }
        if (!SteamUser() || !SteamUtils() || !SteamUserStats())
        {
            return nullptr;
        }
        if (!Bridge)
        {
            auto Instance = FSteamSharedModule::Get().ObtainSteamClientInstanceHandle();
            if (!Instance.IsValid() || !Instance->IsInitialized())
            {
                return nullptr;
            }
            Receiver = new FSteamStatsReceiver();
            Bridge = MakeUnique<FSteamStatsBridge>(MoveTemp(Instance), Receiver->Ingress);
        }
        if (!Bridge->Owner.IsValid())
        {
            Bridge->Operations.Detach();
        }
        const auto SameProvider = Bridge->Provider == Oss && Bridge->ProviderIdentity.IsValid() &&
            Bridge->ProviderIdentity.Pin() == Identity;
        const auto ProviderEpoch = SameProvider ? Bridge->ProviderEpoch : Bridge->ProviderEpoch + 1;
        const FIdentity NativeIdentity{static_cast<uint64>(InOwner->GetUniqueID()),
            SteamUser()->GetSteamID().ConvertToUint64(), ProviderEpoch, SteamUtils()->GetAppID()};
        const auto PreviousOwner = Bridge->Owner;
        if (!Bridge->Operations.Acquire(NativeIdentity))
        {
            return nullptr;
        }
        if (PreviousOwner.Get() != InOwner)
        {
            Bridge->StagedStats.Empty();
            Bridge->StagedAchievements.Empty();
        }
        Bridge->Owner = InOwner;
        Bridge->Provider = Oss;
        Bridge->ProviderIdentity = Identity;
        Bridge->ProviderEpoch = ProviderEpoch;
        return SteamUserStats();
    }
#endif

    auto IsValidApiName(const FString& InName) -> bool
    {
        if (InName.IsEmpty() || InName.Len() > 128)
        {
            return false;
        }
        for (const auto Character : InName)
        {
            if (Character == 0)
            {
                return false;
            }
        }
        return true;
    }
}

class FCkOnlineAchievementsModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override
    {
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
#if WITH_CK_ACHIEVEMENTS_STEAM
            if (Bridge)
            {
                Bridge->Tick();
            }
#endif
            return true;
        }));
    }

    auto ShutdownModule() -> void override
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
#if WITH_CK_ACHIEVEMENTS_STEAM
        ShuttingDown = true;
        if (Receiver)
        {
            Receiver->Unregister();
        }
        // No callback touches Bridge or its shared Steam handle. The receiver/queue remain
        // valid for dispatched calls; normal engine shutdown keeps module code mapped.
        Bridge.Reset();
#endif
    }

    auto SupportsDynamicReloading() -> bool override { return false; }

private:
    FTSTicker::FDelegateHandle TickHandle;
};

IMPLEMENT_MODULE(FCkOnlineAchievementsModule, CkOnlineAchievements)

auto UCk_OnlineAchievements_Subsystem_UE::Deinitialize() -> void
{
    OnStatsStored.Clear();
#if WITH_CK_ACHIEVEMENTS_STEAM
    if (Bridge && Bridge->Owner.Get() == this)
    {
        Bridge->Operations.Detach();
        Bridge->Owner.Reset();
    }
#endif
    Super::Deinitialize();
}

auto UCk_OnlineAchievements_Subsystem_UE::Get_IsAvailable() -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    return GetStats(this) != nullptr;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::Get_IsReady() -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    const auto* Name = Stats ? Stats->GetAchievementName(0) : nullptr;
    return Name && *Name;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::TryGetAchievement(const FString& InApiName, bool& OutUnlocked) -> bool
{
    OutUnlocked = false;
    if (!IsValidApiName(InApiName))
    {
        return false;
    }
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    bool Value = false;
    if (!Stats || !Stats->GetAchievement(TCHAR_TO_UTF8(*InApiName), &Value))
    {
        return false;
    }
    OutUnlocked = Value;
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::TryGetStatInt(const FString& InApiName, int32& OutValue) -> bool
{
    OutValue = 0;
    if (!IsValidApiName(InApiName))
    {
        return false;
    }
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    int32 Value = 0;
    if (!Stats || !Stats->GetStat(TCHAR_TO_UTF8(*InApiName), &Value))
    {
        return false;
    }
    OutValue = Value;
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::SetStatInt(const FString& InApiName, int32 InValue) -> bool
{
    if (!IsValidApiName(InApiName))
    {
        return false;
    }
#if WITH_CK_ACHIEVEMENTS_STEAM
    if (!GetStats(this) || !Bridge->Operations.CanMutate(Bridge->Operations.GetIdentity()))
    {
        return false;
    }
    Bridge->StagedStats.Add(InApiName, InValue);
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::SetAchievement(const FString& InApiName) -> bool
{
    if (!IsValidApiName(InApiName))
    {
        return false;
    }
#if WITH_CK_ACHIEVEMENTS_STEAM
    if (!GetStats(this) || !Bridge->Operations.CanMutate(Bridge->Operations.GetIdentity()))
    {
        return false;
    }
    Bridge->StagedAchievements.Add(InApiName);
    return true;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::IndicateAchievementProgress(const FString& InApiName, int32 InCurrent, int32 InMaximum) -> bool
{
    if (!IsValidApiName(InApiName) || InCurrent <= 0 || InCurrent >= InMaximum)
    {
        return false;
    }
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    if (!Stats || Bridge->StagedStats.Num() != 0 || Bridge->StagedAchievements.Num() != 0)
    {
        return false;
    }
    return Bridge->Operations.TryBegin(EOperation::Progress, Bridge->Operations.GetIdentity(), FPlatformTime::Seconds(), [&]
    {
        return Stats->IndicateAchievementProgress(TCHAR_TO_UTF8(*InApiName), static_cast<uint32>(InCurrent), static_cast<uint32>(InMaximum));
    });
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::ResetAllStats(bool InAchievementsToo) -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    if (!Stats)
    {
        return false;
    }
    const auto Queued = Bridge->Operations.TryBegin(EOperation::Reset, Bridge->Operations.GetIdentity(), FPlatformTime::Seconds(), [&]
    {
        return Stats->ResetAllStats(InAchievementsToo);
    });
    if (Queued)
    {
        Bridge->StagedStats.Empty();
        if (InAchievementsToo)
        {
            Bridge->StagedAchievements.Empty();
        }
    }
    return Queued;
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::StoreStats() -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    auto* Stats = GetStats(this);
    if (!Stats)
    {
        return false;
    }
    return Bridge->Operations.TryBegin(EOperation::Store, Bridge->Operations.GetIdentity(), FPlatformTime::Seconds(), [&]
    {
        bool Accepted = true;
        for (const auto& Entry : Bridge->StagedStats)
        {
            if (!Stats->SetStat(TCHAR_TO_UTF8(*Entry.Key), Entry.Value))
            {
                Accepted = false;
            }
        }
        for (const auto& Name : Bridge->StagedAchievements)
        {
            if (!Stats->SetAchievement(TCHAR_TO_UTF8(*Name)))
            {
                Accepted = false;
            }
        }
        return Accepted && Stats->StoreStats();
    });
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::Get_IsStorePending() const -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    return IsInGameThread() && Bridge && Bridge->Operations.IsPending();
#else
    return false;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::Get_StoreId() const -> int64
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    return IsInGameThread() && Bridge && Bridge->Owner.Get() == this ? Bridge->Operations.GetId() : 0;
#else
    return 0;
#endif
}

auto UCk_OnlineAchievements_Subsystem_UE::Get_IsStoreUnconfirmed() const -> bool
{
#if WITH_CK_ACHIEVEMENTS_STEAM
    return IsInGameThread() && Bridge && Bridge->Operations.IsUnconfirmed();
#else
    return false;
#endif
}
