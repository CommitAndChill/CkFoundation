#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

#include "Input/DragAndDrop.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Input/SButton.h"

class SCkUiItemSlot;
class FCkUiItemSlotOwner;
class FSlateRect;

/** Immutable drag proposal. Its source and document identity are weak. */
class CKSLATELAYOUT_API FCkUiItemSlotDragOperation final : public FDragDropOperation
{
public:
    DRAG_DROP_OPERATOR_TYPE(FCkUiItemSlotDragOperation, FDragDropOperation)
    FCkUiItemSlotDragOperation() = default;

    auto GetKey() const -> const FString& { return _Key; }
    auto GetKind() const -> const FString& { return _Kind; }
    auto GetDragLabel() const -> const FString& { return _DragLabel; }
    auto GetDragImageBrush() const -> const FSlateBrush* { return _DragBrush.Get(); }
    auto GetDefaultDecorator() const -> TSharedPtr<SWidget> override;

private:
    friend class SCkUiItemSlot;
    friend class FCkUiItemSlotOwner;
    struct FCollectionRevision
    {
        TWeakPtr<FCkUiCollection> Collection;
        int64 Revision = 0;
    };
    TWeakPtr<SCkUiItemSlot> _Source;
    TWeakPtr<FCkUiItemSlotOwner> _VisualOwner;
    TWeakPtr<int32> _Domain;
    uint64 _Generation = 0;
    bool _Consumed = false;
    TArray<FCollectionRevision> _Collections;
    FString _Key;
    FString _Kind;
    FString _DragLabel;
    TSharedPtr<FSlateBrush> _DragBrush;
};

/** Focusable authored item or equipment slot. Drop proposals never mutate model state. */
class CKSLATELAYOUT_API SCkUiItemSlot final : public SButton
{
public:
    SLATE_BEGIN_ARGS(SCkUiItemSlot) {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TWeakPtr<FCkUiItemSlotOwner> InOwner);
    auto GetItemKey() const -> FString;
    auto GetItemKind() const -> FString;
    auto TryBeginDrag() -> TSharedPtr<FCkUiItemSlotDragOperation>;
    auto CanAcceptDrag(const TSharedPtr<FCkUiItemSlotDragOperation>& InDrag) const -> bool;
    auto TryAcceptDrag(const TSharedPtr<FCkUiItemSlotDragOperation>& InDrag) -> bool;
    auto HasAuthoredTooltip() const -> bool;
    auto GetTooltipMaxSize() const -> FVector2D;
    auto GetIsTooltipOpen() const -> bool;
    /** Returns actual popup and owner-client bounds in screen space after layout. */
    auto TryGetTooltipScreenBounds(FSlateRect& OutPopup, FSlateRect& OutViewport) const -> bool;
    auto RefreshStyle() -> void;

    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual void OnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& InEvent) override;
    virtual auto OnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual void OnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InEvent) override;
    virtual void OnDragLeave(const FDragDropEvent& InEvent) override;
    virtual auto OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InEvent) -> FReply override;
    virtual auto OnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InEvent) -> FReply override;
    virtual void OnFocusLost(const FFocusEvent& InEvent) override;
    virtual void Tick(const FGeometry& InGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    friend class FCkUiItemSlotOwner;
    TWeakPtr<FCkUiItemSlotOwner> _Owner;
    FButtonStyle _Style;
    bool _Focused = false;
    bool _DragOver = false;
    bool _DragValid = false;
    bool _LastSelected = false;
    float _LastRarity = 0.0f;
};

/** Registers the retained native item-slot control. */
struct CKSLATELAYOUT_API FCkUiItemSlot final
{
    static auto RegisterItemSlotUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
};

CKSLATELAYOUT_API auto RegisterItemSlotUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
