#include "CkSlateLayout/CkUiItemSlot.h"

#include "CkSlateLayout/CkFlexLayoutTypes.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ui_item_slot
{
    struct FVisual
    {
        FLinearColor Background = FLinearColor(0.10f, 0.14f, 0.18f);
        FLinearColor Border = FLinearColor(0.30f, 0.35f, 0.40f);
        FLinearColor Focus = FLinearColor(1.0f, 0.85f, 0.45f);
        FLinearColor Selected = FLinearColor(0.85f, 0.65f, 0.25f);
        FLinearColor Valid = FLinearColor(0.25f, 0.80f, 0.40f);
        FLinearColor Invalid = FLinearColor(0.85f, 0.25f, 0.25f);
        float BorderWidth = 1.0f;
        float Padding = 4.0f;
    };

    struct FConfiguration
    {
        TAttribute<FText> Key;
        TAttribute<FText> Kind;
        TAttribute<FText> Accepts;
        TAttribute<bool> Selected;
        TAttribute<bool> Enabled;
        TAttribute<bool> DragEnabled;
        TAttribute<float> Rarity;
        TAttribute<FText> DragLabel;
        TAttribute<const FSlateBrush*> DragImage;
        TAttribute<float> DragIndex;
        int32 DragColumns = 0;
        int32 DragRows = 0;
        TAttribute<bool> CanDispatch;
        FCkUiOnStringChanged Select;
        FCkUiOnStringChanged Drop;
        TWeakPtr<FCkUiView> OwnerView;
        TWeakPtr<int32> Domain;
        TArray<TWeakPtr<FCkUiCollection>> InteractionCollections;
        TSharedPtr<SWidget> Content;
        TSharedPtr<SWidget> Tooltip;
        FVisual Visual;
    };

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& In, FConfiguration& Out, FString& Error) -> bool
    {
        const auto* Select = In.StringChanged.Find(TEXT("select"));
        Out.Content = In.Slots.FindRef(TEXT("content"));
        Out.Tooltip = In.Slots.FindRef(TEXT("tooltip"));
        if (In.Id.IsEmpty() || Select == nullptr || !Select->IsBound() || !Out.Content.IsValid()
            || !In.TextBindings.Contains(TEXT("key")) || !In.TextBindings.Contains(TEXT("kind"))
            || !In.OwnerView.IsValid() || !In.ItemSlotDomain.IsValid())
        {
            Error = TEXT("Item slot requires id, key/kind bindings, select callback, content and an owning view.");
            return false;
        }
        Out.Key = In.TextBindings.FindRef(TEXT("key"));
        Out.Kind = In.TextBindings.FindRef(TEXT("kind"));
        Out.Accepts = TAttribute<FText>(In.TextProperties.FindRef(TEXT("accepts")));
        Out.Selected = In.BoolBindings.FindRef(TEXT("selected"));
        Out.Enabled = In.BoolBindings.FindRef(TEXT("enabled"));
        Out.DragEnabled = In.BoolBindings.FindRef(TEXT("drag-enabled"));
        Out.Rarity = In.NumberBindings.FindRef(TEXT("rarity"));
        Out.DragLabel = In.TextBindings.FindRef(TEXT("drag-label"));
        const bool HasDragImage = In.ImageBindings.Contains(TEXT("drag-image"));
        const bool HasDragIndex = In.NumberBindings.Contains(TEXT("drag-index"));
        const float* DragColumns = In.NumberProperties.Find(TEXT("drag-columns"));
        const float* DragRows = In.NumberProperties.Find(TEXT("drag-rows"));
        const bool HasAtlas = HasDragImage || HasDragIndex || DragColumns != nullptr || DragRows != nullptr;
        if (HasAtlas)
        {
            const bool ValidDimensions = DragColumns != nullptr && DragRows != nullptr
                && FMath::IsFinite(*DragColumns) && FMath::IsFinite(*DragRows)
                && *DragColumns >= 1.0f && *DragColumns <= 64.0f
                && *DragRows >= 1.0f && *DragRows <= 64.0f
                && *DragColumns == FMath::FloorToFloat(*DragColumns)
                && *DragRows == FMath::FloorToFloat(*DragRows);
            if (!HasDragImage || !HasDragIndex || !ValidDimensions
                || !In.ImageBindings.FindRef(TEXT("drag-image")).IsSet()
                || !In.NumberBindings.FindRef(TEXT("drag-index")).IsSet())
            {
                Error = TEXT("Item slot drag atlas requires image/index bindings and integer columns/rows in [1,64].");
                return false;
            }
            Out.DragImage = In.ImageBindings.FindRef(TEXT("drag-image"));
            Out.DragIndex = In.NumberBindings.FindRef(TEXT("drag-index"));
            Out.DragColumns = static_cast<int32>(*DragColumns);
            Out.DragRows = static_cast<int32>(*DragRows);
        }
        Out.CanDispatch = In.CanDispatchEvents;
        Out.Select = *Select;
        Out.Drop = In.StringChanged.FindRef(TEXT("drop"));
        Out.OwnerView = In.OwnerView;
        Out.Domain = In.ItemSlotDomain;
        Out.InteractionCollections = In.InteractionCollections;
        const auto Color = [&In, &Error](const TCHAR* Name, FLinearColor& Value) -> bool
        {
            if (const auto* Property = In.Style.CustomProperties.Find(Name))
            {
                const FLinearColor Input = Property->Color;
                if (Property->Kind != ECkUiCustomStyleKind::Color || !FMath::IsFinite(Input.R)
                    || !FMath::IsFinite(Input.G) || !FMath::IsFinite(Input.B) || !FMath::IsFinite(Input.A))
                { Error = FString::Printf(TEXT("Item slot %s requires a finite color."), Name); return false; }
                Value = Input;
            }
            return true;
        };
        const auto Length = [&In, &Error](const TCHAR* Name, float& Value) -> bool
        {
            if (const auto* Property = In.Style.CustomProperties.Find(Name))
            {
                if (Property->Kind != ECkUiCustomStyleKind::Length || !FMath::IsFinite(Property->Number)
                    || Property->Number < 0.0f || Property->Number > 512.0f)
                { Error = FString::Printf(TEXT("Item slot %s requires a finite length in [0, 512]."), Name); return false; }
                Value = Property->Number;
            }
            return true;
        };
        return Color(TEXT("-ck-item-slot-background"), Out.Visual.Background)
            && Color(TEXT("-ck-item-slot-border"), Out.Visual.Border)
            && Color(TEXT("-ck-item-slot-focus"), Out.Visual.Focus)
            && Color(TEXT("-ck-item-slot-selected"), Out.Visual.Selected)
            && Color(TEXT("-ck-item-slot-valid"), Out.Visual.Valid)
            && Color(TEXT("-ck-item-slot-invalid"), Out.Visual.Invalid)
            && Length(TEXT("-ck-item-slot-border-width"), Out.Visual.BorderWidth)
            && Length(TEXT("-ck-item-slot-padding"), Out.Visual.Padding);
    }
}

