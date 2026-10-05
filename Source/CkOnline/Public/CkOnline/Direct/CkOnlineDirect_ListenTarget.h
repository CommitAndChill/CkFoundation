#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"

#include "GameFramework/GameModeBase.h"

#include "CkOnlineDirect_ListenTarget.generated.h"

/** Validated caller-owned listen intent; no asset loading, travel or gameplay admission is performed. */
USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_ListenTarget
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_ListenTarget);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _TravelUrl;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _GameplayEndpoint;

public:
    CK_PROPERTY_GET(_TravelUrl);
    CK_PROPERTY_GET(_GameplayEndpoint);

    static auto TryBuild(
        const TSoftObjectPtr<UWorld>& InMap,
        TSubclassOf<AGameModeBase> InGameMode,
        const FString& InHostAddress,
        int32 InPort,
        FCk_OnlineDirect_ListenTarget& OutTarget) -> bool;
};
