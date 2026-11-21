#include "CkEditorToolbar_Style_Utils.h"

#include "CkEditorTools/Style/CkStyle.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EditorToolbar_Style_UE::
    Get_ThemeInk(
        FLinearColor InDarkThemeHue)
    -> FSlateColor
{
    return CkStyle::InkOf(InDarkThemeHue);
}

// --------------------------------------------------------------------------------------------------------------------
