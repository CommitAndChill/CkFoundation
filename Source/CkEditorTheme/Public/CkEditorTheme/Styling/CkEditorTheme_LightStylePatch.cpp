#include "CkEditorTheme_LightStylePatch.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Theme/CkTheme_Utils.h"

#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateColorBrush.h"

#include "HAL/IConsoleManager.h"
#include "Styling/SegmentedControlStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_light_style_patch
{
    struct FStyleSetAccess : public FSlateStyleSet
    {
        using FSlateStyleSet::WidgetStyleValues;
        using FSlateStyleSet::SlateColorValues;
    };

    // Added by the //++Ck SGraphPin patch: pin label ink, literal white before it. Its presence means node text can
    // follow the theme, so graph nodes go light too.
    const auto PinLabelColorKey = FName{TEXT("Graph.Node.PinLabelColor")};

    TAutoConsoleVariable<bool> CVarForceLightGraphNodes(
        TEXT("Ck.EditorTheme.ForceLightGraphNodes"),
        false,
        TEXT("Dev: light graph nodes without the engine pin-label patch (pin labels stay white). Read at startup."),
        ECVF_Default);

    auto Get_IsMenuRow(
        const FTableRowStyle& InStyle)
        -> bool
    {
        return InStyle.TextColor == FStyleColors::White
            && InStyle.EvenRowBackgroundBrush.DrawAs == ESlateBrushDrawType::NoDrawType
            && InStyle.OddRowBackgroundBrush.DrawAs == ESlateBrushDrawType::NoDrawType;
    }

    // Graph canvases and node art stay dark in every theme, so text drawn on them keeps the Dark theme's ink
    // (StyleColors.cpp InitializeDefaults: Foreground #C0C0C0, ForegroundHover #FFFFFF).
    auto Get_DarkIslandInk(
        const FSlateColor& InColor)
        -> TOptional<FSlateColor>
    {
        if (InColor == FStyleColors::Foreground)
        { return FSlateColor{FLinearColor::FromSRGBColor(FColor{0xC0, 0xC0, 0xC0})}; }

        if (InColor == FStyleColors::ForegroundHover)
        { return FSlateColor{FLinearColor::White}; }

        return {};
    }

    // Hover/press overlays such as FColor(255, 255, 255, 40): they brighten on Dark and vanish on Light.
    auto Get_TranslucentWhiteOverlayAlpha(
        const FSlateColor& InColor)
        -> TOptional<float>
    {
        const auto Specified = InColor.GetSpecifiedColor();
        if (InColor != FSlateColor{Specified} || Specified.A <= 0.0f || Specified.A >= 0.5f)
        { return {}; }

        if (Specified.R < 0.95f || Specified.G < 0.95f || Specified.B < 0.95f)
        { return {}; }

        return Specified.A;
    }

    auto Get_IsAccentFill(
        const FSlateBrush& InBrush)
        -> bool
    {
        if (InBrush.DrawAs == ESlateBrushDrawType::NoDrawType || InBrush.DrawAs == ESlateBrushDrawType::Image)
        { return false; }

        return InBrush.TintColor == FStyleColors::Primary || InBrush.TintColor == FStyleColors::Select;
    }

    // Hyperlink underlines are white PNG strips (Old/HyperlinkDotted, Old/HyperlinkUnderline) drawn untinted.
    auto Get_IsWhiteUnderline(
        const FSlateBrush& InBrush)
        -> bool
    {
        return InBrush.DrawAs != ESlateBrushDrawType::NoDrawType
            && InBrush.TintColor == FSlateColor{FLinearColor::White}
            && InBrush.GetResourceName().ToString().Contains(TEXT("Old/Hyperlink"));
    }

    // Class icons are white art made for a dark tile; Light keeps the tile dark so they stay legible.
    auto Get_ThumbnailTile()
        -> FSlateColor
    {
        return FSlateColor{FLinearColor::FromSRGBColor(FColor{0x2E, 0x2E, 0x2E})};
    }

    // A color baked from GetSpecifiedColor() at style creation, as opposed to a theme slot that resolves at paint time.
    auto Get_IsBakedDark(
        const FSlateColor& InColor)
        -> bool
    {
        const auto Specified = InColor.GetSpecifiedColor();
        return InColor == FSlateColor{Specified} && Specified.GetLuminance() < 0.5f;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    auto
        Get_Brightness()
        -> ECk_EditorTheme_Brightness
    {
        return ck::theme::Get_IsEditorThemeLight()
            ? ECk_EditorTheme_Brightness::Light
            : ECk_EditorTheme_Brightness::Dark;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_LightStylePatch::
    Request_Capture(
        FSlateStyleSet& InStyleSet)
    -> int32
{
    using namespace ck_editor_theme_light_style_patch;

    auto& WidgetStyles = InStyleSet.*(&FStyleSetAccess::WidgetStyleValues);
    const auto& SlateColors = InStyleSet.*(&FStyleSetAccess::SlateColorValues);
    const auto FieldsBefore = _Fields.Num() + _LinearFields.Num() + _BrushFields.Num() + _NamedColors.Num();

    const auto HasPinLabel = SlateColors.Contains(PinLabelColorKey);
    const auto LightGraphNodes = HasPinLabel || CVarForceLightGraphNodes.GetValueOnGameThread();
    _LightGraphNodes |= LightGraphNodes;

    if (HasPinLabel)
    { DoCapture_NamedColor(InStyleSet, PinLabelColorKey, FStyleColors::Foreground); }

    // Added by the //++Ck MainFrameModule patch: the title-bar FPS / Mem / Objs ink, literal light grays before it.
    DoCapture_NamedColor(InStyleSet, TEXT("MainFrame.DebugTools.LabelColor"), FSlateColor{FLinearColor::FromSRGBColor(FColor{0x6E, 0x6E, 0x6E})});
    DoCapture_NamedColor(InStyleSet, TEXT("MainFrame.DebugTools.ValueColor"), FStyleColors::Foreground);

    for (auto& Entry : WidgetStyles)
    {
        auto& Style = Entry.Value.Get();
        const auto TypeName = Style.GetTypeName();

        // Text typed into a focused box is White (StarshipCoreStyle NormalEditableTextBoxStyle), on a white input.
        auto* TextBox = static_cast<FEditableTextBoxStyle*>(nullptr);
        if (TypeName == FEditableTextBoxStyle::TypeName)
        { TextBox = &static_cast<FEditableTextBoxStyle&>(Style); }
        else if (TypeName == FSearchBoxStyle::TypeName)
        { TextBox = &static_cast<FSearchBoxStyle&>(Style).TextBoxStyle; }
        else if (TypeName == FInlineEditableTextBlockStyle::TypeName)
        { TextBox = &static_cast<FInlineEditableTextBlockStyle&>(Style).EditableTextBoxStyle; }

        // Boxes filled with the ink itself (Graph.CommentBubble.EditableText) are dark in Light; White stays right there.
        if (TextBox != nullptr
            && TextBox->FocusedForegroundColor == FStyleColors::White
            && TextBox->BackgroundImageFocused.TintColor != FStyleColors::Foreground
            && TextBox->BackgroundImageFocused.TintColor != FStyleColors::ForegroundHover)
        { DoCapture_Field(TextBox->FocusedForegroundColor, FStyleColors::ForegroundHover); }

        // Dark node art keeps the Dark ink on its text. Light node art lets it follow the theme, except node titles: they
        // sit on the colored title band in both themes, so they stay white. Graph tooltip text sits on the (light)
        // tooltip background either way.
        if (const auto Key = Entry.Key.ToString(); Key.StartsWith(TEXT("Graph.")) && NOT Key.Contains(TEXT("Tooltip")))
        {
            auto* GraphText = static_cast<FSlateColor*>(nullptr);
            if (TypeName == FTextBlockStyle::TypeName)
            { GraphText = &static_cast<FTextBlockStyle&>(Style).ColorAndOpacity; }
            else if (TypeName == FInlineEditableTextBlockStyle::TypeName)
            { GraphText = &static_cast<FInlineEditableTextBlockStyle&>(Style).TextStyle.ColorAndOpacity; }

            if (GraphText != nullptr)
            {
                const auto DarkInk = Get_DarkIslandInk(*GraphText);
                if (NOT LightGraphNodes && DarkInk.IsSet())
                { DoCapture_Field(*GraphText, DarkInk.GetValue()); }
                else if (LightGraphNodes && DarkInk.IsSet() && Key.StartsWith(TEXT("Graph.Node.NodeTitle")))
                { DoCapture_Field(*GraphText, FSlateColor{FLinearColor::White}); }
                // The title subline is yellow at 50%, which muddies on the solid Light title band.
                else if (LightGraphNodes && Key == TEXT("Graph.Node.NodeTitleExtraLines"))
                { DoCapture_Field(*GraphText, FSlateColor{FLinearColor{1.0f, 1.0f, 1.0f, 0.8f}}); }
                // Compact node labels (+, ==, AND, conversions) are white at 50% on the variable-node body, which is
                // light with light nodes; Light keeps the strength in a dark ink.
                else if (LightGraphNodes && (Key == TEXT("Graph.CompactNode.Title") || Key == TEXT("Graph.ArrayCompactNode.Title"))
                    && GraphText->GetSpecifiedColor().R >= 0.95f)
                {
                    auto Ink = FLinearColor::FromSRGBColor(FColor{0x3A, 0x3A, 0x3A});
                    Ink.A = GraphText->GetSpecifiedColor().A;
                    DoCapture_Field(*GraphText, FSlateColor{Ink});
                }
                continue;
            }
        }

        // Graph breadcrumbs are White at half alpha on the title bar, which is light in Light.
        if (TypeName == FTextBlockStyle::TypeName && Entry.Key == FName{TEXT("GraphBreadcrumbButtonText")})
        {
            auto& Text = static_cast<FTextBlockStyle&>(Style);
            if (Text.ColorAndOpacity.IsColorSpecified() && Text.ColorAndOpacity.GetSpecifiedColor().R >= 0.95f)
            {
                auto Ink = FLinearColor::FromSRGBColor(FColor{0x3A, 0x3A, 0x3A});
                Ink.A = FMath::Max(Text.ColorAndOpacity.GetSpecifiedColor().A, 0.75f);
                DoCapture_Field(Text.ColorAndOpacity, FSlateColor{Ink});
            }
            continue;
        }

        // Emphasis text is literal white with a black drop shadow, used as headings on panels.
        if (TypeName == FTextBlockStyle::TypeName
            && (Entry.Key == FName{TEXT("NormalText.Important")} || Entry.Key == FName{TEXT("LargeText")}))
        {
            auto& Text = static_cast<FTextBlockStyle&>(Style);
            if (Text.ColorAndOpacity == FSlateColor{FLinearColor::White})
            {
                DoCapture_Field(Text.ColorAndOpacity, FStyleColors::ForegroundHover);
                DoCapture_Field(Text.ShadowColorAndOpacity, FLinearColor::Transparent);
            }
            continue;
        }

        // Segmented toggles mark the selected segment with Dropdown, which is the menu color itself in Light.
        auto* Segment = static_cast<FCheckBoxStyle*>(nullptr);
        if (TypeName == FCheckBoxStyle::TypeName)
        { Segment = &static_cast<FCheckBoxStyle&>(Style); }
        else if (TypeName == FSegmentedControlStyle::TypeName)
        { Segment = &static_cast<FSegmentedControlStyle&>(Style).ControlStyle; }

        // Accent-filled toggles label their checked state with ForegroundHover: white in Dark, but dark ink in Light.
        auto* Toggle = Segment;
        if (TypeName == FToolBarStyle::TypeName)
        { Toggle = &static_cast<FToolBarStyle&>(Style).ToggleButton; }

        if (Toggle != nullptr && Toggle->CheckBoxType == ESlateCheckBoxType::ToggleButton && Get_IsAccentFill(Toggle->CheckedImage))
        {
            for (auto* Foreground : {&Toggle->CheckedForeground, &Toggle->CheckedHoveredForeground, &Toggle->CheckedPressedForeground})
            {
                if (*Foreground == FStyleColors::ForegroundHover)
                { DoCapture_Field(*Foreground, FSlateColor{FLinearColor::White}); }
            }
        }

        if (Segment != nullptr
            && Segment->CheckBoxType == ESlateCheckBoxType::ToggleButton
            && Segment->UncheckedImage.DrawAs == ESlateBrushDrawType::NoDrawType
            && Segment->CheckedImage.TintColor == FStyleColors::Dropdown)
        { DoCapture_Field(Segment->CheckedImage.TintColor, FStyleColors::SelectInactive); }

        // Menu check boxes tint their check mark White on a light box; the standard check box uses Primary.
        if (TypeName == FCheckBoxStyle::TypeName)
        {
            auto& Check = static_cast<FCheckBoxStyle&>(Style);
            if (Check.CheckBoxType == ESlateCheckBoxType::CheckBox)
            {
                for (auto* Brush : {&Check.CheckedImage, &Check.CheckedHoveredImage, &Check.CheckedPressedImage,
                                    &Check.UndeterminedImage, &Check.UndeterminedHoveredImage, &Check.UndeterminedPressedImage})
                {
                    if (Brush->DrawAs != ESlateBrushDrawType::NoDrawType && Brush->TintColor == FStyleColors::White)
                    { DoCapture_Field(Brush->TintColor, FStyleColors::Primary); }
                }
            }
        }

        // Menu radio items draw an Input dot when unchecked and a White dot when checked, both tinted by the menu ink;
        // in Light every item gets the same dark dot. Win11 menus mark only the selected item.
        if (TypeName == FCheckBoxStyle::TypeName && Entry.Key == FName{TEXT("Menu.RadioButton")})
        {
            auto& Radio = static_cast<FCheckBoxStyle&>(Style);
            for (auto* Brush : {&Radio.UncheckedImage, &Radio.UncheckedHoveredImage, &Radio.UndeterminedImage})
            {
                if (Brush->TintColor == FStyleColors::Input)
                { DoCapture_Field(Brush->TintColor, FStyleColors::Transparent); }
            }
        }

        // The value fill (the "slider") is Dropdown or Secondary, which equal the input fill in Light.
        if (TypeName == FSpinBoxStyle::TypeName)
        {
            auto& SpinBox = static_cast<FSpinBoxStyle&>(Style);
            if (SpinBox.InactiveFillBrush.TintColor == FStyleColors::Dropdown || SpinBox.InactiveFillBrush.TintColor == FStyleColors::Secondary)
            { DoCapture_Field(SpinBox.InactiveFillBrush.TintColor, FStyleColors::Hover); }

            for (auto* Brush : {&SpinBox.HoveredFillBrush, &SpinBox.ActiveFillBrush})
            {
                if (Brush->TintColor == FStyleColors::Hover)
                { DoCapture_Field(Brush->TintColor, FStyleColors::Hover2); }
            }
            continue;
        }

        auto* Button = static_cast<FButtonStyle*>(nullptr);
        if (TypeName == FButtonStyle::TypeName)
        { Button = &static_cast<FButtonStyle&>(Style); }
        else if (TypeName == FHyperlinkStyle::TypeName)
        { Button = &static_cast<FHyperlinkStyle&>(Style).UnderlineStyle; }

        if (Button != nullptr)
        {
            for (auto* Brush : {&Button->Normal, &Button->Hovered, &Button->Pressed, &Button->Disabled})
            {
                if (Get_IsWhiteUnderline(*Brush))
                { DoCapture_Field(Brush->TintColor, FStyleColors::Foreground); }
                else if (const auto Alpha = Get_TranslucentWhiteOverlayAlpha(Brush->TintColor); Alpha.IsSet())
                { DoCapture_Field(Brush->TintColor, FSlateColor{FLinearColor{0.0f, 0.0f, 0.0f, Alpha.GetValue()}}); }
            }
            continue;
        }

        // Dropdown rows (ComboBox.Row and copies) draw literal White text on the menu and hover with the accent.
        if (TypeName == FTableRowStyle::TypeName)
        {
            auto& Row = static_cast<FTableRowStyle&>(Style);
            if (NOT Get_IsMenuRow(Row))
            { continue; }

            DoCapture_Field(Row.TextColor, FStyleColors::Foreground);
            DoCapture_Field(Row.SelectedTextColor, FStyleColors::Foreground);

            for (auto* Brush : {&Row.EvenRowBackgroundHoveredBrush, &Row.OddRowBackgroundHoveredBrush,
                                &Row.ActiveHoveredBrush, &Row.ActiveHighlightedBrush,
                                &Row.InactiveHoveredBrush, &Row.InactiveHighlightedBrush})
            {
                if (Brush->TintColor == FStyleColors::Primary)
                { DoCapture_Field(Brush->TintColor, FStyleColors::Hover); }
            }
        }
        else if (TypeName == FDockTabStyle::TypeName)
        {
            // Dock tab titles are literal White, and the hovered fill is Panel baked to its Dark value when the style was built.
            auto& Tab = static_cast<FDockTabStyle&>(Style);

            for (auto* Foreground : {&Tab.NormalForegroundColor, &Tab.HoveredForegroundColor,
                                     &Tab.ActiveForegroundColor, &Tab.ForegroundForegroundColor})
            {
                if (*Foreground == FStyleColors::White)
                { DoCapture_Field(*Foreground, FStyleColors::ForegroundHover); }
            }

            if (Get_IsBakedDark(Tab.HoveredBrush.TintColor))
            { DoCapture_Field(Tab.HoveredBrush.TintColor, FStyleColors::Hover); }
        }
    }

    // The small app icon (secondary window title bars) is the full-color logo tinted with the ink: a dark blob in Light.
    if (auto* AppIcon = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("AppIcon.Small"), nullptr, nullptr));
        AppIcon != nullptr && AppIcon->TintColor == FStyleColors::Foreground)
    { DoCapture_Field(AppIcon->TintColor, FStyleColors::White); }

    if (auto* Tile = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("AssetThumbnail.AssetBackground"), nullptr, nullptr));
        Tile != nullptr && Tile->TintColor == FStyleColors::Recessed)
    { DoCapture_Field(Tile->TintColor, Get_ThumbnailTile()); }

    // The graph title bar is a black PNG at half alpha, which no tint can lighten.
    if (auto* TitleBar = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("Graph.TitleBackground"), nullptr, nullptr)))
    { DoCapture_Brush(*TitleBar, FSlateColorBrush{FStyleColors::Title}); }

    // Its back/forward arrows are white SVGs in untinted images (WorkflowTabManager), white on that light bar.
    for (const auto* Key : {TEXT("GraphBreadcrumb.BrowseBack"), TEXT("GraphBreadcrumb.BrowseForward")})
    {
        if (auto* Arrow = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(Key, nullptr, nullptr));
            Arrow != nullptr && Arrow->TintColor == FSlateColor{FLinearColor::White})
        { DoCapture_Field(Arrow->TintColor, FStyleColors::Foreground); }
    }

    if (LightGraphNodes)
    {
        // Variable-node corner glyphs are white SVGs drawn untinted, on a body that is light with light nodes.
        for (const auto* Key : {TEXT("Graph.Function.FunctionParameterIcon"), TEXT("Graph.Function.FunctionLocalVariableIcon")})
        {
            if (auto* Glyph = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(Key, nullptr, nullptr));
                Glyph != nullptr && Glyph->TintColor == FSlateColor{FLinearColor::White})
            { DoCapture_Field(Glyph->TintColor, FStyleColors::Foreground); }
        }

        // The variable-node colour spill is a fixed 132x28 strip stretched across the pill, invisible only while it fades
        // into a dark body; Light gives it the body's 9-slice so the hue fits the pill (IconPatch fills it from the body).
        if (auto* Spill = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("Graph.VarNode.ColorSpill"), nullptr, nullptr)))
        {
            if (const auto* Body = InStyleSet.GetOptionalBrush(TEXT("Graph.VarNode.Body"), nullptr, nullptr))
            {
                // The spill's slot is top-aligned and sized by ImageSize: keep the stock width (the pill's minimum width) and
                // fill a pin row's pill height. The caps are the body's own, in VarNode_body.png (64x28) pixels.
                const auto Size = FVector2f{static_cast<float>(Spill->GetImageSize().X), 36.0f};
                const auto& BodyMargin = Body->GetMargin();
                auto PillSpill = FSlateBoxBrush{Spill->GetResourceName(),
                    FMargin{BodyMargin.Left * 64.0f / Size.X, BodyMargin.Top * 28.0f / Size.Y,
                            BodyMargin.Right * 64.0f / Size.X, BodyMargin.Bottom * 28.0f / Size.Y},
                    FLinearColor::White};
                PillSpill.SetImageSize(Size);
                DoCapture_Brush(*Spill, PillSpill);
            }
        }

        // The development-only and disabled banners are 30% stripes under literal white text, readable only over a dark
        // body. Light keeps the stripes at full strength; IconPatch fills their gaps dark (Get_OpaqueStripes).
        if (auto* Banner = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("Graph.Node.DevelopmentBanner"), nullptr, nullptr)))
        { DoCapture_Field(Banner->TintColor, FSlateColor{FLinearColor::FromSRGBColor(FColor{0xB8, 0x86, 0x0B})}); }

        if (auto* Banner = const_cast<FSlateBrush*>(InStyleSet.GetOptionalBrush(TEXT("Graph.Node.DisabledBanner"), nullptr, nullptr)))
        { DoCapture_Field(Banner->TintColor, FSlateColor{FLinearColor::FromSRGBColor(FColor{0x6E, 0x6E, 0x6E})}); }
    }

    return _Fields.Num() + _LinearFields.Num() + _BrushFields.Num() + _NamedColors.Num() - FieldsBefore;
}

