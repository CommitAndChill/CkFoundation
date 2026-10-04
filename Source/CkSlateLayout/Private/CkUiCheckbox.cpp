#include "CkSlateLayout/CkUiCheckbox.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ui_checkbox
{
    struct FConfiguration
    {
        TAttribute<bool> Value;
        TAttribute<FText> Label;
        TAttribute<bool> Enabled;
        TAttribute<bool> ReadOnly;
        TAttribute<bool> CanDispatchEvents;
        FCkUiOnBoolChanged Changed;
        FString ValueBindingName;
        FCheckBoxStyle NativeStyle;
        TOptional<FLinearColor> LabelColor;
    };

    class FCheckbox final : public ICkUiRetainedWidget, public TSharedFromThis<FCheckbox>
    {
    public:
        explicit FCheckbox(FConfiguration InConfiguration)
            : _Configuration(MoveTemp(InConfiguration))
        {
        }

        auto Initialize(const FString& InId, const FSlateFontInfo& InBaseFont) -> void
        {
            const TWeakPtr<FCheckbox> WeakCheckbox = AsShared();
            // SCheckBox borrows its style. Its checked-state attribute retains this storage even
            // when a caller holds the native widget after the authored view releases its owner.
            const TSharedRef<FCheckBoxStyle> NativeStyle = MakeShared<FCheckBoxStyle>(_Configuration.NativeStyle);
            _NativeStyle = NativeStyle;
            TSharedPtr<SCheckBox> Checkbox;
            SAssignNew(Checkbox, SCheckBox).Tag(FName(*InId))
                .Style(&NativeStyle.Get())
                .IsChecked(TAttribute<ECheckBoxState>::CreateLambda([WeakCheckbox, NativeStyle]()
                {
                    const TSharedPtr<FCheckbox> Control = WeakCheckbox.Pin();
                    return Control.IsValid() && Control->_Configuration.Value.Get(false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                }))
                .IsEnabled(TAttribute<bool>::CreateLambda([WeakCheckbox]()
                {
                    const TSharedPtr<FCheckbox> Control = WeakCheckbox.Pin();
                    return Control.IsValid() && Control->_Configuration.Enabled.Get(true);
                }))
                .OnCheckStateChanged_Lambda([WeakCheckbox](const ECheckBoxState InState)
                {
                    if (const TSharedPtr<FCheckbox> Control = WeakCheckbox.Pin()) { Control->OnCheckStateChanged(InState); }
                })
                [
                    SNew(STextBlock)
                    .Text(TAttribute<FText>::CreateLambda([WeakCheckbox]()
                    {
                        const TSharedPtr<FCheckbox> Control = WeakCheckbox.Pin();
                        return Control.IsValid() ? Control->_Configuration.Label.Get(FText::GetEmpty()) : FText::GetEmpty();
                    }))
                    .Font(InBaseFont)
                    .ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([WeakCheckbox]()
                    {
                        const TSharedPtr<FCheckbox> Control = WeakCheckbox.Pin();
                        return Control.IsValid() && Control->_Configuration.LabelColor.IsSet()
                            ? FSlateColor(Control->_Configuration.LabelColor.GetValue())
                            : FSlateColor::UseForeground();
                    }))
                ];
            _Widget = Checkbox;
        }

        virtual ~FCheckbox() override
        {
            _Active = false;
        }

        virtual auto GetWidget() const -> TSharedRef<SWidget> override
        {
            return _Widget.ToSharedRef();
        }

        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    private:
        class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FPreparedUpdate(TSharedRef<FCheckbox> InCheckbox, FConfiguration InConfiguration)
                : _Checkbox(MoveTemp(InCheckbox))
                , _Configuration(MoveTemp(InConfiguration))
            {
            }

            virtual void Commit() noexcept override
            {
                _Checkbox->_Configuration = MoveTemp(_Configuration);
                *_Checkbox->_NativeStyle = _Checkbox->_Configuration.NativeStyle;
                _Checkbox->_Widget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
            }

        private:
            TSharedRef<FCheckbox> _Checkbox;
            FConfiguration _Configuration;
        };

        auto CanDispatch() const -> bool
        {
            return _Active && _Configuration.CanDispatchEvents.Get(false) && _Configuration.Enabled.Get(true) && !_Configuration.ReadOnly.Get(false);
        }

        auto OnCheckStateChanged(const ECheckBoxState InState) -> void
        {
            if (!CanDispatch()) { return; }
            const FCkUiOnBoolChanged Changed = _Configuration.Changed;
            Changed.ExecuteIfBound(InState == ECheckBoxState::Checked);
        }

        TSharedPtr<SCheckBox> _Widget;
        TSharedPtr<FCheckBoxStyle> _NativeStyle;
        FConfiguration _Configuration;
        bool _Active = true;
    };

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& InArguments, FConfiguration& OutConfiguration, FString& OutFailure) -> bool
    {
        OutConfiguration.NativeStyle = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>(TEXT("Checkbox"));
        const auto SetTint = [&InArguments](const TCHAR* InName, FSlateBrush& InOutBrush)
        {
            if (const FCkUiCustomStyleValue* Value = InArguments.Style.CustomProperties.Find(InName))
            { InOutBrush.TintColor = FSlateColor(Value->Color); }
        };
        for (FSlateBrush* Brush : {&OutConfiguration.NativeStyle.UncheckedImage,
            &OutConfiguration.NativeStyle.UncheckedHoveredImage, &OutConfiguration.NativeStyle.UncheckedPressedImage})
        { SetTint(TEXT("-ck-checkbox-color"), *Brush); }
        for (FSlateBrush* Brush : {&OutConfiguration.NativeStyle.BackgroundImage,
            &OutConfiguration.NativeStyle.BackgroundHoveredImage, &OutConfiguration.NativeStyle.BackgroundPressedImage})
        { SetTint(TEXT("-ck-checkbox-background-color"), *Brush); }
        for (FSlateBrush* Brush : {&OutConfiguration.NativeStyle.CheckedImage,
            &OutConfiguration.NativeStyle.CheckedHoveredImage, &OutConfiguration.NativeStyle.CheckedPressedImage})
        { SetTint(TEXT("-ck-checkbox-checked-color"), *Brush); }
        if (const FCkUiCustomStyleValue* Size = InArguments.Style.CustomProperties.Find(TEXT("-ck-checkbox-size")))
        {
            const bool ValidSize = FMath::IsFinite(Size->Number) && Size->Number > 0.0f && Size->Number <= 512.0f;
            if (!ValidSize) { OutFailure = TEXT("Checkbox size must be greater than zero and at most 512."); return false; }
            for (FSlateBrush* Brush : {&OutConfiguration.NativeStyle.UncheckedImage,
                &OutConfiguration.NativeStyle.UncheckedHoveredImage, &OutConfiguration.NativeStyle.UncheckedPressedImage,
                &OutConfiguration.NativeStyle.CheckedImage, &OutConfiguration.NativeStyle.CheckedHoveredImage,
                &OutConfiguration.NativeStyle.CheckedPressedImage, &OutConfiguration.NativeStyle.UndeterminedImage,
                &OutConfiguration.NativeStyle.UndeterminedHoveredImage, &OutConfiguration.NativeStyle.UndeterminedPressedImage})
            { Brush->ImageSize = FVector2D(Size->Number, Size->Number); }
        }
        if (const FCkUiCustomStyleValue* Color = InArguments.Style.CustomProperties.Find(TEXT("-ck-checkbox-label-color")))
        { OutConfiguration.LabelColor = Color->Color; }
        const TAttribute<bool>* Value = InArguments.BoolBindings.Find(TEXT("value"));
        const FCkUiOnBoolChanged* Changed = InArguments.BoolChanged.Find(TEXT("changed"));
        const FString* ValueBindingName = InArguments.BindingNames.Find(TEXT("value"));
        if (Value == nullptr || !Value->IsSet() || Changed == nullptr || !Changed->IsBound() || ValueBindingName == nullptr || ValueBindingName->IsEmpty())
        {
            OutFailure = TEXT("Checkbox requires value and changed bindings.");
            return false;
        }
        OutConfiguration.Value = *Value;
        OutConfiguration.Label = InArguments.TextBindings.FindRef(TEXT("label"));
        OutConfiguration.Enabled = InArguments.BoolBindings.FindRef(TEXT("enabled"));
        OutConfiguration.ReadOnly = InArguments.BoolBindings.FindRef(TEXT("read-only"));
        OutConfiguration.CanDispatchEvents = InArguments.CanDispatchEvents;
        OutConfiguration.Changed = *Changed;
        OutConfiguration.ValueBindingName = *ValueBindingName;
        return true;
    }

    auto FCheckbox::PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate>
    {
        auto Configuration = FConfiguration{};
        if (!MakeConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
        if (Configuration.ValueBindingName != _Configuration.ValueBindingName)
        {
            OutFailure = TEXT("Checkbox value binding cannot change for a retained id.");
            return {};
        }
        return MakeUnique<FPreparedUpdate>(ConstCastSharedRef<FCheckbox>(AsShared()), MoveTemp(Configuration));
    }
}

auto FCkUiCheckbox::Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    auto Registration = FCkUiCustomWidgetRegistration{};
    Registration.Schema.Tag = TEXT("checkbox");
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-checkbox-size"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-checkbox-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-checkbox-background-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-checkbox-checked-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-checkbox-label-color"), ECkUiCustomStyleKind::Color}
    };
    Registration.Schema.Properties = {
        {TEXT("value"), ECkUiCustomPropertyKind::BoolBinding, true},
        {TEXT("changed"), ECkUiCustomPropertyKind::BoolChanged, true},
        {TEXT("label"), ECkUiCustomPropertyKind::TextBinding, false},
        {TEXT("enabled"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("read-only"), ECkUiCustomPropertyKind::BoolBinding, false},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
    {
        auto Configuration = ck_ui_checkbox::FConfiguration{};
        if (!ck_ui_checkbox::MakeConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
        const TSharedRef<ck_ui_checkbox::FCheckbox> Checkbox = MakeShared<ck_ui_checkbox::FCheckbox>(MoveTemp(Configuration));
        Checkbox->Initialize(InArguments.Id, InArguments.BaseFont);
        return Checkbox;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
