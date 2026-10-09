#include "CkSlateLayout/CkUiInventoryGrid.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiFontRegistry.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Math/TransformCalculus2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ui_inventory_grid
{
    struct FVisual
    {
        FLinearColor Background = FLinearColor(0.055f, 0.058f, 0.050f, 0.98f);
        FLinearColor CellBackground = FLinearColor(0.12f, 0.13f, 0.105f);
        FLinearColor ItemBackground = FLinearColor(0.16f, 0.17f, 0.13f);
        FLinearColor Border = FLinearColor(0.34f, 0.37f, 0.28f);
        FLinearColor Selected = FLinearColor(0.87f, 0.70f, 0.30f);
        int32 LabelFontSize = 10;
    };

    struct FItem
    {
        FString Key;
        FString Name;
        FString Kind;
        int32 X = 0;
        int32 Y = 0;
        int32 Width = 1;
        int32 Height = 1;
        int32 Icon = -1;
        int32 Quantity = 0;
        float IconLeft = 0.0f;
        float IconTop = 0.0f;
        float IconRight = 1.0f;
        float IconBottom = 1.0f;
        bool Rotated = false;
        bool Selected = false;
        bool Enabled = true;
        TSharedPtr<FSlateBrush> AtlasBrush;
    };

    struct FConfiguration
    {
        TSharedPtr<FCkUiCollection> Items;
        TAttribute<const FSlateBrush*> Image;
        TAttribute<FText> SelectedKey;
        TAttribute<bool> CanDispatch;
        FCkUiOnStringChanged Select;
        FCkUiOnStringChanged Move;
        TWeakPtr<FCkUiView> OwnerView;
        TWeakPtr<int32> Domain;
        TArray<TWeakPtr<FCkUiCollection>> InteractionCollections;
        FSlateFontInfo Font;
        FVisual Visual;
        int32 Columns = 1;
        int32 Rows = 1;
        int32 AtlasColumns = 1;
        int32 AtlasRows = 1;
        float CellSize = 48.0f;
        int32 UserIndex = INDEX_NONE;
    };

    auto ReadInteger(const FCkUiRecord& InRecord, const TCHAR* InName, int32 InMin, int32 InMax, int32& Out) -> bool
    {
        const FCkUiFieldValue* Field = InRecord.FindField(InName);
        if (Field == nullptr || Field->Kind != ECkUiFieldKind::Number || !FMath::IsFinite(Field->Number)
            || Field->Number < static_cast<float>(InMin) || Field->Number > static_cast<float>(InMax)
            || Field->Number != FMath::FloorToFloat(Field->Number))
        { return false; }
        Out = static_cast<int32>(Field->Number);
        return true;
    }

    auto ReadItems(const FCkUiCollection& InCollection, const FConfiguration& InConfig,
        TArray<FItem>& OutItems, FString& OutFailure) -> bool
    {
        const auto& Schema = InCollection.GetSchema();
        const auto Has = [&Schema](const TCHAR* Name, ECkUiFieldKind Kind) -> bool
        {
            return Schema.ContainsByPredicate([Name, Kind](const FCkUiFieldSchema& Entry)
            { return Entry.Name == Name && Entry.Kind == Kind; });
        };
        if (!Has(TEXT("name"), ECkUiFieldKind::Text) || !Has(TEXT("kind"), ECkUiFieldKind::Text)
            || !Has(TEXT("x"), ECkUiFieldKind::Number) || !Has(TEXT("y"), ECkUiFieldKind::Number)
            || !Has(TEXT("w"), ECkUiFieldKind::Number) || !Has(TEXT("h"), ECkUiFieldKind::Number)
            || !Has(TEXT("icon"), ECkUiFieldKind::Number) || !Has(TEXT("quantity"), ECkUiFieldKind::Number)
            || !Has(TEXT("rotated"), ECkUiFieldKind::Bool)
            || !Has(TEXT("selected"), ECkUiFieldKind::Bool) || !Has(TEXT("enabled"), ECkUiFieldKind::Bool))
        {
            OutFailure = TEXT("Inventory items require name/kind text, x/y/w/h/icon/quantity numbers, and rotated/selected/enabled booleans.");
            return false;
        }
        const bool AnyCrop = Schema.ContainsByPredicate([](const FCkUiFieldSchema& Entry)
        { return Entry.Name == TEXT("icon-left") || Entry.Name == TEXT("icon-top")
            || Entry.Name == TEXT("icon-right") || Entry.Name == TEXT("icon-bottom"); });
        if (AnyCrop && (!Has(TEXT("icon-left"), ECkUiFieldKind::Number)
            || !Has(TEXT("icon-top"), ECkUiFieldKind::Number)
            || !Has(TEXT("icon-right"), ECkUiFieldKind::Number)
            || !Has(TEXT("icon-bottom"), ECkUiFieldKind::Number)))
        { OutFailure = TEXT("Inventory icon crop schema requires all four Number fields."); return false; }
        TArray<uint8> Occupied;
        Occupied.Init(0, InConfig.Columns * InConfig.Rows);
        for (const auto& Record : InCollection.GetRecords())
        {
            FItem Item;
            if (!Record.IsValid() || Record->GetKey().IsEmpty() || Record->GetKey().Contains(TEXT("|")))
            { OutFailure = TEXT("Inventory item requires a nonempty key without '|'."); return false; }
            Item.Key = Record->GetKey();
            const FCkUiFieldValue* Name = Record->FindField(TEXT("name"));
            const FCkUiFieldValue* Kind = Record->FindField(TEXT("kind"));
            const FCkUiFieldValue* Rotated = Record->FindField(TEXT("rotated"));
            const FCkUiFieldValue* Selected = Record->FindField(TEXT("selected"));
            const FCkUiFieldValue* Enabled = Record->FindField(TEXT("enabled"));
            if (Name == nullptr || Name->Kind != ECkUiFieldKind::Text || Name->Text.IsEmpty()
                || Kind == nullptr || Kind->Kind != ECkUiFieldKind::Text || Kind->Text.IsEmpty()
                || Rotated == nullptr || Rotated->Kind != ECkUiFieldKind::Bool
                || Selected == nullptr || Selected->Kind != ECkUiFieldKind::Bool
                || Enabled == nullptr || Enabled->Kind != ECkUiFieldKind::Bool
                || !ReadInteger(*Record, TEXT("x"), 0, InConfig.Columns - 1, Item.X)
                || !ReadInteger(*Record, TEXT("y"), 0, InConfig.Rows - 1, Item.Y)
                || !ReadInteger(*Record, TEXT("w"), 1, 64, Item.Width)
                || !ReadInteger(*Record, TEXT("h"), 1, 64, Item.Height)
                || !ReadInteger(*Record, TEXT("icon"), -1, InConfig.AtlasColumns * InConfig.AtlasRows - 1, Item.Icon)
                || !ReadInteger(*Record, TEXT("quantity"), 0, 999999, Item.Quantity))
            { OutFailure = TEXT("Inventory item has a missing, mismatched, nonfinite, or out-of-range field."); return false; }
            Item.Name = Name->Text.ToString();
            Item.Kind = Kind->Text.ToString();
            if (AnyCrop)
            {
                const FCkUiFieldValue* Left = Record->FindField(TEXT("icon-left"));
                const FCkUiFieldValue* Top = Record->FindField(TEXT("icon-top"));
                const FCkUiFieldValue* Right = Record->FindField(TEXT("icon-right"));
                const FCkUiFieldValue* Bottom = Record->FindField(TEXT("icon-bottom"));
                if (Left == nullptr || Top == nullptr || Right == nullptr || Bottom == nullptr
                    || Left->Kind != ECkUiFieldKind::Number || Top->Kind != ECkUiFieldKind::Number
                    || Right->Kind != ECkUiFieldKind::Number || Bottom->Kind != ECkUiFieldKind::Number
                    || !FMath::IsFinite(Left->Number) || !FMath::IsFinite(Top->Number)
                    || !FMath::IsFinite(Right->Number) || !FMath::IsFinite(Bottom->Number)
                    || Left->Number < 0.0f || Top->Number < 0.0f
                    || Right->Number > 1.0f || Bottom->Number > 1.0f
                    || Left->Number >= Right->Number || Top->Number >= Bottom->Number)
                { OutFailure = TEXT("Inventory icon crop requires four finite normalized bounds with positive area."); return false; }
                Item.IconLeft = Left->Number;
                Item.IconTop = Top->Number;
                Item.IconRight = Right->Number;
                Item.IconBottom = Bottom->Number;
            }
            Item.Rotated = Rotated->Bool;
            Item.Selected = Selected->Bool;
            Item.Enabled = Enabled->Bool;
            const int32 Width = Item.Rotated ? Item.Height : Item.Width;
            const int32 Height = Item.Rotated ? Item.Width : Item.Height;
            if (Item.X + Width > InConfig.Columns || Item.Y + Height > InConfig.Rows)
            { OutFailure = TEXT("Inventory item extends outside its grid."); return false; }
            for (int32 Y = Item.Y; Y < Item.Y + Height; ++Y)
            {
                for (int32 X = Item.X; X < Item.X + Width; ++X)
                {
                    uint8& Cell = Occupied[Y * InConfig.Columns + X];
                    if (Cell) { OutFailure = TEXT("Inventory items overlap."); return false; }
                    Cell = 1;
                }
            }
            Item.AtlasBrush = MakeShared<FSlateBrush>();
            OutItems.Add(MoveTemp(Item));
        }
        return true;
    }

    auto ParseDimension(const FCkUiCustomWidgetArguments& In, const TCHAR* InName,
        int32 InMinimum, int32 InMaximum, int32& Out, FString& OutFailure) -> bool
    {
        const float* Value = In.NumberProperties.Find(InName);
        if (Value == nullptr || !FMath::IsFinite(*Value) || *Value < InMinimum || *Value > InMaximum
            || *Value != FMath::FloorToFloat(*Value))
        { OutFailure = FString::Printf(TEXT("Inventory grid %s requires an integer in [%d, %d]."), InName, InMinimum, InMaximum); return false; }
        Out = static_cast<int32>(*Value);
        return true;
    }

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& In, FConfiguration& Out, FString& OutFailure) -> bool
    {
        const auto* Items = In.Collections.Find(TEXT("items"));
        const auto* Image = In.ImageBindings.Find(TEXT("image"));
        const auto* Select = In.StringChanged.Find(TEXT("select"));
        const auto* Move = In.StringChanged.Find(TEXT("move"));
        if (In.Id.IsEmpty() || Items == nullptr || !Items->IsValid() || Image == nullptr || !Image->IsSet()
            || Select == nullptr || !Select->IsBound() || Move == nullptr || !Move->IsBound()
            || !In.OwnerView.IsValid() || !In.ItemSlotDomain.IsValid())
        { OutFailure = TEXT("Inventory grid requires id, items/image bindings, select/move callbacks, and an owning view."); return false; }
        Out.Items = *Items;
        Out.Image = *Image;
        Out.Select = *Select;
        Out.Move = *Move;
        Out.SelectedKey = In.TextBindings.FindRef(TEXT("selected-key"));
        Out.CanDispatch = In.CanDispatchEvents;
        Out.OwnerView = In.OwnerView;
        Out.Domain = In.ItemSlotDomain;
        Out.InteractionCollections = In.InteractionCollections;
        Out.UserIndex = In.SlateUserIndex;
        Out.Font = In.BaseFont;
        if (!ParseDimension(In, TEXT("columns"), 1, 64, Out.Columns, OutFailure)
            || !ParseDimension(In, TEXT("rows"), 1, 64, Out.Rows, OutFailure)
            || !ParseDimension(In, TEXT("atlas-columns"), 1, 64, Out.AtlasColumns, OutFailure)
            || !ParseDimension(In, TEXT("atlas-rows"), 1, 64, Out.AtlasRows, OutFailure))
        { return false; }
        const float* CellSize = In.NumberProperties.Find(TEXT("cell-size"));
        if (CellSize == nullptr || !FMath::IsFinite(*CellSize) || *CellSize < 20.0f || *CellSize > 160.0f)
        { OutFailure = TEXT("Inventory grid cell-size requires a finite number in [20, 160]."); return false; }
        Out.CellSize = *CellSize;
        if (const FText* Family = In.TextProperties.Find(TEXT("font-family")))
        {
            FSlateFontInfo Font;
            if (Family->IsEmpty() || !In.FontRegistry.IsValid()
                || !In.FontRegistry->TryResolve(Family->ToString(), TEXT("Regular"), Font))
            { OutFailure = TEXT("Inventory grid font-family must name a registered regular font."); return false; }
            Out.Font = MoveTemp(Font);
        }
        const auto Color = [&In, &OutFailure](const TCHAR* Name, FLinearColor& Target) -> bool
        {
            const FCkUiCustomStyleValue* Value = In.Style.CustomProperties.Find(Name);
            if (Value == nullptr) { return true; }
            const FLinearColor Input = Value->Color;
            if (Value->Kind != ECkUiCustomStyleKind::Color || !FMath::IsFinite(Input.R)
                || !FMath::IsFinite(Input.G) || !FMath::IsFinite(Input.B) || !FMath::IsFinite(Input.A))
            { OutFailure = FString::Printf(TEXT("Inventory grid style '%s' requires a finite color."), Name); return false; }
            Target = Input;
            return true;
        };
        if (!Color(TEXT("-ck-inventory-grid-background"), Out.Visual.Background)
            || !Color(TEXT("-ck-inventory-grid-cell-background"), Out.Visual.CellBackground)
            || !Color(TEXT("-ck-inventory-grid-item-background"), Out.Visual.ItemBackground)
            || !Color(TEXT("-ck-inventory-grid-border"), Out.Visual.Border)
            || !Color(TEXT("-ck-inventory-grid-selected"), Out.Visual.Selected))
        { return false; }
        if (const auto* Size = In.Style.CustomProperties.Find(TEXT("-ck-inventory-grid-label-font-size")))
        {
            if (Size->Kind != ECkUiCustomStyleKind::Length || !FMath::IsFinite(Size->Number)
                || Size->Number < 8.0f || Size->Number > 20.0f)
            { OutFailure = TEXT("Inventory grid label font size requires a finite length in [8, 20]."); return false; }
            Out.Visual.LabelFontSize = FMath::RoundToInt(Size->Number);
        }
        TArray<FItem> Checked;
        return ReadItems(*Out.Items, Out, Checked, OutFailure);
    }

    auto LocalBounds(const FItem& InItem, float InCellSize) -> FSlateRect
    {
        const int32 Width = InItem.Rotated ? InItem.Height : InItem.Width;
        const int32 Height = InItem.Rotated ? InItem.Width : InItem.Height;
        return FSlateRect(InItem.X * InCellSize, InItem.Y * InCellSize,
            (InItem.X + Width) * InCellSize, (InItem.Y + Height) * InCellSize);
    }

    auto AtlasUv(const FItem& InItem, const FConfiguration& InConfig) -> FBox2D
    {
        const FVector2D AtlasCell(1.0 / InConfig.AtlasColumns, 1.0 / InConfig.AtlasRows);
        const FVector2D Origin((InItem.Icon % InConfig.AtlasColumns) * AtlasCell.X,
            (InItem.Icon / InConfig.AtlasColumns) * AtlasCell.Y);
        return FBox2D(Origin + FVector2D(InItem.IconLeft * AtlasCell.X, InItem.IconTop * AtlasCell.Y),
            Origin + FVector2D(InItem.IconRight * AtlasCell.X, InItem.IconBottom * AtlasCell.Y));
    }
}

