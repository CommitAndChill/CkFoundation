#include "CkPixelSprite/CkPixelSprite_Data.h"

#include <cmath>

auto
    ck::pixel_sprite::TryEvaluate_Frame(
        const FCk_PixelSprite_Timing& InTiming,
        FCk_Time InTime,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool
{
    const auto Count = InTiming._Durations.Num();
    const auto Seconds = InTime.Get_Seconds();
    if (Count == 0 || Count > 4096 || NOT FMath::IsFinite(Seconds))
    {
        return false;
    }
    auto Duration = 0.0;
    for (auto Index = 0; Index < Count; ++Index)
    {
        const auto Value = InTiming._Durations[Index].Get_Seconds();
        if (NOT FMath::IsFinite(Value) || Value <= 0.0 || Value > 86400.0)
        {
            return false;
        }
    }
    if (InTiming._Playback != ECk_PixelSprite_Playback::Loop && InTiming._Playback != ECk_PixelSprite_Playback::Once &&
        InTiming._Playback != ECk_PixelSprite_Playback::PingPong)
    {
        return false;
    }
    const auto Visits = InTiming._Playback == ECk_PixelSprite_Playback::PingPong && Count > 1 ? Count * 2 - 2 : Count;
    for (auto Visit = 0; Visit < Visits; ++Visit)
    {
        const auto Index = Visit < Count ? Visit : Count * 2 - 2 - Visit;
        Duration += InTiming._Durations[Index].Get_Seconds();
    }
    auto Selection = FCk_PixelSprite_FrameSelection{};
    Selection._CycleDuration = FCk_Time{Duration};
    auto Position = Seconds;
    if (InTiming._Playback == ECk_PixelSprite_Playback::Once)
    {
        Selection._Complete = Seconds >= Duration;
        if (Selection._Complete)
        {
            Selection._Index = Count - 1;
            Selection._FrameTime = InTiming._Durations.Last();
            OutSelection = Selection;
            return true;
        }
        Position = FMath::Max(0.0, Seconds);
    }
    else
    {
        Position = std::fmod(Seconds, Duration);
        if (Position < 0.0)
        {
            Position += Duration;
        }
        if (Position >= Duration)
        {
            Position = std::nextafter(Duration, 0.0);
        }
    }
    auto Start = 0.0;
    for (auto Visit = 0; Visit < Visits; ++Visit)
    {
        const auto Index = Visit < Count ? Visit : Count * 2 - 2 - Visit;
        const auto FrameDuration = InTiming._Durations[Index].Get_Seconds();
        const auto End = Start + FrameDuration;
        if (Position < End || Visit == Visits - 1)
        {
            Selection._Index = Index;
            Selection._FrameTime = FCk_Time{FMath::Max(0.0, Position - Start)};
            OutSelection = Selection;
            return true;
        }
        Start = End;
    }
    return false;
}

auto
    ck::pixel_sprite::TrySample_Time(
        FCk_Time InTime,
        FCk_Time InInterval,
        FCk_Time& OutTime) -> bool
{
    const auto Seconds = InTime.Get_Seconds();
    const auto Interval = InInterval.Get_Seconds();
    if (NOT FMath::IsFinite(Seconds) || NOT FMath::IsFinite(Interval) || Interval <= 0.0)
    {
        return false;
    }
    const auto Quotient = Seconds / Interval;
    if (NOT FMath::IsFinite(Quotient))
    {
        return false;
    }
    const auto Sample = std::floor(Quotient) * Interval;
    if (NOT FMath::IsFinite(Sample))
    {
        return false;
    }
    OutTime = FCk_Time{Sample};
    return true;
}
