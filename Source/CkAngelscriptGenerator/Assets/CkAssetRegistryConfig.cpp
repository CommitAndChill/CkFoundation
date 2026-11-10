#include "CkAssetRegistryConfig.h"
#include "CkCore/Format/CkFormat.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkAssetRegistryConfig::
    GetDisplayName() const
    -> FString
{
    auto AssetName = GetName();
    const auto Separator = AssetDiscoveryRoot.EndsWith(TEXT("/")) ? TEXT("") : TEXT("/");
    return ck::Format_UE(TEXT("{} ({}{}{} [{}])"), AssetName, AssetDiscoveryRoot, Separator, OutputFileName, Namespace);
}

// --------------------------------------------------------------------------------------------------------------------
