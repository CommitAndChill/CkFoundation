#include "CkSlateLayout/CkUiWorldMap.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiFontRegistry.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

namespace ck_ui_world_map
{
    struct FMarker
    {
        FString Key, Label, Kind;
        FVector2D Point = FVector2D::ZeroVector;
        bool Enabled = false;
        bool Visible = true;
    };
    struct FConfiguration
    {
        TSharedPtr<FCkUiCollection> Markers;
        TAttribute<const FSlateBrush*> Image;
        TAttribute<FText> Selected;
        TAttribute<bool> Route, CanDispatch;
        FCkUiOnStringChanged Select;
        TWeakPtr<FCkUiView> View;
        TWeakPtr<int32> Domain;
        FSlateFontInfo Font;
        FVector2D Origin = FVector2D(0.3, 0.55);
        FLinearColor Background = FLinearColor(0.035f, 0.045f, 0.055f);
        FLinearColor Border = FLinearColor(0.35f, 0.4f, 0.45f);
        FLinearColor Marker = FLinearColor(0.82f, 0.87f, 0.9f);
        FLinearColor QuestColor = FLinearColor(0.94f, 0.65f, 0.25f);
        FLinearColor ServiceColor = FLinearColor(0.30f, 0.75f, 0.78f);
        FLinearColor HazardColor = FLinearColor(0.85f, 0.37f, 0.25f);
        FLinearColor PlayerColor = FLinearColor(0.9f, 0.95f, 0.93f);
        FLinearColor SelectedColor = FLinearColor(1.0f, 0.75f, 0.25f);
        FLinearColor RouteColor = FLinearColor(0.9f, 0.72f, 0.3f);
        int32 User = INDEX_NONE;
    };
    auto Finite(FVector2D Value) -> bool
    { return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y); }
    auto ReadMarkers(const FCkUiCollection& Collection, TArray<FMarker>& Out, FString& Failure) -> bool
    {
        const auto& Schema = Collection.GetSchema();
        const auto Has = [&Schema](const TCHAR* Name, ECkUiFieldKind Kind)
        { return Schema.ContainsByPredicate([=](const FCkUiFieldSchema& Field) { return Field.Name == Name && Field.Kind == Kind; }); };
        const bool HasVisible = Schema.ContainsByPredicate([](const FCkUiFieldSchema& Field) { return Field.Name == TEXT("visible"); });
        if (!Has(TEXT("x"), ECkUiFieldKind::Number) || !Has(TEXT("y"), ECkUiFieldKind::Number)
            || !Has(TEXT("label"), ECkUiFieldKind::Text) || !Has(TEXT("kind"), ECkUiFieldKind::Text)
            || !Has(TEXT("enabled"), ECkUiFieldKind::Bool) || (HasVisible && !Has(TEXT("visible"), ECkUiFieldKind::Bool))
            || Collection.GetRecords().Num() > 1024)
        { Failure = TEXT("World map requires x/y Number, label/kind Text, enabled Bool, optional visible Bool and at most 1024 markers."); return false; }
        TSet<FString> Keys;
        for (const auto& Record : Collection.GetRecords())
        {
            if (!Record.IsValid() || Record->GetKey().IsEmpty() || Keys.Contains(Record->GetKey()))
            { Failure = TEXT("World map markers require unique nonempty stable keys."); return false; }
            const auto* X = Record->FindField(TEXT("x"));
            const auto* Y = Record->FindField(TEXT("y"));
            const auto* Label = Record->FindField(TEXT("label"));
            const auto* Kind = Record->FindField(TEXT("kind"));
            const auto* Enabled = Record->FindField(TEXT("enabled"));
            const auto* Visible = Record->FindField(TEXT("visible"));
            if (X == nullptr || Y == nullptr || Label == nullptr || Kind == nullptr || Enabled == nullptr
                || X->Kind != ECkUiFieldKind::Number || Y->Kind != ECkUiFieldKind::Number
                || Label->Kind != ECkUiFieldKind::Text || Kind->Kind != ECkUiFieldKind::Text
                || Enabled->Kind != ECkUiFieldKind::Bool || (HasVisible && (Visible == nullptr || Visible->Kind != ECkUiFieldKind::Bool))
                || !FMath::IsFinite(X->Number) || !FMath::IsFinite(Y->Number)
                || X->Number < 0 || X->Number > 1 || Y->Number < 0 || Y->Number > 1)
            { Failure = TEXT("World map marker has missing, mismatched or out-of-range fields."); return false; }
            FMarker Marker;
            Marker.Key = Record->GetKey(); Marker.Point = FVector2D(X->Number, Y->Number);
            Marker.Label = Label->Text.ToString(); Marker.Kind = Kind->Text.ToString();
            if (Marker.Kind != TEXT("quest") && Marker.Kind != TEXT("fast-travel") && Marker.Kind != TEXT("vendor")
                && Marker.Kind != TEXT("player") && Marker.Kind != TEXT("hazard") && Marker.Kind != TEXT("generic"))
            { Failure = TEXT("World map marker kind is unsupported."); return false; }
            Marker.Enabled = Enabled->Bool; Marker.Visible = HasVisible ? Visible->Bool : true;
            Keys.Add(Marker.Key); Out.Add(MoveTemp(Marker));
        }
        return true;
    }
    auto Configure(const FCkUiCustomWidgetArguments& In, FConfiguration& Out, TArray<FMarker>& Markers, FString& Failure) -> bool
    {
        const auto* Source = In.Collections.Find(TEXT("markers"));
        const auto* Image = In.ImageBindings.Find(TEXT("image"));
        const auto* Select = In.StringChanged.Find(TEXT("select"));
        if (In.Id.IsEmpty() || Source == nullptr || !Source->IsValid() || Image == nullptr || !Image->IsSet()
            || Select == nullptr || !Select->IsBound() || !In.OwnerView.IsValid() || !In.ItemSlotDomain.IsValid())
        { Failure = TEXT("World map requires id, markers/image bindings, select callback and owning view."); return false; }
        const FSlateBrush* Brush = Image->Get(nullptr);
        if (Brush == nullptr || !Finite(FVector2D(Brush->ImageSize))
            || Brush->ImageSize.X <= 0.0f || Brush->ImageSize.Y <= 0.0f)
        { Failure = TEXT("World map requires an image with finite positive dimensions."); return false; }
        Out.Markers = *Source; Out.Image = *Image; Out.Select = *Select;
        Out.Selected = In.TextBindings.FindRef(TEXT("selected-key")); Out.Route = In.BoolBindings.FindRef(TEXT("route"));
        Out.CanDispatch = In.CanDispatchEvents; Out.View = In.OwnerView; Out.Domain = In.ItemSlotDomain;
        Out.User = In.SlateUserIndex; Out.Font = In.BaseFont;
        if (const auto* Family = In.TextProperties.Find(TEXT("font-family")))
        {
            if (Family->IsEmpty() || !In.FontRegistry.IsValid() || !In.FontRegistry->TryResolve(Family->ToString(), TEXT("Regular"), Out.Font))
            { Failure = TEXT("World map font-family must name a registered regular face."); return false; }
        }
        Out.Font.Size = 14;
        const auto Number = [&In, &Failure](const TCHAR* Name, double& Value)
        {
            if (const float* Input = In.NumberProperties.Find(Name))
            {
                if (!FMath::IsFinite(*Input) || *Input < 0 || *Input > 1)
                { Failure = TEXT("World map origin must be finite and normalized."); return false; }
                Value = *Input;
            }
            return true;
        };
        if (!Number(TEXT("origin-x"), Out.Origin.X) || !Number(TEXT("origin-y"), Out.Origin.Y)) { return false; }
        const auto Color = [&In, &Failure](const TCHAR* Name, FLinearColor& Target)
        {
            const auto* Value = In.Style.CustomProperties.Find(Name);
            if (Value == nullptr) { return true; }
            if (Value->Kind != ECkUiCustomStyleKind::Color || !FMath::IsFinite(Value->Color.R)
                || !FMath::IsFinite(Value->Color.G) || !FMath::IsFinite(Value->Color.B) || !FMath::IsFinite(Value->Color.A))
            { Failure = TEXT("World map colors must be finite."); return false; }
            Target = Value->Color; return true;
        };
        if (!Color(TEXT("-ck-map-background"), Out.Background) || !Color(TEXT("-ck-map-border"), Out.Border)
            || !Color(TEXT("-ck-map-marker"), Out.Marker) || !Color(TEXT("-ck-map-selected"), Out.SelectedColor)
            || !Color(TEXT("-ck-map-quest"), Out.QuestColor) || !Color(TEXT("-ck-map-service"), Out.ServiceColor)
            || !Color(TEXT("-ck-map-hazard"), Out.HazardColor) || !Color(TEXT("-ck-map-player"), Out.PlayerColor)
            || !Color(TEXT("-ck-map-route"), Out.RouteColor)) { return false; }
        if (const auto* Size = In.Style.CustomProperties.Find(TEXT("-ck-map-label-font-size")))
        {
            if (Size->Kind != ECkUiCustomStyleKind::Length || !FMath::IsFinite(Size->Number) || Size->Number < 8 || Size->Number > 48)
            { Failure = TEXT("World map label size must be a finite length in [8,48]."); return false; }
            Out.Font.Size = FMath::RoundToInt(Size->Number);
        }
        return ReadMarkers(*Out.Markers, Markers, Failure);
    }
}

