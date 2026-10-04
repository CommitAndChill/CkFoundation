#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;

/** Retained, focusable radial choice surface. The owner decides when to show it. */
class CKSLATELAYOUT_API SCkUiRadialSelector final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkUiRadialSelector) {} SLATE_END_ARGS()

    void Construct(const FArguments& InArguments, TWeakPtr<class FCkUiRadialSelectorState> InOwner,
        const TSharedRef<SWidget>& InContent);
    void Freeze();
    void CancelTransient();
    auto IsFrozen() const -> bool { return _Frozen; }
    auto GetSelectedKey() const -> FString;

    virtual auto SupportsKeyboardFocus() const -> bool override;
    virtual auto OnQueryShowFocus(const EFocusCause InFocusCause) const -> TOptional<bool> override;
    virtual void Tick(const FGeometry& InGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry,
        const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 InLayer, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const -> int32 override;
    virtual auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InEvent) -> FReply override;
    virtual void OnMouseLeave(const FPointerEvent& InEvent) override;

private:
    auto HitKey(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> FString;
    auto IsOwner(uint32 InUserIndex) const -> bool;
    void Hover(const FString& InKey);
    void Activate(const FString& InKey);

    TWeakPtr<class FCkUiRadialSelectorState> _Owner;
    FString _HoverKey;
    FString _DownKey;
    int32 _DownUser = INDEX_NONE;
    uint32 _DownPointer = 0;
    bool _DownTouch = false;
    bool _Frozen = false;
    int64 _SeenRevision = INDEX_NONE;
    FString _SeenSelection;
};

/** Registers the reusable radial-selector custom widget. */
CKSLATELAYOUT_API auto RegisterRadialUiControls(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
