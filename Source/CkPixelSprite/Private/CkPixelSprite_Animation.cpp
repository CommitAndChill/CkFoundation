#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkPixelSprite_Decimal.h"

#include <cmath>

struct FCk_PixelSprite_PreparedAnimation
{
    ECk_PixelSprite_Playback _Playback = ECk_PixelSprite_Playback::Loop;
    int32 _FrameCount = 0;
    TArray<int32> _VisitFrameIndices;
    TArray<ck::pixel_sprite::decimal_private::FDecimal> _CumulativeEnds;
    TArray<double> _FrameSeconds;
    ck::pixel_sprite::decimal_private::FDecimal _Cycle;
    double _CycleSeconds = 0.0;
};

namespace ck::pixel_sprite::animation_private
{
    auto
        Evaluate(
            const FCk_PixelSprite_PreparedAnimation& InPrepared,
            const decimal_private::FDecimal& InTime,
            FCk_PixelSprite_FrameSelection& OutSelection)
        -> bool
    {
        using namespace decimal_private;
        auto Selection = FCk_PixelSprite_FrameSelection{};
        Selection._CycleDuration = FCk_Time{InPrepared._CycleSeconds};
        auto Magnitude = InTime;
        Magnitude._Negative = false;
        auto Position = FDecimal{};
        if (InPrepared._Playback == ECk_PixelSprite_Playback::Once)
        {
            if (NOT InTime._Negative && CompareMagnitude(Magnitude, InPrepared._Cycle) >= 0)
            {
                Selection._Complete = true;
                Selection._Index = InPrepared._FrameCount - 1;
                Selection._FrameTime = FCk_Time{InPrepared._FrameSeconds.Last()};
                OutSelection = Selection;
                return true;
            }
            if (NOT InTime._Negative)
            {
                Position = MoveTemp(Magnitude);
            }
        }
        else
        {
            if (NOT TryPositiveModulo(Magnitude, InPrepared._Cycle, Position))
            {
                return false;
            }
            if (InTime._Negative && NOT IsZero(Position))
            {
                auto Wrapped = FDecimal{};
                if (NOT TrySubtractPositive(InPrepared._Cycle, Position, Wrapped))
                {
                    return false;
                }
                Position = MoveTemp(Wrapped);
            }
        }

        auto Low = int32{0};
        auto High = InPrepared._CumulativeEnds.Num();
        while (Low < High)
        {
            const auto Mid = Low + (High - Low) / 2;
            if (CompareMagnitude(Position, InPrepared._CumulativeEnds[Mid]) < 0)
            {
                High = Mid;
            }
            else
            {
                Low = Mid + 1;
            }
        }
        if (NOT InPrepared._VisitFrameIndices.IsValidIndex(Low))
        {
            return false;
        }

        const auto Index = InPrepared._VisitFrameIndices[Low];
        const auto Start = Low == 0 ? FDecimal{} : InPrepared._CumulativeEnds[Low - 1];
        auto Local = FDecimal{};
        auto LocalSeconds = double{0.0};
        if (NOT TrySubtractPositive(Position, Start, Local) || NOT TryToSeconds(Local, LocalSeconds))
        {
            return false;
        }
        const auto FrameSeconds = InPrepared._FrameSeconds[Index];
        if (LocalSeconds >= FrameSeconds)
        {
            LocalSeconds = std::nextafter(FrameSeconds, 0.0);
        }
        Selection._Index = Index;
        Selection._FrameTime = FCk_Time{LocalSeconds};
        OutSelection = Selection;
        return true;
    }
}