auto
    FCk_EditorTheme_LightStylePatch::
    Request_Apply(
        ECk_EditorTheme_Brightness InBrightness)
    -> bool
{
    if (_Applied.IsSet() && _Applied.GetValue() == InBrightness)
    { return false; }

    const auto IsLight = InBrightness == ECk_EditorTheme_Brightness::Light;

    for (const auto& Captured : _Fields)
    { *Captured._Field = IsLight ? Captured._LightValue : Captured._DarkValue; }

    for (const auto& Captured : _LinearFields)
    { *Captured._Field = IsLight ? Captured._LightValue : Captured._DarkValue; }

    for (const auto& Captured : _BrushFields)
    { *Captured._Field = IsLight ? Captured._LightValue : Captured._DarkValue; }

    using namespace ck_editor_theme_light_style_patch;
    for (const auto& Captured : _NamedColors)
    {
        if (auto* Color = (Captured._StyleSet->*(&FStyleSetAccess::SlateColorValues)).Find(Captured._Key))
        { *Color = IsLight ? Captured._LightValue : Captured._DarkValue; }
    }

    _Applied = InBrightness;
    return true;
}

auto
    FCk_EditorTheme_LightStylePatch::
    DoCapture_Field(
        FSlateColor& InField,
        const FSlateColor& InLightValue)
    -> void
{
    if (_Fields.ContainsByPredicate([&](const TCapturedField<FSlateColor>& InCaptured) { return InCaptured._Field == &InField; }))
    { return; }

    _Fields.Add({&InField, InField, InLightValue});
}

