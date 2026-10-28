#include "CkPacing_Utils.h"

#include "CkEcs/Pacing/CkSharedPacedWork.h"
#include <CoreGlobals.h>

auto
    UCk_Utils_Pacing_UE::Create_SharedWork(
        int32 InMaxSteps,
        FCk_Time InTimeBudget)
    -> FCk_Pacing_SharedWork
{
    auto Result = FCk_Pacing_SharedWork{};
    auto State = MakeShared<ck::FSharedPacedWork>(InMaxSteps, InTimeBudget);
    if (State->Get_IsValid())
    {
        Result._State = MoveTemp(State);
    }
    return Result;
}

auto
    UCk_Utils_Pacing_UE::IsValid_SharedWork(const FCk_Pacing_SharedWork& InWork)
    -> bool
{
    return InWork._State.IsValid() && InWork._State->Get_IsValid();
}

auto
    UCk_Utils_Pacing_UE::Enqueue_SharedWork(const FCk_Pacing_SharedWork& InWork, int64 InWorkId)
    -> bool
{
    return InWork._State.IsValid() && InWork._State->Enqueue(InWorkId);
}

auto
    UCk_Utils_Pacing_UE::Cancel_SharedWork(const FCk_Pacing_SharedWork& InWork, int64 InWorkId)
    -> bool
{
    return InWork._State.IsValid() && InWork._State->Cancel(InWorkId);
}

auto
    UCk_Utils_Pacing_UE::Clear_SharedWork(const FCk_Pacing_SharedWork& InWork)
    -> void
{
    if (InWork._State.IsValid())
    {
        InWork._State->Clear();
    }
}

auto
    UCk_Utils_Pacing_UE::Pump_SharedWork(
        const FCk_Pacing_SharedWork& InWork,
        FCk_Delegate_Pacing_Discover InDiscover,
        FCk_Delegate_Pacing_Step InStep)
    -> FCk_Pacing_Report
{
    // A callback may replace the caller's handle; retain the original state until return.
    const auto State = InWork._State;
    if (NOT State.IsValid() || NOT InDiscover.IsBound() || NOT InStep.IsBound())
    {
        return {};
    }
    const auto Native = State->Pump(static_cast<int64>(GFrameCounter),
        [&]() { return InDiscover.Execute(); },
        [&](int64 InWorkId) { return InStep.Execute(InWorkId); },
        [&]() { return InDiscover.IsBound() && InStep.IsBound(); });
    auto Result = FCk_Pacing_Report{};
    Result.Pumped = Native.Pumped;
    Result.Steps = Native.ExecutedSteps;
    Result.DiscoverySteps = Native.DiscoverySteps;
    Result.Elapsed = Native.Elapsed;
    return Result;
}
