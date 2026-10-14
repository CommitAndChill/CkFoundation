#pragma once

#include "CkCore/Time/CkTime.h"

namespace ck::procedural_action
{
    // A sampled boundary's fraction is only a scheduler coordinate. Retain its original
    // authored time so a floating-point round trip cannot move a window edge across a receipt.
    inline auto
        ResolveClipSampleTime(
            FCk_Time InFrom,
            FCk_Time InRequested,
            FCk_Time InMaximum,
            const TArray<double>& InCriticalFractions,
            const TArray<FCk_Time>& InCriticalTimes,
            double InFraction,
            FCk_Time& OutTime)
        -> bool
    {
        OutTime = {};
        if (InCriticalFractions.Num() != InCriticalTimes.Num()
            || NOT FMath::IsFinite(InFraction) || InFraction <= 0.0 || InFraction > 1.0
            || NOT FMath::IsFinite(InFrom.Get_Seconds())
            || NOT FMath::IsFinite(InRequested.Get_Seconds())
            || NOT FMath::IsFinite(InMaximum.Get_Seconds())
            || InFrom >= InMaximum || InMaximum > InRequested)
        { return false; }
        for (auto Index = 0; Index < InCriticalFractions.Num(); ++Index)
        {
            if (NOT FMath::IsFinite(InCriticalFractions[Index])
                || InCriticalFractions[Index] <= 0.0 || InCriticalFractions[Index] > 1.0
                || NOT FMath::IsFinite(InCriticalTimes[Index].Get_Seconds())
                || InCriticalTimes[Index] <= InFrom || InCriticalTimes[Index] > InMaximum)
            { return false; }
            if (InFraction == InCriticalFractions[Index])
            {
                OutTime = InCriticalTimes[Index];
                return true;
            }
        }
        const auto Interpolated = InFrom.Get_Seconds()
            + (InRequested.Get_Seconds() - InFrom.Get_Seconds()) * InFraction;
        if (NOT FMath::IsFinite(Interpolated)) { return false; }
        OutTime = FCk_Time{FMath::Clamp(Interpolated, InFrom.Get_Seconds(), InMaximum.Get_Seconds())};
        return OutTime > InFrom;
    }
}
