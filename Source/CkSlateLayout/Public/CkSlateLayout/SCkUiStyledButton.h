#pragma once

#include "CkSlateLayout/CkUiDocument.h"

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"

struct FSlateRoundedBoxBrush;

/**
 * Foundation-owned authored button whose FButtonStyle and state brushes remain
 * alive for the complete native button lifetime.
 */
class CKSLATELAYOUT_API SCkUiStyledButton final : public SButton
{
public:
    SLATE_BEGIN_ARGS(SCkUiStyledButton)
        : _VisualStyle()
        , _IsEnabled(true)
        , _ToolTipText(FText::GetEmpty())
        , _Tag(NAME_None)
    {}
        SLATE_ARGUMENT(FCkUiButtonVisualStyle, VisualStyle)
        SLATE_ATTRIBUTE(bool, IsEnabled)
        SLATE_ATTRIBUTE(FText, ToolTipText)
        SLATE_EVENT(FOnClicked, OnClicked)
        SLATE_ARGUMENT(FName, Tag)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    auto IsAuthoredFocused() const -> bool { return _FocusActive; }
    auto GetFocusBrush() const -> const FSlateBrush* override;
    auto OnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) -> FReply override;
    void OnFocusLost(const FFocusEvent& InFocusEvent) override;
    auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry, const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override;

private:
    static auto MakeButtonStyle(const FCkUiButtonVisualStyle& InVisualStyle) -> FButtonStyle;

    FButtonStyle _ButtonStyle;
    FButtonStyle _FocusedButtonStyle;
    TSharedPtr<FSlateRoundedBoxBrush> _FocusOutlineBrush;
    float _FocusOutlineGap = 2.0f;
    float _FocusOutlineWidth = 2.0f;
    bool _FocusActive = false;
    bool _HasAuthoredFocusStyle = false;
};