class FCkUiInventoryGridState final : public ICkUiRetainedWidget, public TSharedFromThis<FCkUiInventoryGridState>
{
public:
    explicit FCkUiInventoryGridState(ck_ui_inventory_grid::FConfiguration InConfig) : Config(MoveTemp(InConfig)) {}

    void Initialize(const FString& InId)
    {
        SAssignNew(Widget, SCkUiInventoryGrid, AsShared()).Tag(FName(*InId));
        Refresh();
    }
    virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
    virtual void ReleaseTransientInteraction() override
    { ++Generation; if (Widget.IsValid()) { Widget->CancelTransient(); } }
    virtual void ReleaseOwnerInteraction() override
    { Active = false; ReleaseTransientInteraction(); if (Widget.IsValid()) { Widget->Freeze(); } }
    auto IsLive() const -> bool
    { return Active && Config.OwnerView.IsValid() && Config.Domain.IsValid() && Config.CanDispatch.Get(false); }
    auto HasCurrentLayout() const -> bool
    { return IsLive() && Config.Items.IsValid() && Config.Items->GetRevision() == AcceptedRevision; }
    auto IsOwner(uint32 InUser) const -> bool
    { return Config.UserIndex == INDEX_NONE || Config.UserIndex == static_cast<int32>(InUser); }
    auto Refresh() -> void
    {
        if (!Active || !Config.Items.IsValid() || Config.Items->GetRevision() == SeenRevision) { return; }
        SeenRevision = Config.Items->GetRevision();
        TArray<ck_ui_inventory_grid::FItem> Candidate;
        FString Failure;
        if (ck_ui_inventory_grid::ReadItems(*Config.Items, Config, Candidate, Failure))
        {
            Items = MoveTemp(Candidate);
            AcceptedRevision = SeenRevision;
        }
        ++Generation;
        if (Widget.IsValid()) { Widget->Invalidate(EInvalidateWidgetReason::Paint); }
    }
    auto FindItem(const FString& InKey) const -> const ck_ui_inventory_grid::FItem*
    { return Items.FindByPredicate([&InKey](const auto& Item) { return Item.Key == InKey; }); }
    auto CanPlace(const FString& InKey, int32 InX, int32 InY, int32 InWidth, int32 InHeight,
        bool InRotated, bool InSameCollection) const -> bool
    {
        if (!HasCurrentLayout()) { return false; }
        const int32 Width = InRotated ? InHeight : InWidth;
        const int32 Height = InRotated ? InWidth : InHeight;
        if (Width <= 0 || Height <= 0 || Width > Config.Columns || Height > Config.Rows
            || InX < 0 || InY < 0 || InX > Config.Columns - Width || InY > Config.Rows - Height)
        { return false; }
        for (const auto& Item : Items)
        {
            if (InSameCollection && Item.Key == InKey) { continue; }
            const auto B = ck_ui_inventory_grid::LocalBounds(Item, 1.0f);
            if (InX < B.Right && InX + Width > B.Left && InY < B.Bottom && InY + Height > B.Top)
            { return false; }
        }
        return true;
    }
    auto Propose(const FString& InKey, int32 InX, int32 InY, bool InRotated) -> bool
    {
        if (!HasCurrentLayout()) { return false; }
        const auto* Item = FindItem(InKey);
        if (Item == nullptr || !Item->Enabled
            || !CanPlace(InKey, InX, InY, Item->Width, Item->Height, InRotated, true)) { return false; }
        const FString Proposal = FString::Printf(TEXT("%s|%d|%d|%d"), *InKey, InX, InY, InRotated ? 1 : 0);
        const auto Callback = Config.Move;
        Callback.ExecuteIfBound(Proposal);
        return true;
    }
    virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArgs, FString& OutFailure) const
        -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    ck_ui_inventory_grid::FConfiguration Config;
    TArray<ck_ui_inventory_grid::FItem> Items;
    TSharedPtr<SCkUiInventoryGrid> Widget;
    int64 SeenRevision = INDEX_NONE;
    int64 AcceptedRevision = INDEX_NONE;
    uint64 Generation = 1;
    bool Active = true;
};

