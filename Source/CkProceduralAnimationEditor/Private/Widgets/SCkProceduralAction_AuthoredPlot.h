#pragma once

#include "Widgets/SLeafWidget.h"

class FCk_ProceduralActionAuthoringSession;

class SCkProceduralAction_AuthoredPlot : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_AuthoredPlot) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(FName, Slot)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    auto
        RefreshSamples()
        -> void;

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
    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    FName _Slot;
    TArray<FVector> _Positions;
    double _MinComponent = 0.0;
    double _MaxComponent = 1.0;
    uint64 _SampledRevision = 0;
};