auto
    ck::pixel_sprite::TryPrepare_Animation(
        const FCk_PixelSprite_Animation& InAnimation,
        TSharedPtr<const FCk_PixelSprite_PreparedAnimation>& OutPrepared) -> bool
{
    using namespace decimal_private;
    const auto Count = InAnimation._Frames.Num();
    if (Count == 0 || Count > 4096 ||
        (InAnimation._Playback != ECk_PixelSprite_Playback::Loop &&
         InAnimation._Playback != ECk_PixelSprite_Playback::Once &&
         InAnimation._Playback != ECk_PixelSprite_Playback::PingPong))
    {
        return false;
    }

    auto Durations = TArray<FDecimal>{};
    Durations.Reserve(Count);
    auto Candidate = MakeShared<FCk_PixelSprite_PreparedAnimation>();
    Candidate->_Playback = InAnimation._Playback;
    Candidate->_FrameCount = Count;
    Candidate->_FrameSeconds.Reserve(Count);
    for (const auto& Frame : InAnimation._Frames)
    {
        const auto FrameSeconds = Frame.Get_Duration().Get_Seconds();
        if (NOT FMath::IsFinite(Frame._DurationMilliseconds) ||
            Frame._DurationMilliseconds <= 0.0 || Frame._DurationMilliseconds > 86400000.0 ||
            FrameSeconds <= 0.0)
        {
            return false;
        }
        auto Duration = FDecimal{};
        if (NOT TryFromDouble(Frame._DurationMilliseconds, 0, Duration))
        {
            return false;
        }
        Durations.Add(MoveTemp(Duration));
        Candidate->_FrameSeconds.Add(FrameSeconds);
    }

    const auto Visits = InAnimation._Playback == ECk_PixelSprite_Playback::PingPong && Count > 1
        ? Count * 2 - 2 : Count;
    Candidate->_VisitFrameIndices.Reserve(Visits);
    Candidate->_CumulativeEnds.Reserve(Visits);
    auto Cycle = FDecimal{};
    for (auto Visit = int32{0}; Visit < Visits; ++Visit)
    {
        const auto FrameIndex = Visit < Count ? Visit : Count * 2 - 2 - Visit;
        auto Next = FDecimal{};
        if (NOT TryAddPositive(Cycle, Durations[FrameIndex], Next))
        {
            return false;
        }
        Candidate->_VisitFrameIndices.Add(FrameIndex);
        Candidate->_CumulativeEnds.Add(Next);
        Cycle = MoveTemp(Next);
    }
    auto CycleSeconds = double{0.0};
    if (NOT TryToSeconds(Cycle, CycleSeconds) || CycleSeconds <= 0.0)
    {
        return false;
    }
    Candidate->_Cycle = MoveTemp(Cycle);
    Candidate->_CycleSeconds = CycleSeconds;
    OutPrepared = Candidate;
    return true;
}

auto
    ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        double InMilliseconds,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool
{
    auto Time = decimal_private::FDecimal{};
    if (NOT decimal_private::TryFromDouble(InMilliseconds, 0, Time))
    {
        return false;
    }
    return animation_private::Evaluate(InPrepared, Time, OutSelection);
}

auto
    ck::pixel_sprite::TryGet_PreparedFrameStartMilliseconds(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        int32 InAnimationFrameIndex,
        double& OutMilliseconds) -> bool
{
    if (InAnimationFrameIndex < 0 || InAnimationFrameIndex >= InPrepared._FrameCount)
    {
        return false;
    }
    if (InAnimationFrameIndex == 0)
    {
        OutMilliseconds = 0.0;
        return true;
    }

    // TryToSeconds divides a decimal millisecond value by 1000. Shifting its
    // exponent first converts this prefix directly to milliseconds as a double.
    auto Milliseconds = InPrepared._CumulativeEnds[InAnimationFrameIndex - 1];
    if (Milliseconds._Exponent > MAX_int32 - 3)
    {
        return false;
    }
    Milliseconds._Exponent += 3;
    auto Candidate = double{0.0};
    if (NOT decimal_private::TryToSeconds(Milliseconds, Candidate))
    {
        return false;
    }
    OutMilliseconds = Candidate;
    return true;
}

auto
    ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation,
        double InMilliseconds,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool
{
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    return TryPrepare_Animation(InAnimation, Prepared) &&
        TryEvaluate_PreparedAnimationMilliseconds(*Prepared, InMilliseconds, OutSelection);
}

auto
    ck::pixel_sprite::TryEvaluate_Animation(
        const FCk_PixelSprite_Animation& InAnimation,
        FCk_Time InTime,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool
{
    auto Time = decimal_private::FDecimal{};
    if (NOT decimal_private::TryFromDouble(InTime.Get_Seconds(), 3, Time))
    {
        return false;
    }
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    return TryPrepare_Animation(InAnimation, Prepared) &&
        animation_private::Evaluate(*Prepared, Time, OutSelection);
}