namespace ck_ui_inventory_grid
{
    class FPrepared final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FPrepared(TSharedRef<FCkUiInventoryGridState> InOwner, FConfiguration InConfig,
            TArray<FItem> InItems)
            : Owner(MoveTemp(InOwner)), Config(MoveTemp(InConfig)), Items(MoveTemp(InItems)) {}
        virtual void Commit() noexcept override
        {
            if (!Owner->Active) { return; }
            Owner->Config = MoveTemp(Config);
            Owner->Items = MoveTemp(Items);
            Owner->SeenRevision = Owner->Config.Items->GetRevision();
            Owner->AcceptedRevision = Owner->SeenRevision;
            ++Owner->Generation;
            Owner->Widget->CancelTransient();
            Owner->Widget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
        }
    private:
        TSharedRef<FCkUiInventoryGridState> Owner;
        FConfiguration Config;
        TArray<FItem> Items;
    };
}

auto FCkUiInventoryGridState::PrepareReload(const FCkUiCustomWidgetArguments& InArgs, FString& OutFailure) const
    -> TUniquePtr<ICkUiPreparedWidgetUpdate>
{
    ck_ui_inventory_grid::FConfiguration Next;
    if (!Active || !ck_ui_inventory_grid::MakeConfiguration(InArgs, Next, OutFailure)) { return {}; }
    TArray<ck_ui_inventory_grid::FItem> NextItems;
    if (!ck_ui_inventory_grid::ReadItems(*Next.Items, Next, NextItems, OutFailure)) { return {}; }
    return MakeUnique<ck_ui_inventory_grid::FPrepared>(
        ConstCastSharedRef<FCkUiInventoryGridState>(AsShared()), MoveTemp(Next), MoveTemp(NextItems));
}

