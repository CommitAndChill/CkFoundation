#include "CkSlateLayout/CkUiRadialSelector.h"

#include "CkSlateLayout/CkUiCollection.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

namespace ck_ui_radial
{
    struct FItem
    {
        FString Key;
        FString Actor;
        FString Glyph;
        float Angle = 0.0f;
        float Cooldown = 0.0f;
        bool Enabled = false;
        bool Queued = false;
    };

    struct FVisual
    {
        FLinearColor Ring = FLinearColor(0.10f, 0.63f, 0.78f, 0.80f);
        FLinearColor Selected = FLinearColor(0.40f, 0.90f, 1.0f);
        FLinearColor Queued = FLinearColor(1.0f, 0.63f, 0.20f);
        FLinearColor Disabled = FLinearColor(0.92f, 0.20f, 0.17f);
        float TileSize = 42.0f;
        bool SectorMode = false;
    };

    struct FConfiguration
    {
        TSharedPtr<FCkUiCollection> Items;
        TAttribute<FString> Selection;
        FCkUiOnStringChanged Select;
        FCkUiOnStringChanged Activate;
        TAttribute<bool> CanDispatch;
        TSharedPtr<SWidget> Content;
        FVisual Visual;
        int32 UserIndex = INDEX_NONE;
    };

    auto FiniteColor(const FLinearColor& InColor) -> bool
    {
        return FMath::IsFinite(InColor.R) && FMath::IsFinite(InColor.G)
            && FMath::IsFinite(InColor.B) && FMath::IsFinite(InColor.A);
    }

    auto Field(const FCkUiRecord& InRecord, const TCHAR* InName, ECkUiFieldKind InKind) -> const FCkUiFieldValue*
    {
        const FCkUiFieldValue* Value = InRecord.FindField(InName);
        return Value != nullptr && Value->Kind == InKind ? Value : nullptr;
    }

