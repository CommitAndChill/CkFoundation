#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <Kismet/BlueprintFunctionLibrary.h>
#include <Styling/SlateColor.h>

#include "CkEditorToolbar_Style_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class CKEDITORTOOLBAR_API UCk_Utils_EditorToolbar_Style_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_EditorToolbar_Style_UE);

public:
    // For a toolbar widget's text binding: the hue itself in a dark editor theme, darkened to stay legible in a light one
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EditorToolbar",
              DisplayName = "[Ck][EditorToolbar] Get Theme Ink")
    static FSlateColor
    Get_ThemeInk(
        FLinearColor InDarkThemeHue);
};

// --------------------------------------------------------------------------------------------------------------------
