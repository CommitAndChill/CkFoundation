#pragma once

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkSettings/UserSettings/CkUserSettings.h"

#include "CkEditorTheme_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECk_EditorTheme_Mode : uint8
{
    Manual,
    Dark,
    Light,
    FollowWindows UMETA(DisplayName = "Follow Windows")
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(meta = (DisplayName = "Editor Theme"))
class CKEDITORTHEME_API UCk_EditorTheme_UserSettings_UE : public UCk_Plugin_UserSettings_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EditorTheme_UserSettings_UE);

public:
    auto GetCategoryName() const -> FName override { return TEXT("Ck"); }
    auto GetSectionName() const -> FName override { return TEXT("Editor Theme"); }

private:
    // Manual leaves the theme to Editor Preferences > Appearance; picking a theme there while another mode is
    // active switches this back to Manual.
    UPROPERTY(Config, EditAnywhere, Category = "Theme",
              meta = (AllowPrivateAccess = true))
    ECk_EditorTheme_Mode _Mode = ECk_EditorTheme_Mode::Manual;

    // Legacy PNG icons with a baked dark outline turn into dark blobs on Light; draw them as outline-only glyphs.
    UPROPERTY(Config, EditAnywhere, Category = "Theme",
              meta = (AllowPrivateAccess = true, DisplayName = "Outline Legacy Icons In Light"))
    ECk_EnableDisable _OutlineLegacyIconsInLight = ECk_EnableDisable::Enable;

public:
    CK_PROPERTY(_Mode);
    CK_PROPERTY(_OutlineLegacyIconsInLight);
};

// --------------------------------------------------------------------------------------------------------------------
