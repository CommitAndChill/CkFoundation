#pragma once

#include "CkEditorTheme/Settings/CkEditorTheme_Settings.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    // Engine default Dark theme id (StyleColors.cpp, EnsureValidCurrentTheme); identical on 5.7 and 5.8.
    CKEDITORTHEME_API auto Get_DarkThemeId() -> FGuid;
    CKEDITORTHEME_API auto Get_LightThemeId() -> FGuid;

    // InAppsUseLightTheme is HKCU\...\Themes\Personalize\AppsUseLightTheme as QueryRegKey formats it ("1"/"0");
    // anything else, including a missing value, resolves to Dark.
    CKEDITORTHEME_API auto Get_IsWindowsLight(
        const FString& InAppsUseLightTheme) -> bool;

    // Unset for Manual: the plugin leaves the theme alone.
    CKEDITORTHEME_API auto Get_DesiredThemeId(
        ECk_EditorTheme_Mode InMode,
        const FString& InAppsUseLightTheme) -> TOptional<FGuid>;

    // A re-broadcast of the previous theme (Import, a cancelled theme editor) is not a user pick.
    CKEDITORTHEME_API auto Get_ShouldDemoteToManual(
        ECk_EditorTheme_Mode InMode,
        const FGuid& InAppliedThemeId,
        const FGuid& InPreviousThemeId,
        const TOptional<FGuid>& InDesiredThemeId) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
