#include "CkEditorToolbar_CaptionInk.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkUICore/CkUI_Utils.h"

#include <Components/TextBlock.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_toolbar_caption_ink
{
    // Neutral greys and white are labels that inherit or match the foreground; only hues are accents.
    constexpr auto MinAccentChroma = 0.15f;

    auto Get_AccentHue(
        const FSlateColor& InColor)
        -> TOptional<FLinearColor>
    {
        if (NOT InColor.IsColorSpecified())
        { return {}; }

        const auto Hue = InColor.GetSpecifiedColor();
        if (FMath::Max3(Hue.R, Hue.G, Hue.B) - FMath::Min3(Hue.R, Hue.G, Hue.B) < MinAccentChroma)
        { return {}; }

        return Hue;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_EditorToolbar_CaptionInk_UE::
    Request_BindCaptions(
        UWidget* InRoot,
        UObject* InOuter,
        TArray<TObjectPtr<UCk_EditorToolbar_CaptionInk_UE>>& OutInks)
    -> int32
{
    using namespace ck_editor_toolbar_caption_ink;

    auto Bound = int32{};

    UCk_Utils_UI_UE::ForEachWidgetAndChildren_IncludingUserWidgets(InRoot, [&](UWidget* InWidget) -> ECk_UI_ForEachWidgetResult
    {
        auto* Text = Cast<UTextBlock>(InWidget);
        if (Text == nullptr || Text->ColorAndOpacityDelegate.IsBound())
        { return ECk_UI_ForEachWidgetResult::Continue; }

        const auto Hue = Get_AccentHue(Text->GetColorAndOpacity());
        if (NOT Hue.IsSet())
        { return ECk_UI_ForEachWidgetResult::Continue; }

        auto* Ink = NewObject<UCk_EditorToolbar_CaptionInk_UE>(InOuter);
        Ink->_DarkThemeHue = Hue.GetValue();
        Text->ColorAndOpacityDelegate.BindUFunction(Ink, GET_FUNCTION_NAME_CHECKED(UCk_EditorToolbar_CaptionInk_UE, Get_Ink));
        OutInks.Add(Ink);
        ++Bound;

        return ECk_UI_ForEachWidgetResult::Continue;
    });

    return Bound;
}

auto
    UCk_EditorToolbar_CaptionInk_UE::
    Get_Ink() const
    -> FSlateColor
{
    return CkStyle::InkOf(_DarkThemeHue);
}

// --------------------------------------------------------------------------------------------------------------------
