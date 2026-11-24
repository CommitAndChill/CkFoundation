#include "CkEditorTheme_Resolver.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    auto
        Get_DarkThemeId()
        -> FGuid
    {
        return FGuid{0x13438026, 0x5FBB4A9C, 0xA00A1DC9, 0x770217B8};
    }

    auto
        Get_LightThemeId()
        -> FGuid
    {
        return FGuid{0x6C1A9E1F, 0x4B7D4E0B, 0x9A512C0F, 0xDA11C0DE};
    }

    auto
        Get_IsWindowsLight(
            const FString& InAppsUseLightTheme)
        -> bool
    {
        return InAppsUseLightTheme.TrimStartAndEnd() == TEXT("1");
    }

    auto
        Get_DesiredThemeId(
            ECk_EditorTheme_Mode InMode,
            const FString& InAppsUseLightTheme)
        -> TOptional<FGuid>
    {
        switch (InMode)
        {
            case ECk_EditorTheme_Mode::Dark:
            { return Get_DarkThemeId(); }
            case ECk_EditorTheme_Mode::Light:
            { return Get_LightThemeId(); }
            case ECk_EditorTheme_Mode::FollowWindows:
            { return Get_IsWindowsLight(InAppsUseLightTheme) ? Get_LightThemeId() : Get_DarkThemeId(); }
            case ECk_EditorTheme_Mode::Manual:
            default:
            { return {}; }
        }
    }

    auto
        Get_ShouldDemoteToManual(
            ECk_EditorTheme_Mode InMode,
            const FGuid& InAppliedThemeId,
            const FGuid& InPreviousThemeId,
            const TOptional<FGuid>& InDesiredThemeId)
        -> bool
    {
        if (InMode == ECk_EditorTheme_Mode::Manual)
        { return false; }

        if (NOT InDesiredThemeId.IsSet())
        { return false; }

        if (InAppliedThemeId == InPreviousThemeId)
        { return false; }

        return InAppliedThemeId != InDesiredThemeId.GetValue();
    }
}

// --------------------------------------------------------------------------------------------------------------------
