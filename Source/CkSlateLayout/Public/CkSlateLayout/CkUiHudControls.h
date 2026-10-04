#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"
#include "Widgets/SLeafWidget.h"

struct FSlateRoundedBoxBrush;

struct FCkUiHudMeterVisual
{
    FLinearColor Fill = FLinearColor(0.20f, 0.78f, 0.92f);
    FLinearColor Background = FLinearColor(0.035f, 0.075f, 0.11f);
    FLinearColor Trail = FLinearColor(1.0f, 0.57f, 0.18f);
    FLinearColor Border = FLinearColor(0.28f, 0.52f, 0.60f);
    float BorderWidth = 1.0f;
    float Radius = 3.0f;
    int32 Segments = 1;
    bool Radial = false;
    float TrailDelay = 0.35f;
    float Response = 0.18f;
    float LowThreshold = 0.25f;
};

/** Passive native meter. AdvancePresentation is also the production Tick transition. */
class CKSLATELAYOUT_API SCkUiHudMeter final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkUiHudMeter) {} SLATE_END_ARGS()

    void Construct(const FArguments& InArguments);
    void Configure(TAttribute<float> InValue, TAttribute<FLinearColor> InTint, const FCkUiHudMeterVisual& InVisual);
    void AdvancePresentation(float InDeltaTime);
    void Freeze();
    auto GetDisplayedFraction() const -> float { return _Displayed; }
    auto GetTrailFraction() const -> float { return _Trail; }
    auto GetTargetFraction() const -> float { return _Target; }
    auto IsFrozen() const -> bool { return _Frozen; }

    virtual void Tick(const FGeometry& InGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual auto ComputeDesiredSize(float InScale) const -> FVector2D override;
    virtual auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry,
        const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 InLayer, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const -> int32 override;

private:
    auto ReadTarget() const -> float;
    auto ReadTint() const -> FLinearColor;
    auto DrawArc(const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements,
        int32 InLayer, float InFraction, float InThickness, const FLinearColor& InColor) const -> void;

    TAttribute<float> _Value;
    TAttribute<FLinearColor> _Tint;
    FCkUiHudMeterVisual _Visual;
    TSharedPtr<FSlateRoundedBoxBrush> _Brush;
    float _Target = 0.0f;
    float _Displayed = 0.0f;
    float _Trail = 0.0f;
    float _DelayRemaining = 0.0f;
    float _PulseTime = 0.0f;
    bool _Initialized = false;
    bool _Frozen = false;
};

/** Registers passive, retained hud-meter and hud-graphic controls. */
class CKSLATELAYOUT_API FCkUiHudControls final
{
public:
    static auto Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
};
