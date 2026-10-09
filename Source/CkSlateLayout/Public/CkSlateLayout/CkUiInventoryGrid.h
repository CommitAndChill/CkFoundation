#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

#include "Fonts/SlateFontInfo.h"
#include "Input/DragAndDrop.h"
#include "Layout/Geometry.h"
#include "Widgets/SLeafWidget.h"

class FCkUiInventoryGridState;

/** A model-owned inventory transfer proposal. The source and document domain are weak. */
class CKSLATELAYOUT_API FCkUiInventoryGridDragOperation final : public FDragDropOperation
{
public:
    DRAG_DROP_OPERATOR_TYPE(FCkUiInventoryGridDragOperation, FDragDropOperation)

    auto GetKey() const -> const FString& { return Key; }
    auto GetRotated() const -> bool { return Rotated; }
    auto GetDefaultDecorator() const -> TSharedPtr<SWidget> override;

private:
    friend class SCkUiInventoryGrid;
    TWeakPtr<FCkUiInventoryGridState> Source;
    TWeakPtr<int32> Domain;
    TArray<TPair<TWeakPtr<class FCkUiCollection>, int64>> Revisions;
    FString Key;
    FString Name;
    FSlateFontInfo Font;
    TSharedPtr<FSlateBrush> Brush;
    int32 Width = 0;
    int32 Height = 0;
    bool Rotated = false;
    bool Consumed = false;
    uint64 Generation = 0;
};

/** A fixed-cell inventory surface. Its callbacks propose moves; the host publishes the model. */
class CKSLATELAYOUT_API SCkUiInventoryGrid final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkUiInventoryGrid) {} SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TWeakPtr<FCkUiInventoryGridState> InOwner);
    auto GetItemBounds(const FString& InKey) const -> TOptional<FSlateRect>;
    auto CanPlace(const FString& InKey, int32 InX, int32 InY, bool InRotated) const -> bool;
    auto TryMove(const FString& InKey, int32 InX, int32 InY, bool InRotated) -> bool;
    auto GetColumns() const -> int32;
    auto GetRows() const -> int32;
    auto GetCellSize() const -> float;
    auto GetItemCount() const -> int32;
    auto GetAcceptedRevision() const -> int64;
    void Freeze();
    void CancelTransient();

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;
    virtual auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry,
        const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 InLayer, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const -> int32 override;
    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }
    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual void OnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InEvent) override;
    virtual auto OnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InEvent) -> FReply override;
    virtual void OnDragLeave(const FDragDropEvent& InEvent) override;
    virtual auto OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InEvent) -> FReply override;
    virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InEvent) -> FReply override;

private:
    auto HitKey(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> FString;
    auto ProposalCell(const FGeometry& InGeometry, const FVector2D& InScreenPosition,
        const FCkUiInventoryGridDragOperation& InDrag) const -> FIntPoint;
    auto ValidDrag(const TSharedPtr<FCkUiInventoryGridDragOperation>& InDrag) const -> bool;

    TWeakPtr<FCkUiInventoryGridState> Owner;
    TWeakPtr<FCkUiInventoryGridDragOperation> HoverDrag;
    FIntPoint HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE);
    FString PressedKey;
    uint64 PressedGeneration = 0;
    int32 PressedUser = INDEX_NONE;
    uint32 PressedPointer = 0;
    bool Frozen = false;
};

CKSLATELAYOUT_API auto RegisterInventoryGridUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
