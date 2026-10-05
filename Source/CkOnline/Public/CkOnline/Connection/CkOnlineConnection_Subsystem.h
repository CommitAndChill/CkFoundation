#pragma once

#include "CkOnline/Connection/CkOnlineConnection_Fragment_Data.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "CkOnlineConnection_Subsystem.generated.h"

namespace ck { class FOnlineServiceHost; }

/** Independent persistent connection owner; Direct profile stop cannot destroy recovery. */
UCLASS(BlueprintType)
class CKONLINE_API UCk_OnlineConnection_Subsystem_UE : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsTickable() const -> bool override;
    auto IsTickableWhenPaused() const -> bool override { return true; }
    auto GetStatId() const -> TStatId override;
    auto GetTickableGameObjectWorld() const -> UWorld* override;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    FCk_Handle_OnlineConnection Get_Service() const;
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Connection")
    bool TryRequest(const FCk_Request_OnlineConnection& InRequest, FCk_OnlineConnection_Operation& OutOperation);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    FCk_OnlineConnection_Snapshot Get_Snapshot() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    FCk_OnlineConnection_Completion Get_LastCompletion() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    int32 Get_CompletedCount() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Connection")
    bool Get_IsIdle() const;
private:
    TSharedPtr<ck::FOnlineServiceHost> _Host;
};