void SCkUiInventoryGrid::Construct(const FArguments&, TWeakPtr<FCkUiInventoryGridState> InOwner)
{ Owner = MoveTemp(InOwner); SetVisibility(EVisibility::Visible); }

void SCkUiInventoryGrid::Freeze()
{ Frozen = true; CancelTransient(); Invalidate(EInvalidateWidgetReason::Paint); }
void SCkUiInventoryGrid::CancelTransient()
{ HoverDrag.Reset(); HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE); PressedKey.Reset(); PressedGeneration = 0; PressedUser = INDEX_NONE; }

auto SCkUiInventoryGrid::GetColumns() const -> int32
{ const auto State = Owner.Pin(); return State.IsValid() ? State->Config.Columns : 0; }
auto SCkUiInventoryGrid::GetRows() const -> int32
{ const auto State = Owner.Pin(); return State.IsValid() ? State->Config.Rows : 0; }
auto SCkUiInventoryGrid::GetCellSize() const -> float
{ const auto State = Owner.Pin(); return State.IsValid() ? State->Config.CellSize : 0.0f; }
auto SCkUiInventoryGrid::GetItemCount() const -> int32
{ const auto State = Owner.Pin(); if (!State.IsValid()) { return 0; } State->Refresh(); return State->Items.Num(); }
auto SCkUiInventoryGrid::GetAcceptedRevision() const -> int64
{ const auto State = Owner.Pin(); if (!State.IsValid()) { return INDEX_NONE; } State->Refresh(); return State->AcceptedRevision; }
auto SCkUiInventoryGrid::GetItemBounds(const FString& InKey) const -> TOptional<FSlateRect>
{
    const auto State = Owner.Pin();
    if (!State.IsValid()) { return {}; }
    State->Refresh();
    const auto* Item = State->FindItem(InKey);
    return Item != nullptr ? TOptional<FSlateRect>(ck_ui_inventory_grid::LocalBounds(*Item, State->Config.CellSize)) : TOptional<FSlateRect>{};
}
auto SCkUiInventoryGrid::CanPlace(const FString& InKey, int32 InX, int32 InY, bool InRotated) const -> bool
{
    const auto State = Owner.Pin();
    if (!State.IsValid() || Frozen) { return false; }
    State->Refresh();
    const auto* Item = State->FindItem(InKey);
    return Item != nullptr && Item->Enabled
        && State->CanPlace(InKey, InX, InY, Item->Width, Item->Height, InRotated, true);
}
auto SCkUiInventoryGrid::TryMove(const FString& InKey, int32 InX, int32 InY, bool InRotated) -> bool
{ const auto State = Owner.Pin(); if (!State.IsValid() || Frozen) { return false; } State->Refresh(); return State->Propose(InKey, InX, InY, InRotated); }
auto SCkUiInventoryGrid::ComputeDesiredSize(float) const -> FVector2D
{ return FVector2D(GetColumns() * GetCellSize(), GetRows() * GetCellSize()); }

