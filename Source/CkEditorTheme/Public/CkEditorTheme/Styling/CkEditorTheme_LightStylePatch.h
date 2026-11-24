#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"

class FSlateStyleSet;

// --------------------------------------------------------------------------------------------------------------------

enum class ECk_EditorTheme_Brightness : uint8
{
    Dark,
    Light
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    CKEDITORTHEME_API auto Get_Brightness() -> ECk_EditorTheme_Brightness;
}

// --------------------------------------------------------------------------------------------------------------------

// Engine styles whose colors are right for Dark but unreadable in Light, and that no theme value can fix because the
// same slot is correct elsewhere. Light writes a replacement into each captured field; Dark restores what was captured.
//  - Dropdown rows (ComboBox.Row and copies): literal White text on the menu, accent hover.
//  - App/document dock tabs (Docking.MajorTab): literal White active/hovered titles.
//  - Dock tabs' hovered background: Panel baked to its Dark value when the style was built.
//  - Accent-filled toggles: checked labels use ForegroundHover, white in Dark and dark ink in Light.
//  - Graph.* text styles: without the engine's Graph.Node.PinLabelColor (pin labels literal white) node art stays dark,
//    so their text keeps the Dark ink. With it, nodes go light, node text follows the theme and pin labels use the ink.
//  - Emphasis text (NormalText.Important, LargeText): literal white with a black drop shadow, used as headings on
//    panels; Light uses the emphasis ink and drops the shadow.
//  - Button overlays that are translucent white (window min/max hover, hover hints): Light uses translucent black.
//  - Focused text boxes (incl. search boxes): typed text is literal White on the white input.
//  - Segmented toggles: the selected segment is filled with Dropdown, the menu color itself in Light.
//  - Menu radio items: unchecked dots are hidden so only the selected item shows a dot.
//  - Check boxes (incl. Menu.CheckBox) whose check mark is White: Light uses the accent, like the standard check box.
//  - Spin boxes: the value fill is Dropdown/Secondary, the input fill itself in Light; Light uses Hover/Hover2.
//  - Hyperlink underlines: untinted white strips; Light tints them with the ink.
//  - AppIcon.Small: the full-color logo tinted with the ink; Light draws it untinted.
//  - AssetThumbnail.AssetBackground: class icons are white art, so Light keeps their tile dark.
//  - MainFrame.DebugTools.Label/ValueColor (engine patch 03): the title-bar stats ink.
class CKEDITORTHEME_API FCk_EditorTheme_LightStylePatch
{
public:
    auto Request_Capture(
        FSlateStyleSet& InStyleSet) -> int32;

    auto Request_Apply(
        ECk_EditorTheme_Brightness InBrightness) -> bool;

    // True once a captured style set carries the engine's Graph.Node.PinLabelColor (or the dev CVar forces it).
    auto Get_LightGraphNodes() const -> bool { return _LightGraphNodes; }

private:
    auto DoCapture_Field(
        FSlateColor& InField,
        const FSlateColor& InLightValue) -> void;

    auto DoCapture_Field(
        FLinearColor& InField,
        const FLinearColor& InLightValue) -> void;

    // Named style colors live inline in the style set's map, so they are found again on every apply instead of held.
    auto DoCapture_NamedColor(
        FSlateStyleSet& InStyleSet,
        FName InKey,
        const FSlateColor& InLightValue) -> void;

private:
    struct FCapturedNamedColor
    {
        FSlateStyleSet* _StyleSet = nullptr;
        FName _Key;
        FSlateColor _DarkValue;
        FSlateColor _LightValue;
    };

    TArray<FCapturedNamedColor> _NamedColors;

    template <typename T_Color>
    struct TCapturedField
    {
        T_Color* _Field = nullptr;
        T_Color _DarkValue;
        T_Color _LightValue;
    };

    TArray<TCapturedField<FSlateColor>> _Fields;
    TArray<TCapturedField<FLinearColor>> _LinearFields;
    TOptional<ECk_EditorTheme_Brightness> _Applied;
    bool _LightGraphNodes = false;
};

// --------------------------------------------------------------------------------------------------------------------