auto
    FCk_EditorTheme_LightStylePatch::
    DoCapture_Field(
        FLinearColor& InField,
        const FLinearColor& InLightValue)
    -> void
{
    if (_LinearFields.ContainsByPredicate([&](const TCapturedField<FLinearColor>& InCaptured) { return InCaptured._Field == &InField; }))
    { return; }

    _LinearFields.Add({&InField, InField, InLightValue});
}

auto
    FCk_EditorTheme_LightStylePatch::
    DoCapture_Brush(
        FSlateBrush& InBrush,
        const FSlateBrush& InLightValue)
    -> void
{
    if (_BrushFields.ContainsByPredicate([&](const TCapturedField<FSlateBrush>& InCaptured) { return InCaptured._Field == &InBrush; }))
    { return; }

    _BrushFields.Add({&InBrush, InBrush, InLightValue});
}

auto
    FCk_EditorTheme_LightStylePatch::
    DoCapture_NamedColor(
        FSlateStyleSet& InStyleSet,
        FName InKey,
        const FSlateColor& InLightValue)
    -> void
{
    using namespace ck_editor_theme_light_style_patch;

    const auto* Color = (InStyleSet.*(&FStyleSetAccess::SlateColorValues)).Find(InKey);
    if (Color == nullptr
        || _NamedColors.ContainsByPredicate([&](const FCapturedNamedColor& InCaptured)
            { return InCaptured._StyleSet == &InStyleSet && InCaptured._Key == InKey; }))
    { return; }

    _NamedColors.Add({&InStyleSet, InKey, *Color, InLightValue});
}

// --------------------------------------------------------------------------------------------------------------------