class FCkUiWorldMapState final : public ICkUiRetainedWidget, public TSharedFromThis<FCkUiWorldMapState>
{
public:
    ck_ui_world_map::FConfiguration Config;
    TArray<ck_ui_world_map::FMarker> Markers;
    TSharedPtr<SCkUiWorldMap> Widget;
    int64 SeenRevision = INDEX_NONE, AcceptedRevision = INDEX_NONE;
    uint64 Generation = 1;
    bool Active = true;
    auto IsLive() const -> bool
    { return Active && Config.View.IsValid() && Config.Domain.IsValid() && Config.CanDispatch.Get(false); }
    auto IsCurrent() const -> bool
    { return IsLive() && Config.Markers.IsValid() && Config.Markers->GetRevision() == AcceptedRevision; }
    auto IsOwner(uint32 User) const -> bool { return Config.User == INDEX_NONE || Config.User == static_cast<int32>(User); }
    void Refresh()
    {
        if (!Active || !Config.Markers.IsValid() || SeenRevision == Config.Markers->GetRevision()) { return; }
        SeenRevision = Config.Markers->GetRevision();
        TArray<ck_ui_world_map::FMarker> Next; FString Failure;
        if (ck_ui_world_map::ReadMarkers(*Config.Markers, Next, Failure))
        { Markers = MoveTemp(Next); AcceptedRevision = SeenRevision; }
        ++Generation;
        if (Widget.IsValid()) { Widget->Invalidate(EInvalidateWidgetReason::Paint); }
    }
    auto Find(const FString& Key) const -> const ck_ui_world_map::FMarker*
    { return Markers.FindByPredicate([&](const auto& Marker) { return Marker.Key == Key; }); }
    auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
    auto GetPointerCaptures() const -> TArray<FCkUiPointerCapture> override { return Widget->GetOwnedCapture(); }
    void ReleaseTransientInteraction() override { ++Generation; Widget->CancelTransient(); }
    void ReleaseOwnerInteraction() override { Active = false; ReleaseTransientInteraction(); }
    auto PrepareReload(const FCkUiCustomWidgetArguments&, FString&) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;
};