auto SCkUiInventoryGrid::HitKey(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> FString
{
    const auto State = Owner.Pin();
    if (!State.IsValid()) { return {}; }
    State->Refresh();
    const FVector2D Local = InGeometry.AbsoluteToLocal(InScreenPosition);
    for (const auto& Item : State->Items)
    {
        const auto Bounds = ck_ui_inventory_grid::LocalBounds(Item, State->Config.CellSize);
        if (Local.X >= Bounds.Left && Local.X < Bounds.Right && Local.Y >= Bounds.Top && Local.Y < Bounds.Bottom)
        { return Item.Key; }
    }
    return {};
}

auto SCkUiInventoryGrid::ProposalCell(const FGeometry& InGeometry, const FVector2D& InScreenPosition,
    const FCkUiInventoryGridDragOperation& InDrag) const -> FIntPoint
{
    const float Cell = GetCellSize();
    if (Cell <= 0.0f) { return FIntPoint(INDEX_NONE, INDEX_NONE); }
    const FVector2D Local = InGeometry.AbsoluteToLocal(InScreenPosition);
    const int32 Width = InDrag.Rotated ? InDrag.Height : InDrag.Width;
    const int32 Height = InDrag.Rotated ? InDrag.Width : InDrag.Height;
    return FIntPoint(FMath::FloorToInt(Local.X / Cell - Width * 0.5f),
        FMath::FloorToInt(Local.Y / Cell - Height * 0.5f));
}

auto SCkUiInventoryGrid::ValidDrag(const TSharedPtr<FCkUiInventoryGridDragOperation>& InDrag) const -> bool
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || !State->HasCurrentLayout() || !InDrag.IsValid() || InDrag->Consumed
        || !State->Config.Move.IsBound() || State->Config.Domain.Pin() != InDrag->Domain.Pin()) { return false; }
    const auto Source = InDrag->Source.Pin();
    if (!Source.IsValid() || Source->Generation != InDrag->Generation || !Source->HasCurrentLayout()) { return false; }
    const auto* Item = Source->FindItem(InDrag->Key);
    if (Item == nullptr || !Item->Enabled || Item->Width != InDrag->Width || Item->Height != InDrag->Height)
    { return false; }
    for (const auto& Revision : InDrag->Revisions)
    {
        const auto Collection = Revision.Key.Pin();
        if (!Collection.IsValid() || Collection->GetRevision() != Revision.Value) { return false; }
    }
    return true;
}

