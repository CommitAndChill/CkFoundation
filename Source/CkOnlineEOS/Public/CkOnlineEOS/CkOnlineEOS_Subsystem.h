#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CkOnlineEOS_Subsystem.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineEOS_LoginComplete, bool, InSuccess, FString, InError);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineEOS_SessionComplete, bool, InSuccess, FName, InSessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_Delegate_OnlineEOS_LoginStatusChanged, bool, InLoggedIn, FString, InError);

struct FCk_OnlineEOS_State;

UCLASS(DisplayName = "CkSubsystem_OnlineEOS")
class CKONLINEEOS_API UCk_OnlineEOS_Subsystem_UE : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    auto Initialize(FSubsystemCollectionBase& Collection) -> void override;
    auto Deinitialize() -> void override;

    UFUNCTION(BlueprintCallable, Category = "Ck|Online|EOS")
    void Request_Login(bool InUseDeviceId, FCk_Delegate_OnlineEOS_LoginComplete InCallback);

    UFUNCTION(BlueprintPure, Category = "Ck|Online|EOS")
    bool Get_IsSteamAvailable() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Online|EOS")
    bool Get_IsLoggedIn() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Online|EOS")
    FString Get_DisplayName() const;

    UFUNCTION(BlueprintCallable, Category = "Ck|Online|EOS")
    void Request_CreateSession(FName InSessionName, int32 InPublicConnections, FCk_Delegate_OnlineEOS_SessionComplete InCallback);

    // Also cancels an in-flight create. The name stays reserved until native cleanup finishes.
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|EOS")
    void Request_DestroySession(FName InSessionName, FCk_Delegate_OnlineEOS_SessionComplete InCallback);

    UFUNCTION(BlueprintPure, Category = "Ck|Online|EOS")
    FString Get_ResolvedConnectString(FName InSessionName) const;

    UFUNCTION(BlueprintPure, Category = "Ck|Online|EOS")
    bool Get_IsValidConnectString(const FString& InAddress) const;

    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|EOS")
    FCk_Delegate_OnlineEOS_LoginStatusChanged OnLoginStatusChanged;

private:
    TSharedPtr<FCk_OnlineEOS_State> _State;
};