class FCkUiItemSlotOwner final : public ICkUiRetainedWidget, public TSharedFromThis<FCkUiItemSlotOwner>
{
public:
    explicit FCkUiItemSlotOwner(ck_ui_item_slot::FConfiguration In) : Config(MoveTemp(In)) {}

    void Initialize(const FString& Id)
    {
        const TWeakPtr<FCkUiItemSlotOwner> Weak = AsShared();
        SAssignNew(Widget, SCkUiItemSlot, Weak)[Config.Content.ToSharedRef()];
        Widget->SetTag(FName(*Id));
        SAssignNew(Anchor, SMenuAnchor)
            .Placement(MenuPlacement_BelowAnchor)
            .FitInWindow(true)
            .Method(EPopupMethod::UseCurrentWindow)
            .UseApplicationMenuStack(false)
            .ShowMenuBackground(false)
            .OnMenuOpenChanged_Lambda([Weak](bool Open)
            { if (!Open) { if (const auto Owner = Weak.Pin()) { Owner->RestoreParking(); } } })
            .OnGetMenuContent_Lambda([Weak]() -> TSharedRef<SWidget>
            {
                const auto Owner = Weak.Pin();
                if (!Owner.IsValid() || !Owner->HasTooltipContent()) { return SNullWidget::NullWidget; }
                const FVector2D TooltipSize = Owner->MeasureTooltip();
                Owner->Parking->SetContent(SNullWidget::NullWidget);
                TSharedPtr<SBox> Popup;
                SAssignNew(Popup, SBox)
                    .WidthOverride(TooltipSize.X)
                    .HeightOverride(TooltipSize.Y)
                    .MaxDesiredWidth_Lambda([Weak]() -> FOptionalSize
                    { const auto Current = Weak.Pin(); return Current.IsValid() ? Current->TooltipLimit(true) : FOptionalSize{}; })
                    .MaxDesiredHeight_Lambda([Weak]() -> FOptionalSize
                    { const auto Current = Weak.Pin(); return Current.IsValid() ? Current->TooltipLimit(false) : FOptionalSize{}; })
                    [Owner->Config.Tooltip.ToSharedRef()];
                Owner->TooltipBox = Popup;
                return Popup.ToSharedRef();
            })[Widget.ToSharedRef()];
        SAssignNew(Parking, SBox).Visibility(EVisibility::Collapsed)
            [Config.Tooltip.IsValid() ? Config.Tooltip.ToSharedRef() : SNullWidget::NullWidget];
        SAssignNew(Root, SOverlay)
            + SOverlay::Slot()[Anchor.ToSharedRef()]
            + SOverlay::Slot()[Parking.ToSharedRef()];
        Root->AddMetadata(MakeShared<FCkFlexMeasureMetaData>(
            [Weak](const FCkFlexMeasureArgs& Args)
            {
                const auto Owner = Weak.Pin();
                if (!Owner.IsValid() || !Owner->Config.Content.IsValid()) { return FVector2D::ZeroVector; }
                FCkFlexMeasureArgs Inner = Args;
                const float Pad = Owner->Config.Visual.Padding * 2.0f;
                if (Inner.WidthMode != YGMeasureModeUndefined) { Inner.AvailableWidth = FMath::Max(0.0f, Inner.AvailableWidth - Pad); }
                if (Inner.HeightMode != YGMeasureModeUndefined) { Inner.AvailableHeight = FMath::Max(0.0f, Inner.AvailableHeight - Pad); }
                const auto Measure = Owner->SlotMeasure(Owner->Config.Content);
                const FVector2D Size = Measure.IsValid() ? Measure->Measure(Inner) : Owner->Config.Content->GetDesiredSize();
                return Size + FVector2D(Pad, Pad);
            },
            [Weak](float Width, float Height)
            {
                const auto Owner = Weak.Pin();
                if (!Owner.IsValid() || !Owner->Config.Content.IsValid()) { return; }
                const auto Measure = Owner->SlotMeasure(Owner->Config.Content);
                if (Measure.IsValid())
                {
                    const float Pad = Owner->Config.Visual.Padding * 2.0f;
                    Measure->NotifyArranged(FMath::Max(0.0f, Width - Pad), FMath::Max(0.0f, Height - Pad));
                }
            }));
    }

