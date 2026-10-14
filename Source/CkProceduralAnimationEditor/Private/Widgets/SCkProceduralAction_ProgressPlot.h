#pragma once

#include "Widgets/SLeafWidget.h"

namespace ck::procedural_action
{
    struct FCompiledPhase;
}

// Draws one phase's progress curve: raw elapsed fraction across, motion progress up. The samples come from
// EvaluateProgress, the evaluator the runtime plays, and are taken when the phase or the clip revision changes.
class SCkProceduralAction_ProgressPlot : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_ProgressPlot) {}
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    auto
        SetPhase(
            const ck::procedural_action::FCompiledPhase* InPhase,
            const TArray<FVector2D>& InKeys)
        -> void;
    auto
        Get_SampleCount() const
        -> int32
    { return _Samples.Num(); }

    auto
        OnPaint(
            const FPaintArgs& InArgs,
            const FGeometry& InGeometry,
            const FSlateRect& InCullingRect,
            FSlateWindowElementList& OutDrawElements,
            int32 InLayerId,
            const FWidgetStyle& InWidgetStyle,
            bool InParentEnabled) const
        -> int32 override;
    auto
        ComputeDesiredSize(
            float InLayoutScaleMultiplier) const
        -> FVector2D override;

private:
    TArray<FVector2D> _Samples;
    TArray<FVector2D> _Keys;
};
