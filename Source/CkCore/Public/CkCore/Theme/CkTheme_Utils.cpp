#include "CkTheme_Utils.h"

#include <Styling/StyleColors.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::theme
{
    auto
    Get_IsLightBackground(
        const FLinearColor& InBackground) -> bool
    {
        return InBackground.GetLuminance() > 0.5f;
    }

    auto
    Get_IsEditorThemeLight() -> bool
    {
        return Get_IsLightBackground(USlateThemeManager::Get().GetColor(EStyleColor::Background));
    }
}

// --------------------------------------------------------------------------------------------------------------------
