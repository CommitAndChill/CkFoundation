#include "CkSlateLayout/CkUiKeyHint.h"

#include "CkSlateLayout/CkUiFontRegistry.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ui_key_hint
{
    struct FVisual
    {
        FLinearColor Foreground = FLinearColor(0.94f, 0.96f, 1.0f);
        FLinearColor Background = FLinearColor(0.08f, 0.12f, 0.18f, 0.72f);
        FLinearColor Border = FLinearColor(0.59f, 0.70f, 0.82f, 0.65f);
        float Radius = 4.0f;
        float BorderWidth = 1.0f;
        float PaddingX = 8.0f;
        float PaddingY = 4.0f;
        FSlateFontInfo Font;
    };

    struct FConfiguration
    {
        FText Label;
        FSimpleDelegate Action;
        FString ActionName;
        TAttribute<bool> CanDispatchEvents;
        FVisual Visual;
    };

    auto ReadColor(const FCkUiStyle& InStyle, const TCHAR* InName, FLinearColor& OutColor,
        FString& OutFailure) -> bool
    {
        const FCkUiCustomStyleValue* Value = InStyle.CustomProperties.Find(InName);
        if (Value == nullptr)
        { return true; }
        const bool Valid = Value->Kind == ECkUiCustomStyleKind::Color
            && FMath::IsFinite(Value->Color.R) && FMath::IsFinite(Value->Color.G)
            && FMath::IsFinite(Value->Color.B) && FMath::IsFinite(Value->Color.A);
        if (!Valid)
        {
            OutFailure = FString::Printf(TEXT("Key hint %s requires a finite color."), InName);
            return false;
        }
        OutColor = Value->Color;
        return true;
    }

    auto ReadLength(const FCkUiStyle& InStyle, const TCHAR* InName, const float InMinimum,
        const float InMaximum, float& OutLength, FString& OutFailure) -> bool
    {
        const FCkUiCustomStyleValue* Value = InStyle.CustomProperties.Find(InName);
        if (Value == nullptr)
        { return true; }
        const bool Valid = Value->Kind == ECkUiCustomStyleKind::Length
            && FMath::IsFinite(Value->Number) && Value->Number >= InMinimum && Value->Number <= InMaximum;
        if (!Valid)
        {
            OutFailure = FString::Printf(TEXT("Key hint %s requires a finite length in [%g, %g]."),
                InName, InMinimum, InMaximum);
            return false;
        }
        OutLength = Value->Number;
        return true;
    }

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& InArguments, FConfiguration& Out,
        FString& OutFailure) -> bool
    {
        const FText* Label = InArguments.TextProperties.Find(TEXT("label"));
        const bool HasLabel = !InArguments.Id.IsEmpty() && Label != nullptr
            && !Label->ToString().TrimStartAndEnd().IsEmpty();
        if (!HasLabel)
        {
            OutFailure = TEXT("Key hint requires an id and nonempty literal label.");
            return false;
        }
        const FSimpleDelegate* Action = InArguments.Actions.Find(TEXT("action"));
        const FString* ActionName = InArguments.BindingNames.Find(TEXT("action"));
        const bool HasAction = Action != nullptr || ActionName != nullptr;
        const bool ValidAction = !HasAction || (Action != nullptr && Action->IsBound()
            && ActionName != nullptr && !ActionName->IsEmpty());
        if (!ValidAction)
        {
            OutFailure = TEXT("Key hint action must name a bound callback.");
            return false;
        }
        Out.Label = *Label;
        Out.Action = Action != nullptr ? *Action : FSimpleDelegate{};
        Out.ActionName = ActionName != nullptr ? *ActionName : FString{};
        Out.CanDispatchEvents = InArguments.CanDispatchEvents;
        Out.Visual.Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
        if (const FText* Family = InArguments.TextProperties.Find(TEXT("font-family")))
        {
            FSlateFontInfo Resolved;
            const bool ValidFamily = !Family->IsEmpty() && InArguments.FontRegistry.IsValid()
                && InArguments.FontRegistry->TryResolve(Family->ToString(), TEXT("Regular"), Resolved);
            if (!ValidFamily)
            {
                OutFailure = TEXT("Key hint font-family must name a registered regular font.");
                return false;
            }
            Out.Visual.Font = MoveTemp(Resolved);
        }
        float FontSize = Out.Visual.Font.Size;
        if (!ReadLength(InArguments.Style, TEXT("-ck-key-hint-font-size"), 1.0f, 256.0f,
                FontSize, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-key-hint-foreground"), Out.Visual.Foreground, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-key-hint-background"), Out.Visual.Background, OutFailure)
            || !ReadColor(InArguments.Style, TEXT("-ck-key-hint-border-color"), Out.Visual.Border, OutFailure)
            || !ReadLength(InArguments.Style, TEXT("-ck-key-hint-border-width"), 0.0f, 32.0f,
                Out.Visual.BorderWidth, OutFailure)
            || !ReadLength(InArguments.Style, TEXT("-ck-key-hint-radius"), 0.0f, 128.0f,
                Out.Visual.Radius, OutFailure)
            || !ReadLength(InArguments.Style, TEXT("-ck-key-hint-padding-x"), 0.0f, 256.0f,
                Out.Visual.PaddingX, OutFailure)
            || !ReadLength(InArguments.Style, TEXT("-ck-key-hint-padding-y"), 0.0f, 256.0f,
                Out.Visual.PaddingY, OutFailure))
        { return false; }
        Out.Visual.Font.Size = FMath::RoundToInt(FontSize);
        return true;
    }

    class SKeyHint final : public SButton
    {
    public:
        SLATE_BEGIN_ARGS(SKeyHint) {}
            SLATE_ARGUMENT(FVisual, Visual)
            SLATE_ARGUMENT(FText, Label)
            SLATE_ARGUMENT(bool, Interactive)
            SLATE_EVENT(FOnClicked, OnClicked)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            _Interactive = InArgs._Interactive;
            _Style = MakeStyle(InArgs._Visual);
            SButton::Construct(SButton::FArguments()
                .ButtonStyle(&_Style)
                .ContentPadding(FMargin(InArgs._Visual.PaddingX, InArgs._Visual.PaddingY))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .IsFocusable(true)
                .OnClicked(InArgs._OnClicked)
                [
                    SAssignNew(_Text, STextBlock)
                    .Text(InArgs._Label)
                    .Font(InArgs._Visual.Font)
                    .ColorAndOpacity(InArgs._Visual.Foreground)
                    .Justification(ETextJustify::Center)
                ]);
            SetVisibility(_Interactive ? EVisibility::Visible : EVisibility::SelfHitTestInvisible);
        }

        virtual auto SupportsKeyboardFocus() const -> bool override
        {
            return _Interactive;
        }

        void Apply(const FConfiguration& InConfiguration)
        {
            _Style = MakeStyle(InConfiguration.Visual);
            SetButtonStyle(&_Style);
            SetContentPadding(FMargin(InConfiguration.Visual.PaddingX, InConfiguration.Visual.PaddingY));
            _Text->SetText(InConfiguration.Label);
            _Text->SetFont(InConfiguration.Visual.Font);
            _Text->SetColorAndOpacity(InConfiguration.Visual.Foreground);
            _Interactive = InConfiguration.Action.IsBound();
            SetVisibility(_Interactive ? EVisibility::Visible : EVisibility::SelfHitTestInvisible);
            Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
        }

        void Freeze()
        {
            _Interactive = false;
            SetVisibility(EVisibility::SelfHitTestInvisible);
        }

    private:
        static auto MakeStyle(const FVisual& InVisual) -> FButtonStyle
        {
            const FSlateRoundedBoxBrush Brush(InVisual.Background, InVisual.Radius,
                InVisual.Border, InVisual.BorderWidth);
            return FButtonStyle()
                .SetNormal(Brush).SetHovered(Brush).SetPressed(Brush).SetDisabled(Brush)
                .SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
        }

        TSharedPtr<STextBlock> _Text;
        FButtonStyle _Style;
        bool _Interactive = false;
    };

    class FKeyHint final : public ICkUiRetainedWidget, public TSharedFromThis<FKeyHint>
    {
    public:
        explicit FKeyHint(FConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        virtual ~FKeyHint() override
        {
            if (_Widget.IsValid())
            { _Widget->Freeze(); }
        }

        void Initialize(const FString& InId)
        {
            _Id = InId;
            const TWeakPtr<FKeyHint> Weak = AsShared();
            SAssignNew(_Widget, SKeyHint)
                .Tag(FName(*InId))
                .Visual(_Configuration.Visual)
                .Label(_Configuration.Label)
                .Interactive(_Configuration.Action.IsBound())
                .OnClicked_Lambda([Weak]() -> FReply
                {
                    const TSharedPtr<FKeyHint> Hint = Weak.Pin();
                    if (!Hint.IsValid() || !Hint->_Active || !Hint->_Configuration.Action.IsBound())
                    { return FReply::Unhandled(); }
                    if (!Hint->_Configuration.CanDispatchEvents.Get(false))
                    { return FReply::Handled(); }
                    const FSimpleDelegate Action = Hint->_Configuration.Action;
                    Action.ExecuteIfBound();
                    return FReply::Handled();
                });
        }

        virtual auto GetWidget() const -> TSharedRef<SWidget> override
        {
            return _Widget.ToSharedRef();
        }

        virtual void ReleaseOwnerInteraction() override
        {
            _Active = false;
            _Configuration.Action = FSimpleDelegate{};
            _Configuration.CanDispatchEvents = TAttribute<bool>{};
            if (_Widget.IsValid())
            { _Widget->Freeze(); }
        }

        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            if (!_Active)
            {
                OutFailure = TEXT("Released key hint cannot reload.");
                return {};
            }
            if (InArguments.Id != _Id)
            {
                OutFailure = TEXT("Key hint retained id cannot change.");
                return {};
            }
            FConfiguration Candidate;
            if (!MakeConfiguration(InArguments, Candidate, OutFailure))
            { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FKeyHint>(AsShared()), MoveTemp(Candidate));
        }

    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FKeyHint> InHint, FConfiguration InConfiguration)
                : _Hint(MoveTemp(InHint)), _Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                if (!_Hint->_Active)
                { return; }
                _Hint->_Configuration = MoveTemp(_Configuration);
                _Hint->_Widget->Apply(_Hint->_Configuration);
            }
        private:
            TSharedRef<FKeyHint> _Hint;
            FConfiguration _Configuration;
        };

        FConfiguration _Configuration;
        FString _Id;
        TSharedPtr<SKeyHint> _Widget;
        bool _Active = true;
    };
}

auto RegisterKeyHintUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("key-hint");
    Registration.Schema.Properties = {
        {TEXT("label"), ECkUiCustomPropertyKind::Text, true},
        {TEXT("action"), ECkUiCustomPropertyKind::Action, false},
        {TEXT("font-family"), ECkUiCustomPropertyKind::Text, false},
    };
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-key-hint-font-size"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-key-hint-foreground"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-key-hint-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-key-hint-border-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-key-hint-border-width"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-key-hint-radius"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-key-hint-padding-x"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-key-hint-padding-y"), ECkUiCustomStyleKind::Length},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_key_hint::FConfiguration Configuration;
        if (!ck_ui_key_hint::MakeConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        const TSharedRef<ck_ui_key_hint::FKeyHint> Hint = MakeShared<ck_ui_key_hint::FKeyHint>(MoveTemp(Configuration));
        Hint->Initialize(InArguments.Id);
        return Hint;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
