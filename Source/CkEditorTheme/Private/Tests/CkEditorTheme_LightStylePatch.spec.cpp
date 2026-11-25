#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEditorTheme/Styling/CkEditorTheme_LightStylePatch.h"

#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_light_style_patch_tests
{
    auto Make_MenuRow()
        -> FTableRowStyle
    {
        return FTableRowStyle()
            .SetEvenRowBackgroundBrush(FSlateNoResource())
            .SetOddRowBackgroundBrush(FSlateNoResource())
            .SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Primary))
            .SetActiveHoveredBrush(FSlateColorBrush(FStyleColors::Primary))
            .SetActiveBrush(FSlateNoResource())
            .SetTextColor(FStyleColors::White)
            .SetSelectedTextColor(FStyleColors::White);
    }

    auto Make_MajorTab()
        -> FDockTabStyle
    {
        return FDockTabStyle()
            .SetHoveredBrush(FSlateColorBrush(FLinearColor{0.02f, 0.02f, 0.02f, 0.8f}))
            .SetNormalForegroundColor(FStyleColors::Foreground)
            .SetActiveForegroundColor(FStyleColors::White)
            .SetForegroundForegroundColor(FStyleColors::White)
            .SetHoveredForegroundColor(FStyleColors::White);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_LightStylePatch_LightAndBack,
    "Ck.EditorTheme.LightStylePatch.LightAndBack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_LightStylePatch_LightAndBack::RunTest(const FString& Parameters)
{
    using namespace ck_editor_theme_light_style_patch_tests;

    auto StyleSet = FSlateStyleSet{TEXT("CkEditorThemeLightStylePatchTest")};
    StyleSet.Set("MenuRow", Make_MenuRow());
    StyleSet.Set("OwnBackgroundRow", FTableRowStyle(Make_MenuRow()).SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Recessed)));
    StyleSet.Set("MajorTab", Make_MajorTab());
    StyleSet.Set("MinorTab", FDockTabStyle()
        .SetHoveredBrush(FSlateColorBrush(FStyleColors::Panel))
        .SetActiveForegroundColor(FStyleColors::ForegroundHover));

    StyleSet.Set("Graph.Node.NodeTitle", FTextBlockStyle().SetColorAndOpacity(FStyleColors::Foreground));
    StyleSet.Set("Graph.Node.PinName", FTextBlockStyle().SetColorAndOpacity(FStyleColors::ForegroundHover));
    StyleSet.Set("Graph.TransitionNode.TooltipName", FTextBlockStyle().SetColorAndOpacity(FStyleColors::Foreground));
    StyleSet.Set("Outliner.Text", FTextBlockStyle().SetColorAndOpacity(FStyleColors::Foreground));
    StyleSet.Set("NormalText.Important", FTextBlockStyle()
        .SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
        .SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f)));
    StyleSet.Set("FlatButton.DefaultTextStyle", FTextBlockStyle()
        .SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
        .SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f)));

    StyleSet.Set("Window.MinMaxRestoreButtonHover", FButtonStyle()
        .SetNormal(FSlateNoResource())
        .SetHovered(FSlateColorBrush(FColor(255, 255, 255, 40))));
    StyleSet.Set("SolidWhiteButton", FButtonStyle().SetHovered(FSlateColorBrush(FLinearColor::White)));
    StyleSet.Set("ToggleButtonCheckboxAlt", FCheckBoxStyle()
        .SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
        .SetUncheckedImage(FSlateNoResource())
        .SetCheckedImage(FSlateRoundedBoxBrush(FStyleColors::Dropdown, 4.0f)));
    const auto AccentToggle = FCheckBoxStyle()
        .SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
        .SetCheckedImage(FSlateRoundedBoxBrush(FStyleColors::Primary, 4.0f))
        .SetCheckedForegroundColor(FStyleColors::ForegroundHover)
        .SetCheckedHoveredForegroundColor(FStyleColors::ForegroundHover)
        .SetCheckedPressedForegroundColor(FStyleColors::ForegroundHover);
    StyleSet.Set("ToggleButtonCheckbox", AccentToggle);
    StyleSet.Set("SlimToolBar", FToolBarStyle().SetToggleButtonStyle(AccentToggle));
    StyleSet.Set("Graph.CommentBubble.EditableText", FEditableTextBoxStyle()
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FStyleColors::Foreground, 4.0f))
        .SetFocusedForegroundColor(FStyleColors::White));
    StyleSet.Set("Menu.RadioButton", FCheckBoxStyle()
        .SetUncheckedImage(FSlateRoundedBoxBrush(FStyleColors::Input, 4.0f))
        .SetUncheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::Input, 4.0f))
        .SetCheckedImage(FSlateRoundedBoxBrush(FStyleColors::White, 4.0f))
        .SetUndeterminedImage(FSlateRoundedBoxBrush(FStyleColors::Input, 4.0f)));
    StyleSet.Set("SearchBox", FSearchBoxStyle().SetTextBoxStyle(FEditableTextBoxStyle()
        .SetForegroundColor(FStyleColors::Foreground)
        .SetFocusedForegroundColor(FStyleColors::White)));
    StyleSet.Set("SpinBox", FSpinBoxStyle()
        .SetInactiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Dropdown, 4.0f))
        .SetHoveredFillBrush(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
        .SetActiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Primary, 4.0f)));
    StyleSet.Set("Hyperlink", FHyperlinkStyle().SetUnderlineStyle(FButtonStyle()
        .SetNormal(FSlateBorderBrush(TEXT("Slate/Old/HyperlinkDotted.png"), FMargin(0.0f, 0.0f, 0.0f, 0.2f)))
        .SetPressed(FSlateNoResource())
        .SetHovered(FSlateBorderBrush(TEXT("Slate/Old/HyperlinkUnderline.png"), FMargin(0.0f, 0.0f, 0.0f, 0.2f)))));
    StyleSet.Set("AppIcon.Small", new FSlateColorBrush(FStyleColors::Foreground));
    StyleSet.Set("AssetThumbnail.AssetBackground", new FSlateColorBrush(FStyleColors::Recessed));

    auto Patch = FCk_EditorTheme_LightStylePatch{};
    // Menu row: text, selected text, two accent hover brushes. Major tab: three White titles and the baked hover.
    // Graph: node title and pin name (not the tooltip text, not a non-graph style). Emphasis text: color and shadow
    // (not FlatButton text, which sits on a colored button). Overlay button: the translucent-white hover (not a solid
    // white brush). Search box: the focused foreground. Segmented toggle: the checked fill. Menu radio: three
    // unchecked/undetermined dots plus the White checked dot. Accent toggle and toolbar toggle: three checked labels
    // each (not the comment bubble, whose box is filled with the ink). Spin box: inactive and hovered fill (not the
    // accent active fill). Hyperlink: both underline strips. Brushes: the small app icon and the thumbnail tile.
    TestEqual(TEXT("Captured fields"), Patch.Request_Capture(StyleSet), 31);
    TestEqual(TEXT("Capturing again adds nothing"), Patch.Request_Capture(StyleSet), 0);

    const auto& MenuRow = StyleSet.GetWidgetStyle<FTableRowStyle>("MenuRow");
    const auto& MajorTab = StyleSet.GetWidgetStyle<FDockTabStyle>("MajorTab");
    const auto& MinorTab = StyleSet.GetWidgetStyle<FDockTabStyle>("MinorTab");
    const auto BakedHover = MajorTab.HoveredBrush.TintColor;

    TestTrue(TEXT("Applying Light changes styles"), Patch.Request_Apply(ECk_EditorTheme_Brightness::Light));
    TestTrue(TEXT("Light: menu row text uses Foreground"), MenuRow.TextColor == FStyleColors::Foreground);
    TestTrue(TEXT("Light: menu row hover is the subtle Hover"), MenuRow.EvenRowBackgroundHoveredBrush.TintColor == FStyleColors::Hover);
    TestTrue(TEXT("Light: active tab title is dark"), MajorTab.ActiveForegroundColor == FStyleColors::ForegroundHover);
    TestTrue(TEXT("Light: hovered tab title is dark"), MajorTab.HoveredForegroundColor == FStyleColors::ForegroundHover);
    TestTrue(TEXT("Light: tab hover background follows the theme"), MajorTab.HoveredBrush.TintColor == FStyleColors::Hover);
    TestTrue(TEXT("A theme-slot tab hover is untouched"), MinorTab.HoveredBrush.TintColor == FStyleColors::Panel);
    TestTrue(TEXT("A non-White tab title is untouched"), MinorTab.ActiveForegroundColor == FStyleColors::ForegroundHover);
    TestTrue(TEXT("A row with its own background is untouched"),
        StyleSet.GetWidgetStyle<FTableRowStyle>("OwnBackgroundRow").TextColor == FStyleColors::White);
    const auto DarkForeground = FSlateColor{FLinearColor::FromSRGBColor(FColor{0xC0, 0xC0, 0xC0})};
    TestTrue(TEXT("Light: graph node title keeps the Dark ink"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitle").ColorAndOpacity == DarkForeground);
    TestTrue(TEXT("Light: graph pin name keeps the Dark emphasis ink"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.PinName").ColorAndOpacity == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Graph tooltip text still follows the theme"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.TransitionNode.TooltipName").ColorAndOpacity == FStyleColors::Foreground);
    TestTrue(TEXT("Non-graph text still follows the theme"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Outliner.Text").ColorAndOpacity == FStyleColors::Foreground);
    const auto& Important = StyleSet.GetWidgetStyle<FTextBlockStyle>("NormalText.Important");
    TestTrue(TEXT("Light: emphasis text uses the emphasis ink"), Important.ColorAndOpacity == FStyleColors::ForegroundHover);
    TestTrue(TEXT("Light: emphasis text drops its shadow"), Important.ShadowColorAndOpacity.A == 0.0f);
    TestTrue(TEXT("FlatButton text stays white"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("FlatButton.DefaultTextStyle").ColorAndOpacity == FSlateColor{FLinearColor::White});
    const auto& OverlayHover = StyleSet.GetWidgetStyle<FButtonStyle>("Window.MinMaxRestoreButtonHover").Hovered.TintColor.GetSpecifiedColor();
    TestTrue(TEXT("Light: white overlay becomes a black overlay of the same strength"),
        OverlayHover.R == 0.0f && FMath::IsNearlyEqual(OverlayHover.A, 40.0f / 255.0f, 0.01f));
    TestTrue(TEXT("A solid white button brush is untouched"),
        StyleSet.GetWidgetStyle<FButtonStyle>("SolidWhiteButton").Hovered.TintColor == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Light: text typed into a focused search box is dark"),
        StyleSet.GetWidgetStyle<FSearchBoxStyle>("SearchBox").TextBoxStyle.FocusedForegroundColor == FStyleColors::ForegroundHover);
    TestTrue(TEXT("Light: the selected segment is visibly filled"),
        StyleSet.GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckboxAlt").CheckedImage.TintColor == FStyleColors::SelectInactive);
    const auto& Radio = StyleSet.GetWidgetStyle<FCheckBoxStyle>("Menu.RadioButton");
    TestTrue(TEXT("Light: unchecked menu radio dot hidden"), Radio.UncheckedImage.TintColor == FStyleColors::Transparent);
    TestTrue(TEXT("Light: checked menu radio dot uses the accent"), Radio.CheckedImage.TintColor == FStyleColors::Primary);
    TestTrue(TEXT("Light: accent toggle label is white"),
        StyleSet.GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox").CheckedForeground == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Light: toolbar toggle label is white"),
        StyleSet.GetWidgetStyle<FToolBarStyle>("SlimToolBar").ToggleButton.CheckedForeground == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("A text box filled with the ink keeps its white focused text"),
        StyleSet.GetWidgetStyle<FEditableTextBoxStyle>("Graph.CommentBubble.EditableText").FocusedForegroundColor == FStyleColors::White);
    const auto& SpinBox = StyleSet.GetWidgetStyle<FSpinBoxStyle>("SpinBox");
    TestTrue(TEXT("Light: spin box value fill is visible"), SpinBox.InactiveFillBrush.TintColor == FStyleColors::Hover);
    TestTrue(TEXT("Light: hovered spin box fill is darker"), SpinBox.HoveredFillBrush.TintColor == FStyleColors::Hover2);
    TestTrue(TEXT("The accent active fill is untouched"), SpinBox.ActiveFillBrush.TintColor == FStyleColors::Primary);
    const auto& Underline = StyleSet.GetWidgetStyle<FHyperlinkStyle>("Hyperlink").UnderlineStyle;
    TestTrue(TEXT("Light: hyperlink underline uses the ink"), Underline.Normal.TintColor == FStyleColors::Foreground);
    TestTrue(TEXT("Light: hovered hyperlink underline uses the ink"), Underline.Hovered.TintColor == FStyleColors::Foreground);
    TestTrue(TEXT("Light: small app icon draws untinted"), StyleSet.GetBrush("AppIcon.Small")->TintColor == FStyleColors::White);
    TestTrue(TEXT("Light: thumbnail tile stays dark"),
        StyleSet.GetBrush("AssetThumbnail.AssetBackground")->TintColor.GetSpecifiedColor().GetLuminance() < 0.1f);
    TestFalse(TEXT("Applying Light twice is a no-op"), Patch.Request_Apply(ECk_EditorTheme_Brightness::Light));

    TestTrue(TEXT("Applying Dark changes styles"), Patch.Request_Apply(ECk_EditorTheme_Brightness::Dark));
    TestTrue(TEXT("Dark: menu row text restored"), MenuRow.TextColor == FStyleColors::White);
    TestTrue(TEXT("Dark: menu row hover restored"), MenuRow.EvenRowBackgroundHoveredBrush.TintColor == FStyleColors::Primary);
    TestTrue(TEXT("Dark: tab title restored"), MajorTab.ActiveForegroundColor == FStyleColors::White);
    TestTrue(TEXT("Dark: tab hover restored"), MajorTab.HoveredBrush.TintColor == BakedHover);
    TestTrue(TEXT("Dark: accent toggle label back on the theme slot"),
        StyleSet.GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox").CheckedForeground == FStyleColors::ForegroundHover);
    TestTrue(TEXT("Dark: emphasis text white again"), Important.ColorAndOpacity == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Dark: emphasis shadow restored"), FMath::IsNearlyEqual(Important.ShadowColorAndOpacity.A, 0.9f));
    TestTrue(TEXT("Dark: spin box fill restored"), SpinBox.InactiveFillBrush.TintColor == FStyleColors::Dropdown);
    TestTrue(TEXT("Dark: hyperlink underline untinted again"), Underline.Normal.TintColor == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Dark: thumbnail tile back on the theme slot"),
        StyleSet.GetBrush("AssetThumbnail.AssetBackground")->TintColor == FStyleColors::Recessed);
    TestTrue(TEXT("Dark: graph node title back on the theme slot"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitle").ColorAndOpacity == FStyleColors::Foreground);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_LightStylePatch_LightGraphNodes,
    "Ck.EditorTheme.LightStylePatch.LightGraphNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_LightStylePatch_LightGraphNodes::RunTest(const FString& Parameters)
{
    auto StyleSet = FSlateStyleSet{TEXT("CkEditorThemeLightGraphNodesTest")};
    StyleSet.Set("Graph.Node.PinLabelColor", FSlateColor{FLinearColor::White});
    StyleSet.Set("Graph.Node.NodeTitle", FTextBlockStyle().SetColorAndOpacity(FStyleColors::Foreground));
    StyleSet.Set("Graph.Node.PinName", FTextBlockStyle().SetColorAndOpacity(FStyleColors::Foreground));
    StyleSet.Set("Graph.Node.NodeTitleExtraLines", FTextBlockStyle().SetColorAndOpacity(FLinearColor{0.85f, 0.85f, 0.38f, 0.5f}));
    StyleSet.Set("Graph.CompactNode.Title", FTextBlockStyle().SetColorAndOpacity(FLinearColor{1.0f, 1.0f, 1.0f, 0.5f}));

    auto Patch = FCk_EditorTheme_LightStylePatch{};
    TestFalse(TEXT("No light graph nodes before a capture"), Patch.Get_LightGraphNodes());
    TestEqual(TEXT("The pin label ink, node title, title subline and compact title are captured; other graph text follows the theme"),
        Patch.Request_Capture(StyleSet), 4);
    TestTrue(TEXT("The engine's pin label key turns light graph nodes on"), Patch.Get_LightGraphNodes());

    Patch.Request_Apply(ECk_EditorTheme_Brightness::Light);
    TestTrue(TEXT("Light: pin labels use the ink"), StyleSet.GetSlateColor("Graph.Node.PinLabelColor") == FStyleColors::Foreground);
    TestTrue(TEXT("Light: node title stays white on its colored band"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitle").ColorAndOpacity == FSlateColor{FLinearColor::White});
    const auto Subline = StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitleExtraLines").ColorAndOpacity.GetSpecifiedColor();
    TestTrue(TEXT("Light: title subline is soft white"), Subline.R == 1.0f && Subline.B == 1.0f && FMath::IsNearlyEqual(Subline.A, 0.8f));
    const auto Compact = StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.CompactNode.Title").ColorAndOpacity.GetSpecifiedColor();
    TestTrue(TEXT("Light: compact node label is dark ink at the same strength"), Compact.R < 0.1f && FMath::IsNearlyEqual(Compact.A, 0.5f));
    TestTrue(TEXT("Light: other node text follows the theme"),
        StyleSet.GetWidgetStyle<FTextBlockStyle>("Graph.Node.PinName").ColorAndOpacity == FStyleColors::Foreground);

    Patch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
    TestTrue(TEXT("Dark: pin labels white again"), StyleSet.GetSlateColor("Graph.Node.PinLabelColor") == FSlateColor{FLinearColor::White});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_LightStylePatch_GraphChrome,
    "Ck.EditorTheme.LightStylePatch.GraphChrome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_LightStylePatch_GraphChrome::RunTest(const FString& Parameters)
{
    auto StyleSet = FSlateStyleSet{TEXT("CkEditorThemeGraphChromeTest")};
    StyleSet.Set("Graph.Node.PinLabelColor", FSlateColor{FLinearColor::White});
    StyleSet.Set("Graph.TitleBackground", new FSlateColorBrush(FLinearColor{0.0f, 0.0f, 0.0f, 0.5f}));
    StyleSet.Set("GraphBreadcrumbButtonText", FTextBlockStyle().SetColorAndOpacity(FLinearColor{1.0f, 1.0f, 1.0f, 0.5f}));
    StyleSet.Set("Graph.Function.FunctionParameterIcon", new FSlateColorBrush(FLinearColor::White));
    StyleSet.Set("Graph.Function.FunctionLocalVariableIcon", new FSlateColorBrush(FLinearColor::White));
    StyleSet.Set("Graph.Node.DevelopmentBanner", new FSlateImageBrush(FName{TEXT("/Graph/GraphPanel_StripesBackground.png")},
        FVector2f{64.0f, 64.0f}, FLinearColor{1.0f, 1.0f, 0.0f, 0.3f}, ESlateBrushTileType::Both));
    StyleSet.Set("Graph.Node.DisabledBanner", new FSlateImageBrush(FName{TEXT("/Graph/GraphPanel_StripesBackground.png")},
        FVector2f{64.0f, 64.0f}, FLinearColor{0.5f, 0.5f, 0.5f, 0.3f}, ESlateBrushTileType::Both));
    StyleSet.Set("GraphBreadcrumb.BrowseBack", new FSlateColorBrush(FLinearColor::White));
    StyleSet.Set("GraphBreadcrumb.BrowseForward", new FSlateColorBrush(FLinearColor::White));

    auto Patch = FCk_EditorTheme_LightStylePatch{};
    TestEqual(TEXT("Pin label ink, title bar, breadcrumbs, two history arrows, two corner glyphs and both banners are captured"),
        Patch.Request_Capture(StyleSet), 9);

    const auto* TitleBar = StyleSet.GetBrush("Graph.TitleBackground");
    const auto* Glyph = StyleSet.GetBrush("Graph.Function.FunctionParameterIcon");
    const auto* Banner = StyleSet.GetBrush("Graph.Node.DevelopmentBanner");
    const auto& Breadcrumb = StyleSet.GetWidgetStyle<FTextBlockStyle>("GraphBreadcrumbButtonText");

    Patch.Request_Apply(ECk_EditorTheme_Brightness::Light);
    TestTrue(TEXT("Light: the title bar is the Title fill, in place"), TitleBar->TintColor == FStyleColors::Title);
    TestTrue(TEXT("Light: breadcrumbs are dark ink"), Breadcrumb.ColorAndOpacity.GetSpecifiedColor().R < 0.1f);
    TestTrue(TEXT("Light: the corner glyph takes the ink"), Glyph->TintColor == FStyleColors::Foreground);
    TestTrue(TEXT("Light: the history arrow takes the ink"), StyleSet.GetBrush("GraphBreadcrumb.BrowseBack")->TintColor == FStyleColors::Foreground);
    TestTrue(TEXT("Light: the banner stripes are at full strength"), Banner->TintColor.GetSpecifiedColor().A == 1.0f);
    TestTrue(TEXT("Light: the disabled banner stripes are at full strength"), StyleSet.GetBrush("Graph.Node.DisabledBanner")->TintColor.GetSpecifiedColor().A == 1.0f);
    TestTrue(TEXT("Light: the banner keeps its stripes, tiled"),
        Banner->GetResourceName() == FName{TEXT("/Graph/GraphPanel_StripesBackground.png")} && Banner->GetTiling() == ESlateBrushTileType::Both);

    Patch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
    TestTrue(TEXT("Dark: the title bar is restored"), FMath::IsNearlyEqual(TitleBar->TintColor.GetSpecifiedColor().A, 0.5f));
    TestTrue(TEXT("Dark: breadcrumbs white again"), Breadcrumb.ColorAndOpacity.GetSpecifiedColor().R == 1.0f);
    TestTrue(TEXT("Dark: the corner glyph untinted again"), Glyph->TintColor == FSlateColor{FLinearColor::White});
    TestTrue(TEXT("Dark: the banner restored"), FMath::IsNearlyEqual(Banner->TintColor.GetSpecifiedColor().A, 0.3f));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
