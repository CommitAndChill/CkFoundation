#include "CkEcs/Pacing/CkSharedPacedWork.h"

#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    FSharedFrameBudget::FSharedFrameBudget(
        int32 InMaxSteps,
        FCk_Time InMaxTime,
        TFunction<double()> InClock)
        : _Clock{MoveTemp(InClock)}
        , _MaxSteps{InMaxSteps}
        , _MaxTime{InMaxTime}
    {
        if (NOT _Clock)
        { _Clock = [] { return FPlatformTime::Seconds(); }; }
    }

    auto FSharedFrameBudget::Get_IsValid() const -> bool
    {
        return _MaxSteps > 0 && FMath::IsFinite(_MaxTime.Get_Seconds()) &&
            _MaxTime > FCk_Time::ZeroSecond() && bool{_Clock};
    }

    auto FSharedFrameBudget::BeginFrame(int64 InFrame) -> bool
    {
        if (NOT Get_IsValid() || _Pumping || InFrame < 0 || InFrame <= _Frame)
        { return false; }

        _Frame = InFrame;
        _StepsConsumed = 0;
        _Elapsed = FCk_Time{};
        _PumpElapsed = FCk_Time{};
        _ClockUsable = true;
        return true;
    }

    auto FSharedFrameBudget::BeginPump() -> bool
    {
        if (NOT Get_IsValid() || _Frame < 0 || _Pumping || NOT _ClockUsable)
        { return false; }

        const auto Now = _Clock();
        if (NOT FMath::IsFinite(Now))
        {
            _ClockUsable = false;
            return false;
        }

        _PumpStart = Now;
        _LastSample = Now;
        _PumpElapsed = FCk_Time{};
        _Pumping = true;
        return true;
    }

    auto FSharedFrameBudget::Do_UpdateElapsed() -> bool
    {
        const auto Now = _Clock();
        if (NOT FMath::IsFinite(Now) || Now < _LastSample)
        {
            _ClockUsable = false;
            return false;
        }

        _LastSample = Now;
        _PumpElapsed = FCk_Time{Now - _PumpStart};
        return true;
    }

    auto FSharedFrameBudget::TryConsumeStep() -> bool
    {
        if (NOT _Pumping || NOT _ClockUsable || NOT Do_UpdateElapsed())
        { return false; }

        if (_StepsConsumed >= _MaxSteps || _Elapsed + _PumpElapsed >= _MaxTime)
        { return false; }

        ++_StepsConsumed;
        return true;
    }

    auto FSharedFrameBudget::EndPump() -> FCk_Time
    {
        if (NOT _Pumping)
        { return {}; }

        if (_ClockUsable)
        { Do_UpdateElapsed(); }

        _Elapsed += _PumpElapsed;
        _Pumping = false;
        return _PumpElapsed;
    }

    auto FSharedFrameBudget::Get_StepsConsumed() const -> int32
    { return _StepsConsumed; }

    auto FSharedFrameBudget::Get_Elapsed() const -> FCk_Time
    { return _Elapsed + (_Pumping ? _PumpElapsed : FCk_Time{}); }

    // --------------------------------------------------------------------------------------------------------------------

    FSharedPacedWork::FSharedPacedWork(
        int32 InMaxSteps,
        FCk_Time InMaxTime,
        TFunction<double()> InClock)
        : _Budget{InMaxSteps, InMaxTime, MoveTemp(InClock)}
    {
    }

    auto FSharedPacedWork::Get_IsValid() const -> bool
    { return _Budget.Get_IsValid(); }

    auto FSharedPacedWork::Get_NumPending() const -> int32
    { return _Queue.Num(); }

    auto FSharedPacedWork::Get_Contains(int64 InWorkId) const -> bool
    {
        return _Queue.ContainsByPredicate([InWorkId](const FEntry& InEntry) { return InEntry.Id == InWorkId; });
    }

    auto FSharedPacedWork::Enqueue(int64 InWorkId) -> bool
    {
        if (NOT Get_IsValid() || Get_Contains(InWorkId))
        { return false; }

        _Queue.Add(FEntry{InWorkId, ++_Generation});
        return true;
    }

    auto FSharedPacedWork::Do_RemoveAt(int32 InIndex) -> void
    {
        _Queue.RemoveAt(InIndex);
        if (InIndex < _NextIndex)
        { --_NextIndex; }

        if (_NextIndex >= _Queue.Num())
        { _NextIndex = 0; }
    }

    auto FSharedPacedWork::Cancel(int64 InWorkId) -> bool
    {
        const auto Index = _Queue.IndexOfByPredicate(
            [InWorkId](const FEntry& InEntry) { return InEntry.Id == InWorkId; });
        if (Index == INDEX_NONE)
        { return false; }

        Do_RemoveAt(Index);
        return true;
    }

    auto FSharedPacedWork::Clear() -> void
    {
        _Queue.Reset();
        _NextIndex = 0;
        _DiscoverNext = true;
        ++_Epoch;
    }

    auto FSharedPacedWork::Pump(
        int64 InFrame,
        const TFunction<bool()>& InDiscover,
        const TFunction<bool(int64)>& InWork,
        const TFunction<bool()>& InCanContinue)
        -> FSharedPacedWorkReport
    {
        auto Report = FSharedPacedWorkReport{};
        if (_Pumping || NOT InWork || InFrame <= _LastPumpedFrame || NOT _Budget.BeginFrame(InFrame))
        { return Report; }

        if (NOT _Budget.BeginPump())
        { return Report; }

        _LastPumpedFrame = InFrame;
        _Pumping = true;
        Report.Pumped = true;
        const auto Epoch = _Epoch;
        auto DiscoveryRemaining = bool{InDiscover};

        while (_Epoch == Epoch && (DiscoveryRemaining || NOT _Queue.IsEmpty()))
        {
            if (InCanContinue && NOT InCanContinue())
            { break; }

            if (_Epoch != Epoch || (NOT DiscoveryRemaining && _Queue.IsEmpty()))
            { break; }

            if (NOT _Budget.TryConsumeStep())
            { break; }

            if (DiscoveryRemaining && (_DiscoverNext || _Queue.IsEmpty()))
            {
                _DiscoverNext = false;
                ++Report.DiscoverySteps;
                DiscoveryRemaining = InDiscover();
                continue;
            }

            const auto Entry = _Queue[_NextIndex];
            _NextIndex = (_NextIndex + 1) % _Queue.Num();
            _DiscoverNext = true;
            ++Report.ExecutedSteps;
            const auto Done = InWork(Entry.Id);
            if (_Epoch != Epoch)
            { break; }

            if (Done)
            {
                const auto Index = _Queue.IndexOfByPredicate([Entry](const FEntry& InEntry)
                { return InEntry.Id == Entry.Id && InEntry.Generation == Entry.Generation; });
                if (Index != INDEX_NONE)
                { Do_RemoveAt(Index); }
            }
        }

        Report.Elapsed = _Budget.EndPump();
        _Pumping = false;
        return Report;
    }
}

// --------------------------------------------------------------------------------------------------------------------
