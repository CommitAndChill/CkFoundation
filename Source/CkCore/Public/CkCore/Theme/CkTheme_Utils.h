#pragma once

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::theme
{
    CKCORE_API auto
    Get_IsLightBackground(
        const FLinearColor& InBackground) -> bool;

    CKCORE_API auto
    Get_IsEditorThemeLight() -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
