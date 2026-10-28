#pragma once

#include "CkCore/Time/CkTime.h"
#include <Kismet/BlueprintFunctionLibrary.h>
#include "CkPacing_Utils.generated.h"

namespace ck { class FSharedPacedWork; }

DECLARE_DYNAMIC_DELEGATE_RetVal(bool, FCk_Delegate_Pacing_Discover);
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FCk_Delegate_Pacing_Step, int64, InWorkId);

// Session-only shared ownership: copying this value aliases the queue and frame allowance.
// No delegates or gameplay objects are retained by this handle.
USTRUCT(BlueprintType)
struct CKECS_API FCk_Pacing_SharedWork
{
    GENERATED_BODY()

private:
    TSharedPtr<ck::FSharedPacedWork> _State;
    friend class UCk_Utils_Pacing_UE;
};

USTRUCT(BlueprintType)
struct CKECS_API FCk_Pacing_Report
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ck|Pacing")
    bool Pumped = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ck|Pacing")
    int32 Steps = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ck|Pacing")
    int32 DiscoverySteps = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ck|Pacing")
    FCk_Time Elapsed;
};

UCLASS(NotBlueprintable)
class CKECS_API UCk_Utils_Pacing_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Invalid (nonpositive/nonfinite) configuration returns an invalid handle.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Pacing")
    static FCk_Pacing_SharedWork Create_SharedWork(int32 InMaxSteps, FCk_Time InTimeBudget);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|Pacing")
    static bool IsValid_SharedWork(const FCk_Pacing_SharedWork& InWork);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Pacing")
    static bool Enqueue_SharedWork(const FCk_Pacing_SharedWork& InWork, int64 InWorkId);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Pacing")
    static bool Cancel_SharedWork(const FCk_Pacing_SharedWork& InWork, int64 InWorkId);

    // Clear invalidates queued generations but cannot replenish this frame's allowance.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Pacing")
    static void Clear_SharedWork(const FCk_Pacing_SharedWork& InWork);

    // Synchronous, game-thread only. Discover executes one bounded discovery unit and returns
    // whether more remain. Step executes one bounded unit for its ID and returns true when done.
    // Discovery and execution alternate; one indivisible callback may overrun the soft time limit.
    // Both delegates must stay bound. Neither is retained after return. At most one pump per frame.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Pacing")
    static FCk_Pacing_Report Pump_SharedWork(
        const FCk_Pacing_SharedWork& InWork,
        FCk_Delegate_Pacing_Discover InDiscover,
        FCk_Delegate_Pacing_Step InStep);
};