auto SCkUiInventoryGrid::OnPaint(const FPaintArgs&, const FGeometry& InGeometry,
    const FSlateRect&, FSlateWindowElementList& OutDrawElements,
    int32 InLayer, const FWidgetStyle& InWidgetStyle, bool) const -> int32
{
    const auto State = Owner.Pin();
    if (!State.IsValid()) { return InLayer; }
    State->Refresh();
    const float Cell = State->Config.CellSize;
    const auto& Visual = State->Config.Visual;
    const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    const auto Box = [&OutDrawElements, &InGeometry, White, Tint](int32 Layer,
        float X, float Y, float Width, float Height, FLinearColor Color)
    {
        if (Width <= 0.0f || Height <= 0.0f) { return; }
        FSlateDrawElement::MakeBox(OutDrawElements, Layer,
            InGeometry.ToPaintGeometry(FVector2f(Width, Height), FSlateLayoutTransform(FVector2f(X, Y))),
            White, ESlateDrawEffect::None, Color * Tint);
    };
    Box(InLayer, 0.0f, 0.0f, State->Config.Columns * Cell, State->Config.Rows * Cell,
        Visual.Background);
    for (int32 Y = 0; Y < State->Config.Rows; ++Y)
    {
        for (int32 X = 0; X < State->Config.Columns; ++X)
        {
            const float Left = X * Cell + 1.0f;
            const float Top = Y * Cell + 1.0f;
            Box(InLayer + 1, Left, Top, Cell - 2.0f, Cell - 2.0f,
                (X + Y) % 2 == 0 ? Visual.CellBackground
                    : Visual.CellBackground * FLinearColor(0.89f, 0.89f, 0.89f, 1.0f));
        }
    }
    FSlateFontInfo LabelFont = State->Config.Font;
    LabelFont.Size = Visual.LabelFontSize;
    FSlateFontInfo QuantityFont = State->Config.Font;
    QuantityFont.Size = FMath::Max(8, Visual.LabelFontSize - 1);
    const FSlateBrush* Atlas = Frozen ? nullptr : State->Config.Image.Get(nullptr);
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    for (const auto& Item : State->Items)
    {
        const FSlateRect Bounds = ck_ui_inventory_grid::LocalBounds(Item, Cell);
        const float Width = Bounds.Right - Bounds.Left;
        const float Height = Bounds.Bottom - Bounds.Top;
        const FLinearColor Edge = !Item.Enabled ? Visual.Border * FLinearColor(0.66f, 0.66f, 0.66f)
            : Item.Selected ? Visual.Selected : Visual.Border;
        Box(InLayer + 2, Bounds.Left + 2.0f, Bounds.Top + 2.0f, Width - 4.0f, Height - 4.0f, Edge);
        Box(InLayer + 3, Bounds.Left + 4.0f, Bounds.Top + 4.0f, Width - 8.0f, Height - 8.0f,
            Item.Enabled ? Visual.ItemBackground
                : Visual.ItemBackground * FLinearColor(0.55f, 0.55f, 0.55f));
        const float ArtLeft = Bounds.Left + 5.0f;
        const float ArtTop = Bounds.Top + 5.0f;
        const float ArtWidth = FMath::Max(0.0f, Width - 10.0f);
        const float ArtHeight = FMath::Max(0.0f, Height - 10.0f);
        if (Atlas != nullptr && Item.Icon >= 0 && Item.AtlasBrush.IsValid()
            && FMath::IsFinite(Atlas->ImageSize.X) && FMath::IsFinite(Atlas->ImageSize.Y)
            && Atlas->ImageSize.X > 0.0f && Atlas->ImageSize.Y > 0.0f && ArtWidth > 0.0f && ArtHeight > 0.0f)
        {
            *Item.AtlasBrush = *Atlas;
            const FBox2D UV = ck_ui_inventory_grid::AtlasUv(Item, State->Config);
            Item.AtlasBrush->SetUVRegion(UV);
            const float SourceWidth = static_cast<float>(Atlas->ImageSize.X * (UV.Max.X - UV.Min.X));
            const float SourceHeight = static_cast<float>(Atlas->ImageSize.Y * (UV.Max.Y - UV.Min.Y));
            if (FMath::IsFinite(SourceWidth) && FMath::IsFinite(SourceHeight)
                && SourceWidth > 0.01f && SourceHeight > 0.01f)
            {
                const float AvailableWidth = Item.Rotated ? ArtHeight : ArtWidth;
                const float AvailableHeight = Item.Rotated ? ArtWidth : ArtHeight;
                const float Scale = FMath::Min(AvailableWidth / SourceWidth, AvailableHeight / SourceHeight);
                const float DrawWidth = SourceWidth * Scale;
                const float DrawHeight = SourceHeight * Scale;
                const FVector2f ArtCenter(ArtLeft + ArtWidth * 0.5f, ArtTop + ArtHeight * 0.5f);
                const FVector2f DrawPosition = ArtCenter - FVector2f(DrawWidth, DrawHeight) * 0.5f;
                const FPaintGeometry ArtGeometry = Item.Rotated
                    ? InGeometry.ToPaintGeometry(FVector2f(DrawWidth, DrawHeight),
                        FSlateLayoutTransform(DrawPosition),
                        FSlateRenderTransform(FQuat2D(PI * 0.5f), FVector2f::ZeroVector),
                        FVector2f(0.5f, 0.5f))
                    : InGeometry.ToPaintGeometry(FVector2f(DrawWidth, DrawHeight),
                        FSlateLayoutTransform(DrawPosition));
                FSlateDrawElement::MakeBox(OutDrawElements, InLayer + 4,
                    ArtGeometry, Item.AtlasBrush.Get(), ESlateDrawEffect::None,
                    (Item.Enabled ? FLinearColor::White : FLinearColor(0.48f, 0.51f, 0.52f)) * Tint);
            }
        }
        FString Name = Item.Name;
        if (FontMeasure->Measure(Name, LabelFont).X > Width - 12.0f)
        {
            while (!Name.IsEmpty() && FontMeasure->Measure(Name + TEXT("…"), LabelFont).X > Width - 12.0f)
            { Name.LeftChopInline(1); }
            Name += TEXT("…");
        }
        Box(InLayer + 5, Bounds.Left + 4.0f, Bounds.Top + 4.0f,
            Width - 8.0f, 15.0f, FLinearColor(0.025f, 0.035f, 0.04f, 0.78f));
        FSlateDrawElement::MakeText(OutDrawElements, InLayer + 6,
            InGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2f(Bounds.Left + 6.0f, Bounds.Top + 5.0f))),
            Name, LabelFont, ESlateDrawEffect::None,
            (Item.Enabled ? FLinearColor(0.89f, 0.91f, 0.88f) : FLinearColor(0.54f, 0.57f, 0.56f)) * Tint);
        if (Item.Quantity > 0)
        {
            const FString Count = FString::FromInt(Item.Quantity);
            const float CountWidth = static_cast<float>(FontMeasure->Measure(Count, QuantityFont).X);
            Box(InLayer + 5, Bounds.Right - CountWidth - 13.0f, Bounds.Bottom - 19.0f,
                CountWidth + 9.0f, 15.0f, FLinearColor(0.025f, 0.035f, 0.04f, 0.82f));
            FSlateDrawElement::MakeText(OutDrawElements, InLayer + 6,
                InGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2f(Bounds.Right - CountWidth - 8.0f,
                    Bounds.Bottom - 18.0f))), Count, QuantityFont, ESlateDrawEffect::None,
                FLinearColor(0.96f, 0.85f, 0.60f) * Tint);
        }
    }
    const auto Drag = HoverDrag.Pin();
    if (Drag.IsValid())
    {
        const bool Valid = ValidDrag(Drag) && State->CanPlace(Drag->Key, HoverCell.X, HoverCell.Y,
            Drag->Width, Drag->Height, Drag->Rotated, State == Drag->Source.Pin());
        const float Width = (Drag->Rotated ? Drag->Height : Drag->Width) * Cell;
        const float Height = (Drag->Rotated ? Drag->Width : Drag->Height) * Cell;
        Box(InLayer + 6, HoverCell.X * Cell + 2.0f, HoverCell.Y * Cell + 2.0f,
            Width - 4.0f, Height - 4.0f,
            Valid ? FLinearColor(0.20f, 0.82f, 0.39f, 0.52f) : FLinearColor(0.90f, 0.20f, 0.20f, 0.52f));
    }
    return InLayer + 7;
}