    auto IsActive() const -> bool
    { return Active && Config.OwnerView.IsValid() && Config.Domain.IsValid() && Config.CanDispatch.Get(false) && Config.Enabled.Get(true); }
    auto CanDrag() const -> bool
    { return IsActive() && Config.DragEnabled.Get(true) && !Config.Key.Get(FText::GetEmpty()).IsEmpty()
        && !Config.Kind.Get(FText::GetEmpty()).IsEmpty(); }
    auto Key() const -> FString { return Config.Key.Get(FText::GetEmpty()).ToString(); }
    auto Kind() const -> FString { return Config.Kind.Get(FText::GetEmpty()).ToString(); }
    auto HasTooltipContent() const -> bool
    {
        if (!Config.Tooltip.IsValid()) { return false; }
        const FChildren* Children = Config.Tooltip->GetChildren();
        return Children != nullptr && Children->Num() > 0
            && &Children->GetChildAt(0).Get() != &SNullWidget::NullWidget.Get();
    }
    auto SlotMeasure(const TSharedPtr<SWidget>& Slot) const -> TSharedPtr<FCkFlexMeasureMetaData>
    {
        if (!Slot.IsValid()) { return {}; }
        auto Measure = Slot->GetMetaData<FCkFlexMeasureMetaData>();
        const FChildren* Children = Slot->GetChildren();
        if (!Measure.IsValid() && Children != nullptr && Children->Num() == 1)
        { Measure = Children->GetChildAt(0)->GetMetaData<FCkFlexMeasureMetaData>(); }
        return Measure;
    }
    auto MeasureTooltip() const -> FVector2D
    {
        if (!Config.Tooltip.IsValid()) { return FVector2D::ZeroVector; }
        const FVector2D Max{TooltipLimit(true).Get(), TooltipLimit(false).Get()};
        const float MaxWidth = static_cast<float>(Max.X);
        const float MaxHeight = static_cast<float>(Max.Y);
        const auto Measure = SlotMeasure(Config.Tooltip);
        const FVector2D Desired = Measure.IsValid()
            ? Measure->Measure({MaxWidth, YGMeasureModeAtMost, MaxHeight, YGMeasureModeAtMost, 1.0f})
            : Config.Tooltip->GetDesiredSize();
        const FVector2D Size{FMath::Clamp(Desired.X, 0.0, Max.X), FMath::Clamp(Desired.Y, 0.0, Max.Y)};
        if (Measure.IsValid()) { Measure->NotifyArranged(Size.X, Size.Y); }
        return Size;
    }
    auto CanAccept(const FCkUiItemSlotDragOperation& Drag) const -> bool
    {
        for (const auto& Snapshot : Drag._Collections)
        {
            const auto Collection = Snapshot.Collection.Pin();
            if (!Collection.IsValid() || Collection->GetRevision() != Snapshot.Revision) { return false; }
        }
        const auto Domain = Config.Domain.Pin();
        const auto Source = Drag._Source.Pin();
        const auto SourceOwner = Source.IsValid() ? Source->_Owner.Pin() : nullptr;
        const auto Accepts = Config.Accepts.Get(FText::GetEmpty()).ToString();
        return IsActive() && Config.Drop.IsBound() && !Drag._Consumed && Domain.IsValid() && Domain == Drag._Domain.Pin()
            && !Accepts.IsEmpty() && (Accepts == TEXT("*") || Accepts == Drag._Kind)
            && Source.IsValid() && Source.Get() != Widget.Get()
            && SourceOwner.IsValid() && SourceOwner->CanDrag() && SourceOwner->Generation == Drag._Generation
            && SourceOwner->Key() == Drag._Key && SourceOwner->Kind() == Drag._Kind;
    }
    void Select()
    {
        if (!IsActive()) { return; }
        const FString StableKey = Key();
        if (StableKey.IsEmpty()) { return; }
        const auto Callback = Config.Select;
        Callback.ExecuteIfBound(StableKey);
        if (IsActive() && Widget.IsValid()
            && (Widget->IsHovered() || Widget->HasAnyUserFocus().IsSet()))
        { ShowTooltip(); }
    }
    auto Drop(const TSharedPtr<FCkUiItemSlotDragOperation>& Drag) -> bool
    {
        if (!Drag.IsValid() || !CanAccept(*Drag)) { return false; }
        const FString StableKey = Drag->_Key;
        const auto Callback = Config.Drop;
        Drag->_Consumed = true;
        Callback.ExecuteIfBound(StableKey);
        return true;
    }

    virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Root.ToSharedRef(); }
    virtual void ReleaseTransientInteraction() override { ++Generation; HideTooltip(); Widget->RefreshStyle(); }
    virtual void ReleaseOwnerInteraction() override { Active = false; ReleaseTransientInteraction(); }
    virtual auto PrepareReload(const FCkUiCustomWidgetArguments& In, FString& Error) const
        -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    auto TooltipLimit(bool Width) const -> FOptionalSize
    {
        if (!Widget.IsValid() || !FSlateApplication::IsInitialized()) { return FOptionalSize{0.0f}; }
        const auto Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
        if (!Window.IsValid()) { return FOptionalSize{0.0f}; }
        const FVector2D Size = Window->GetClientSizeInScreen();
        return FOptionalSize{static_cast<float>(FMath::Max(0.0, (Width ? Size.X : Size.Y) - 16.0))};
    }
    void ShowTooltip()
    {
        if (IsActive() && HasTooltipContent() && Anchor.IsValid()
            && TooltipLimit(true).Get() > 0.0f && TooltipLimit(false).Get() > 0.0f)
        {
            Anchor->SetIsOpen(true, false);
            if (!Anchor->IsOpen()) { RestoreParking(); }
        }
    }
    void RestoreParking()
    {
        if (!Parking.IsValid()) { return; }
        if (const auto Popup = TooltipBox.Pin()) { Popup->SetContent(SNullWidget::NullWidget); }
        if (Config.Tooltip.IsValid() && Config.Tooltip->GetParentWidget().Get() != Parking.Get())
        { Parking->SetContent(Config.Tooltip.ToSharedRef()); }
        TooltipBox.Reset();
    }
    void HideTooltip()
    {
        if (Anchor.IsValid() && Anchor->IsOpen()) { Anchor->SetIsOpen(false, false); }
        RestoreParking();
    }

    ck_ui_item_slot::FConfiguration Config;
    TSharedPtr<SCkUiItemSlot> Widget;
    TSharedPtr<SMenuAnchor> Anchor;
    TSharedPtr<SOverlay> Root;
    TSharedPtr<SBox> Parking;
    TWeakPtr<SBox> TooltipBox;
    uint64 Generation = 1;
    bool Active = true;
};