namespace ck_ui_world_map
{
    class FPrepared final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FPrepared(TSharedRef<FCkUiWorldMapState> InOwner, FConfiguration InConfig, TArray<FMarker> InMarkers)
            : Owner(InOwner), Config(MoveTemp(InConfig)), Markers(MoveTemp(InMarkers)), Revision(Config.Markers->GetRevision()) {}
        void Commit() noexcept override
        {
            if (!Owner->Active) { return; }
            Owner->Config = MoveTemp(Config); Owner->Markers = MoveTemp(Markers);
            Owner->SeenRevision = Revision; Owner->AcceptedRevision = Revision; ++Owner->Generation;
            Owner->Widget->CancelTransient(); Owner->Widget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
        }
    private:
        TSharedRef<FCkUiWorldMapState> Owner;
        FConfiguration Config;
        TArray<FMarker> Markers;
        int64 Revision;
    };
}

auto FCkUiWorldMapState::PrepareReload(const FCkUiCustomWidgetArguments& In, FString& Failure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate>
{
    if (!Active || Widget->HasActivePointer()) { Failure = TEXT("World map cannot reload while retired or during a pointer gesture."); return {}; }
    ck_ui_world_map::FConfiguration Next; TArray<ck_ui_world_map::FMarker> NextMarkers;
    if (!ck_ui_world_map::Configure(In, Next, NextMarkers, Failure)) { return {}; }
    return MakeUnique<ck_ui_world_map::FPrepared>(ConstCastSharedRef<FCkUiWorldMapState>(AsShared()), MoveTemp(Next), MoveTemp(NextMarkers));
}

void SCkUiWorldMap::Construct(const FArguments&, TWeakPtr<FCkUiWorldMapState> InOwner)
{ Owner = MoveTemp(InOwner); SetVisibility(EVisibility::Visible); }
void SCkUiWorldMap::CancelTransient()
{ PointerActive = false; Dragging = false; PointerUser = INDEX_NONE; PressKey.Reset(); HoverKey.Reset(); Invalidate(EInvalidateWidgetReason::Paint); }
auto SCkUiWorldMap::GetOwnedCapture() const -> TArray<FCkUiPointerCapture>
{
    if (!PointerActive || !FSlateApplication::IsInitialized()) { return {}; }
    const auto User = FSlateApplication::Get().GetUser(PointerUser);
    if (!User.IsValid() || User->GetPointerCaptor(PointerIndex).Get() != this) { return {}; }
    return {{PointerUser, PointerIndex, ConstCastSharedRef<SWidget>(AsShared())}};
}
auto SCkUiWorldMap::ClampCenter(FVector2D Value, float Scale) const -> FVector2D
{
    const FVector2D Size = FitSize * Scale;
    Value.X = Size.X <= ViewSize.X ? 0.5 : FMath::Clamp(Value.X, ViewSize.X / (2 * Size.X), 1 - ViewSize.X / (2 * Size.X));
    Value.Y = Size.Y <= ViewSize.Y ? 0.5 : FMath::Clamp(Value.Y, ViewSize.Y / (2 * Size.Y), 1 - ViewSize.Y / (2 * Size.Y));
    return Value;
}
auto SCkUiWorldMap::UpdateGeometry(const FGeometry& Geometry) const -> bool
{
    GeometryValid = false;
    const auto State = Owner.Pin();
    if (!State.IsValid() || !State->Config.Image.IsSet()) { return false; }
    const FSlateBrush* Brush = State->Config.Image.Get(nullptr);
    if (Brush == nullptr) { return false; }
    const FVector2D ImageSize(Brush->ImageSize);
    const FVector2D Size = Geometry.GetLocalSize();
    if (!ck_ui_world_map::Finite(ImageSize) || !ck_ui_world_map::Finite(Size) || ImageSize.X <= 0 || ImageSize.Y <= 0
        || Size.X <= 0 || Size.Y <= 0 || !ck_ui_world_map::Finite(Geometry.GetAbsolutePosition())
        || !FMath::IsFinite(Geometry.Scale) || Geometry.Scale <= 0) { return false; }
    const double Fit = FMath::Min(Size.X / ImageSize.X, Size.Y / ImageSize.Y);
    const FVector2D Fitted = ImageSize * Fit;
    if (!ck_ui_world_map::Finite(Fitted) || Fitted.X <= 0 || Fitted.Y <= 0 || !ck_ui_world_map::Finite(Fitted * 8)) { return false; }
    ViewSize = Size; FitSize = Fitted; Center = ClampCenter(Center, Zoom); GeometryValid = true; return true;
}
auto SCkUiWorldMap::CanInteract() const -> bool
{
    if (!IsEnabled() || !GetVisibility().IsVisible()) { return false; }
    const auto State = Owner.Pin();
    if (!State.IsValid()) { return false; }
    State->Refresh();
    return State->IsCurrent() && UpdateGeometry(GetCachedGeometry());
}
auto SCkUiWorldMap::Position(FVector2D Point) const -> FVector2D
{ return ViewSize * 0.5 + (Point - Center) * FitSize * Zoom; }
auto SCkUiWorldMap::TrySetCamera(FVector2D Value, float Scale) -> bool
{
    if (PointerActive || !ck_ui_world_map::Finite(Value) || !FMath::IsFinite(Scale) || Scale < 1 || Scale > 8 || !CanInteract()) { return false; }
    Center = ClampCenter(Value, Scale); Zoom = Scale; Invalidate(EInvalidateWidgetReason::Paint); return true;
}
auto SCkUiWorldMap::TryZoomBy(float Factor) -> bool
{
    if (!FMath::IsFinite(Factor) || Factor <= 0 || !FMath::IsFinite(Zoom * Factor)) { return false; }
    return TrySetCamera(Center, FMath::Clamp(Zoom * Factor, 1.0f, 8.0f));
}
auto SCkUiWorldMap::TryRevealMarker(const FString& Key) -> bool
{
    if (!CanInteract()) { return false; }
    const auto State = Owner.Pin(); const auto* Marker = State->Find(Key);
    return Marker != nullptr && Marker->Visible && Marker->Enabled && TrySetCamera(Marker->Point, Zoom);
}
auto SCkUiWorldMap::GetMarkerLocalPosition(const FString& Key) const -> TOptional<FVector2D>
{
    const auto State = Owner.Pin(); if (!State.IsValid() || !UpdateGeometry(GetCachedGeometry())) { return {}; }
    State->Refresh(); const auto* Marker = State->Find(Key);
    return Marker != nullptr && Marker->Visible ? TOptional<FVector2D>(Position(Marker->Point)) : TOptional<FVector2D>();
}
auto SCkUiWorldMap::GetAcceptedRevision() const -> int64
{ const auto State = Owner.Pin(); if (!State.IsValid()) { return INDEX_NONE; } State->Refresh(); return State->AcceptedRevision; }
auto SCkUiWorldMap::GetMarkerCount() const -> int32
{ const auto State = Owner.Pin(); if (!State.IsValid()) { return 0; } State->Refresh(); return State->Markers.Num(); }
auto SCkUiWorldMap::GetVisibleMarkerCount() const -> int32
{
    const auto State = Owner.Pin(); if (!State.IsValid()) { return 0; } State->Refresh();
    int32 Count = 0; for (const auto& Marker : State->Markers) { if (Marker.Visible) { ++Count; } } return Count;
}
auto SCkUiWorldMap::TryProjectToLocal(FVector2D Point, FVector2D Viewport, FVector2D& OutLocal) const -> bool
{
    const auto State = Owner.Pin();
    if (!State.IsValid() || !ck_ui_world_map::Finite(Point) || Point.X < 0 || Point.Y < 0 || Point.X > 1 || Point.Y > 1
        || !ck_ui_world_map::Finite(Viewport) || Viewport.X <= 0 || Viewport.Y <= 0) { return false; }
    const auto* Brush = State->Config.Image.Get(nullptr);
    if (Brush == nullptr) { return false; }
    const FVector2D ImageSize(Brush->ImageSize);
    if (!ck_ui_world_map::Finite(ImageSize) || ImageSize.X <= 0 || ImageSize.Y <= 0) { return false; }
    const FVector2D Size = ImageSize * FMath::Min(Viewport.X / ImageSize.X, Viewport.Y / ImageSize.Y) * Zoom;
    if (!ck_ui_world_map::Finite(Size) || Size.X <= 0 || Size.Y <= 0) { return false; }
    FVector2D Camera = Center;
    Camera.X = Size.X <= Viewport.X ? 0.5 : FMath::Clamp(Camera.X, Viewport.X / (2 * Size.X), 1 - Viewport.X / (2 * Size.X));
    Camera.Y = Size.Y <= Viewport.Y ? 0.5 : FMath::Clamp(Camera.Y, Viewport.Y / (2 * Size.Y), 1 - Viewport.Y / (2 * Size.Y));
    const FVector2D Result = Viewport * 0.5 + (Point - Camera) * Size;
    if (!ck_ui_world_map::Finite(Result)) { return false; } OutLocal = Result; return true;
}
auto SCkUiWorldMap::TryUnprojectFromLocal(FVector2D Local, FVector2D Viewport, FVector2D& OutPoint) const -> bool
{
    if (!ck_ui_world_map::Finite(Local) || !ck_ui_world_map::Finite(Viewport) || Local.X < 0 || Local.Y < 0
        || Local.X > Viewport.X || Local.Y > Viewport.Y) { return false; }
    FVector2D Min, Max;
    if (!TryProjectToLocal(FVector2D::ZeroVector, Viewport, Min) || !TryProjectToLocal(FVector2D(1,1), Viewport, Max)
        || Local.X < Min.X || Local.Y < Min.Y || Local.X > Max.X || Local.Y > Max.Y) { return false; }
    const FVector2D Result = (Local - Min) / (Max - Min);
    if (!ck_ui_world_map::Finite(Result)) { return false; } OutPoint = Result; return true;
}
auto SCkUiWorldMap::ComputeDesiredSize(float) const -> FVector2D { return FVector2D(640, 480); }
auto SCkUiWorldMap::IsMapPoint(FVector2D Local) const -> bool
{
    const FVector2D Min = Position(FVector2D::ZeroVector), Max = Position(FVector2D(1, 1));
    return ck_ui_world_map::Finite(Local) && Local.X >= 0 && Local.Y >= 0 && Local.X <= ViewSize.X && Local.Y <= ViewSize.Y
        && Local.X >= Min.X && Local.Y >= Min.Y && Local.X <= Max.X && Local.Y <= Max.Y;
}
auto SCkUiWorldMap::HitKey(FVector2D Local) const -> FString
{
    if (!IsMapPoint(Local)) { return {}; }
    const auto State = Owner.Pin(); if (!State.IsValid()) { return {}; }
    for (int32 Index = State->Markers.Num() - 1; Index >= 0; --Index)
    {
        const auto& Marker = State->Markers[Index];
        if (Marker.Enabled && Marker.Visible && (Position(Marker.Point) - Local).SizeSquared() <= 100) { return Marker.Key; }
    }
    return {};
}
auto SCkUiWorldMap::EmitSelection(const FString& Key) -> bool
{
    if (!CanInteract()) { return false; }
    const auto State = Owner.Pin(); const auto* Marker = State->Find(Key);
    if (Marker == nullptr || !Marker->Enabled || !Marker->Visible) { return false; }
    const FString Value = Marker->Key; const auto Callback = State->Config.Select;
    Callback.ExecuteIfBound(Value); return true;
}
void SCkUiWorldMap::Tick(const FGeometry& Geometry, double, float)
{
    UpdateGeometry(Geometry);
    const auto State = Owner.Pin();
    if (PointerActive && (!State.IsValid() || !State->IsLive()))
    {
        if (FSlateApplication::IsInitialized())
        {
            const auto User = FSlateApplication::Get().GetUser(PointerUser);
            if (User.IsValid() && User->GetPointerCaptor(PointerIndex).Get() == this) { User->ReleaseCapture(PointerIndex); }
        }
        CancelTransient();
    }
}
auto SCkUiWorldMap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
{
    const auto State = Owner.Pin();
    if (PointerActive || Event.GetEffectingButton() != EKeys::LeftMouseButton || !State.IsValid()
        || !State->IsOwner(Event.GetUserIndex()) || !CanInteract() || !UpdateGeometry(Geometry)) { return FReply::Unhandled(); }
    const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if (!IsMapPoint(Local)) { return FReply::Unhandled(); }
    PointerActive = true; Dragging = false; PointerUser = Event.GetUserIndex(); PointerIndex = Event.GetPointerIndex();
    PressPosition = Local; PressCenter = Center; PressKey = HitKey(Local); PressGeneration = State->Generation; PressRevision = State->AcceptedRevision;
    return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
}
auto SCkUiWorldMap::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
{
    const auto State = Owner.Pin();
    if (!State.IsValid() || !State->IsOwner(Event.GetUserIndex())
        || (PointerActive && (PointerUser != static_cast<int32>(Event.GetUserIndex()) || PointerIndex != Event.GetPointerIndex()))) { return FReply::Unhandled(); }
    if (!CanInteract() || !UpdateGeometry(Geometry)) { return FReply::Unhandled(); }
    const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if (!ck_ui_world_map::Finite(Local)) { return FReply::Unhandled(); }
    if (PointerActive)
    {
        if (State->Generation != PressGeneration || State->AcceptedRevision != PressRevision) { return FReply::Handled(); }
        if ((Local - PressPosition).SizeSquared() > 25) { Dragging = true; }
        if (Dragging) { Center = ClampCenter(PressCenter - (Local - PressPosition) / (FitSize * Zoom), Zoom); }
    }
    HoverKey = HitKey(Local); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled();
}
auto SCkUiWorldMap::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
{
    if (!PointerActive || PointerUser != static_cast<int32>(Event.GetUserIndex()) || PointerIndex != Event.GetPointerIndex()
        || Event.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
    const auto State = Owner.Pin();
    const bool Valid = State.IsValid() && CanInteract() && UpdateGeometry(Geometry)
        && State->Generation == PressGeneration && State->AcceptedRevision == PressRevision;
    const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    const FString Selection = Valid && !Dragging && (Local - PressPosition).SizeSquared() <= 25 && HitKey(Local) == PressKey ? PressKey : FString();
    // Release only this gesture before consumer code can retire the view or acquire another capture.
    if (FSlateApplication::IsInitialized())
    {
        const auto User = FSlateApplication::Get().GetUser(PointerUser);
        if (User.IsValid() && User->GetPointerCaptor(PointerIndex).Get() == this)
        { User->ReleaseCapture(PointerIndex); }
    }
    CancelTransient();
    if (!Selection.IsEmpty()) { EmitSelection(Selection); }
    return FReply::Handled();
}
auto SCkUiWorldMap::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
{
    const auto State = Owner.Pin();
    if (PointerActive || !State.IsValid() || !State->IsOwner(Event.GetUserIndex()) || !CanInteract() || !UpdateGeometry(Geometry)
        || !FMath::IsFinite(Event.GetWheelDelta())) { return FReply::Unhandled(); }
    const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if (!IsMapPoint(Local)) { return FReply::Unhandled(); }
    const float Factor = FMath::Pow(1.2f, Event.GetWheelDelta());
    if (!FMath::IsFinite(Factor) || Factor <= 0 || !FMath::IsFinite(Zoom * Factor)) { return FReply::Unhandled(); }
    const float NextZoom = FMath::Clamp(Zoom * Factor, 1.0f, 8.0f);
    const FVector2D Anchor = Center + (Local - ViewSize * 0.5) / (FitSize * Zoom);
    Center = ClampCenter(Anchor - (Local - ViewSize * 0.5) / (FitSize * NextZoom), NextZoom); Zoom = NextZoom;
    Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled();
}
auto SCkUiWorldMap::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) -> FReply
{
    const auto State = Owner.Pin();
    if (!State.IsValid() || !State->IsOwner(Event.GetUserIndex())) { return FReply::Unhandled(); }
    const FKey Key = Event.GetKey();
    if (Key == EKeys::Escape)
    {
        if (!PointerActive) { return FReply::Unhandled(); }
        CancelTransient(); return FReply::Handled().ReleaseMouseCapture();
    }
    if (PointerActive || !CanInteract() || !UpdateGeometry(Geometry)) { return FReply::Unhandled(); }
    FVector2D Delta = FVector2D::ZeroVector;
    if (Key == EKeys::W) { Delta.Y = -32; } else if (Key == EKeys::S) { Delta.Y = 32; }
    else if (Key == EKeys::A) { Delta.X = -32; } else if (Key == EKeys::D) { Delta.X = 32; }
    if (!Delta.IsZero()) { Center = ClampCenter(Center + Delta / (FitSize * Zoom), Zoom); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
    if (Key == EKeys::Add || Key == EKeys::Equals || Key == EKeys::Gamepad_RightShoulder) { TryZoomBy(1.2f); return FReply::Handled(); }
    if (Key == EKeys::Subtract || Key == EKeys::Hyphen || Key == EKeys::Gamepad_LeftShoulder) { TryZoomBy(1 / 1.2f); return FReply::Handled(); }
    const bool Previous = Key == EKeys::Left || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up;
    const bool Next = Key == EKeys::Right || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down;
    // Keyboard selection needs the model's current key; omission leaves pointer selection available.
    const bool Confirm = Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom;
    if ((Previous || Next || Confirm) && !State->Config.Selected.IsSet()) { return FReply::Unhandled(); }
    const FString Selected = State->Config.Selected.Get(FText::GetEmpty()).ToString();
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) { return EmitSelection(Selected) ? FReply::Handled() : FReply::Unhandled(); }
    if (Previous || Next)
    {
        TArray<FString> Keys;
        for (const auto& Marker : State->Markers) { if (Marker.Enabled && Marker.Visible) { Keys.Add(Marker.Key); } }
        if (Keys.IsEmpty()) { return FReply::Handled(); }
        int32 Index = Keys.IndexOfByKey(Selected);
        Index = Index == INDEX_NONE ? (Previous ? Keys.Num() - 1 : 0) : (Index + (Previous ? Keys.Num() - 1 : 1)) % Keys.Num();
        const FString Selection = Keys[Index]; TryRevealMarker(Selection); EmitSelection(Selection); return FReply::Handled();
    }
    return FReply::Unhandled();
}
void SCkUiWorldMap::OnMouseCaptureLost(const FCaptureLostEvent&) { CancelTransient(); }
void SCkUiWorldMap::OnMouseLeave(const FPointerEvent& Event)
{
    const auto State = Owner.Pin();
    if (State.IsValid() && State->IsOwner(Event.GetUserIndex())) { HoverKey.Reset(); Invalidate(EInvalidateWidgetReason::Paint); }
}

auto SCkUiWorldMap::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
    FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const -> int32
{
    const auto State = Owner.Pin(); if (!State.IsValid()) { return Layer; }
    State->Refresh();
    if (!UpdateGeometry(Geometry)) { return Layer; }
    // Copy the attribute's brush and all paint inputs before issuing draw commands.
    const FSlateBrush* Source = State->Config.Image.Get(nullptr); if (Source == nullptr) { return Layer; }
    const FSlateBrush Brush = *Source;
    const auto Config = State->Config; const auto Markers = State->Markers;
    const FString Selected = Config.Selected.Get(FText::GetEmpty()).ToString();
    const bool Route = Config.Route.Get(false) && State->IsCurrent();
    const auto Tint = Style.GetColorAndOpacityTint();
    Out.PushClip(FSlateClippingZone(Geometry));
    const auto Box = [&](int32 Z, FVector2D At, FVector2D Size, const FSlateBrush* Image, FLinearColor Color)
    { FSlateDrawElement::MakeBox(Out, Z, Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(At))), Image, ESlateDrawEffect::None, Color * Tint); };
    const auto Line = [&](int32 Z, TArray<FVector2f> Points, FLinearColor Color, float Thickness = 1.5f)
    { FSlateDrawElement::MakeLines(Out, Z, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color * Tint, true, Thickness); };
    const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    Box(Layer, FVector2D::ZeroVector, ViewSize, White, Config.Background);
    Box(Layer + 1, Position(FVector2D::ZeroVector), FitSize * Zoom, &Brush, FLinearColor::White);
    const auto* SelectedMarker = Markers.FindByPredicate([&](const auto& Marker) { return Marker.Key == Selected && Marker.Enabled && Marker.Visible; });
    if (Route && SelectedMarker != nullptr)
    { Line(Layer + 2, {FVector2f(Position(Config.Origin)), FVector2f(Position(SelectedMarker->Point))}, Config.RouteColor, 2); }
    for (const auto& Marker : Markers)
    {
        if (!Marker.Visible) { continue; }
        const FVector2D P = Position(Marker.Point);
        if (P.X < -12 || P.Y < -12 || P.X > ViewSize.X + 12 || P.Y > ViewSize.Y + 12) { continue; }
        const FVector2f C(P); const float R = Marker.Key == Selected ? 9.0f : 6.0f;
        const FLinearColor KindColor = Marker.Kind == TEXT("quest") ? Config.QuestColor
            : Marker.Kind == TEXT("hazard") ? Config.HazardColor
            : Marker.Kind == TEXT("player") ? Config.PlayerColor
            : Marker.Kind == TEXT("fast-travel") || Marker.Kind == TEXT("vendor") ? Config.ServiceColor : Config.Marker;
        const FLinearColor BaseColor = Marker.Key == Selected ? Config.SelectedColor : KindColor;
        const FLinearColor Color = BaseColor.CopyWithNewOpacity(BaseColor.A * (Marker.Enabled ? 1.0f : 0.35f));
        TArray<FVector2f> Points;
        if (Marker.Kind == TEXT("quest")) { Points = {C + FVector2f(0,-R), C + FVector2f(R,0), C + FVector2f(0,R), C + FVector2f(-R,0)}; }
        else if (Marker.Kind == TEXT("vendor")) { Points = {C + FVector2f(-R,-R), C + FVector2f(R,-R), C + FVector2f(R,R), C + FVector2f(-R,R)}; }
        else if (Marker.Kind == TEXT("player") || Marker.Kind == TEXT("hazard")) { Points = {C + FVector2f(0,-R), C + FVector2f(R,R), C + FVector2f(-R,R)}; }
        else { for (int32 I = 0; I < 24; ++I) { const float Angle = I * 2 * PI / 24; Points.Add(C + FVector2f(FMath::Cos(Angle),FMath::Sin(Angle)) * R); } }
        const FVector2f FirstPoint = Points[0];
        Points.Add(FirstPoint); Line(Layer + 3, MoveTemp(Points), Color, 2);
        if (Marker.Kind == TEXT("hazard")) { Line(Layer + 4, {C + FVector2f(0,-2), C + FVector2f(0,2)}, Color, 2); Box(Layer + 4, P + FVector2D(-1,4), FVector2D(2,2), White, Color); }
        if ((Marker.Key == HoverKey || Marker.Key == Selected) && !Marker.Label.IsEmpty() && FSlateApplication::IsInitialized())
        {
            const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Marker.Label, Config.Font);
            const FVector2D PanelSize(FMath::Min(TextSize.X + 16, ViewSize.X), FMath::Min(TextSize.Y + 10, ViewSize.Y));
            const FVector2D At(FMath::Clamp(P.X + 12, 0.0, FMath::Max(0.0, ViewSize.X - PanelSize.X)), FMath::Clamp(P.Y - PanelSize.Y * 0.5, 0.0, FMath::Max(0.0, ViewSize.Y - PanelSize.Y)));
            Box(Layer + 5, At, PanelSize, White, Config.Background.CopyWithNewOpacity(0.95f));
            const FGeometry LabelGeometry = Geometry.MakeChild(FVector2f(PanelSize), FSlateLayoutTransform(FVector2f(At)));
            Out.PushClip(FSlateClippingZone(LabelGeometry));
            FSlateDrawElement::MakeText(Out, Layer + 6, Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(At + FVector2D(8,5)))), Marker.Label, Config.Font, ESlateDrawEffect::None, Color * Tint);
            Out.PopClip();
        }
    }
    Line(Layer + 7, {FVector2f(0,0),FVector2f(ViewSize.X,0),FVector2f(ViewSize),FVector2f(0,ViewSize.Y),FVector2f(0,0)}, Config.Border);
    Out.PopClip(); return Layer + 7;
}