    auto ReadItems(const FCkUiCollection& InCollection, TArray<FItem>& OutItems, FString& OutFailure) -> bool
    {
        const auto& Schema = InCollection.GetSchema();
        const auto Has = [&Schema](const TCHAR* Name, ECkUiFieldKind Kind)
        {
            return Schema.ContainsByPredicate([Name, Kind](const FCkUiFieldSchema& FieldSchema)
            { return FieldSchema.Name == Name && FieldSchema.Kind == Kind; });
        };
        if (!Has(TEXT("actor"), ECkUiFieldKind::Text) || !Has(TEXT("glyph"), ECkUiFieldKind::Text)
            || !Has(TEXT("angle"), ECkUiFieldKind::Number) || !Has(TEXT("cooldown"), ECkUiFieldKind::Number)
            || !Has(TEXT("enabled"), ECkUiFieldKind::Bool) || !Has(TEXT("queued"), ECkUiFieldKind::Bool))
        {
            OutFailure = TEXT("Radial selector items require actor/glyph text, angle/cooldown numbers, and enabled/queued booleans.");
            return false;
        }
        const auto& Records = InCollection.GetRecords();
        if (Records.IsEmpty() || Records.Num() > 24)
        {
            OutFailure = TEXT("Radial selector requires between 1 and 24 items.");
            return false;
        }
        TArray<float> Angles;
        for (const TSharedPtr<const FCkUiRecord>& Record : Records)
        {
            if (!Record.IsValid() || Record->GetKey().IsEmpty())
            {
                OutFailure = TEXT("Radial selector item requires a stable key.");
                return false;
            }
            const FCkUiFieldValue* Actor = Field(*Record, TEXT("actor"), ECkUiFieldKind::Text);
            const FCkUiFieldValue* Glyph = Field(*Record, TEXT("glyph"), ECkUiFieldKind::Text);
            const FCkUiFieldValue* Angle = Field(*Record, TEXT("angle"), ECkUiFieldKind::Number);
            const FCkUiFieldValue* Cooldown = Field(*Record, TEXT("cooldown"), ECkUiFieldKind::Number);
            const FCkUiFieldValue* Enabled = Field(*Record, TEXT("enabled"), ECkUiFieldKind::Bool);
            const FCkUiFieldValue* Queued = Field(*Record, TEXT("queued"), ECkUiFieldKind::Bool);
            if (Actor == nullptr || Glyph == nullptr || Angle == nullptr || Cooldown == nullptr
                || Enabled == nullptr || Queued == nullptr || Actor->Text.IsEmpty() || Glyph->Text.IsEmpty()
                || !FMath::IsFinite(Angle->Number) || !FMath::IsFinite(Cooldown->Number)
                || Cooldown->Number < 0.0f || Cooldown->Number > 1.0f)
            {
                OutFailure = TEXT("Radial selector item has a missing, mismatched, or nonfinite field.");
                return false;
            }
            const float Wrapped = FMath::Fmod(FMath::Fmod(Angle->Number, 360.0f) + 360.0f, 360.0f);
            for (const float Existing : Angles)
            {
                const float Distance = FMath::Abs(Wrapped - Existing);
                if (FMath::Min(Distance, 360.0f - Distance) < 15.0f)
                {
                    OutFailure = TEXT("Radial selector item angles must be separated by at least 15 degrees.");
                    return false;
                }
            }
            Angles.Add(Wrapped);
            OutItems.Add({Record->GetKey(), Actor->Text.ToString(), Glyph->Text.ToString(),
                Angle->Number, Cooldown->Number, Enabled->Bool, Queued->Bool});
        }
        return true;
    }

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& InArguments, FConfiguration& Out, FString& OutFailure) -> bool
    {
        const TSharedPtr<FCkUiCollection>* Items = InArguments.Collections.Find(TEXT("items"));
        if (InArguments.Id.IsEmpty() || Items == nullptr || !Items->IsValid())
        {
            OutFailure = TEXT("Radial selector requires an id and items binding.");
            return false;
        }
        TArray<FItem> CheckedItems;
        if (!ReadItems(**Items, CheckedItems, OutFailure))
        { return false; }
        Out.Items = *Items;
        Out.Selection = InArguments.StringBindings.FindRef(TEXT("selection"));
        Out.Select = InArguments.StringChanged.FindRef(TEXT("select"));
        Out.Activate = InArguments.StringChanged.FindRef(TEXT("activate"));
        Out.CanDispatch = InArguments.CanDispatchEvents;
        Out.Content = InArguments.Slots.FindRef(TEXT("content"));
        Out.UserIndex = InArguments.SlateUserIndex;
        if (InArguments.Slots.Contains(TEXT("content")) && !Out.Content.IsValid())
        {
            OutFailure = TEXT("Radial selector content slot is invalid.");
            return false;
        }
        const auto Color = [&InArguments, &OutFailure](const TCHAR* Name, FLinearColor& Target) -> bool
        {
            const FCkUiCustomStyleValue* Value = InArguments.Style.CustomProperties.Find(Name);
            if (Value == nullptr)
            { return true; }
            if (Value->Kind != ECkUiCustomStyleKind::Color || !FiniteColor(Value->Color))
            {
                OutFailure = FString::Printf(TEXT("Radial selector style '%s' requires a finite color."), Name);
                return false;
            }
            Target = Value->Color;
            return true;
        };
        if (!Color(TEXT("-ck-radial-ring"), Out.Visual.Ring)
            || !Color(TEXT("-ck-radial-selected"), Out.Visual.Selected)
            || !Color(TEXT("-ck-radial-queued"), Out.Visual.Queued)
            || !Color(TEXT("-ck-radial-disabled"), Out.Visual.Disabled))
        { return false; }
        if (const FCkUiCustomStyleValue* Size = InArguments.Style.CustomProperties.Find(TEXT("-ck-radial-tile-size")))
        {
            if (Size->Kind != ECkUiCustomStyleKind::Length || !FMath::IsFinite(Size->Number)
                || Size->Number < 20.0f || Size->Number > 128.0f)
            {
                OutFailure = TEXT("Radial selector tile size must be a finite length in [20, 128].");
                return false;
            }
            Out.Visual.TileSize = Size->Number;
        }
        if (const FCkUiCustomStyleValue* Mode = InArguments.Style.CustomProperties.Find(TEXT("-ck-radial-sector-mode")))
        {
            if (Mode->Kind != ECkUiCustomStyleKind::Number || !FMath::IsFinite(Mode->Number)
                || (Mode->Number != 0.0f && Mode->Number != 1.0f))
            {
                OutFailure = TEXT("Radial selector sector mode must be 0 or 1.");
                return false;
            }
            Out.Visual.SectorMode = Mode->Number == 1.0f;
        }
        return true;
    }

    auto Position(const FVector2D& InSize, float InAngle) -> FVector2f
    {
        const float Radius = 0.39f * static_cast<float>(FMath::Min(InSize.X, InSize.Y));
        const float Radians = FMath::DegreesToRadians(InAngle);
        return FVector2f(static_cast<float>(InSize.X) * 0.5f + Radius * FMath::Cos(Radians),
            static_cast<float>(InSize.Y) * 0.5f + Radius * FMath::Sin(Radians));
    }

    void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
        TArray<FVector2f> Points, FLinearColor Color, float Thickness = 1.0f)
    {
        FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points),
            ESlateDrawEffect::None, Color, true, Thickness);
    }

    void Glyph(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
        const FVector2f& Center, float Radius, const FString& Name, FLinearColor Color)
    {
        const FString Lower = Name.ToLower();
        TArray<FVector2f> Shape;
        if (Lower == TEXT("warp") || Lower == TEXT("singularity") || Lower == TEXT("nova"))
        {
            for (int32 Index = 0; Index <= 24; ++Index)
            {
                const float Angle = static_cast<float>(Index) * TWO_PI / 24.0f;
                const float R = Radius * (Lower == TEXT("singularity") ? 0.30f + 0.65f * Index / 24.0f : 0.65f);
                Shape.Add(Center + FVector2f(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R));
            }
        }
        else if (Lower == TEXT("barrier") || Lower == TEXT("shield"))
        { Shape = {Center + FVector2f(-Radius, Radius * 0.5f), Center + FVector2f(-Radius, -Radius * 0.5f),
            Center + FVector2f(0.0f, -Radius), Center + FVector2f(Radius, -Radius * 0.5f),
            Center + FVector2f(Radius, Radius * 0.5f), Center + FVector2f(0.0f, Radius),
            Center + FVector2f(-Radius, Radius * 0.5f)}; }
        else if (Lower == TEXT("ammo") || Lower == TEXT("concussive"))
        { Shape = {Center + FVector2f(-Radius, -Radius * 0.35f), Center + FVector2f(Radius * 0.2f, -Radius * 0.35f),
            Center + FVector2f(Radius, 0.0f), Center + FVector2f(Radius * 0.2f, Radius * 0.35f),
            Center + FVector2f(-Radius, Radius * 0.35f)}; }
        else if (Lower == TEXT("overload") || Lower == TEXT("charge"))
        { Shape = {Center + FVector2f(-Radius * 0.3f, -Radius), Center + FVector2f(Radius * 0.2f, -Radius * 0.2f),
            Center + FVector2f(-Radius * 0.15f, -Radius * 0.2f), Center + FVector2f(Radius * 0.3f, Radius)}; }
        else if (Lower == TEXT("incinerate"))
        { Shape = {Center + FVector2f(0.0f, -Radius), Center + FVector2f(Radius * 0.35f, -Radius * 0.2f),
            Center + FVector2f(Radius * 0.18f, Radius * 0.1f), Center + FVector2f(Radius * 0.65f, -Radius * 0.1f),
            Center + FVector2f(Radius * 0.50f, Radius * 0.65f), Center + FVector2f(0.0f, Radius),
            Center + FVector2f(-Radius * 0.55f, Radius * 0.6f), Center + FVector2f(-Radius * 0.35f, -Radius * 0.1f),
            Center + FVector2f(0.0f, -Radius)}; }
        else if (Lower == TEXT("energy-drain"))
        { Shape = {Center + FVector2f(-Radius, -Radius * 0.55f), Center + FVector2f(-Radius * 0.4f, -Radius),
            Center + FVector2f(Radius * 0.4f, -Radius), Center + FVector2f(Radius, -Radius * 0.55f),
            Center + FVector2f(Radius * 0.65f, 0.0f), Center + FVector2f(Radius, Radius * 0.55f),
            Center + FVector2f(Radius * 0.4f, Radius), Center + FVector2f(-Radius * 0.4f, Radius),
            Center + FVector2f(-Radius, Radius * 0.55f)}; }
        else if (Lower == TEXT("mine"))
        {
            for (int32 Index = 0; Index <= 16; ++Index)
            {
                const float Angle = static_cast<float>(Index) * TWO_PI / 16.0f;
                const float R = Index % 2 == 0 ? Radius : Radius * 0.48f;
                Shape.Add(Center + FVector2f(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R));
            }
        }
        else if (Lower == TEXT("adrenaline"))
        { Shape = {Center + FVector2f(-Radius, -Radius * 0.7f), Center + FVector2f(0.0f, 0.0f),
            Center + FVector2f(Radius, -Radius * 0.7f)}; }
        else if (Lower == TEXT("stasis"))
        { Shape = {Center + FVector2f(-Radius * 0.35f, -Radius), Center + FVector2f(-Radius, -Radius),
            Center + FVector2f(-Radius, Radius), Center + FVector2f(-Radius * 0.35f, Radius)}; }
        else
        { Shape = {Center + FVector2f(-Radius, 0.0f), Center + FVector2f(0.0f, -Radius),
            Center + FVector2f(Radius, 0.0f), Center + FVector2f(0.0f, Radius),
            Center + FVector2f(-Radius, 0.0f)}; }
        Line(Out, Layer, Geometry, MoveTemp(Shape), Color, 2.0f);
        if (Lower == TEXT("throw") || Lower == TEXT("concussive"))
        { Line(Out, Layer, Geometry, {Center + FVector2f(-Radius, 0.0f), Center + FVector2f(Radius, 0.0f)}, Color, 1.5f); }
        if (Lower == TEXT("incinerate"))
        { Line(Out, Layer, Geometry, {Center + FVector2f(0.0f, Radius * 0.65f),
            Center + FVector2f(-Radius * 0.17f, Radius * 0.22f), Center + FVector2f(0.0f, -Radius * 0.25f)}, Color, 1.0f); }
        if (Lower == TEXT("energy-drain"))
        { Line(Out, Layer, Geometry, {Center + FVector2f(-Radius * 0.5f, 0.0f),
            Center + FVector2f(Radius * 0.35f, 0.0f), Center + FVector2f(Radius * 0.12f, -Radius * 0.25f)}, Color, 1.5f); }
        if (Lower == TEXT("adrenaline"))
        { Line(Out, Layer, Geometry, {Center + FVector2f(-Radius, Radius * 0.1f),
            Center + FVector2f(0.0f, Radius * 0.8f), Center + FVector2f(Radius, Radius * 0.1f)}, Color, 2.0f); }
        if (Lower == TEXT("stasis"))
        { Line(Out, Layer, Geometry, {Center + FVector2f(Radius * 0.35f, -Radius),
            Center + FVector2f(Radius, -Radius), Center + FVector2f(Radius, Radius),
            Center + FVector2f(Radius * 0.35f, Radius)}, Color, 2.0f); }
    }
}

