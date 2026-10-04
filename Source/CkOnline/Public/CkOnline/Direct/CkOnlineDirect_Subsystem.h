#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CkOnlineDirect_Subsystem.generated.h"

/** Typed facade over the persistent native service; owns no Actor or secondary registry. */
UCLASS(BlueprintType)
class CKONLINE_API UCk_OnlineDirect_Subsystem_UE : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    bool Request_Start(const FCk_OnlineDirect_Spec& InSpec);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    void Request_Stop();
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Direct")
    bool Request(ECk_OnlineDirect_Action InAction, const FString& InText, bool InReady);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    bool Get_IsStarted() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    FCk_Handle_OnlineDirect Get_Service() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    FCk_OnlineDirect_Snapshot Get_Snapshot() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    FCk_OnlineDirect_Completion Get_LastCompletion() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Direct")
    int32 Get_CompletedCount() const;
private:
    FGuid _Generation;
};
