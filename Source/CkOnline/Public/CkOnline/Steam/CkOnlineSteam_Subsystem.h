#pragma once

#include "CkOnline/Steam/CkOnlineSteam_Fragment_Data.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CkOnlineSteam_Subsystem.generated.h"

namespace ck::steam { class ITransport; }

/** Explicit, optional Steam profile facade. It never starts or falls back to Direct automatically. */
UCLASS(BlueprintType)
class CKONLINE_API UCk_OnlineSteam_Subsystem_UE : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    bool Request_Start(const FCk_OnlineSteam_Spec& InSpec);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    void Request_Stop();
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    bool Request(ECk_OnlineSteam_Action InAction, int32 InLobbyIndex, FGuid InSearchGeneration,
        double InTimeoutSeconds, FGuid& OutOperationId);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Steam")
    bool Request_Cancel(FGuid InTargetOperationId, FGuid& OutOperationId);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    FCk_Handle_OnlineSteam Get_Service() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    FCk_OnlineSteam_Snapshot Get_Snapshot() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    FCk_OnlineSteam_Completion Get_LastCompletion() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    int32 Get_CompletedCount() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Steam")
    bool Get_IsStarted() const;
private:
    FGuid _Generation;
    TSharedPtr<ck::steam::ITransport> _Transport;
};