namespace ck_ui_item_slot
{
    class FPrepared final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FPrepared(TSharedRef<FCkUiItemSlotOwner> InOwner, FConfiguration InConfig)
            : Owner(MoveTemp(InOwner)), Config(MoveTemp(InConfig)) {}
        virtual void Commit() noexcept override
        {
            ++Owner->Generation;
            Owner->HideTooltip();
            Owner->Config = MoveTemp(Config);
            Owner->Widget->RefreshStyle();
        }
    private:
        TSharedRef<FCkUiItemSlotOwner> Owner;
        FConfiguration Config;
    };
}

auto FCkUiItemSlotOwner::PrepareReload(const FCkUiCustomWidgetArguments& In, FString& Error) const
    -> TUniquePtr<ICkUiPreparedWidgetUpdate>
{
    ck_ui_item_slot::FConfiguration Next;
    if (!ck_ui_item_slot::MakeConfiguration(In, Next, Error)) { return {}; }
    if (Next.Content != Config.Content || Next.Tooltip != Config.Tooltip
        || Next.Domain.Pin() != Config.Domain.Pin())
    { Error = TEXT("Item slot content, tooltip slot and drag domain cannot change under one id."); return {}; }
    return MakeUnique<ck_ui_item_slot::FPrepared>(ConstCastSharedRef<FCkUiItemSlotOwner>(AsShared()), MoveTemp(Next));
}

void SCkUiItemSlot::Construct(const FArguments& InArgs, TWeakPtr<FCkUiItemSlotOwner> InOwner)
{
    _Owner = MoveTemp(InOwner);
    _Style = FButtonStyle().SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f,
        FLinearColor::Transparent, 0.0f));
    SButton::Construct(SButton::FArguments().ButtonStyle(&_Style).IsFocusable(true)
        .OnClicked_Lambda([Weak = _Owner]()
        {
            if (const auto Owner = Weak.Pin()) { Owner->Select(); }
            return FReply::Handled();
        })[InArgs._Content.Widget]);
    SetEnabled(TAttribute<bool>::CreateLambda([Weak = _Owner]()
    { const auto Owner = Weak.Pin(); return Owner.IsValid() && Owner->IsActive(); }));
    RefreshStyle();
}

auto SCkUiItemSlot::GetItemKey() const -> FString
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() ? Owner->Key() : FString{}; }
auto SCkUiItemSlot::GetItemKind() const -> FString
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() ? Owner->Kind() : FString{}; }

