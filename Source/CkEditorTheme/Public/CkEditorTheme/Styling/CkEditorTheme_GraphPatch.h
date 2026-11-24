#pragma once

#include "CkEditorTheme/Styling/CkEditorTheme_LightStylePatch.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

// Graph canvas colors that live in editor settings rather than styles: the grid lines (Editor Preferences > Appearance
// > Graphs) and the white execution wire (Graph Editor > Pin and Wire Colors). Light swaps a value only while it is the
// engine default, and Dark swaps it back only while it is the Light value, so a user's own color is never touched and
// a Light value saved by an unrelated settings save turns back into the default on Dark.
class CKEDITORTHEME_API FCk_EditorTheme_GraphPatch
{
public:
    struct FSwap
    {
        FLinearColor* _Field = nullptr;
        FLinearColor _DarkValue;
        FLinearColor _LightValue;
    };

    static auto Request_Apply(
        TArrayView<const FSwap> InSwaps,
        ECk_EditorTheme_Brightness InBrightness) -> bool;

    auto Request_Apply(
        ECk_EditorTheme_Brightness InBrightness) -> bool;

private:
    auto Get_Swaps() const -> TArray<FSwap>;
};

// --------------------------------------------------------------------------------------------------------------------
