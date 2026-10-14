#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "CkTween/CkTween_Easing_Utils.h"

namespace ck::procedural_action
{
    namespace progress_detail
    {
        constexpr auto MaxKeys = 64;
        constexpr auto InverseIterations = 48;

        auto
        ToTween(
            ECk_ProceduralAction_Easing InEasing)
            -> ECk_TweenEasing
        {
            switch (InEasing)
            {
            case ECk_ProceduralAction_Easing::InQuad: return ECk_TweenEasing::InQuad;
            case ECk_ProceduralAction_Easing::OutQuad: return ECk_TweenEasing::OutQuad;
            case ECk_ProceduralAction_Easing::InOutQuad: return ECk_TweenEasing::InOutQuad;
            case ECk_ProceduralAction_Easing::InCubic: return ECk_TweenEasing::InCubic;
            case ECk_ProceduralAction_Easing::OutCubic: return ECk_TweenEasing::OutCubic;
            case ECk_ProceduralAction_Easing::InOutCubic: return ECk_TweenEasing::InOutCubic;
            default: return ECk_TweenEasing::Linear;
            }
        }

        auto
        EvaluateCurve(
            const FCompiledProgressCurve& InCurve,
            double InRaw)
            -> double
        {
            const auto& Times = InCurve.Get_Times();
            const auto& Values = InCurve.Get_Values();
            const auto& Tangents = InCurve.Get_Tangents();
            if (InRaw <= Times[0])
            { return Values[0]; }
            if (InRaw >= Times.Last())
            { return Values.Last(); }
            auto Segment = 0;
            while (Segment + 2 < Times.Num() && InRaw >= Times[Segment + 1])
            { ++Segment; }
            const auto Span = Times[Segment + 1] - Times[Segment];
            const auto S = (InRaw - Times[Segment]) / Span;
            const auto S2 = S * S;
            const auto S3 = S2 * S;
            const auto H01 = -2.0 * S3 + 3.0 * S2;
            const auto H10 = S3 - 2.0 * S2 + S;
            const auto H11 = S3 - S2;
            // Written as an offset from the segment start so a plateau (equal values, zero tangents) is held exactly.
            return Values[Segment] + (Values[Segment + 1] - Values[Segment]) * H01
                + Span * (Tangents[Segment] * H10 + Tangents[Segment + 1] * H11);
        }
    }

    auto
    BuildMonotoneProgress(
        const TArray<FCk_ProceduralAction_ProgressKey>& InKeys,
        FCompiledProgressCurve& OutCurve)
        -> bool
    {
        OutCurve = {};
        const auto Count = InKeys.Num();
        if (Count < 2 || Count > progress_detail::MaxKeys)
        { return false; }
        auto Times = TArray<double>{};
        auto Values = TArray<double>{};
        Times.Reserve(Count);
        Values.Reserve(Count);
        for (auto Index = 0; Index < Count; ++Index)
        {
            const auto Time = InKeys[Index].Get_Time().Get_Value();
            const auto Value = InKeys[Index].Get_Value().Get_Value();
            if (NOT FMath::IsFinite(Time) || NOT FMath::IsFinite(Value) || Time < 0.0 || Time > 1.0 || Value < 0.0 || Value > 1.0
                || (Index > 0 && (Time <= Times.Last() || Value < Values.Last())))
            { return false; }
            Times.Add(Time);
            Values.Add(Value);
        }
        if (Times[0] != 0.0 || Values[0] != 0.0 || Times.Last() != 1.0 || Values.Last() != 1.0)
        { return false; }

        auto Slopes = TArray<double>{};
        Slopes.Reserve(Count - 1);
        for (auto Index = 0; Index + 1 < Count; ++Index)
        { Slopes.Add((Values[Index + 1] - Values[Index]) / (Times[Index + 1] - Times[Index])); }

        auto Tangents = TArray<double>{};
        Tangents.SetNumZeroed(Count);
        Tangents[0] = Slopes[0];
        Tangents.Last() = Slopes.Last();
        for (auto Index = 1; Index + 1 < Count; ++Index)
        {
            Tangents[Index] = Slopes[Index - 1] > 0.0 && Slopes[Index] > 0.0
                ? (Slopes[Index - 1] + Slopes[Index]) * 0.5 : 0.0;
        }
        for (auto Index = 0; Index + 1 < Count; ++Index)
        {
            if (Slopes[Index] == 0.0)
            {
                Tangents[Index] = 0.0;
                Tangents[Index + 1] = 0.0;
            }
        }
        for (auto Index = 0; Index + 1 < Count; ++Index)
        {
            if (Slopes[Index] == 0.0)
            { continue; }
            const auto Alpha = Tangents[Index] / Slopes[Index];
            const auto Beta = Tangents[Index + 1] / Slopes[Index];
            const auto Magnitude = Alpha * Alpha + Beta * Beta;
            if (Magnitude > 9.0)
            {
                const auto Tau = 3.0 / FMath::Sqrt(Magnitude);
                Tangents[Index] = Tau * Alpha * Slopes[Index];
                Tangents[Index + 1] = Tau * Beta * Slopes[Index];
            }
        }
        for (const auto Tangent : Tangents)
        {
            if (NOT FMath::IsFinite(Tangent))
            { return false; }
        }
        OutCurve.Get_Times() = MoveTemp(Times);
        OutCurve.Get_Values() = MoveTemp(Values);
        OutCurve.Get_Tangents() = MoveTemp(Tangents);
        return true;
    }

    auto
    EvaluateProgress(
        const FCompiledPhase& InPhase,
        double InRaw)
        -> double
    {
        const auto& Curve = InPhase.Get_Progress();
        if (Curve.Get_Times().IsEmpty())
        {
            return UCk_Utils_TweenEasing_UE::Apply(
                progress_detail::ToTween(InPhase.Get_Easing()), FCk_FloatRange_0to1{InRaw});
        }
        return progress_detail::EvaluateCurve(Curve, InRaw);
    }

    auto
    InvertProgress(
        const FCompiledPhase& InPhase,
        double InProgress)
        -> double
    {
        auto Low = 0.0;
        auto High = 1.0;
        for (auto Index = 0; Index < progress_detail::InverseIterations; ++Index)
        {
            const auto Mid = (Low + High) * 0.5;
            if (EvaluateProgress(InPhase, Mid) < InProgress)
            { Low = Mid; }
            else
            { High = Mid; }
        }
        return High;
    }
}
