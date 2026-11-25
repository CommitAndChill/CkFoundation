#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <Styling/SlateColor.h>
#include <UObject/Object.h>

#include "CkEditorToolbar_CaptionInk.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UWidget;

// Binds a toolbar caption's colour to the theme: the hue itself in Dark, darkened to stay legible in Light. The colour
// is evaluated every frame, so a live theme switch needs no rebuild.
UCLASS(Transient)
class CKEDITORTOOLBAR_API UCk_EditorToolbar_CaptionInk_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EditorToolbar_CaptionInk_UE);

public:
    // Binds every text block with a coloured (not grey, not foreground) Specified colour under InRoot.
    static auto
    Request_BindCaptions(
        UWidget* InRoot,
        UObject* InOuter,
        TArray<TObjectPtr<UCk_EditorToolbar_CaptionInk_UE>>& OutInks) -> int32;

private:
    UFUNCTION()
    FSlateColor
    Get_Ink() const;

private:
    UPROPERTY()
    FLinearColor _DarkThemeHue = FLinearColor::White;
};

// --------------------------------------------------------------------------------------------------------------------
