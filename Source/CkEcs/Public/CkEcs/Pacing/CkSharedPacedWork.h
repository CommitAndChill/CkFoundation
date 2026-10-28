#pragma once

#include "CkCore/Time/CkTime.h"

#include "Containers/Array.h"
#include "Templates/Function.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKECS_API FSharedFrameBudget
    {
    public:
        FSharedFrameBudget() = default;
        FSharedFrameBudget(int32 InMaxSteps, FCk_Time InMaxTime, TFunction<double()> InClock = {});

        auto Get_IsValid() const -> bool;
        auto BeginFrame(int64 InFrame) -> bool;
        auto BeginPump() -> bool;
        auto TryConsumeStep() -> bool;
        auto EndPump() -> FCk_Time;
        auto Get_StepsConsumed() const -> int32;
        auto Get_Elapsed() const -> FCk_Time;

    private:
        auto Do_UpdateElapsed() -> bool;

        TFunction<double()> _Clock;
        int32 _MaxSteps = 0;
        FCk_Time _MaxTime;
        int64 _Frame = -1;
        int32 _StepsConsumed = 0;
        FCk_Time _Elapsed;
        FCk_Time _PumpElapsed;
        double _PumpStart = 0.0;
        double _LastSample = 0.0;
        bool _Pumping = false;
        bool _ClockUsable = true;
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECS_API FSharedPacedWorkReport
    {
        bool Pumped = false;
        int32 DiscoverySteps = 0;
        int32 ExecutedSteps = 0;
        FCk_Time Elapsed;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Owns scheduling only. Consumers own work lifetimes and cancel or finish obsolete IDs.
    // Callbacks are synchronous; a step can overrun the time limit and is never interrupted.
    class CKECS_API FSharedPacedWork
    {
    public:
        FSharedPacedWork() = default;
        FSharedPacedWork(int32 InMaxSteps, FCk_Time InMaxTime, TFunction<double()> InClock = {});

        auto Get_IsValid() const -> bool;
        auto Get_NumPending() const -> int32;
        auto Get_Contains(int64 InWorkId) const -> bool;
        auto Enqueue(int64 InWorkId) -> bool;
        auto Cancel(int64 InWorkId) -> bool;
        auto Clear() -> void;

        // Discovery performs one bounded unit and returns whether more discovery remains.
        // Work performs one bounded unit and returns whether this ID has finished.
        auto Pump(
            int64 InFrame,
            const TFunction<bool()>& InDiscover,
            const TFunction<bool(int64)>& InWork,
            const TFunction<bool()>& InCanContinue = {})
            -> FSharedPacedWorkReport;

    private:
        struct FEntry
        {
            int64 Id = 0;
            uint64 Generation = 0;
        };

        auto Do_RemoveAt(int32 InIndex) -> void;

        FSharedFrameBudget _Budget;
        TArray<FEntry> _Queue;
        int32 _NextIndex = 0;
        int64 _LastPumpedFrame = -1;
        uint64 _Generation = 0;
        uint64 _Epoch = 0;
        bool _Pumping = false;
        bool _DiscoverNext = true;
    };
}

// --------------------------------------------------------------------------------------------------------------------
