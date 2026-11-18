#pragma once

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "CkOnlineAchievements_Subsystem.generated.h"

// Native Steam EResult values are preserved; zero means no callback, -1 means unconfirmed timeout.
UENUM(BlueprintType)
enum class ECk_OnlineAchievements_Operation : uint8 { Store, Progress, Reset };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FCk_OnlineAchievements_OnStored,
    int64, InGameId, int32, InResult, int64, InStoreId, ECk_OnlineAchievements_Operation, InOperation);

/** Local-account Steam statistics. The consuming project owns Steam initialization and callback pumping. */
UCLASS(NotBlueprintable, BlueprintType, DisplayName="CkSubsystem_OnlineAchievements")
class CKONLINEACHIEVEMENTS_API UCk_OnlineAchievements_Subsystem_UE : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    auto Deinitialize() -> void override;

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool Get_IsAvailable();

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool Get_IsReady();

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool TryGetAchievement(const FString& InApiName, bool& OutUnlocked);

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool TryGetStatInt(const FString& InApiName, int32& OutValue);

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool SetStatInt(const FString& InApiName, int32 InValue);

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool SetAchievement(const FString& InApiName);

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool IndicateAchievementProgress(const FString& InApiName, int32 InCurrent, int32 InMaximum);

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool ResetAllStats(bool InAchievementsToo);

    /** True means queued. Only OnStatsStored with native result 1 confirms persistence. */
    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool StoreStats();

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool Get_IsStorePending() const;

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    bool Get_IsStoreUnconfirmed() const;

    UFUNCTION(BlueprintCallable, Category="Ck|Online|Achievements")
    int64 Get_StoreId() const;

    UPROPERTY(BlueprintAssignable, Category="Ck|Online|Achievements")
    FCk_OnlineAchievements_OnStored OnStatsStored;
};