class FCkUiRadialSelectorState final : public ICkUiRetainedWidget, public TSharedFromThis<FCkUiRadialSelectorState>
{
public:
    explicit FCkUiRadialSelectorState(ck_ui_radial::FConfiguration InConfiguration)
        : _Configuration(MoveTemp(InConfiguration)) {}

    void Initialize(const FString& InId)
    {
        const TSharedRef<SWidget> Content = _Configuration.Content.IsValid()
            ? _Configuration.Content.ToSharedRef() : SNullWidget::NullWidget;
        SAssignNew(_Widget, SCkUiRadialSelector, AsShared(), Content);
        _Widget->SetTag(FName(*InId));
    }
    virtual ~FCkUiRadialSelectorState() override
    {
        _Active = false;
        if (_Widget.IsValid())
        { _Widget->Freeze(); }
    }
    virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
    virtual void ReleaseOwnerInteraction() override
    {
        _Active = false;
        if (_Widget.IsValid())
        { _Widget->Freeze(); }
    }
    virtual void ReleaseTransientInteraction() override
    {
        if (_Widget.IsValid())
        { _Widget->CancelTransient(); }
    }
    auto CanDispatch() const -> bool { return _Active && _Configuration.CanDispatch.Get(false); }
    auto IsOwner(uint32 InUser) const -> bool
    { return _Configuration.UserIndex == INDEX_NONE || _Configuration.UserIndex == static_cast<int32>(InUser); }
    auto Items() const -> TArray<ck_ui_radial::FItem>
    {
        TArray<ck_ui_radial::FItem> Result;
        FString Failure;
        if (_Active && _Configuration.Items.IsValid()
            && !ck_ui_radial::ReadItems(*_Configuration.Items, Result, Failure))
        { Result.Reset(); }
        return Result;
    }
    auto Selection() const -> FString
    { return _Active ? _Configuration.Selection.Get(FString()) : FString(); }
    auto Revision() const -> int64
    { return _Active && _Configuration.Items.IsValid() ? _Configuration.Items->GetRevision() : INDEX_NONE; }
    auto Visual() const -> const ck_ui_radial::FVisual& { return _Configuration.Visual; }
    void Select(const FString& InKey)
    {
        if (CanDispatch())
        {
            const FCkUiOnStringChanged Callback = _Configuration.Select;
            Callback.ExecuteIfBound(InKey);
        }
    }
    void Activate(const FString& InKey)
    {
        if (!CanDispatch())
        { return; }
        const auto ItemsSnapshot = Items();
        const auto* Item = ItemsSnapshot.FindByPredicate([&InKey](const auto& Candidate) { return Candidate.Key == InKey; });
        if (Item != nullptr && Item->Enabled && Item->Cooldown <= 0.0f)
        {
            const FCkUiOnStringChanged Callback = _Configuration.Activate;
            Callback.ExecuteIfBound(InKey);
        }
    }
    virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
        -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
    {
        ck_ui_radial::FConfiguration Next;
        if (!ck_ui_radial::MakeConfiguration(InArguments, Next, OutFailure))
        { return {}; }
        if (Next.Content != _Configuration.Content || Next.UserIndex != _Configuration.UserIndex)
        {
            OutFailure = TEXT("Radial selector content slot and owner cannot change for a retained id.");
            return {};
        }
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FCkUiRadialSelectorState> InOwner, ck_ui_radial::FConfiguration InNext)
                : Owner(MoveTemp(InOwner)), Next(MoveTemp(InNext)) {}
            virtual void Commit() noexcept override
            {
                Owner->_Configuration = MoveTemp(Next);
                Owner->_Widget->Invalidate(EInvalidateWidgetReason::Paint);
            }
            TSharedRef<FCkUiRadialSelectorState> Owner;
            ck_ui_radial::FConfiguration Next;
        };
        return MakeUnique<FUpdate>(ConstCastSharedRef<FCkUiRadialSelectorState>(AsShared()), MoveTemp(Next));
    }