auto SCkUiInventoryGrid::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{
    const auto State = Owner.Pin();
    if (State.IsValid()) { State->Refresh(); }
    if (Frozen || !State.IsValid() || !State->IsOwner(InEvent.GetUserIndex())
        || !State->HasCurrentLayout() || InEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }
    PressedKey = HitKey(InGeometry, InEvent.GetScreenSpacePosition());
    if (PressedKey.IsEmpty()) { return FReply::Unhandled(); }
    const auto* Item = State->FindItem(PressedKey);
    if (Item == nullptr || !Item->Enabled) { PressedKey.Reset(); return FReply::Unhandled(); }
    PressedUser = static_cast<int32>(InEvent.GetUserIndex());
    PressedPointer = InEvent.GetPointerIndex();
    PressedGeneration = State->Generation;
    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse)
        .DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}
auto SCkUiInventoryGrid::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || PressedKey.IsEmpty()
        || PressedGeneration != State->Generation || InEvent.GetEffectingButton() != EKeys::LeftMouseButton
        || PressedUser != static_cast<int32>(InEvent.GetUserIndex())
        || PressedPointer != InEvent.GetPointerIndex() || !State->HasCurrentLayout())
    { return FReply::Unhandled(); }
    const FString Key = PressedKey;
    PressedKey.Reset();
    PressedGeneration = 0;
    if (HitKey(InGeometry, InEvent.GetScreenSpacePosition()) == Key)
    {
        const auto Callback = State->Config.Select;
        Callback.ExecuteIfBound(Key);
    }
    return FReply::Handled();
}
auto SCkUiInventoryGrid::OnDragDetected(const FGeometry&, const FPointerEvent&) -> FReply
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || !State->HasCurrentLayout() || PressedKey.IsEmpty()
        || PressedGeneration != State->Generation)
    { return FReply::Unhandled(); }
    const auto* Item = State->FindItem(PressedKey);
    if (Item == nullptr || !Item->Enabled) { return FReply::Unhandled(); }
    auto Drag = MakeShared<FCkUiInventoryGridDragOperation>();
    Drag->Source = State;
    Drag->Domain = State->Config.Domain;
    Drag->Generation = State->Generation;
    Drag->Key = Item->Key;
    Drag->Name = Item->Name;
    Drag->Font = State->Config.Font;
    Drag->Font.Size = 12;
    Drag->Width = Item->Width;
    Drag->Height = Item->Height;
    Drag->Rotated = Item->Rotated;
    const FSlateBrush* Atlas = State->Config.Image.Get(nullptr);
    if (Atlas != nullptr && Item->Icon >= 0 && FMath::IsFinite(Atlas->ImageSize.X)
        && FMath::IsFinite(Atlas->ImageSize.Y) && Atlas->ImageSize.X > 0.0f && Atlas->ImageSize.Y > 0.0f)
    {
        Drag->Brush = MakeShared<FSlateBrush>(*Atlas);
        const FBox2D UV = ck_ui_inventory_grid::AtlasUv(*Item, State->Config);
        Drag->Brush->SetUVRegion(UV);
        Drag->Brush->ImageSize = FVector2D(Atlas->ImageSize.X * (UV.Max.X - UV.Min.X),
            Atlas->ImageSize.Y * (UV.Max.Y - UV.Min.Y));
        if (!FMath::IsFinite(Drag->Brush->ImageSize.X) || !FMath::IsFinite(Drag->Brush->ImageSize.Y)
            || Drag->Brush->ImageSize.X <= 0.01f || Drag->Brush->ImageSize.Y <= 0.01f)
        { Drag->Brush.Reset(); }
    }
    for (const auto& WeakCollection : State->Config.InteractionCollections)
    {
        const auto Collection = WeakCollection.Pin();
        if (!Collection.IsValid()) { return FReply::Unhandled(); }
        Drag->Revisions.Add(TPair<TWeakPtr<FCkUiCollection>, int64>(WeakCollection, Collection->GetRevision()));
    }
    Drag->Revisions.Add(TPair<TWeakPtr<FCkUiCollection>, int64>(State->Config.Items,
        State->Config.Items->GetRevision()));
    Drag->Construct();
    PressedKey.Reset();
    PressedGeneration = 0;
    return FReply::Handled().BeginDragDrop(Drag);
}
void SCkUiInventoryGrid::OnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InEvent)
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || !State->IsOwner(InEvent.GetUserIndex()))
    {
        HoverDrag.Reset();
        HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE);
        Invalidate(EInvalidateWidgetReason::Paint);
        return;
    }
    const auto Drag = InEvent.GetOperationAs<FCkUiInventoryGridDragOperation>();
    if (!ValidDrag(Drag))
    {
        HoverDrag.Reset();
        HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE);
        Invalidate(EInvalidateWidgetReason::Paint);
        return;
    }
    HoverDrag = Drag;
    HoverCell = ProposalCell(InGeometry, InEvent.GetScreenSpacePosition(), *Drag);
    if (FSlateApplication::IsInitialized())
    { FSlateApplication::Get().SetUserFocus(InEvent.GetUserIndex(), SharedThis(this), EFocusCause::SetDirectly); }
    Invalidate(EInvalidateWidgetReason::Paint);
}
auto SCkUiInventoryGrid::OnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InEvent) -> FReply
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || !State->IsOwner(InEvent.GetUserIndex()))
    {
        HoverDrag.Reset();
        HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Unhandled();
    }
    const auto Drag = InEvent.GetOperationAs<FCkUiInventoryGridDragOperation>();
    if (!ValidDrag(Drag))
    {
        HoverDrag.Reset();
        HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Unhandled();
    }
    HoverDrag = Drag;
    HoverCell = ProposalCell(InGeometry, InEvent.GetScreenSpacePosition(), *Drag);
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}
void SCkUiInventoryGrid::OnDragLeave(const FDragDropEvent&)
{ HoverDrag.Reset(); HoverCell = FIntPoint(INDEX_NONE, INDEX_NONE); Invalidate(EInvalidateWidgetReason::Paint); }
auto SCkUiInventoryGrid::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InEvent) -> FReply
{
    const auto State = Owner.Pin();
    const auto Drag = InEvent.GetOperationAs<FCkUiInventoryGridDragOperation>();
    HoverDrag.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
    if (!State.IsValid() || !State->IsOwner(InEvent.GetUserIndex()) || !ValidDrag(Drag))
    { return FReply::Unhandled(); }
    const FIntPoint Cell = ProposalCell(InGeometry, InEvent.GetScreenSpacePosition(), *Drag);
    if (!State->CanPlace(Drag->Key, Cell.X, Cell.Y, Drag->Width, Drag->Height, Drag->Rotated,
        State == Drag->Source.Pin())) { return FReply::Unhandled(); }
    const FString Proposal = FString::Printf(TEXT("%s|%d|%d|%d"), *Drag->Key,
        Cell.X, Cell.Y, Drag->Rotated ? 1 : 0);
    const auto Callback = State->Config.Move;
    Drag->Consumed = true;
    Callback.ExecuteIfBound(Proposal);
    return FReply::Handled();
}
auto SCkUiInventoryGrid::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InEvent) -> FReply
{
    const auto State = Owner.Pin();
    if (Frozen || !State.IsValid() || !State->IsOwner(InEvent.GetUserIndex())
        || InEvent.GetKey() != EKeys::R) { return FReply::Unhandled(); }
    if (InEvent.IsRepeat()) { return FReply::Handled(); }
    const auto Drag = HoverDrag.Pin();
    if (ValidDrag(Drag))
    {
        Drag->Rotated = !Drag->Rotated;
        if (FSlateApplication::IsInitialized())
        { HoverCell = ProposalCell(InGeometry, FSlateApplication::Get().GetCursorPos(), *Drag); }
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }
    State->Refresh();
    const FString Selected = State->Config.SelectedKey.Get(FText::GetEmpty()).ToString();
    const auto* Item = State->FindItem(Selected);
    if (Item != nullptr && Item->Enabled && State->CanPlace(Item->Key, Item->X, Item->Y,
        Item->Width, Item->Height, !Item->Rotated, true))
    { return TryMove(Item->Key, Item->X, Item->Y, !Item->Rotated) ? FReply::Handled() : FReply::Unhandled(); }
    return FReply::Unhandled();
}

