#include "CkSlateLayout/CkUiAtlasImage.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"

namespace ck_ui_atlas_image
{
    struct FConfiguration
    {
        TAttribute<const FSlateBrush*> Image;
        TAttribute<float> Index;
        TAttribute<float> CropLeft;
        TAttribute<float> CropTop;
        TAttribute<float> CropRight;
        TAttribute<float> CropBottom;
        bool HasCrop = false;
        bool AspectFit = false;
        int32 Columns = 1;
        int32 Rows = 1;
    };

    auto ValidCrop(float InLeft, float InTop, float InRight, float InBottom) -> bool
    {
        return FMath::IsFinite(InLeft) && FMath::IsFinite(InTop)
            && FMath::IsFinite(InRight) && FMath::IsFinite(InBottom)
            && InLeft >= 0.0f && InTop >= 0.0f && InRight <= 1.0f && InBottom <= 1.0f
            && InLeft < InRight && InTop < InBottom;
    }

    auto Configure(const FCkUiCustomWidgetArguments& InArgs, FConfiguration& Out, FString& OutFailure) -> bool
    {
        const auto* Image = InArgs.ImageBindings.Find(TEXT("image"));
        const auto* Index = InArgs.NumberBindings.Find(TEXT("index"));
        const auto* Columns = InArgs.NumberProperties.Find(TEXT("columns"));
        const auto* Rows = InArgs.NumberProperties.Find(TEXT("rows"));
        if (InArgs.Id.IsEmpty() || Image == nullptr || !Image->IsSet() || Index == nullptr || !Index->IsSet()
            || Columns == nullptr || Rows == nullptr || !FMath::IsFinite(*Columns) || !FMath::IsFinite(*Rows)
            || *Columns < 1.0f || *Columns > 64.0f || *Rows < 1.0f || *Rows > 64.0f
            || *Columns != FMath::FloorToFloat(*Columns) || *Rows != FMath::FloorToFloat(*Rows))
        {
            OutFailure = TEXT("Atlas image requires image/index bindings and integer columns/rows in [1,64].");
            return false;
        }
        Out.Image = *Image;
        Out.Index = *Index;
        Out.Columns = static_cast<int32>(*Columns);
        Out.Rows = static_cast<int32>(*Rows);
        Out.AspectFit = InArgs.BoolProperties.FindRef(TEXT("aspect-fit"));
        const bool HasLeft = InArgs.NumberBindings.Contains(TEXT("crop-left"));
        const bool HasTop = InArgs.NumberBindings.Contains(TEXT("crop-top"));
        const bool HasRight = InArgs.NumberBindings.Contains(TEXT("crop-right"));
        const bool HasBottom = InArgs.NumberBindings.Contains(TEXT("crop-bottom"));
        Out.HasCrop = HasLeft && HasTop && HasRight && HasBottom;
        if ((HasLeft || HasTop || HasRight || HasBottom) && !Out.HasCrop)
        { OutFailure = TEXT("Atlas crop requires left, top, right, and bottom bindings together."); return false; }
        if (Out.HasCrop)
        {
            Out.CropLeft = InArgs.NumberBindings.FindRef(TEXT("crop-left"));
            Out.CropTop = InArgs.NumberBindings.FindRef(TEXT("crop-top"));
            Out.CropRight = InArgs.NumberBindings.FindRef(TEXT("crop-right"));
            Out.CropBottom = InArgs.NumberBindings.FindRef(TEXT("crop-bottom"));
            if (!Out.CropLeft.IsSet() || !Out.CropTop.IsSet() || !Out.CropRight.IsSet() || !Out.CropBottom.IsSet()
                || !ValidCrop(Out.CropLeft.Get(), Out.CropTop.Get(), Out.CropRight.Get(), Out.CropBottom.Get()))
            { OutFailure = TEXT("Atlas crop bindings require finite ordered values within [0,1]."); return false; }
        }
        return true;
    }

