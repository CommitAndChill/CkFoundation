#include "Widgets/SCkProceduralAction_AuthoredPlot.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

#include "Rendering/DrawElementTypes.h"

auto
    SCkProceduralAction_AuthoredPlot::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _Slot = InArgs._Slot;
    RefreshSamples();
}

auto
    SCkProceduralAction_AuthoredPlot::
    RefreshSamples()
    -> void
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || _SampledRevision == Session->GetRevision())
    { return; }
    _SampledRevision = Session->GetRevision();
    _Positions.Reset();
    _MinComponent = TNumericLimits<double>::Max();
    _MaxComponent = TNumericLimits<double>::Lowest();
    if (NOT Session->CanPlay())
    { return; }

    constexpr auto SampleCount = 64;
    const auto Duration = Session->GetDuration().Get_Seconds();
    if (Duration <= 0.0)
    { return; }

    _Positions.Reserve(SampleCount + 1);
    for (auto Index = 0; Index <= SampleCount; ++Index)
    {
        auto Pose = ck::procedural_action::FEvaluatedPose{};
        if (NOT Session->EvaluateAt(FCk_Time{Duration * Index / SampleCount}, Pose))
        { _Positions.Reset(); return; }

        auto Position = Pose.Get_Body().GetTranslation();
        if (NOT _Slot.IsNone())
        {
            const auto* Limb = Pose.Get_Limbs().FindByPredicate(
                [this](const ck::procedural_action::FLimbPose& InLimb)
                { return InLimb.Get_Slot() == _Slot; });
            if (Limb == nullptr)
            { _Positions.Reset(); return; }
            Position = Limb->Get_Position();
        }

        _Positions.Add(Position);
        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            _MinComponent = FMath::Min(_MinComponent, Position[Axis]);
            _MaxComponent = FMath::Max(_MaxComponent, Position[Axis]);
        }
    }
}

auto
    SCkProceduralAction_AuthoredPlot::
    ComputeDesiredSize(
        float) const
    -> FVector2D
{
    return FVector2D{600.0, 140.0};
}

auto
    SCkProceduralAction_AuthoredPlot::
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
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || _Positions.IsEmpty())
    { return InLayerId; }

    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto Range = FMath::Max(_MaxComponent - _MinComponent, 1.0);
    const FLinearColor Colors[3] = {
        FLinearColor{0.95f, 0.35f, 0.32f},
        FLinearColor{0.38f, 0.87f, 0.48f},
        FLinearColor{0.37f, 0.61f, 0.98f}};
    for (auto Axis = 0; Axis < 3; ++Axis)
    {
        auto Points = TArray<FVector2f>{};
        Points.Reserve(_Positions.Num());
        for (auto Index = 0; Index < _Positions.Num(); ++Index)
        {
            const auto X = 8.0f + (Width - 16.0f) * Index / (_Positions.Num() - 1);
            const auto Y = Height - 8.0f
                - (Height - 16.0f) * static_cast<float>((_Positions[Index][Axis] - _MinComponent) / Range);
            Points.Add(FVector2f{X, Y});
        }
        FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, InGeometry.ToPaintGeometry(),
            MoveTemp(Points), ESlateDrawEffect::None, Colors[Axis], true, 1.5f);
    }

    const auto CursorX = 8.0f + (Width - 16.0f)
        * static_cast<float>(Session->GetTime().Get_Seconds()
            / Session->GetDuration().Get_Seconds());
    const auto Cursor = TArray<FVector2f>{FVector2f{CursorX, 0.0f}, FVector2f{CursorX, Height}};
    FSlateDrawElement::MakeLines(OutDrawElements, InLayerId + 1, InGeometry.ToPaintGeometry(),
        Cursor, ESlateDrawEffect::None, FLinearColor::White, true, 1.0f);
    return InLayerId + 1;
}