auto SCkUiItemSlot::TryBeginDrag() -> TSharedPtr<FCkUiItemSlotDragOperation>
{
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid() || !Owner->CanDrag()) { return {}; }
    auto Drag = MakeShared<FCkUiItemSlotDragOperation>();
    Drag->_Source = SharedThis(this);
    Drag->_VisualOwner = Owner;
    Drag->_Domain = Owner->Config.Domain;
    Drag->_Generation = Owner->Generation;
    Drag->_Key = Owner->Key();
    Drag->_Kind = Owner->Kind();
    Drag->_DragLabel = Owner->Config.DragLabel.IsSet()
        ? Owner->Config.DragLabel.Get(FText::GetEmpty()).ToString() : Drag->_Key;
    if (Drag->_DragLabel.IsEmpty()) { Drag->_DragLabel = Drag->_Key; }
    if (Owner->Config.DragColumns > 0 && Owner->Config.DragRows > 0)
    {
        const float Raw = Owner->Config.DragIndex.Get(-1.0f);
        const int32 Count = Owner->Config.DragColumns * Owner->Config.DragRows;
        if (FMath::IsFinite(Raw) && Raw >= 0.0f && Raw < Count && Raw == FMath::FloorToFloat(Raw))
        {
            if (const FSlateBrush* SourceBrush = Owner->Config.DragImage.Get(nullptr);
                SourceBrush != nullptr && FMath::IsFinite(SourceBrush->ImageSize.X)
                && FMath::IsFinite(SourceBrush->ImageSize.Y)
                && SourceBrush->ImageSize.X > 0.0f && SourceBrush->ImageSize.Y > 0.0f)
            {
                Drag->_DragBrush = MakeShared<FSlateBrush>(*SourceBrush);
                const int32 Index = static_cast<int32>(Raw);
                const FVector2D Cell(1.0 / Owner->Config.DragColumns, 1.0 / Owner->Config.DragRows);
                const FVector2D Min((Index % Owner->Config.DragColumns) * Cell.X,
                    (Index / Owner->Config.DragColumns) * Cell.Y);
                Drag->_DragBrush->SetUVRegion(FBox2D(Min, Min + Cell));
                Drag->_DragBrush->ImageSize = FVector2D(SourceBrush->ImageSize.X * Cell.X,
                    SourceBrush->ImageSize.Y * Cell.Y);
            }
        }
    }
    for (const auto& WeakCollection : Owner->Config.InteractionCollections)
    {
        const auto Collection = WeakCollection.Pin();
        if (!Collection.IsValid()) { return {}; }
        Drag->_Collections.Add(FCkUiItemSlotDragOperation::FCollectionRevision{WeakCollection,
            Collection->GetRevision()});
    }
    Drag->Construct();
    return Drag;
}

auto SCkUiItemSlot::CanAcceptDrag(const TSharedPtr<FCkUiItemSlotDragOperation>& InDrag) const -> bool
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() && InDrag.IsValid() && Owner->CanAccept(*InDrag); }
auto SCkUiItemSlot::TryAcceptDrag(const TSharedPtr<FCkUiItemSlotDragOperation>& InDrag) -> bool
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() && Owner->Drop(InDrag); }
auto SCkUiItemSlot::HasAuthoredTooltip() const -> bool
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() && Owner->HasTooltipContent(); }
auto SCkUiItemSlot::GetTooltipMaxSize() const -> FVector2D
{
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid()) { return FVector2D::ZeroVector; }
    return FVector2D{Owner->TooltipLimit(true).Get(), Owner->TooltipLimit(false).Get()};
}
auto SCkUiItemSlot::GetIsTooltipOpen() const -> bool
{ const auto Owner = _Owner.Pin(); return Owner.IsValid() && Owner->Anchor.IsValid() && Owner->Anchor->IsOpen(); }
auto SCkUiItemSlot::TryGetTooltipScreenBounds(FSlateRect& OutPopup, FSlateRect& OutViewport) const -> bool
{
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid() || !Owner->Anchor.IsValid() || !Owner->Anchor->IsOpen()
        || !FSlateApplication::IsInitialized() || !Owner->Widget.IsValid()) { return false; }
    const auto Popup = Owner->TooltipBox.Pin();
    const auto Window = FSlateApplication::Get().FindWidgetWindow(Owner->Widget.ToSharedRef());
    if (!Popup.IsValid() || !Window.IsValid()) { return false; }
    const FSlateRect Bounds = Popup->GetCachedGeometry().GetRenderBoundingRect();
    if (Bounds.Right <= Bounds.Left || Bounds.Bottom <= Bounds.Top) { return false; }
    OutPopup = Bounds;
    OutViewport = Window->GetClientRectInScreen();
    return true;
}

