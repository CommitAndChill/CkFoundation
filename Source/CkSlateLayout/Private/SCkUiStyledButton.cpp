#include "CkSlateLayout/SCkUiStyledButton.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"

namespace ck_ui_styled_button
{
    const FLinearColor DefaultBackground(0x1A / 255.0f, 0x2A / 255.0f, 0x38 / 255.0f, 1.0f);
    const FLinearColor DefaultBorder(0x31 / 255.0f, 0x49 / 255.0f, 0x5B / 255.0f, 1.0f);
    const FLinearColor DefaultHoverBackground(0x22 / 255.0f, 0x37 / 255.0f, 0x46 / 255.0f, 1.0f);
    const FLinearColor DefaultHoverBorder(0x48 / 255.0f, 0x70 / 255.0f, 0x84 / 255.0f, 1.0f);
    const FLinearColor DefaultPressedBackground(0x1C / 255.0f, 0x30 / 255.0f, 0x3E / 255.0f, 1.0f);
    const FLinearColor DefaultPressedBorder(0x3D / 255.0f, 0x63 / 255.0f, 0x76 / 255.0f, 1.0f);
    const FLinearColor DefaultForeground(0xE9 / 255.0f, 0xF1 / 255.0f, 0xF7 / 255.0f, 1.0f);
    const FLinearColor DefaultDisabledForeground(0x90 / 255.0f, 0xA4 / 255.0f, 0xB5 / 255.0f, 0.55f);

    auto ReadColor(const TOptional<FLinearColor>& InValue, const FLinearColor& InDefault) -> FLinearColor
    {
        return InValue.Get(InDefault);
    }

    auto MakeDisabledColor(const FLinearColor& InColor) -> FLinearColor
    {
        auto Result = InColor;
        Result.A *= 0.55f;
        return Result;
    }

}

void SCkUiStyledButton::Construct(const FArguments& InArgs)
{
    using namespace ck_ui_styled_button;
    _ButtonStyle = MakeButtonStyle(InArgs._VisualStyle);
    _FocusedButtonStyle = _ButtonStyle;
    const auto& VisualStyle = InArgs._VisualStyle;
    _HasAuthoredFocusStyle = VisualStyle.FocusBackground.IsSet() || VisualStyle.FocusBorderColor.IsSet()
        || VisualStyle.FocusColor.IsSet() || VisualStyle.FocusOutlineColor.IsSet()
        || VisualStyle.FocusOutlineWidth.IsSet() || VisualStyle.FocusOutlineGap.IsSet();
    const float Radius = VisualStyle.Radius.Get(4.0f);
    const float BorderWidth = VisualStyle.OutlineWidth.Get(1.0f);
    const FLinearColor FocusBackground = ReadColor(VisualStyle.FocusBackground,
        ReadColor(VisualStyle.HoverBackground, DefaultHoverBackground));
    const FLinearColor FocusBorder = ReadColor(VisualStyle.FocusBorderColor,
        ReadColor(VisualStyle.HoverBorderColor, DefaultHoverBorder));
    _FocusedButtonStyle.SetNormal(FSlateRoundedBoxBrush(FocusBackground, Radius, FocusBorder, BorderWidth));
    _FocusedButtonStyle.SetHovered(FSlateRoundedBoxBrush(FocusBackground, Radius, FocusBorder, BorderWidth));
    const FSlateColor FocusForeground(ReadColor(VisualStyle.FocusColor,
        ReadColor(VisualStyle.Color, DefaultForeground)));
    _FocusedButtonStyle.SetNormalForeground(FocusForeground);
    _FocusedButtonStyle.SetHoveredForeground(FocusForeground);
    _FocusOutlineGap = VisualStyle.FocusOutlineGap.Get(2.0f);
    _FocusOutlineWidth = VisualStyle.FocusOutlineWidth.Get(2.0f);
    if (_HasAuthoredFocusStyle && _FocusOutlineWidth > 0.0f)
    {
        _FocusOutlineBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::Transparent,
            Radius + _FocusOutlineGap + _FocusOutlineWidth,
            ReadColor(VisualStyle.FocusOutlineColor, FocusBorder), _FocusOutlineWidth);
    }
    SButton::Construct(SButton::FArguments()
        .ButtonStyle(&_ButtonStyle)
        .ContentPadding(InArgs._VisualStyle.ContentPadding)
        .OnClicked(InArgs._OnClicked)
        [
            InArgs._Content.Widget
        ]);
    // This class calls SButton::Construct directly, so inherited SWidget
    // arguments are not applied by the usual SNew declarative wrapper.
    SetEnabled(InArgs._IsEnabled);
    SetToolTipText(InArgs._ToolTipText);
    SetTag(InArgs._Tag);
}

