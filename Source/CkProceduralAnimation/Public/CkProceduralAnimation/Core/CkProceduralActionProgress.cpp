#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "CkTween/CkTween_Easing_Utils.h"

namespace ck::procedural_action
{
    namespace progress_detail
    {
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
    }

    auto
    EvaluateProgress(
        const FCompiledPhase& InPhase,
        double InRaw)
        -> double
    {
        return UCk_Utils_TweenEasing_UE::Apply(
            progress_detail::ToTween(InPhase.Get_Easing()), FCk_FloatRange_0to1{InRaw});
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
