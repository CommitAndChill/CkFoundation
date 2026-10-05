#include "CkOnlineDirect_ListenTarget.h"

#include "CkOnline/Direct/CkOnlineDirect_Transport.h"

#include "Engine/EngineBaseTypes.h"
#include "Misc/PackageName.h"

auto
    FCk_OnlineDirect_ListenTarget::
    TryBuild(
        const TSoftObjectPtr<UWorld>& InMap,
        TSubclassOf<AGameModeBase> InGameMode,
        const FString& InHostAddress,
        int32 InPort,
        FCk_OnlineDirect_ListenTarget& OutTarget)
    -> bool
{
    if (NOT IsInGameThread() || InHostAddress.Len() > 15)
    {
        OutTarget = {};
        return false;
    }
    const auto HostAddress = InHostAddress;
    OutTarget = {};
    if (InPort < 1 || InPort > 65535 || NOT ck::direct::FTransport::ValidateHostAddress(HostAddress))
    { return false; }

    const auto& MapPath = InMap.ToSoftObjectPath();
    constexpr auto MaxPathLength = 512;
    if (MapPath.IsNull() || NOT MapPath.GetSubPathUtf8String().IsEmpty() ||
        MapPath.GetAssetPath().GetPackageName().GetStringLength() > MaxPathLength ||
        MapPath.GetAssetPath().GetAssetName().GetStringLength() > MaxPathLength)
    { return false; }
    const auto MapObjectPath = MapPath.ToString();
    const auto MapPackage = MapPath.GetLongPackageName();
    const auto MapAsset = MapPath.GetAssetName();
    if (MapObjectPath.Len() > MaxPathLength || NOT FPackageName::IsValidLongPackageName(MapPackage) ||
        NOT FPackageName::IsValidObjectPath(MapObjectPath) || MapAsset.IsEmpty() ||
        MapAsset != FPackageName::GetShortName(MapPackage) ||
        MapObjectPath != MapPackage + TEXT(".") + MapAsset)
    { return false; }

    const auto* GameMode = InGameMode.GetGCPtr().Get();
    if (NOT IsValid(GameMode))
    { return false; }
    if (NOT GameMode->IsChildOf(AGameModeBase::StaticClass()) ||
        GameMode->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    { return false; }
    const auto GameModePath = GameMode->GetPathName();
    if (GameModePath.Len() > MaxPathLength || NOT FPackageName::IsValidObjectPath(GameModePath))
    { return false; }

    const auto GameOption = FString::Printf(TEXT("game=%s"), *GameModePath);
    auto ListenUrl = FURL{nullptr, *MapPackage, TRAVEL_Absolute};
    ListenUrl.Host.Empty();
    ListenUrl.Portal.Empty();
    ListenUrl.RedirectURL.Empty();
    ListenUrl.Port = InPort;
    ListenUrl.AddOption(*GameOption);
    ListenUrl.AddOption(TEXT("listen"));
    const auto TravelUrl = ListenUrl.ToString();
    const auto RoundTrip = FURL{nullptr, *TravelUrl, TRAVEL_Absolute};
    const auto ExactListenIntent = [&MapPackage, InPort, &GameOption](const FURL& InUrl)
    {
        return InUrl.Valid && InUrl.IsLocalInternal() && InUrl.Map == MapPackage && InUrl.Port == InPort &&
            InUrl.Portal.IsEmpty() && InUrl.RedirectURL.IsEmpty() && InUrl.Op.Num() == 2 &&
            InUrl.Op[0] == GameOption && InUrl.Op[1] == TEXT("listen");
    };
    if (NOT ExactListenIntent(ListenUrl) || NOT ExactListenIntent(RoundTrip) ||
        RoundTrip.ToString() != TravelUrl)
    { return false; }

    auto Candidate = FCk_OnlineDirect_ListenTarget{};
    Candidate._TravelUrl = TravelUrl;
    Candidate._GameplayEndpoint = FString::Printf(TEXT("%s:%d"), *HostAddress, InPort);
    OutTarget = MoveTemp(Candidate);
    return true;
}