auto SCkUiItemSlot::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
{
    FReply Reply = SButton::OnMouseButtonDown(Geometry, Event);
    const auto Owner = _Owner.Pin();
    if (Owner.IsValid() && Owner->CanDrag() && Event.GetEffectingButton() == EKeys::LeftMouseButton)
    { Reply.DetectDrag(SharedThis(this), EKeys::LeftMouseButton); }
    return Reply;
}
void SCkUiItemSlot::OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{ SButton::OnMouseEnter(Geometry, Event); if (const auto Owner = _Owner.Pin()) { Owner->ShowTooltip(); } }
void SCkUiItemSlot::OnMouseLeave(const FPointerEvent& Event)
{ if (const auto Owner = _Owner.Pin()) { Owner->HideTooltip(); } SButton::OnMouseLeave(Event); }
auto SCkUiItemSlot::OnDragDetected(const FGeometry&, const FPointerEvent&) -> FReply
{
    const auto Drag = TryBeginDrag();
    if (!Drag.IsValid()) { return FReply::Unhandled(); }
    Release();
    if (const auto Owner = _Owner.Pin()) { Owner->HideTooltip(); }
    return FReply::Handled().BeginDragDrop(Drag.ToSharedRef());
}
void SCkUiItemSlot::OnDragEnter(const FGeometry&, const FDragDropEvent& Event)
{
    const auto Owner = _Owner.Pin();
    _DragOver = Owner.IsValid() && Owner->IsActive();
    _DragValid = _DragOver && CanAcceptDrag(Event.GetOperationAs<FCkUiItemSlotDragOperation>());
    RefreshStyle();
}
void SCkUiItemSlot::OnDragLeave(const FDragDropEvent&)
{ _DragOver = false; _DragValid = false; RefreshStyle(); }
auto SCkUiItemSlot::OnDrop(const FGeometry&, const FDragDropEvent& Event) -> FReply
{
    _DragOver = false; _DragValid = false; RefreshStyle();
    return TryAcceptDrag(Event.GetOperationAs<FCkUiItemSlotDragOperation>()) ? FReply::Handled() : FReply::Unhandled();
}
auto SCkUiItemSlot::OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) -> FReply
{
    _Focused = true; RefreshStyle();
    const FReply Reply = SButton::OnFocusReceived(Geometry, Event);
    if (const auto Owner = _Owner.Pin()) { Owner->ShowTooltip(); }
    return Reply;
}
void SCkUiItemSlot::OnFocusLost(const FFocusEvent& Event)
{
    if (const auto Owner = _Owner.Pin()) { Owner->HideTooltip(); }
    _Focused = false; RefreshStyle(); SButton::OnFocusLost(Event);
}
void SCkUiItemSlot::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SButton::Tick(Geometry, Time, Delta);
    const auto Owner = _Owner.Pin();
    const bool Selected = Owner.IsValid() && Owner->Config.Selected.Get(false);
    const float Rarity = Owner.IsValid() ? Owner->Config.Rarity.Get(0.0f) : 0.0f;
    if (Selected != _LastSelected || Rarity != _LastRarity) { RefreshStyle(); }
}
auto SCkUiItemSlot::RefreshStyle() -> void
{
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid()) { return; }
    const auto& Visual = Owner->Config.Visual;
    _LastSelected = Owner->Config.Selected.Get(false);
    _LastRarity = Owner->Config.Rarity.Get(0.0f);
    const int32 Tier = FMath::IsFinite(_LastRarity)
        ? FMath::FloorToInt(FMath::Clamp(_LastRarity, 0.0f, 3.0f)) : 0;
    const FLinearColor TierBorder = Tier == 1 ? FLinearColor(0.26f, 0.52f, 0.90f)
        : Tier == 2 ? FLinearColor(0.95f, 0.79f, 0.25f)
        : Tier == 3 ? FLinearColor(0.95f, 0.56f, 0.16f) : Visual.Border;
    const FLinearColor Border = _DragOver ? (_DragValid ? Visual.Valid : Visual.Invalid)
        : _Focused ? Visual.Focus : _LastSelected ? Visual.Selected : TierBorder;
    _Style = FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(Visual.Background, 4.0f, Border, Visual.BorderWidth))
        .SetHovered(FSlateRoundedBoxBrush(Visual.Background, 4.0f, Border, Visual.BorderWidth))
        .SetPressed(FSlateRoundedBoxBrush(Visual.Background, 4.0f, Border, Visual.BorderWidth))
        .SetDisabled(FSlateRoundedBoxBrush(Visual.Background, 4.0f, Visual.Border, Visual.BorderWidth))
        .SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
    SetButtonStyle(&_Style);
    SetContentPadding(FMargin(Visual.Padding));
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto FCkUiItemSlotDragOperation::GetDefaultDecorator() const -> TSharedPtr<SWidget>
{
    if (!_DragBrush.IsValid()) { return SNew(STextBlock).Text(FText::FromString(_DragLabel)); }
    const TWeakPtr<FCkUiItemSlotOwner> WeakOwner = _VisualOwner;
    const uint64 Generation = _Generation;
    const TSharedPtr<FSlateBrush> Brush = _DragBrush;
    const FString Key = _Key;
    const FString Kind = _Kind;
    const TArray<FCollectionRevision> Collections = _Collections;
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SBox).WidthOverride(48.0f).HeightOverride(48.0f)
            [SNew(SImage).Image_Lambda([WeakOwner, Generation, Key, Kind, Collections, Brush]() -> const FSlateBrush*
            {
                const auto Owner = WeakOwner.Pin();
                if (!Owner.IsValid() || !Owner->IsActive() || Owner->Generation != Generation
                    || Owner->Key() != Key || Owner->Kind() != Kind) { return nullptr; }
                for (const auto& Snapshot : Collections)
                {
                    const auto Collection = Snapshot.Collection.Pin();
                    if (!Collection.IsValid() || Collection->GetRevision() != Snapshot.Revision) { return nullptr; }
                }
                return Brush.Get();
            })]]
        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.0f, 0.0f))
        [SNew(STextBlock).Text(FText::FromString(_DragLabel))];
}

