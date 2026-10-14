#include "Widgets/SCkProceduralAction_ProgressPlot.h"

#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "Rendering/DrawElementTypes.h"

auto
    SCkProceduralAction_ProgressPlot::
    Construct(
        const FArguments&)
    -> void
{
}

auto
    SCkProceduralAction_ProgressPlot::
    SetPhase(
        const ck::procedural_action::FCompiledPhase* InPhase,
        const TArray<FVector2D>& InKeys)
    -> void
{
    constexpr auto SampleCount = 64;
    _Samples.Reset();
    _Keys = InKeys;
    if (InPhase != nullptr)
    {
        _Samples.Reserve(SampleCount + 1);
        for (auto Index = 0; Index <= SampleCount; ++Index)
        {
            const auto Raw = static_cast<double>(Index) / SampleCount;
            _Samples.Add(FVector2D{Raw, ck::procedural_action::EvaluateProgress(*InPhase, Raw)});
        }
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkProceduralAction_ProgressPlot::
    ComputeDesiredSize(
        float) const
    -> FVector2D
{
    return FVector2D{240.0, 110.0};
}

auto
    SCkProceduralAction_ProgressPlot::
    OnPaint(
        const FPaintArgs&,
        const FGeometry& InGeometry,
        const FSlateRect&,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle&,
        bool) const
    -> int32
{
    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X) - 16.0f;
    const auto Height = static_cast<float>(Size.Y) - 16.0f;
    const auto ToLocal = [Width, Height](const FVector2D& InPoint)
    {
        return FVector2f{8.0f + Width * static_cast<float>(InPoint.X), 8.0f + Height * (1.0f - static_cast<float>(InPoint.Y))};
    };
    const auto Frame = TArray<FVector2f>{ToLocal(FVector2D{0.0, 0.0}), ToLocal(FVector2D{1.0, 0.0}),
        ToLocal(FVector2D{1.0, 1.0}), ToLocal(FVector2D{0.0, 1.0}), ToLocal(FVector2D{0.0, 0.0})};
    FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, InGeometry.ToPaintGeometry(), Frame,
        ESlateDrawEffect::None, FLinearColor{0.25f, 0.25f, 0.25f}, true, 1.0f);
    if (_Samples.IsEmpty())
    { return InLayerId; }

    auto Points = TArray<FVector2f>{};
    Points.Reserve(_Samples.Num());
    for (const auto& Sample : _Samples)
    { Points.Add(ToLocal(Sample)); }
    FSlateDrawElement::MakeLines(OutDrawElements, InLayerId + 1, InGeometry.ToPaintGeometry(), MoveTemp(Points),
        ESlateDrawEffect::None, FLinearColor{0.95f, 0.75f, 0.25f}, true, 2.0f);
    for (const auto& Key : _Keys)
    {
        const auto Center = ToLocal(Key);
        const auto Marker = TArray<FVector2f>{Center + FVector2f{-3.0f, -3.0f}, Center + FVector2f{3.0f, -3.0f},
            Center + FVector2f{3.0f, 3.0f}, Center + FVector2f{-3.0f, 3.0f}, Center + FVector2f{-3.0f, -3.0f}};
        FSlateDrawElement::MakeLines(OutDrawElements, InLayerId + 2, InGeometry.ToPaintGeometry(), Marker,
            ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);
    }
    return InLayerId + 2;
}