private:
    ck_ui_radial::FConfiguration _Configuration;
    TSharedPtr<SCkUiRadialSelector> _Widget;
    bool _Active = true;
};

void SCkUiRadialSelector::Construct(const FArguments&, TWeakPtr<FCkUiRadialSelectorState> InOwner,
    const TSharedRef<SWidget>& InContent)
{
    _Owner = MoveTemp(InOwner);
    ChildSlot[SNew(SBox).Visibility(EVisibility::HitTestInvisible)[InContent]];
}

void SCkUiRadialSelector::Freeze()
{
    _Frozen = true;
    _DownKey.Reset();
    _HoverKey.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCkUiRadialSelector::CancelTransient()
{
    _DownKey.Reset();
    _HoverKey.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkUiRadialSelector::GetSelectedKey() const -> FString
{
    if (_Frozen)
    { return {}; }
    if (!_HoverKey.IsEmpty())
    { return _HoverKey; }
    const auto Owner = _Owner.Pin();
    return Owner.IsValid() ? Owner->Selection() : FString();
}

auto SCkUiRadialSelector::SupportsKeyboardFocus() const -> bool
{
    const auto Owner = _Owner.Pin();
    return !_Frozen && Owner.IsValid() && Owner->CanDispatch();
}

void SCkUiRadialSelector::Tick(const FGeometry& InGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(InGeometry, InCurrentTime, InDeltaTime);
    if (_Frozen)
    { return; }
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid())
    { return; }
    const int64 Revision = Owner->Revision();
    const FString Selection = Owner->Selection();
    if (Revision != _SeenRevision || Selection != _SeenSelection)
    {
        _SeenRevision = Revision;
        _SeenSelection = Selection;
        if (!_HoverKey.IsEmpty())
        {
            const auto Items = Owner->Items();
            if (!Items.ContainsByPredicate([this](const auto& Item) { return Item.Key == _HoverKey; }))
            { _HoverKey.Reset(); _DownKey.Reset(); }
        }
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

auto SCkUiRadialSelector::IsOwner(uint32 InUserIndex) const -> bool
{
    const auto Owner = _Owner.Pin();
    return !_Frozen && Owner.IsValid() && Owner->CanDispatch() && Owner->IsOwner(InUserIndex);
}

auto SCkUiRadialSelector::HitKey(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> FString
{
    const auto Owner = _Owner.Pin();
    if (_Frozen || !Owner.IsValid())
    { return {}; }
    const FVector2D Local = InGeometry.AbsoluteToLocal(InScreenPosition);
    const FVector2D Size = InGeometry.GetLocalSize();
    const float Radius = Owner->Visual().TileSize * 0.65f;
    float Best = Radius * Radius;
    FString Key;
    for (const auto& Item : Owner->Items())
    {
        const FVector2f Position = ck_ui_radial::Position(Size, Item.Angle);
        const float Distance = FVector2D::DistSquared(Local, FVector2D(Position.X, Position.Y));
        if (Distance < Best)
        { Best = Distance; Key = Item.Key; }
    }
    return Key;
}

void SCkUiRadialSelector::Hover(const FString& InKey)
{
    if (_Frozen || InKey == _HoverKey)
    { return; }
    _HoverKey = InKey;
    Invalidate(EInvalidateWidgetReason::Paint);
    if (!InKey.IsEmpty())
    {
        if (const auto Owner = _Owner.Pin())
        { Owner->Select(InKey); }
    }
}

void SCkUiRadialSelector::Activate(const FString& InKey)
{
    if (const auto Owner = _Owner.Pin())
    { Owner->Activate(InKey); }
}

auto SCkUiRadialSelector::OnQueryShowFocus(const EFocusCause InFocusCause) const -> TOptional<bool>
{
    // The selected glyph supplies focus feedback inside the radial composition.
    return false;
}

auto SCkUiRadialSelector::OnPaint(const FPaintArgs& InArgs, const FGeometry& InGeometry,
    const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 InLayer, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const -> int32
{
    const auto Owner = _Owner.Pin();
    if (Owner.IsValid() && !_Frozen)
    {
        const auto Items = Owner->Items();
        const FVector2D Size = InGeometry.GetLocalSize();
        const FVector2f Center(static_cast<float>(Size.X) * 0.5f, static_cast<float>(Size.Y) * 0.5f);
        const float Radius = 0.39f * static_cast<float>(FMath::Min(Size.X, Size.Y));
        const auto Visual = Owner->Visual();
        const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
        if (Visual.SectorMode)
        {
            TArray<float> Angles;
            Angles.Reserve(Items.Num());
            for (const auto& Item : Items)
            { Angles.Add(FMath::Fmod(FMath::Fmod(Item.Angle, 360.0f) + 360.0f, 360.0f)); }
            Angles.Sort();
            for (int32 Index = 0; Index < Angles.Num(); ++Index)
            {
                const float Start = Angles[Index];
                const float Next = Index + 1 < Angles.Num() ? Angles[Index + 1] : Angles[0] + 360.0f;
                const float Boundary = FMath::DegreesToRadians((Start + Next) * 0.5f);
                const FVector2f Direction(FMath::Cos(Boundary), FMath::Sin(Boundary));
                ck_ui_radial::Line(OutDrawElements, InLayer, InGeometry,
                    {Center + Direction * (Radius * 0.34f), Center + Direction * (Radius * 1.13f)},
                    Visual.Ring * Tint, 1.4f);
            }
            for (int32 Circle = 0; Circle < 2; ++Circle)
            {
                TArray<FVector2f> Outline;
                const float CircleRadius = Radius * (Circle == 0 ? 0.34f : 1.13f);
                for (int32 Step = 0; Step <= 96; ++Step)
                {
                    const float Angle = static_cast<float>(Step) * TWO_PI / 96.0f;
                    Outline.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * CircleRadius);
                }
                ck_ui_radial::Line(OutDrawElements, InLayer, InGeometry, MoveTemp(Outline),
                    Visual.Ring * Tint, Circle == 0 ? 2.0f : 2.8f);
            }
            const FString SelectedKey = GetSelectedKey();
            const auto* SelectedItem = Items.FindByPredicate([&SelectedKey](const auto& Item) { return Item.Key == SelectedKey; });
            if (SelectedItem != nullptr && Angles.Num() > 1)
            {
                float SelectedAngle = FMath::Fmod(FMath::Fmod(SelectedItem->Angle, 360.0f) + 360.0f, 360.0f);
                const int32 Index = Angles.IndexOfByKey(SelectedAngle);
                if (Index != INDEX_NONE)
                {
                    const float Previous = Index > 0 ? Angles[Index - 1] : Angles.Last() - 360.0f;
                    const float Next = Index + 1 < Angles.Num() ? Angles[Index + 1] : Angles[0] + 360.0f;
                    const float Begin = (Previous + SelectedAngle) * 0.5f;
                    const float End = (SelectedAngle + Next) * 0.5f;
                    TArray<FVector2f> Highlight;
                    for (int32 Step = 0; Step <= 18; ++Step)
                    {
                        const float Angle = FMath::DegreesToRadians(FMath::Lerp(Begin, End, static_cast<float>(Step) / 18.0f));
                        Highlight.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (Radius * 1.13f));
                    }
                    ck_ui_radial::Line(OutDrawElements, InLayer + 1, InGeometry, MoveTemp(Highlight),
                        Visual.Selected * Tint, 8.0f);
                }
            }
        }
        else for (int32 Segment = 0; Segment < 6; ++Segment)
        {
            TArray<FVector2f> Arc;
            for (int32 Step = 0; Step <= 12; ++Step)
            {
                const float Angle = static_cast<float>(Segment * 60 + Step * 4 + 8) * TWO_PI / 360.0f;
                Arc.Add(Center + FVector2f(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius));
            }
            ck_ui_radial::Line(OutDrawElements, InLayer, InGeometry, MoveTemp(Arc), Visual.Ring * Tint, 1.5f);
        }
        const FString Selected = GetSelectedKey();
        const float Tile = Visual.TileSize;
        const FSlateFontInfo GlyphFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
        const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);
        TSharedPtr<FSlateFontMeasure> FontMeasure;
        if (Visual.SectorMode)
        { FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService(); }
        for (const auto& Item : Items)
        {
            const FVector2f Position = ck_ui_radial::Position(Size, Item.Angle);
            const float Half = Tile * 0.5f;
            const FLinearColor Rim = Item.Queued ? Visual.Queued
                : Item.Key == Selected ? Visual.Selected : Visual.Ring;
            const TArray<FVector2f> Frame = {
                Position + FVector2f(-Half, -Half * 0.65f), Position + FVector2f(-Half * 0.65f, -Half),
                Position + FVector2f(Half * 0.65f, -Half), Position + FVector2f(Half, -Half * 0.65f),
                Position + FVector2f(Half, Half * 0.65f), Position + FVector2f(Half * 0.65f, Half),
                Position + FVector2f(-Half * 0.65f, Half), Position + FVector2f(-Half, Half * 0.65f),
                Position + FVector2f(-Half, -Half * 0.65f)};
            ck_ui_radial::Line(OutDrawElements, InLayer + 1, InGeometry, Frame, Rim * Tint,
                Item.Key == Selected ? 3.0f : 1.5f);
            if (Visual.SectorMode)
            {
                const FLinearColor Ink = !Item.Enabled ? Visual.Disabled
                    : Item.Queued ? Visual.Queued
                    : Item.Key == Selected ? Visual.Selected
                    : FMath::Lerp(Visual.Ring, Visual.Selected, 0.65f);
                const FVector2D GlyphSize = FontMeasure->Measure(Item.Glyph, GlyphFont);
                const FVector2f GlyphPosition = Position - FVector2f(GlyphSize) * 0.5f;
                FSlateDrawElement::MakeText(OutDrawElements, InLayer + 2,
                    InGeometry.ToPaintGeometry(FSlateLayoutTransform(GlyphPosition)), Item.Glyph,
                    GlyphFont, ESlateDrawEffect::None, Ink * Tint);
                const FVector2D LabelSize = FontMeasure->Measure(Item.Actor, LabelFont);
                const FVector2f LabelPosition(Position.X - static_cast<float>(LabelSize.X) * 0.5f,
                    Position.Y + Half + 2.0f);
                FSlateDrawElement::MakeText(OutDrawElements, InLayer + 2,
                    InGeometry.ToPaintGeometry(FSlateLayoutTransform(LabelPosition)), Item.Actor,
                    LabelFont, ESlateDrawEffect::None, Ink * Tint);
            }
            else
            {
                ck_ui_radial::Glyph(OutDrawElements, InLayer + 2, InGeometry, Position, Half * 0.52f,
                    Item.Glyph, (Item.Enabled ? Rim : Visual.Disabled) * Tint);
            }
            if (Item.Cooldown > 0.0f)
            {
                FSlateDrawElement::MakeBox(OutDrawElements, InLayer + 3,
                    InGeometry.ToPaintGeometry(FVector2f(Tile, Tile * Item.Cooldown),
                        FSlateLayoutTransform(Position + FVector2f(-Half, Half - Tile * Item.Cooldown))),
                    FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None,
                    FLinearColor(0.08f, 0.12f, 0.16f, 0.68f) * Tint);
            }
        }
    }
    return SCompoundWidget::OnPaint(InArgs, InGeometry, InCullingRect, OutDrawElements,
        InLayer + 4, InWidgetStyle, InParentEnabled);
}

auto SCkUiRadialSelector::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{
    if (!IsOwner(InEvent.GetUserIndex()))
    { return FReply::Unhandled(); }
    Hover(HitKey(InGeometry, InEvent.GetScreenSpacePosition()));
    return FReply::Handled();
}

auto SCkUiRadialSelector::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{
    if (!IsOwner(InEvent.GetUserIndex()) || !_DownKey.IsEmpty()
        || (!InEvent.IsTouchEvent() && InEvent.GetEffectingButton() != EKeys::LeftMouseButton))
    { return FReply::Unhandled(); }
    const FString Key = HitKey(InGeometry, InEvent.GetScreenSpacePosition());
    if (Key.IsEmpty())
    { return FReply::Unhandled(); }
    _DownKey = Key;
    _DownUser = static_cast<int32>(InEvent.GetUserIndex());
    _DownPointer = InEvent.GetPointerIndex();
    _DownTouch = InEvent.IsTouchEvent();
    Hover(Key);
    if (!IsOwner(InEvent.GetUserIndex()) || _DownKey != Key)
    { return FReply::Unhandled(); }
    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

auto SCkUiRadialSelector::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{
    if (_DownKey.IsEmpty() || !IsOwner(InEvent.GetUserIndex())
        || _DownUser != static_cast<int32>(InEvent.GetUserIndex())
        || _DownPointer != InEvent.GetPointerIndex() || _DownTouch != InEvent.IsTouchEvent()
        || (!InEvent.IsTouchEvent() && InEvent.GetEffectingButton() != EKeys::LeftMouseButton))
    { return FReply::Unhandled(); }
    const FString Pressed = _DownKey;
    _DownKey.Reset();
    if (HitKey(InGeometry, InEvent.GetScreenSpacePosition()) == Pressed)
    { Activate(Pressed); }
    return FReply::Handled();
}

auto SCkUiRadialSelector::OnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{ return OnMouseButtonDown(InGeometry, InEvent); }

auto SCkUiRadialSelector::OnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply
{ return OnMouseButtonUp(InGeometry, InEvent); }

void SCkUiRadialSelector::OnMouseLeave(const FPointerEvent& InEvent)
{
    _DownKey.Reset();
    Hover({});
    SCompoundWidget::OnMouseLeave(InEvent);
}

auto SCkUiRadialSelector::OnKeyDown(const FGeometry&, const FKeyEvent& InEvent) -> FReply
{
    if (!IsOwner(InEvent.GetUserIndex()))
    { return FReply::Unhandled(); }
    const FKey Key = InEvent.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
    {
        if (!InEvent.IsRepeat())
        { Activate(GetSelectedKey()); }
        return FReply::Handled();
    }
    const bool Next = Key == EKeys::Right || Key == EKeys::Down || Key == EKeys::D || Key == EKeys::S;
    const bool Previous = Key == EKeys::Left || Key == EKeys::Up || Key == EKeys::A || Key == EKeys::W;
    if (!Next && !Previous)
    { return FReply::Unhandled(); }
    const auto Owner = _Owner.Pin();
    if (!Owner.IsValid())
    { return FReply::Unhandled(); }
    auto Items = Owner->Items();
    Items.Sort([](const auto& Left, const auto& Right) { return Left.Angle < Right.Angle; });
    if (Items.IsEmpty())
    { return FReply::Handled(); }
    const FString Current = GetSelectedKey();
    const int32 Index = Items.IndexOfByPredicate([&Current](const auto& Item) { return Item.Key == Current; });
    const int32 Target = Index == INDEX_NONE ? 0
        : (Index + (Next ? 1 : Items.Num() - 1)) % Items.Num();
    Hover(Items[Target].Key);
    return FReply::Handled();
}

auto RegisterRadialUiControls(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("radial-selector");
    Registration.Schema.Properties = {
        {TEXT("items"), ECkUiCustomPropertyKind::CollectionBinding, true},
        {TEXT("selection"), ECkUiCustomPropertyKind::StringBinding, false},
        {TEXT("select"), ECkUiCustomPropertyKind::StringChanged, false},
        {TEXT("activate"), ECkUiCustomPropertyKind::StringChanged, false},
    };
    Registration.Schema.Slots = {{TEXT("content"), false}};
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-radial-ring"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-radial-selected"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-radial-queued"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-radial-disabled"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-radial-tile-size"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-radial-sector-mode"), ECkUiCustomStyleKind::Number},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_radial::FConfiguration Configuration;
        if (!ck_ui_radial::MakeConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        const TSharedRef<FCkUiRadialSelectorState> State = MakeShared<FCkUiRadialSelectorState>(MoveTemp(Configuration));
        State->Initialize(InArguments.Id);
        return State;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