auto FCkUiItemSlot::RegisterItemSlotUiControl(FCkUiWidgetRegistry& Registry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("item-slot");
    Registration.Schema.Properties = {
        {TEXT("key"), ECkUiCustomPropertyKind::TextBinding, true},
        {TEXT("kind"), ECkUiCustomPropertyKind::TextBinding, true},
        {TEXT("accepts"), ECkUiCustomPropertyKind::Text, false},
        {TEXT("select"), ECkUiCustomPropertyKind::StringChanged, true},
        {TEXT("drop"), ECkUiCustomPropertyKind::StringChanged, false},
        {TEXT("selected"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("enabled"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("drag-enabled"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("rarity"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("drag-label"), ECkUiCustomPropertyKind::TextBinding, false},
        {TEXT("drag-image"), ECkUiCustomPropertyKind::ImageBinding, false},
        {TEXT("drag-index"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("drag-columns"), ECkUiCustomPropertyKind::Number, false},
        {TEXT("drag-rows"), ECkUiCustomPropertyKind::Number, false},
    };
    Registration.Schema.Slots = {{TEXT("content"), true}, {TEXT("tooltip"), false}};
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-item-slot-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-border"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-focus"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-selected"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-valid"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-invalid"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-item-slot-border-width"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-item-slot-padding"), ECkUiCustomStyleKind::Length},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& Args, FString& Error) -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_item_slot::FConfiguration Config;
        if (!ck_ui_item_slot::MakeConfiguration(Args, Config, Error)) { return {}; }
        const auto Owner = MakeShared<FCkUiItemSlotOwner>(MoveTemp(Config));
        Owner->Initialize(Args.Id);
        return Owner;
    };
    return Registry.Register(MoveTemp(Registration));
}

auto RegisterItemSlotUiControl(FCkUiWidgetRegistry& Registry) -> FCkUiLoadResult
{ return FCkUiItemSlot::RegisterItemSlotUiControl(Registry); }
