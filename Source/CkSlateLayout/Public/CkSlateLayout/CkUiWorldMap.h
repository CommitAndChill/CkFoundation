#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"
#include "Widgets/SLeafWidget.h"

class FCkUiWorldMapState;

/** Retained, normalized world map. Camera belongs to this view; selection belongs to the model. */
class CKSLATELAYOUT_API SCkUiWorldMap final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkUiWorldMap) {} SLATE_END_ARGS()
    void Construct(const FArguments&, TWeakPtr<FCkUiWorldMapState> InOwner);
    auto GetCameraCenter() const -> FVector2D { return Center; }
    auto GetZoom() const -> float { return Zoom; }
    auto TrySetCamera(FVector2D InCenter, float InZoom) -> bool;
    auto TryZoomBy(float InFactor) -> bool;
    auto TryRevealMarker(const FString& InKey) -> bool;
    auto GetMarkerLocalPosition(const FString& InKey) const -> TOptional<FVector2D>;
    auto GetAcceptedRevision() const -> int64;
    auto GetMarkerCount() const -> int32;
    auto GetVisibleMarkerCount() const -> int32;
    auto TryProjectToLocal(FVector2D InMapPoint, FVector2D InViewportSize, FVector2D& OutLocal) const -> bool;
    auto TryUnprojectFromLocal(FVector2D InLocalPoint, FVector2D InViewportSize, FVector2D& OutMapPoint) const -> bool;
    void CancelTransient();
    auto GetOwnedCapture() const -> TArray<FCkUiPointerCapture>;
    auto HasActivePointer() const -> bool { return PointerActive; }
    virtual auto ComputeDesiredSize(float) const -> FVector2D override;
    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }
    virtual void Tick(const FGeometry&, double, float) override;
    virtual auto OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
        FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const -> int32 override;
    virtual auto OnMouseButtonDown(const FGeometry&, const FPointerEvent&) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry&, const FPointerEvent&) -> FReply override;
    virtual auto OnMouseMove(const FGeometry&, const FPointerEvent&) -> FReply override;
    virtual auto OnMouseWheel(const FGeometry&, const FPointerEvent&) -> FReply override;
    virtual auto OnKeyDown(const FGeometry&, const FKeyEvent&) -> FReply override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override;
private:
    auto UpdateGeometry(const FGeometry&) const -> bool;
    auto CanInteract() const -> bool;
    auto Position(FVector2D InNormalized) const -> FVector2D;
    auto HitKey(FVector2D InLocal) const -> FString;
    auto IsMapPoint(FVector2D InLocal) const -> bool;
    auto ClampCenter(FVector2D InCenter, float InZoom) const -> FVector2D;
    auto EmitSelection(const FString&) -> bool;
    TWeakPtr<FCkUiWorldMapState> Owner;
    mutable FVector2D ViewSize = FVector2D::ZeroVector;
    mutable FVector2D FitSize = FVector2D::ZeroVector;
    mutable FVector2D Center = FVector2D(0.5, 0.5);
    mutable float Zoom = 1.0f;
    mutable bool GeometryValid = false;
    FVector2D PressPosition = FVector2D::ZeroVector;
    FVector2D PressCenter = FVector2D::ZeroVector;
    FString PressKey;
    FString HoverKey;
    uint64 PressGeneration = 0;
    int64 PressRevision = INDEX_NONE;
    int32 PointerUser = INDEX_NONE;
    uint32 PointerIndex = 0;
    bool PointerActive = false;
    bool Dragging = false;
};

CKSLATELAYOUT_API auto RegisterWorldMapUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