    class FAtlas final : public ICkUiRetainedWidget, public TSharedFromThis<FAtlas>
    {
    public:
        explicit FAtlas(FConfiguration InConfig) : _Config(MoveTemp(InConfig)) {}
        void Initialize(const FString& InId)
        {
            const TWeakPtr<FAtlas> Weak = AsShared();
            SAssignNew(_Widget, SImage).Tag(FName(*InId)).Image_Lambda([Weak]() -> const FSlateBrush*
            {
                const auto Owner = Weak.Pin();
                return Owner.IsValid() ? Owner->ResolveBrush() : nullptr;
            }).DesiredSizeOverride_Lambda([Weak]() -> TOptional<FVector2D>
            {
                const auto Owner = Weak.Pin();
                const FSlateBrush* Brush = Owner.IsValid() ? Owner->ResolveBrush() : nullptr;
                return Brush != nullptr ? TOptional<FVector2D>(Brush->ImageSize) : TOptional<FVector2D>{};
            });
            _Widget->SetVisibility(EVisibility::HitTestInvisible);
            if (_Config.AspectFit)
            {
                SAssignNew(_ScaleBox, SScaleBox)
                    .Tag(FName(*InId))
                    .Stretch(EStretch::ScaleToFit)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [_Widget.ToSharedRef()];
                _ScaleBox->SetVisibility(EVisibility::HitTestInvisible);
                _Root = _ScaleBox;
            }
            else { _Root = _Widget; }
        }
        auto GetWidget() const -> TSharedRef<SWidget> override { return _Root.ToSharedRef(); }
        void ReleaseOwnerInteraction() override
        {
            _Active = false;
            _Config.Image = {};
            _Config.Index = {};
            _Config.CropLeft = {};
            _Config.CropTop = {};
            _Config.CropRight = {};
            _Config.CropBottom = {};
            _Widget->Invalidate(EInvalidateWidgetReason::Paint);
        }
        auto PrepareReload(const FCkUiCustomWidgetArguments& InArgs, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FConfiguration Candidate;
            if (!_Active || !Configure(InArgs, Candidate, OutFailure)) { return {}; }
            if (Candidate.AspectFit != _Config.AspectFit)
            { OutFailure = TEXT("Atlas image aspect-fit mode cannot change for a retained id."); return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FAtlas>(AsShared()), MoveTemp(Candidate));
        }
    private:
        auto ResolveBrush() -> const FSlateBrush*
        {
            if (!_Active) { return nullptr; }
            const float Raw = _Config.Index.Get(-1.0f);
            if (!FMath::IsFinite(Raw) || Raw < 0.0f || Raw >= _Config.Columns * _Config.Rows
                || Raw != FMath::FloorToFloat(Raw)) { return nullptr; }
            const auto* Source = _Config.Image.Get(nullptr);
            if (Source == nullptr || !FMath::IsFinite(Source->ImageSize.X) || !FMath::IsFinite(Source->ImageSize.Y)
                || Source->ImageSize.X <= 0.0f || Source->ImageSize.Y <= 0.0f) { return nullptr; }
            const float Left = _Config.HasCrop ? _Config.CropLeft.Get(-1.0f) : 0.0f;
            const float Top = _Config.HasCrop ? _Config.CropTop.Get(-1.0f) : 0.0f;
            const float Right = _Config.HasCrop ? _Config.CropRight.Get(-1.0f) : 1.0f;
            const float Bottom = _Config.HasCrop ? _Config.CropBottom.Get(-1.0f) : 1.0f;
            if (!ValidCrop(Left, Top, Right, Bottom)) { return nullptr; }
            const int32 Index = static_cast<int32>(Raw);
            const FVector2D Cell(1.0 / _Config.Columns, 1.0 / _Config.Rows);
            const FVector2D CellMin((Index % _Config.Columns) * Cell.X, (Index / _Config.Columns) * Cell.Y);
            const FVector2D Min = CellMin + FVector2D(Left * Cell.X, Top * Cell.Y);
            const FVector2D Max = CellMin + FVector2D(Right * Cell.X, Bottom * Cell.Y);
            const FVector2D PixelSize(Source->ImageSize.X * (Max.X - Min.X),
                Source->ImageSize.Y * (Max.Y - Min.Y));
            if (!FMath::IsFinite(PixelSize.X) || !FMath::IsFinite(PixelSize.Y)
                || PixelSize.X <= 0.0f || PixelSize.Y <= 0.0f) { return nullptr; }
            if (!_HasBrush || _LastSourceBrush != *Source || _LastIndex != Index
                || _LastMin != Min || _LastMax != Max)
            {
                _BrushIndex = 1 - _BrushIndex;
                FSlateBrush& Brush = _Brushes[_BrushIndex];
                Brush = *Source;
                Brush.SetUVRegion(FBox2D(Min, Max));
                Brush.ImageSize = PixelSize;
                _LastSourceBrush = *Source; _LastIndex = Index;
                _LastMin = Min; _LastMax = Max;
                _HasBrush = true;
            }
            return &_Brushes[_BrushIndex];
        }
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FAtlas> InOwner, FConfiguration InConfig) : _Owner(InOwner), _Config(MoveTemp(InConfig)) {}
            void Commit() noexcept override
            {
                if (!_Owner->_Active) { return; }
                _Owner->_Config = MoveTemp(_Config);
                _Owner->_Widget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
            }
        private:
            TSharedRef<FAtlas> _Owner;
            FConfiguration _Config;
        };
        FConfiguration _Config;
        FSlateBrush _Brushes[2];
        FSlateBrush _LastSourceBrush;
        FVector2D _LastMin = FVector2D::ZeroVector;
        FVector2D _LastMax = FVector2D::ZeroVector;
        int32 _LastIndex = INDEX_NONE;
        int32 _BrushIndex = 0;
        bool _HasBrush = false;
        TSharedPtr<SImage> _Widget;
        TSharedPtr<SScaleBox> _ScaleBox;
        TSharedPtr<SWidget> _Root;
        bool _Active = true;
    };
}

auto RegisterAtlasImageUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("atlas-image");
    Registration.Schema.Properties = {
        {TEXT("image"), ECkUiCustomPropertyKind::ImageBinding, true},
        {TEXT("index"), ECkUiCustomPropertyKind::NumberBinding, true},
        {TEXT("columns"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("rows"), ECkUiCustomPropertyKind::Number, true},
        {TEXT("crop-left"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("crop-top"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("crop-right"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("crop-bottom"), ECkUiCustomPropertyKind::NumberBinding, false},
        {TEXT("aspect-fit"), ECkUiCustomPropertyKind::Bool, false}
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArgs, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_atlas_image::FConfiguration Config;
        if (!ck_ui_atlas_image::Configure(InArgs, Config, OutFailure)) { return {}; }
        const auto Atlas = MakeShared<ck_ui_atlas_image::FAtlas>(MoveTemp(Config));
        Atlas->Initialize(InArgs.Id);
        return Atlas;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
