#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
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

// Engine styles that read on Dark but not on Light, where no theme slot can fix them. Light writes a replacement into
// each captured field and Dark restores the capture; the reason for each rule sits beside it in the .cpp.
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

    // Widgets hold brush pointers, so a brush is replaced in place; Dark restores the captured original.
    auto DoCapture_Brush(
        FSlateBrush& InBrush,
        const FSlateBrush& InLightValue) -> void;

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
    TArray<TCapturedField<FSlateBrush>> _BrushFields;
    TOptional<ECk_EditorTheme_Brightness> _Applied;
    bool _LightGraphNodes = false;
};

// --------------------------------------------------------------------------------------------------------------------
