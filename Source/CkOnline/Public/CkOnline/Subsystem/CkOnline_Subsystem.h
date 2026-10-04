#pragma once

#include "CkOnline/CkOnline_Host.h"

#include <Subsystems/GameInstanceSubsystem.h>
#include <Tickable.h>

#include "CkOnline_Subsystem.generated.h"

UCLASS(NotBlueprintable, BlueprintType, DisplayName = "CkSubsystem_Online")
class CKONLINE_API UCk_Online_Subsystem_UE : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    auto Deinitialize() -> void override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsTickable() const -> bool override;
    auto IsTickableWhenPaused() const -> bool override;
    auto GetStatId() const -> TStatId override;
    auto GetTickableGameObjectWorld() const -> UWorld* override;

    auto TryStart(TSharedRef<ck::IOnlineServiceAdapter> InAdapter) -> bool;
    auto TryStartFeature(TSharedRef<ck::IOnlineServiceFeature> InFeature) -> bool;

    UFUNCTION(BlueprintCallable, Category = "Ck|Online")
    void Request_Stop();

    UFUNCTION(BlueprintPure, Category = "Ck|Online")
    FCk_Handle_OnlineService Get_Service() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Online")
    FCk_OnlineService_Diagnostics Get_Diagnostics() const;

private:
    TSharedPtr<ck::FOnlineServiceHost> _Host;
};