auto SCkUiStyledButton::GetFocusBrush() const -> const FSlateBrush*
{
    // Authored focus paints its own ring; a second platform ring can cover the label.
    return _HasAuthoredFocusStyle ? nullptr : SButton::GetFocusBrush();
}

auto SCkUiStyledButton::OnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) -> FReply
{
    _FocusActive = true;
    if (_HasAuthoredFocusStyle) { SetButtonStyle(&_FocusedButtonStyle); }
    return SButton::OnFocusReceived(InGeometry, InFocusEvent);
}

void SCkUiStyledButton::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    SButton::OnFocusLost(InFocusEvent);
    _FocusActive = false;
    SetButtonStyle(&_ButtonStyle);
}

auto SCkUiStyledButton::OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry,
    const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements, int32 InLayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const -> int32
{
    const int32 ContentLayer = SButton::OnPaint(InArgs, InGeometry, InCullingRect, OutDrawElements,
        InLayerId, InWidgetStyle, bParentEnabled);
    if (!_FocusActive || !IsEnabled() || !_FocusOutlineBrush.IsValid()) { return ContentLayer; }
    const float Extent = _FocusOutlineGap + _FocusOutlineWidth;
    FSlateDrawElement::MakeBox(OutDrawElements, ContentLayer + 1,
        InGeometry.ToInflatedPaintGeometry(FVector2D{Extent, Extent}),
        _FocusOutlineBrush.Get(), ESlateDrawEffect::None,
        InWidgetStyle.GetColorAndOpacityTint() * _FocusOutlineBrush->GetTint(InWidgetStyle));
    return ContentLayer + 1;
}

auto SCkUiStyledButton::MakeButtonStyle(const FCkUiButtonVisualStyle& InVisualStyle) -> FButtonStyle
{
    using namespace ck_ui_styled_button;

    const FLinearColor Background = ReadColor(InVisualStyle.Background, DefaultBackground);
    const FLinearColor Border = ReadColor(InVisualStyle.BorderColor, DefaultBorder);
    const FLinearColor HoverBackground = ReadColor(InVisualStyle.HoverBackground, DefaultHoverBackground);
    const FLinearColor HoverBorder = ReadColor(InVisualStyle.HoverBorderColor, DefaultHoverBorder);
    const FLinearColor PressedBackground = ReadColor(InVisualStyle.PressedBackground, DefaultPressedBackground);
    const FLinearColor PressedBorder = ReadColor(InVisualStyle.PressedBorderColor, DefaultPressedBorder);
    const FLinearColor DisabledBackground = ReadColor(InVisualStyle.DisabledBackground, MakeDisabledColor(Background));
    const FLinearColor DisabledBorder = ReadColor(InVisualStyle.DisabledBorderColor, MakeDisabledColor(Border));
    const FLinearColor Foreground = ReadColor(InVisualStyle.Color, DefaultForeground);
    const FLinearColor PressedForeground = ReadColor(InVisualStyle.PressedColor, Foreground);
    const FLinearColor DisabledForeground = ReadColor(InVisualStyle.DisabledColor, DefaultDisabledForeground);
    const float Radius = InVisualStyle.Radius.Get(4.0f);
    const float OutlineWidth = InVisualStyle.OutlineWidth.Get(1.0f);

    return FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(Background, Radius, Border, OutlineWidth))
        .SetHovered(FSlateRoundedBoxBrush(HoverBackground, Radius, HoverBorder, OutlineWidth))
        .SetPressed(FSlateRoundedBoxBrush(PressedBackground, Radius, PressedBorder, OutlineWidth))
        .SetDisabled(FSlateRoundedBoxBrush(DisabledBackground, Radius, DisabledBorder, OutlineWidth))
        .SetNormalForeground(FSlateColor(Foreground))
        .SetHoveredForeground(FSlateColor(Foreground))
        .SetPressedForeground(FSlateColor(PressedForeground))
        .SetDisabledForeground(FSlateColor(DisabledForeground))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(0.0f));
}
