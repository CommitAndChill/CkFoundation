#include "CkEditorTheme_GraphPatch.h"

#include "CkCore/Macros/CkMacros.h"

#include "GraphEditorSettings.h"
#include "Settings/EditorStyleSettings.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_graph_patch
{
    auto Get_SRGB(
        uint8 InValue)
        -> FLinearColor
    {
        return FLinearColor::FromSRGBColor(FColor{InValue, InValue, InValue});
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_GraphPatch::
    Request_Apply(
        TArrayView<const FSwap> InSwaps,
        ECk_EditorTheme_Brightness InBrightness)
    -> bool
{
    const auto IsLight = InBrightness == ECk_EditorTheme_Brightness::Light;
    auto AnyChanged = false;

    for (const auto& Swap : InSwaps)
    {
        const auto& From = IsLight ? Swap._DarkValue : Swap._LightValue;
        const auto& To = IsLight ? Swap._LightValue : Swap._DarkValue;

        if (NOT Swap._Field->Equals(From))
        { continue; }

        *Swap._Field = To;
        AnyChanged = true;
    }

    return AnyChanged;
}

auto
    FCk_EditorTheme_GraphPatch::
    Request_Apply(
        ECk_EditorTheme_Brightness InBrightness)
    -> bool
{
    return Request_Apply(Get_Swaps(), InBrightness);
}

auto
    FCk_EditorTheme_GraphPatch::
    Get_Swaps() const
    -> TArray<FSwap>
{
    using namespace ck_editor_theme_graph_patch;

    // Dark values are the engine defaults (EditorStyleClasses.cpp, GraphEditorSettings.cpp); the Light grid sits a few
    // steps below the #EB canvas, and the execution wire is a mid gray that reads on both the canvas and dark nodes.
    auto* Style = GetMutableDefault<UEditorStyleSettings>();
    auto* Graph = GetMutableDefault<UGraphEditorSettings>();

    return {
        {&Style->RegularColor, FLinearColor{0.024f, 0.024f, 0.024f}, Get_SRGB(0xE0)},
        {&Style->RuleColor, FLinearColor{0.010f, 0.010f, 0.010f}, Get_SRGB(0xD4)},
        {&Style->CenterColor, FLinearColor{0.005f, 0.005f, 0.005f}, Get_SRGB(0xC4)},
        {&Graph->ExecutionPinTypeColor, FLinearColor::White, Get_SRGB(0x80)}
    };
}

// --------------------------------------------------------------------------------------------------------------------
