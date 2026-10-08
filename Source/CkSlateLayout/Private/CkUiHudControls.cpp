#include "CkSlateLayout/CkUiHudControls.h"
#include "CkSlateLayout/CkUiFontRegistry.h"
#include "CkSlateLayout/CkUiKeyHint.h"
#include "CkUiHudGeometry.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Layout/Clipping.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"

namespace ck_ui_hud
{
    auto DrawMesh(const FGeometry& InGeometry, FSlateWindowElementList& OutElements,
        int32 InLayer, const FSlateBrush& InBrush, const ck_ui_hud_geometry::FMesh& InMesh,
        const FLinearColor& InTint) -> void
    {
        if (InMesh.Indices.IsEmpty() || !FSlateApplication::IsInitialized())
        { return; }
        const auto Renderer = FSlateApplication::Get().GetRenderer();
        if (Renderer == nullptr)
        { return; }
        const auto Handle = Renderer->GetResourceHandle(InBrush);
        const auto* Proxy = Handle.GetResourceProxy();
        // Color brushes have no texture proxy. Slate supplies white for those batches,
        // so their vertex colors still render masks and highlights.
        TArray<FSlateVertex> Vertices;
        Vertices.Reserve(InMesh.Vertices.Num());
        for (const auto& Vertex : InMesh.Vertices)
        {
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                InGeometry.GetAccumulatedRenderTransform(), Vertex.Position,
                Proxy != nullptr ? Proxy->StartUV + Vertex.UV * Proxy->SizeUV : Vertex.UV,
                (Vertex.Color * InTint).ToFColor(true)));
        }
        TArray<SlateIndex> Indices;
        Indices.Append(InMesh.Indices);
        FSlateDrawElement::MakeCustomVerts(OutElements, InLayer, Handle, Vertices, Indices,
            nullptr, 0, 0, ESlateDrawEffect::None);
    }

    auto DrawGlassHighlight(const FGeometry& InGeometry, FSlateWindowElementList& OutElements,
        int32 InLayer, const FLinearColor& InTint) -> void
    {
        const FVector2f Size(InGeometry.GetLocalSize());
        const float Radius = FMath::Min(Size.X, Size.Y) * 0.435f;
        const FVector2f Center = Size * 0.5f + FVector2f(0.38f, -0.48f) * Radius;
        const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        for (int32 Lobe = 0; Lobe < 2; ++Lobe)
        {
            ck_ui_hud_geometry::FMesh Mesh;
            const FVector2f Extent = (Lobe == 0 ? FVector2f(0.29f, 0.20f) : FVector2f(0.075f, 0.052f)) * Radius;
            const float Alpha = Lobe == 0 ? 0.17f : 0.82f;
            Mesh.Vertices.Add({Center, FVector2f(0.5f), FLinearColor(0.84f, 0.91f, 1.0f, Alpha)});
            for (int32 Index = 0; Index <= 48; ++Index)
            {
                const float Angle = TWO_PI * Index / 48.0f;
                Mesh.Vertices.Add({Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Extent,
                    FVector2f(0.5f), FLinearColor(0.84f, 0.91f, 1.0f, 0.0f)});
                if (Index > 0)
                { Mesh.Indices.Append({0, static_cast<uint16>(Index), static_cast<uint16>(Index + 1)}); }
            }
            DrawMesh(InGeometry, OutElements, InLayer + Lobe, *White, Mesh, InTint);
        }
    }

    auto FiniteColor(const FLinearColor& InColor) -> bool
    {
        return FMath::IsFinite(InColor.R) && FMath::IsFinite(InColor.G)
            && FMath::IsFinite(InColor.B) && FMath::IsFinite(InColor.A);
    }

    auto UnitFraction(const float InValue) -> float
    {
        return FMath::IsFinite(InValue) ? FMath::Clamp(InValue, 0.0f, 1.0f) : 0.0f;
    }

    auto ReadColor(const FCkUiStyle& InStyle, const TCHAR* InName, FLinearColor& OutColor, FString& OutFailure) -> bool
    {
        const FCkUiCustomStyleValue* Value = InStyle.CustomProperties.Find(InName);
        if (Value == nullptr)
        { return true; }
        const bool Valid = Value->Kind == ECkUiCustomStyleKind::Color && FiniteColor(Value->Color);
        if (!Valid)
        {
            OutFailure = FString::Printf(TEXT("HUD style '%s' requires a finite color."), InName);
            return false;
        }
        OutColor = Value->Color;
        return true;
    }

    auto ReadNumber(const FCkUiStyle& InStyle, const TCHAR* InName, const ECkUiCustomStyleKind InKind,
        const float InMin, const float InMax, float& OutNumber, FString& OutFailure) -> bool
    {
        const FCkUiCustomStyleValue* Value = InStyle.CustomProperties.Find(InName);
        if (Value == nullptr)
        { return true; }
        const bool Valid = Value->Kind == InKind && FMath::IsFinite(Value->Number)
            && Value->Number >= InMin && Value->Number <= InMax;
        if (!Valid)
        {
            OutFailure = FString::Printf(TEXT("HUD style '%s' requires a finite value in [%g, %g]."), InName, InMin, InMax);
            return false;
        }
        OutNumber = Value->Number;
        return true;
    }

    struct FMeterConfiguration
    {
        TAttribute<float> Value;
        TAttribute<FLinearColor> Tint;
        FCkUiHudMeterVisual Visual;
    };

    auto MakeMeterConfiguration(const FCkUiCustomWidgetArguments& InArguments,
        FMeterConfiguration& Out, FString& OutFailure) -> bool
    {
        const TAttribute<float>* Value = InArguments.NumberBindings.Find(TEXT("value"));
        const bool HasValue = !InArguments.Id.IsEmpty() && Value != nullptr && Value->IsSet();
        if (!HasValue)
        {
            OutFailure = TEXT("HUD meter requires an id and value binding.");
            return false;
        }
        auto Visual = FCkUiHudMeterVisual{};
        if (!ReadColor(InArguments.Style, TEXT("-ck-hud-meter-fill"), Visual.Fill, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-hud-meter-background"), Visual.Background, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-hud-meter-trail"), Visual.Trail, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-hud-meter-border"), Visual.Border, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-border-width"), ECkUiCustomStyleKind::Length,
                0.0f, 128.0f, Visual.BorderWidth, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-radius"), ECkUiCustomStyleKind::Length,
                0.0f, 128.0f, Visual.Radius, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-trail-delay"), ECkUiCustomStyleKind::Number,
                0.0f, 5.0f, Visual.TrailDelay, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-response"), ECkUiCustomStyleKind::Number,
                0.0f, 5.0f, Visual.Response, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-low-threshold"), ECkUiCustomStyleKind::Number,
                0.0f, 1.0f, Visual.LowThreshold, OutFailure))
        { return false; }
        float Segments = static_cast<float>(Visual.Segments);
        float Radial = Visual.Radial ? 1.0f : 0.0f;
        if (!ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-segments"), ECkUiCustomStyleKind::Number,
                1.0f, 64.0f, Segments, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-meter-radial"), ECkUiCustomStyleKind::Number,
                0.0f, 1.0f, Radial, OutFailure))
        { return false; }
        if (Segments != FMath::FloorToFloat(Segments) || (Radial != 0.0f && Radial != 1.0f))
        {
            OutFailure = TEXT("HUD meter segments must be an integer and radial must be 0 or 1.");
            return false;
        }
        Visual.Segments = static_cast<int32>(Segments);
        Visual.Radial = Radial == 1.0f;
        Out.Value = *Value;
        Out.Tint = InArguments.ColorBindings.FindRef(TEXT("tint"));
        Out.Visual = Visual;
        return true;
    }

    class FMeter final : public ICkUiRetainedWidget, public TSharedFromThis<FMeter>
    {
    public:
        explicit FMeter(FMeterConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        virtual ~FMeter() override
        {
            if (_Widget.IsValid())
            { _Widget->Freeze(); }
        }
        void Initialize(const FString& InId)
        {
            SAssignNew(_Widget, SCkUiHudMeter).Tag(FName(*InId));
            _Widget->Configure(_Configuration.Value, _Configuration.Tint, _Configuration.Visual);
        }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
        virtual void ReleaseOwnerInteraction() override { _Widget->Freeze(); }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FMeterConfiguration Candidate;
            if (!MakeMeterConfiguration(InArguments, Candidate, OutFailure))
            { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FMeter>(AsShared()), MoveTemp(Candidate));
        }

    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FMeter> InOwner, FMeterConfiguration InConfiguration)
                : _Owner(MoveTemp(InOwner)), _Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                _Owner->_Configuration = MoveTemp(_Configuration);
                _Owner->_Widget->Configure(_Owner->_Configuration.Value, _Owner->_Configuration.Tint,
                    _Owner->_Configuration.Visual);
            }
        private:
            TSharedRef<FMeter> _Owner;
            FMeterConfiguration _Configuration;
        };

        TSharedPtr<SCkUiHudMeter> _Widget;
        FMeterConfiguration _Configuration;
    };

    struct FGraphicConfiguration
    {
        TAttribute<const FSlateBrush*> Image;
        TAttribute<float> Opacity;
    };

    auto MakeGraphicConfiguration(const FCkUiCustomWidgetArguments& InArguments,
        FGraphicConfiguration& Out, FString& OutFailure) -> bool
    {
        const TAttribute<const FSlateBrush*>* Image = InArguments.ImageBindings.Find(TEXT("image"));
        const bool HasImage = !InArguments.Id.IsEmpty() && Image != nullptr && Image->IsSet();
        if (!HasImage)
        {
            OutFailure = TEXT("HUD graphic requires an id and image binding.");
            return false;
        }
        Out.Image = *Image;
        Out.Opacity = InArguments.NumberBindings.FindRef(TEXT("opacity"));
        return true;
    }

    class FGraphic final : public ICkUiRetainedWidget, public TSharedFromThis<FGraphic>
    {
    public:
        explicit FGraphic(FGraphicConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        void Initialize(const FString& InId)
        {
            const TWeakPtr<FGraphic> Weak = AsShared();
            SAssignNew(_Widget, SImage).Tag(FName(*InId))
                .Image_Lambda([Weak]() -> const FSlateBrush*
                {
                    const TSharedPtr<FGraphic> Graphic = Weak.Pin();
                    return Graphic.IsValid() && !Graphic->_Frozen
                        ? Graphic->_Configuration.Image.Get(nullptr) : nullptr;
                })
                .ColorAndOpacity_Lambda([Weak]() -> FSlateColor
                {
                    const TSharedPtr<FGraphic> Graphic = Weak.Pin();
                    if (!Graphic.IsValid() || Graphic->_Frozen)
                    { return FSlateColor(FLinearColor::Transparent); }
                    const float Raw = Graphic->_Configuration.Opacity.Get(1.0f);
                    const float Opacity = FMath::IsFinite(Raw) ? FMath::Clamp(Raw, 0.0f, 1.0f) : 0.0f;
                    return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, Opacity));
                });
            _Widget->SetVisibility(EVisibility::SelfHitTestInvisible);
        }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
        virtual void ReleaseOwnerInteraction() override
        {
            _Frozen = true;
            _Widget->Invalidate(EInvalidateWidgetReason::Paint);
        }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FGraphicConfiguration Candidate;
            if (!MakeGraphicConfiguration(InArguments, Candidate, OutFailure))
            { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FGraphic>(AsShared()), MoveTemp(Candidate));
        }

    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FGraphic> InOwner, FGraphicConfiguration InConfiguration)
                : _Owner(MoveTemp(InOwner)), _Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                _Owner->_Configuration = MoveTemp(_Configuration);
                _Owner->_Widget->Invalidate(EInvalidateWidgetReason::Paint);
            }
        private:
            TSharedRef<FGraphic> _Owner;
            FGraphicConfiguration _Configuration;
        };

        TSharedPtr<SImage> _Widget;
        FGraphicConfiguration _Configuration;
        bool _Frozen = false;
    };

    struct FOrbConfiguration
    {
        TAttribute<float> Value;
        TAttribute<const FSlateBrush*> Liquid;
        TAttribute<const FSlateBrush*> Frame;
        float Response = 0.18f;
        float MotionSpeed = 0.35f;
    };

    auto MakeOrbConfiguration(const FCkUiCustomWidgetArguments& InArguments,
        FOrbConfiguration& Out, FString& OutFailure) -> bool
    {
        const auto* Value = InArguments.NumberBindings.Find(TEXT("value"));
        const auto* Liquid = InArguments.ImageBindings.Find(TEXT("liquid"));
        const auto* Frame = InArguments.ImageBindings.Find(TEXT("frame"));
        if (InArguments.Id.IsEmpty() || Value == nullptr || !Value->IsSet()
            || Liquid == nullptr || !Liquid->IsSet() || Frame == nullptr || !Frame->IsSet())
        {
            OutFailure = TEXT("HUD orb requires an id, value, liquid image, and frame image bindings.");
            return false;
        }
        if (!ReadNumber(InArguments.Style, TEXT("-ck-hud-orb-response"), ECkUiCustomStyleKind::Number,
            0.0f, 5.0f, Out.Response, OutFailure)
            || !ReadNumber(InArguments.Style, TEXT("-ck-hud-orb-motion-speed"), ECkUiCustomStyleKind::Number,
                0.0f, 4.0f, Out.MotionSpeed, OutFailure))
        { return false; }
        Out.Value = *Value;
        Out.Liquid = *Liquid;
        Out.Frame = *Frame;
        return true;
    }

    class FOrb final : public ICkUiRetainedWidget, public TSharedFromThis<FOrb>
    {
    public:
        explicit FOrb(FOrbConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        virtual ~FOrb() override { if (_Widget.IsValid()) { _Widget->Freeze(); } }
        void Initialize(const FString& InId)
        {
            SAssignNew(_Widget, SCkUiHudOrb).Tag(FName(*InId));
            _Widget->Configure(_Configuration.Value, _Configuration.Liquid, _Configuration.Frame,
                _Configuration.Response, _Configuration.MotionSpeed);
        }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
        virtual void ReleaseOwnerInteraction() override { _Widget->Freeze(); }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FOrbConfiguration Candidate;
            if (!MakeOrbConfiguration(InArguments, Candidate, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FOrb>(AsShared()), MoveTemp(Candidate));
        }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FOrb> InOwner, FOrbConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                Owner->_Configuration = MoveTemp(Configuration);
                Owner->_Widget->Configure(Owner->_Configuration.Value, Owner->_Configuration.Liquid,
                    Owner->_Configuration.Frame, Owner->_Configuration.Response, Owner->_Configuration.MotionSpeed);
            }
            TSharedRef<FOrb> Owner;
            FOrbConfiguration Configuration;
        };
        TSharedPtr<SCkUiHudOrb> _Widget;
        FOrbConfiguration _Configuration;
    };

    struct FAbilityConfiguration
    {
        TAttribute<float> Cooldown;
        TAttribute<const FSlateBrush*> Image;
        TAttribute<float> Remaining;
        FSlateFontInfo Font;
    };

    auto MakeAbilityConfiguration(const FCkUiCustomWidgetArguments& InArguments,
        FAbilityConfiguration& Out, FString& OutFailure) -> bool
    {
        const auto* Cooldown = InArguments.NumberBindings.Find(TEXT("cooldown"));
        const auto* Image = InArguments.ImageBindings.Find(TEXT("image"));
        if (InArguments.Id.IsEmpty() || Cooldown == nullptr || !Cooldown->IsSet()
            || Image == nullptr || !Image->IsSet())
        {
            OutFailure = TEXT("HUD ability requires an id, cooldown, and image bindings.");
            return false;
        }
        Out.Cooldown = *Cooldown;
        Out.Image = *Image;
        Out.Font = InArguments.BaseFont;
        Out.Font.Size = 20;
        if (const auto* Remaining = InArguments.NumberBindings.Find(TEXT("remaining")))
        { Out.Remaining = *Remaining; }
        if (const auto* Family = InArguments.TextProperties.Find(TEXT("font-family")))
        {
            if (!InArguments.FontRegistry.IsValid())
            { OutFailure = TEXT("HUD ability font-family requires an admitted font registry."); return false; }
            if (!InArguments.FontRegistry->TryResolve(Family->ToString(), TEXT("Regular"), Out.Font))
            { OutFailure = TEXT("HUD ability font-family must name an admitted regular face."); return false; }
        }
        float FontSize = 20.0f;
        if (!ReadNumber(InArguments.Style, TEXT("-ck-hud-ability-font-size"), ECkUiCustomStyleKind::Length,
            8.0f, 48.0f, FontSize, OutFailure))
        { return false; }
        Out.Font.Size = FMath::RoundToInt(FontSize);
        return true;
    }

    class FAbility final : public ICkUiRetainedWidget, public TSharedFromThis<FAbility>
    {
    public:
        explicit FAbility(FAbilityConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        virtual ~FAbility() override { if (_Widget.IsValid()) { _Widget->Freeze(); } }
        void Initialize(const FString& InId)
        {
            SAssignNew(_Widget, SCkUiHudAbility).Tag(FName(*InId));
            _Widget->Configure(_Configuration.Cooldown, _Configuration.Image, _Configuration.Remaining, _Configuration.Font);
        }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
        virtual void ReleaseOwnerInteraction() override { _Widget->Freeze(); }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FAbilityConfiguration Candidate;
            if (!MakeAbilityConfiguration(InArguments, Candidate, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FAbility>(AsShared()), MoveTemp(Candidate));
        }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FAbility> InOwner, FAbilityConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                Owner->_Configuration = MoveTemp(Configuration);
                Owner->_Widget->Configure(Owner->_Configuration.Cooldown, Owner->_Configuration.Image,
                    Owner->_Configuration.Remaining, Owner->_Configuration.Font);
            }
            TSharedRef<FAbility> Owner;
            FAbilityConfiguration Configuration;
        };
        TSharedPtr<SCkUiHudAbility> _Widget;
        FAbilityConfiguration _Configuration;
    };
}