auto RegisterWorldMapUiControl(FCkUiWidgetRegistry& Registry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("world-map");
    Registration.Schema.Properties = {
        {TEXT("markers"), ECkUiCustomPropertyKind::CollectionBinding, true},
        {TEXT("image"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("select"), ECkUiCustomPropertyKind::StringChanged, true},
        {TEXT("selected-key"), ECkUiCustomPropertyKind::TextBinding, false},
        {TEXT("route"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("origin-x"), ECkUiCustomPropertyKind::Number, false},
        {TEXT("origin-y"), ECkUiCustomPropertyKind::Number, false},
        {TEXT("font-family"), ECkUiCustomPropertyKind::Text, false}};
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-map-background"), ECkUiCustomStyleKind::Color}, {TEXT("-ck-map-border"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-map-marker"), ECkUiCustomStyleKind::Color}, {TEXT("-ck-map-selected"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-map-quest"), ECkUiCustomStyleKind::Color}, {TEXT("-ck-map-service"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-map-hazard"), ECkUiCustomStyleKind::Color}, {TEXT("-ck-map-player"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-map-route"), ECkUiCustomStyleKind::Color}, {TEXT("-ck-map-label-font-size"), ECkUiCustomStyleKind::Length}};
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& In, FString& Failure) -> TSharedPtr<ICkUiRetainedWidget>
    {
        const auto State = MakeShared<FCkUiWorldMapState>();
        if (!ck_ui_world_map::Configure(In, State->Config, State->Markers, Failure)) { return {}; }
        State->SeenRevision = State->Config.Markers->GetRevision(); State->AcceptedRevision = State->SeenRevision;
        SAssignNew(State->Widget, SCkUiWorldMap, State).Tag(FName(*In.Id)); return State;
    };
    return Registry.Register(MoveTemp(Registration));
}