auto FCkUiInventoryGridDragOperation::GetDefaultDecorator() const -> TSharedPtr<SWidget>
{
    if (!Brush.IsValid())
    { return SNew(STextBlock).Font(Font).Text(FText::FromString(Name + TEXT("  [R] rotate"))); }
    const float SourceWidth = static_cast<float>(Brush->ImageSize.X);
    const float SourceHeight = static_cast<float>(Brush->ImageSize.Y);
    const float Scale = 48.0f / FMath::Max(SourceWidth, SourceHeight);
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SBox).WidthOverride(SourceWidth * Scale).HeightOverride(SourceHeight * Scale)
            [SNew(SImage).Image(Brush.Get())]]
        + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.0f, 0.0f))
        [SNew(STextBlock).Font(Font).Text(FText::FromString(Name + TEXT("  [R] rotate")))];
}

auto RegisterInventoryGridUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("inventory-grid");
    Registration.Schema.Properties = {
        {TEXT("items"), ECkUiCustomPropertyKind::CollectionBinding, true},
        {TEXT("image"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("columns"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("rows"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("cell-size"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("atlas-columns"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("atlas-rows"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("select"), ECkUiCustomPropertyKind::StringChanged, true},
        {TEXT("move"), ECkUiCustomPropertyKind::StringChanged, true},
        {TEXT("selected-key"), ECkUiCustomPropertyKind::TextBinding, false},
        {TEXT("font-family"), ECkUiCustomPropertyKind::Text, false},
    };
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-inventory-grid-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-inventory-grid-cell-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-inventory-grid-item-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-inventory-grid-border"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-inventory-grid-selected"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-inventory-grid-label-font-size"), ECkUiCustomStyleKind::Length},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArgs, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_inventory_grid::FConfiguration Config;
        if (!ck_ui_inventory_grid::MakeConfiguration(InArgs, Config, OutFailure)) { return {}; }
        const auto State = MakeShared<FCkUiInventoryGridState>(MoveTemp(Config));
        State->Initialize(InArgs.Id);
        return State;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