void SCkUiHudMeter::Construct(const FArguments&)
{
    SetVisibility(EVisibility::SelfHitTestInvisible);
    SetCanTick(true);
    _Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, _Visual.Radius);
}

void SCkUiHudMeter::Configure(TAttribute<float> InValue, TAttribute<FLinearColor> InTint,
    const FCkUiHudMeterVisual& InVisual)
{
    _Value = MoveTemp(InValue);
    _Tint = MoveTemp(InTint);
    _Visual = InVisual;
    if (_Brush.IsValid())
    { *_Brush = FSlateRoundedBoxBrush(FLinearColor::White, _Visual.Radius); }
    else
    { _Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, _Visual.Radius); }
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

auto SCkUiHudMeter::ReadTarget() const -> float
{
    return ck_ui_hud::UnitFraction(_Value.Get(0.0f));
}

auto SCkUiHudMeter::ReadTint() const -> FLinearColor
{
    const FLinearColor Color = _Tint.Get(_Visual.Fill);
    return ck_ui_hud::FiniteColor(Color) ? Color : _Visual.Fill;
}

void SCkUiHudMeter::AdvancePresentation(const float InDeltaTime)
{
    if (_Frozen)
    { return; }
    const float DeltaTime = FMath::IsFinite(InDeltaTime) ? FMath::Clamp(InDeltaTime, 0.0f, 1.0f) : 0.0f;
    const float NextTarget = ReadTarget();
    if (!_Initialized)
    {
        _Target = NextTarget;
        _Displayed = NextTarget;
        _Trail = NextTarget;
        _Initialized = true;
    }
    if (NextTarget < _Target)
    { _DelayRemaining = _Visual.TrailDelay; }
    _Target = NextTarget;
    const float Alpha = _Visual.Response <= 0.0f ? 1.0f
        : 1.0f - FMath::Exp(-DeltaTime / _Visual.Response);
    _Displayed = FMath::Lerp(_Displayed, _Target, Alpha);
    if (_Target >= _Trail)
    { _Trail = _Displayed; _DelayRemaining = 0.0f; }
    else if (_DelayRemaining > 0.0f)
    { _DelayRemaining = FMath::Max(0.0f, _DelayRemaining - DeltaTime); }
    else
    { _Trail = FMath::Max(_Displayed, FMath::Lerp(_Trail, _Displayed, Alpha)); }
    _PulseTime += DeltaTime;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCkUiHudMeter::Freeze()
{
    _Frozen = true;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCkUiHudMeter::Tick(const FGeometry& InGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SLeafWidget::Tick(InGeometry, InCurrentTime, InDeltaTime);
    AdvancePresentation(InDeltaTime);
}

auto SCkUiHudMeter::ComputeDesiredSize(float) const -> FVector2D
{
    return _Visual.Radial ? FVector2D(48.0f, 48.0f) : FVector2D(120.0f, 12.0f);
}

auto SCkUiHudMeter::DrawArc(const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements,
    const int32 InLayer, const float InFraction, const float InThickness, const FLinearColor& InColor) const -> void
{
    const float Fraction = ck_ui_hud::UnitFraction(InFraction);
    if (Fraction <= 0.0f)
    { return; }
    const FVector2D Size = InGeometry.GetLocalSize();
    const FVector2D Center = Size * 0.5f;
    const float Radius = FMath::Max(0.0f, FMath::Min(Size.X, Size.Y) * 0.5f - InThickness * 0.5f - _Visual.BorderWidth);
    const int32 Steps = FMath::Max(2, FMath::CeilToInt(64.0f * Fraction));
    TArray<FVector2f> Points;
    Points.Reserve(Steps + 1);
    for (int32 Index = 0; Index <= Steps; ++Index)
    {
        const float Angle = -HALF_PI + 2.0f * PI * Fraction * static_cast<float>(Index) / static_cast<float>(Steps);
        Points.Add(FVector2f(Center.X + Radius * FMath::Cos(Angle), Center.Y + Radius * FMath::Sin(Angle)));
    }
    FSlateDrawElement::MakeLines(OutDrawElements, InLayer, InGeometry.ToPaintGeometry(),
        MoveTemp(Points), ESlateDrawEffect::None, InColor, true, InThickness);
}

auto SCkUiHudMeter::OnPaint(const FPaintArgs&, const FGeometry& InGeometry,
    const FSlateRect&, FSlateWindowElementList& OutDrawElements, const int32 InLayer,
    const FWidgetStyle& InWidgetStyle, bool) const -> int32
{
    if (_Frozen || !_Brush.IsValid())
    { return InLayer; }
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    const bool Low = _Target > 0.0f && _Target <= _Visual.LowThreshold;
    FLinearColor Fill = ReadTint();
    if (Low)
    { Fill.A *= 0.70f + 0.30f * (0.5f + 0.5f * FMath::Sin(_PulseTime * 8.0f)); }
    if (_Visual.Radial)
    {
        const float Thickness = FMath::Max(2.0f, FMath::Min(InGeometry.GetLocalSize().X,
            InGeometry.GetLocalSize().Y) * 0.13f);
        DrawArc(InGeometry, OutDrawElements, InLayer, 1.0f, Thickness + _Visual.BorderWidth * 2.0f, _Visual.Border * Tint);
        DrawArc(InGeometry, OutDrawElements, InLayer + 1, 1.0f, Thickness, _Visual.Background * Tint);
        DrawArc(InGeometry, OutDrawElements, InLayer + 2, _Trail, Thickness, _Visual.Trail * Tint);
        DrawArc(InGeometry, OutDrawElements, InLayer + 3, _Displayed, Thickness, Fill * Tint);
        return InLayer + 3;
    }
    const FVector2D Size = InGeometry.GetLocalSize();
    const float Border = FMath::Min(_Visual.BorderWidth, FMath::Min(Size.X, Size.Y) * 0.5f);
    const float Gap = _Visual.Segments > 1 ? FMath::Min(2.0f, Size.X / (4.0f * _Visual.Segments)) : 0.0f;
    const float SegmentWidth = Size.X / static_cast<float>(_Visual.Segments);
    auto Box = [&](const int32 Layer, const float X, const float Y, const float Width, const float Height, const FLinearColor& Color)
    {
        if (Width <= 0.0f || Height <= 0.0f)
        { return; }
        FSlateDrawElement::MakeBox(OutDrawElements, Layer,
            InGeometry.ToPaintGeometry(FVector2f(Width, Height), FSlateLayoutTransform(FVector2f(X, Y))),
            _Brush.Get(), ESlateDrawEffect::None, Color * Tint);
    };
    for (int32 Index = 0; Index < _Visual.Segments; ++Index)
    {
        const float X = Index * SegmentWidth;
        const float Width = SegmentWidth - Gap;
        Box(InLayer, X, 0.0f, Width, Size.Y, _Visual.Border);
        Box(InLayer + 1, X + Border, Border, Width - 2.0f * Border, Size.Y - 2.0f * Border, _Visual.Background);
        const float FillWidth = FMath::Max(0.0f, Width - 2.0f * Border);
        const float TrailPart = FMath::Clamp(_Trail * _Visual.Segments - Index, 0.0f, 1.0f);
        const float DisplayPart = FMath::Clamp(_Displayed * _Visual.Segments - Index, 0.0f, 1.0f);
        Box(InLayer + 2, X + Border, Border, FillWidth * TrailPart, Size.Y - 2.0f * Border, _Visual.Trail);
        Box(InLayer + 3, X + Border, Border, FillWidth * DisplayPart, Size.Y - 2.0f * Border, Fill);
    }
    return InLayer + 3;
}

void SCkUiHudOrb::Construct(const FArguments&)
{
    SetVisibility(EVisibility::SelfHitTestInvisible);
    SetCanTick(true);
}

void SCkUiHudOrb::Configure(TAttribute<float> InValue, TAttribute<const FSlateBrush*> InLiquid,
    TAttribute<const FSlateBrush*> InFrame, const float InResponse, const float InMotionSpeed)
{
    _Value = MoveTemp(InValue);
    _Liquid = MoveTemp(InLiquid);
    _Frame = MoveTemp(InFrame);
    _Response = FMath::IsFinite(InResponse) ? FMath::Clamp(InResponse, 0.0f, 5.0f) : 0.0f;
    _MotionSpeed = FMath::IsFinite(InMotionSpeed) ? FMath::Clamp(InMotionSpeed, 0.0f, 4.0f) : 0.0f;
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SCkUiHudOrb::Freeze()
{
    _Frozen = true;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCkUiHudOrb::Tick(const FGeometry& InGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SLeafWidget::Tick(InGeometry, InCurrentTime, InDeltaTime);
    AdvancePresentation(InDeltaTime);
}

void SCkUiHudOrb::AdvancePresentation(const float InDeltaTime)
{
    if (_Frozen)
    { return; }
    const float Delta = FMath::IsFinite(InDeltaTime) ? FMath::Clamp(InDeltaTime, 0.0f, 1.0f) : 0.0f;
    _Phase = FMath::Fmod(_Phase + Delta * _MotionSpeed, TWO_PI);
    const float Target = ck_ui_hud::UnitFraction(_Value.Get(0.0f));
    if (!_Initialized) { _Displayed = Target; _Initialized = true; }
    else
    {
        const float Alpha = _Response <= 0.0f ? 1.0f : 1.0f - FMath::Exp(-Delta / _Response);
        _Displayed = FMath::Lerp(_Displayed, Target, Alpha);
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkUiHudOrb::ComputeDesiredSize(float) const -> FVector2D { return FVector2D(160.0f, 160.0f); }

auto SCkUiHudOrb::OnPaint(const FPaintArgs&, const FGeometry& InGeometry, const FSlateRect&,
    FSlateWindowElementList& OutDrawElements, const int32 InLayer,
    const FWidgetStyle& InWidgetStyle, bool) const -> int32
{
    if (_Frozen) { return InLayer; }
    const FSlateBrush* Liquid = _Liquid.Get(nullptr);
    const FSlateBrush* Frame = _Frame.Get(nullptr);
    if (Liquid == nullptr || Frame == nullptr) { return InLayer; }
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    const FVector2D Size = InGeometry.GetLocalSize();
    ck_ui_hud_geometry::FMesh Mesh;
    if (!ck_ui_hud_geometry::TryBuildOrb(FVector2f(Size), _Phase, 0.055f, Mesh))
    { return InLayer; }
    ck_ui_hud::DrawMesh(InGeometry, OutDrawElements, InLayer, *Liquid, Mesh, Tint);
    if (_Displayed > 0.0f)
    {
        const float Radius = FMath::Min(Size.X, Size.Y) * 0.435f;
        const float FillTop = Size.Y * 0.5f + Radius * (1.0f - 2.0f * _Displayed);
        const auto FillGeometry = InGeometry.MakeChild(FVector2f(Size.X, Size.Y - FillTop),
            FSlateLayoutTransform(FVector2f(0.0f, FillTop)));
        OutDrawElements.PushClip(FSlateClippingZone(FillGeometry));
        if (ck_ui_hud_geometry::TryBuildOrb(FVector2f(Size), _Phase, 1.0f, Mesh))
        { ck_ui_hud::DrawMesh(InGeometry, OutDrawElements, InLayer + 1, *Liquid, Mesh, Tint); }
        OutDrawElements.PopClip();
    }
    ck_ui_hud::DrawGlassHighlight(InGeometry, OutDrawElements, InLayer + 2, Tint);
    FSlateDrawElement::MakeBox(OutDrawElements, InLayer + 4, InGeometry.ToPaintGeometry(),
        Frame, ESlateDrawEffect::None, Tint);
    return InLayer + 4;
}

void SCkUiHudAbility::Construct(const FArguments&)
{
    SetVisibility(EVisibility::SelfHitTestInvisible);
    SetCanTick(true);
}

void SCkUiHudAbility::Configure(TAttribute<float> InCooldown, TAttribute<const FSlateBrush*> InImage,
    TAttribute<float> InRemaining, const FSlateFontInfo& InFont)
{
    _Cooldown = MoveTemp(InCooldown);
    _Image = MoveTemp(InImage);
    _Remaining = MoveTemp(InRemaining);
    _Font = InFont;
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SCkUiHudAbility::Freeze()
{
    _Frozen = true;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCkUiHudAbility::Tick(const FGeometry& InGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SLeafWidget::Tick(InGeometry, InCurrentTime, InDeltaTime);
    if (!_Frozen) { Invalidate(EInvalidateWidgetReason::Paint); }
}

auto SCkUiHudAbility::ComputeDesiredSize(float) const -> FVector2D { return FVector2D(64.0f, 64.0f); }

auto SCkUiHudAbility::GetRemainingSeconds() const -> int32
{
    if (_Frozen || ck_ui_hud::UnitFraction(_Cooldown.Get(0.0f)) <= 0.0f)
    { return 0; }
    const float Seconds = _Remaining.Get(0.0f);
    return FMath::IsFinite(Seconds) ? FMath::CeilToInt(FMath::Clamp(Seconds, 0.0f, 86400.0f)) : 0;
}

auto SCkUiHudAbility::OnPaint(const FPaintArgs&, const FGeometry& InGeometry, const FSlateRect&,
    FSlateWindowElementList& OutDrawElements, const int32 InLayer,
    const FWidgetStyle& InWidgetStyle, bool) const -> int32
{
    if (_Frozen) { return InLayer; }
    const FSlateBrush* Image = _Image.Get(nullptr);
    if (Image == nullptr) { return InLayer; }
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    const float Cooldown = ck_ui_hud::UnitFraction(_Cooldown.Get(0.0f));
    const FVector2D Size = InGeometry.GetLocalSize();
    FSlateDrawElement::MakeBox(OutDrawElements, InLayer, InGeometry.ToPaintGeometry(),
        Image, ESlateDrawEffect::None, Tint);
    if (Cooldown > 0.0f)
    {
        ck_ui_hud_geometry::FMesh Mask;
        if (ck_ui_hud_geometry::TryBuildCooldown(FVector2f(Size), Cooldown,
            FLinearColor(0.012f, 0.015f, 0.018f, 0.82f), Mask))
        {
            ck_ui_hud::DrawMesh(InGeometry, OutDrawElements, InLayer + 1,
                *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), Mask, Tint);
        }
        const int32 Seconds = GetRemainingSeconds();
        if (Seconds > 0 && FSlateApplication::IsInitialized())
        {
            const auto Renderer = FSlateApplication::Get().GetRenderer();
            if (Renderer != nullptr)
            {
                const FString Label = FString::FromInt(Seconds);
                const auto Measured = Renderer->GetFontMeasureService()->Measure(Label, _Font);
                const FVector2f Origin = (FVector2f(Size) - FVector2f(Measured)) * 0.5f;
                FSlateDrawElement::MakeText(OutDrawElements, InLayer + 2,
                    InGeometry.ToPaintGeometry(FSlateLayoutTransform(Origin + FVector2f(1.0f, 1.0f))),
                    Label, _Font, ESlateDrawEffect::None, FLinearColor::Black * Tint);
                FSlateDrawElement::MakeText(OutDrawElements, InLayer + 3,
                    InGeometry.ToPaintGeometry(FSlateLayoutTransform(Origin)), Label, _Font,
                    ESlateDrawEffect::None, FLinearColor(0.83f, 0.79f, 0.68f) * Tint);
            }
        }
    }
    return InLayer + 3;
}

auto FCkUiHudControls::Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiWidgetRegistry Candidate = InRegistry;
    auto Meter = FCkUiCustomWidgetRegistration{};
    Meter.Schema.Tag = TEXT("hud-meter");
    Meter.Schema.Properties = {
        {TEXT("value"), ECkUiCustomPropertyKind::NumberBinding, true},
        {TEXT("tint"), ECkUiCustomPropertyKind::ColorBinding, false}
    };
    Meter.Schema.StyleProperties = {
        {TEXT("-ck-hud-meter-fill"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-hud-meter-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-hud-meter-trail"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-hud-meter-border"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-hud-meter-border-width"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-hud-meter-radius"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-hud-meter-segments"), ECkUiCustomStyleKind::Number},
        {TEXT("-ck-hud-meter-radial"), ECkUiCustomStyleKind::Number},
        {TEXT("-ck-hud-meter-trail-delay"), ECkUiCustomStyleKind::Number},
        {TEXT("-ck-hud-meter-response"), ECkUiCustomStyleKind::Number},
        {TEXT("-ck-hud-meter-low-threshold"), ECkUiCustomStyleKind::Number}
    };
    Meter.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_hud::FMeterConfiguration Configuration;
        if (!ck_ui_hud::MakeMeterConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        const TSharedRef<ck_ui_hud::FMeter> Control = MakeShared<ck_ui_hud::FMeter>(MoveTemp(Configuration));
        Control->Initialize(InArguments.Id);
        return Control;
    };
    FCkUiLoadResult Result = Candidate.Register(MoveTemp(Meter));
    if (!Result.Succeeded)
    { return Result; }
    auto Graphic = FCkUiCustomWidgetRegistration{};
    Graphic.Schema.Tag = TEXT("hud-graphic");
    Graphic.Schema.Properties = {
        {TEXT("image"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("opacity"), ECkUiCustomPropertyKind::NumberBinding, false}
    };
    Graphic.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_hud::FGraphicConfiguration Configuration;
        if (!ck_ui_hud::MakeGraphicConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        const TSharedRef<ck_ui_hud::FGraphic> Control = MakeShared<ck_ui_hud::FGraphic>(MoveTemp(Configuration));
        Control->Initialize(InArguments.Id);
        return Control;
    };
    Result = Candidate.Register(MoveTemp(Graphic));
    if (!Result.Succeeded) { return Result; }
    auto Orb = FCkUiCustomWidgetRegistration{};
    Orb.Schema.Tag = TEXT("hud-orb");
    Orb.Schema.Properties = {
        {TEXT("value"), ECkUiCustomPropertyKind::NumberBinding, true},
        {TEXT("liquid"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("frame"), ECkUiCustomPropertyKind::ImageBinding, true}
    };
    Orb.Schema.StyleProperties = {
        {TEXT("-ck-hud-orb-response"), ECkUiCustomStyleKind::Number},
        {TEXT("-ck-hud-orb-motion-speed"), ECkUiCustomStyleKind::Number}
    };
    Orb.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_hud::FOrbConfiguration Configuration;
        if (!ck_ui_hud::MakeOrbConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
        const TSharedRef<ck_ui_hud::FOrb> Control = MakeShared<ck_ui_hud::FOrb>(MoveTemp(Configuration));
        Control->Initialize(InArguments.Id);
        return Control;
    };
    Result = Candidate.Register(MoveTemp(Orb));
    if (!Result.Succeeded) { return Result; }
    auto Ability = FCkUiCustomWidgetRegistration{};
    Ability.Schema.Tag = TEXT("hud-ability");
    Ability.Schema.Properties = {
        {TEXT("cooldown"), ECkUiCustomPropertyKind::NumberBinding, true},
        {TEXT("image"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("remaining"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("font-family"), ECkUiCustomPropertyKind::Text, false}
    };
    Ability.Schema.StyleProperties = {{TEXT("-ck-hud-ability-font-size"), ECkUiCustomStyleKind::Length}};
    Ability.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_hud::FAbilityConfiguration Configuration;
        if (!ck_ui_hud::MakeAbilityConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
        const TSharedRef<ck_ui_hud::FAbility> Control = MakeShared<ck_ui_hud::FAbility>(MoveTemp(Configuration));
        Control->Initialize(InArguments.Id);
        return Control;
    };
    Result = Candidate.Register(MoveTemp(Ability));
    if (!Result.Succeeded)
    { return Result; }
    Result = RegisterKeyHintUiControl(Candidate);
    if (Result.Succeeded)
    { InRegistry = MoveTemp(Candidate); }
    return Result;
}
