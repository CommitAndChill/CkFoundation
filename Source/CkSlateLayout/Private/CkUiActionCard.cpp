#include "CkSlateLayout/CkUiActionCard.h"

#include "CkSlateLayout/CkFlexLayoutTypes.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Layout/Children.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"

namespace ck_ui_action_card
{
    struct FVisualStyle
    {
        FLinearColor Background = FLinearColor(0.10f, 0.16f, 0.21f);
        FLinearColor HoverBackground = FLinearColor(0.14f, 0.22f, 0.28f);
        FLinearColor PressedBackground = FLinearColor(0.08f, 0.13f, 0.17f);
        FLinearColor DisabledBackground = FLinearColor(0.08f, 0.10f, 0.12f);
        FLinearColor SelectedBackground = FLinearColor(0.22f, 0.19f, 0.11f);
        FLinearColor Border = FLinearColor(0.20f, 0.31f, 0.38f);
        FLinearColor SelectedBorder = FLinearColor(0.92f, 0.63f, 0.20f);
        FLinearColor FocusBorder = FLinearColor(1.0f, 0.75f, 0.31f);
        float Radius = 6.0f;
        float BorderWidth = 1.0f;
        float Padding = 12.0f;
    };

    struct FConfiguration
    {
        TAttribute<bool> Selected;
        TAttribute<bool> Enabled;
        TAttribute<bool> CanDispatchEvents;
        FSimpleDelegate Action;
        FString ActionName;
        TSharedPtr<SWidget> Content;
        FVisualStyle Visual;
    };

    class SActionCard final : public SButton
    {
    public:
        SLATE_BEGIN_ARGS(SActionCard) {}
            SLATE_ARGUMENT(FVisualStyle, Visual)
            SLATE_ATTRIBUTE(bool, Selected)
            SLATE_ATTRIBUTE(bool, IsEnabled)
            SLATE_EVENT(FOnClicked, OnClicked)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            _Visual = InArgs._Visual;
            _Selected = InArgs._Selected;
            _Style = MakeStyle(false, false);
            SButton::Construct(SButton::FArguments()
                .ButtonStyle(&_Style)
                .ContentPadding(FMargin(_Visual.Padding))
                .OnClicked(InArgs._OnClicked)
                .IsFocusable(true)
                [InArgs._Content.Widget]);
            SetEnabled(InArgs._IsEnabled);
            RefreshStyle();
        }

        void ApplyVisual(const FVisualStyle& InVisual)
        {
            _Visual = InVisual;
            SetContentPadding(FMargin(_Visual.Padding));
            RefreshStyle();
            Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
        }

        auto GetPendingCapture() const -> TOptional<FCkUiPointerCapture>
        {
            if (!_Press.IsSet() || !FSlateApplication::IsInitialized())
            { return {}; }
            const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(_Press->UserIndex);
            if (!User.IsValid() || User->GetPointerCaptor(_Press->PointerIndex).Get() != this)
            { return {}; }
            return _Press;
        }

        auto HasPendingPress() const -> bool
        {
            return _Press.IsSet();
        }

        void BeginCaptureTransfer(const FCkUiPointerCapture& InCapture) noexcept
        {
            _Transfer = InCapture;
        }

        void EndCaptureTransfer(const bool InRestored) noexcept
        {
            _Transfer.Reset();
            if (!InRestored)
            { CancelPress(); }
        }

        void BeginFocusTransfer() noexcept
        {
            _FocusTransfer = true;
            _PressedUserFocusTransferred = false;
        }

        void EndFocusTransfer() noexcept
        {
            _FocusTransfer = false;
            const bool VerifyPressedUserFocus = _PressedUserFocusTransferred;
            _PressedUserFocusTransferred = false;
            if (!VerifyPressedUserFocus || !_Press.IsSet())
            { return; }
            const bool CapturedUserStillFocused = FSlateApplication::IsInitialized()
                && FSlateApplication::Get().GetUser(_Press->UserIndex).IsValid()
                && FSlateApplication::Get().GetUserFocusedWidget(_Press->UserIndex).Get() == this;
            if (!CapturedUserStillFocused)
            { ReleaseOwnedCapture(); }
        }

        void CancelPress()
        {
            _Press.Reset();
            _PressIsTouch = false;
            Release();
        }

        void ReleaseOwnedCapture()
        {
            if (_Press.IsSet() && FSlateApplication::IsInitialized())
            {
                const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(_Press->UserIndex);
                if (User.IsValid() && User->GetPointerCaptor(_Press->PointerIndex).Get() == this)
                { User->ReleaseCapture(_Press->PointerIndex); }
            }
            CancelPress();
        }

        virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override
        {
            if (_Press.IsSet())
            { return FReply::Unhandled(); }
            const FReply Reply = SButton::OnMouseButtonDown(InGeometry, InEvent);
            if (Reply.GetMouseCaptor().Get() == this)
            {
                _Press = FCkUiPointerCapture{static_cast<int32>(InEvent.GetUserIndex()), InEvent.GetPointerIndex(), SharedThis(this)};
                _PressIsTouch = InEvent.IsTouchEvent();
            }
            return Reply;
        }

        virtual auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override
        {
            if (!_Press.IsSet() || _Press->UserIndex != static_cast<int32>(InEvent.GetUserIndex())
                || _Press->PointerIndex != InEvent.GetPointerIndex()
                || _PressIsTouch != InEvent.IsTouchEvent()
                || (!InEvent.IsTouchEvent() && InEvent.GetEffectingButton() != EKeys::LeftMouseButton))
            { return FReply::Unhandled(); }
            // The release can invoke authored code that reloads or removes this card. The outgoing
            // native reply will release capture; a completed press must not be transferred again.
            _Press.Reset();
            _PressIsTouch = false;
            return SButton::OnMouseButtonUp(InGeometry, InEvent);
        }

        virtual auto OnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override
        { return OnMouseButtonDown(InGeometry, InEvent); }

        virtual auto OnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override
        { return OnMouseButtonUp(InGeometry, InEvent); }

        virtual void OnMouseCaptureLost(const FCaptureLostEvent& InEvent) override
        {
            const bool IsSyntheticTransfer = _Transfer.IsSet()
                && _Transfer->UserIndex == InEvent.UserIndex
                && _Transfer->PointerIndex == static_cast<uint32>(InEvent.PointerIndex);
            if (IsSyntheticTransfer)
            { return; }
            if (_Press.IsSet() && (_Press->UserIndex != InEvent.UserIndex
                || _Press->PointerIndex != static_cast<uint32>(InEvent.PointerIndex)))
            { return; }
            if (_Press.IsSet() && _Press->UserIndex == InEvent.UserIndex
                && _Press->PointerIndex == static_cast<uint32>(InEvent.PointerIndex))
            {
                _Press.Reset();
                _PressIsTouch = false;
            }
            SButton::OnMouseCaptureLost(InEvent);
        }

        virtual void Tick(const FGeometry& InGeometry, const double InCurrentTime, const float InDeltaTime) override
        {
            SButton::Tick(InGeometry, InCurrentTime, InDeltaTime);
            const bool Selected = _Selected.Get(false);
            if (Selected != _WasSelected)
            { RefreshStyle(); }
        }

        virtual auto OnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InEvent) -> FReply override
        {
            _Focused = true;
            RefreshStyle();
            return SButton::OnFocusReceived(InGeometry, InEvent);
        }

        virtual void OnFocusLost(const FFocusEvent& InEvent) override
        {
            const bool Captured = GetPendingCapture().IsSet();
            const bool OtherUser = _Press.IsSet() && _Press->UserIndex != static_cast<int32>(InEvent.GetUser());
            if (Captured && (_FocusTransfer || OtherUser))
            {
                if (_FocusTransfer && !OtherUser)
                { _PressedUserFocusTransferred = true; }
                SBorder::OnFocusLost(InEvent);
            }
            else
            {
                ReleaseOwnedCapture();
                SButton::OnFocusLost(InEvent);
            }
            _Focused = false;
            RefreshStyle();
        }

        virtual auto GetFocusBrush() const -> const FSlateBrush* override { return nullptr; }

    private:
        auto MakeStyle(const bool Selected, const bool Focused) const -> FButtonStyle
        {
            const FLinearColor Border = Focused ? _Visual.FocusBorder
                : Selected ? _Visual.SelectedBorder : _Visual.Border;
            const FLinearColor Background = Selected ? _Visual.SelectedBackground : _Visual.Background;
            const FLinearColor Hover = Selected ? _Visual.SelectedBackground : _Visual.HoverBackground;
            const FLinearColor Pressed = Selected ? _Visual.SelectedBackground : _Visual.PressedBackground;
            return FButtonStyle()
                .SetNormal(FSlateRoundedBoxBrush(Background, _Visual.Radius, Border, _Visual.BorderWidth))
                .SetHovered(FSlateRoundedBoxBrush(Hover, _Visual.Radius, Border, _Visual.BorderWidth))
                .SetPressed(FSlateRoundedBoxBrush(Pressed, _Visual.Radius, Border, _Visual.BorderWidth))
                .SetDisabled(FSlateRoundedBoxBrush(_Visual.DisabledBackground, _Visual.Radius, _Visual.Border, _Visual.BorderWidth))
                .SetNormalPadding(FMargin(0.0f))
                .SetPressedPadding(FMargin(0.0f));
        }

        void RefreshStyle()
        {
            _WasSelected = _Selected.Get(false);
            _Style = MakeStyle(_WasSelected, _Focused);
            SetButtonStyle(&_Style);
            Invalidate(EInvalidateWidgetReason::Paint);
        }

        FVisualStyle _Visual;
        TAttribute<bool> _Selected;
        FButtonStyle _Style;
        bool _Focused = false;
        bool _WasSelected = false;
        TOptional<FCkUiPointerCapture> _Press;
        bool _PressIsTouch = false;
        bool _FocusTransfer = false;
        bool _PressedUserFocusTransferred = false;
        TOptional<FCkUiPointerCapture> _Transfer;
    };

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& InArguments, FConfiguration& Out, FString& OutFailure) -> bool
    {
        const FSimpleDelegate* Action = InArguments.Actions.Find(TEXT("action"));
        const FString* ActionName = InArguments.BindingNames.Find(TEXT("action"));
        const TSharedPtr<SWidget> Content = InArguments.Slots.FindRef(TEXT("content"));
        if (InArguments.Id.IsEmpty() || Action == nullptr || !Action->IsBound() || ActionName == nullptr
            || ActionName->IsEmpty() || !Content.IsValid())
        {
            OutFailure = TEXT("Action card requires an id, action binding, and content slot.");
            return false;
        }
        Out.Action = *Action;
        Out.ActionName = *ActionName;
        Out.Content = Content;
        Out.Selected = InArguments.BoolBindings.FindRef(TEXT("selected"));
        Out.Enabled = InArguments.BoolBindings.FindRef(TEXT("enabled"));
        Out.CanDispatchEvents = InArguments.CanDispatchEvents;
        auto Color = [&InArguments, &OutFailure](const TCHAR* InName, FLinearColor& OutColor) -> bool
        {
            if (const FCkUiCustomStyleValue* Value = InArguments.Style.CustomProperties.Find(InName))
            {
                const FLinearColor& Input = Value->Color;
                const bool Valid = Value->Kind == ECkUiCustomStyleKind::Color
                    && FMath::IsFinite(Input.R) && FMath::IsFinite(Input.G)
                    && FMath::IsFinite(Input.B) && FMath::IsFinite(Input.A);
                if (!Valid)
                {
                    OutFailure = FString::Printf(TEXT("Action card %s requires a finite color."), InName);
                    return false;
                }
                OutColor = Input;
            }
            return true;
        };
        if (!Color(TEXT("-ck-action-card-background"), Out.Visual.Background)
            || !Color(TEXT("-ck-action-card-hover-background"), Out.Visual.HoverBackground)
            || !Color(TEXT("-ck-action-card-pressed-background"), Out.Visual.PressedBackground)
            || !Color(TEXT("-ck-action-card-disabled-background"), Out.Visual.DisabledBackground)
            || !Color(TEXT("-ck-action-card-selected-background"), Out.Visual.SelectedBackground)
            || !Color(TEXT("-ck-action-card-border-color"), Out.Visual.Border)
            || !Color(TEXT("-ck-action-card-selected-border-color"), Out.Visual.SelectedBorder)
            || !Color(TEXT("-ck-action-card-focus-border-color"), Out.Visual.FocusBorder))
        { return false; }
        auto Length = [&InArguments, &OutFailure](const TCHAR* InName, float& OutLength) -> bool
        {
            if (const FCkUiCustomStyleValue* Value = InArguments.Style.CustomProperties.Find(InName))
            {
                const bool Valid = Value->Kind == ECkUiCustomStyleKind::Length
                    && FMath::IsFinite(Value->Number) && Value->Number >= 0.0f && Value->Number <= 512.0f;
                if (!Valid)
                {
                    OutFailure = FString::Printf(TEXT("Action card %s requires a finite length between 0 and 512."), InName);
                    return false;
                }
                OutLength = Value->Number;
            }
            return true;
        };
        return Length(TEXT("-ck-action-card-radius"), Out.Visual.Radius)
            && Length(TEXT("-ck-action-card-border-width"), Out.Visual.BorderWidth)
            && Length(TEXT("-ck-action-card-padding"), Out.Visual.Padding);
    }

    class FActionCard final : public ICkUiRetainedWidget, public TSharedFromThis<FActionCard>
    {
    public:
        explicit FActionCard(FConfiguration InConfiguration) : _Configuration(MoveTemp(InConfiguration)) {}
        virtual ~FActionCard() override
        {
            _Active = false;
            if (_Widget.IsValid())
            { _Widget->ReleaseOwnedCapture(); }
        }

        void Initialize(const FString& InId)
        {
            const TWeakPtr<FActionCard> Weak = AsShared();
            SAssignNew(_Widget, SActionCard)
                .Visual(_Configuration.Visual)
                .Selected(TAttribute<bool>::CreateLambda([Weak]()
                {
                    const TSharedPtr<FActionCard> Card = Weak.Pin();
                    return Card.IsValid() && Card->_Configuration.Selected.Get(false);
                }))
                .IsEnabled(TAttribute<bool>::CreateLambda([Weak]()
                {
                    const TSharedPtr<FActionCard> Card = Weak.Pin();
                    return Card.IsValid() && Card->CanDispatch();
                }))
                .OnClicked_Lambda([Weak]() -> FReply
                {
                    if (const TSharedPtr<FActionCard> Card = Weak.Pin())
                    { Card->Dispatch(); }
                    return FReply::Handled();
                })
                [_Configuration.Content.ToSharedRef()];
            _Widget->SetTag(FName(*InId));
            _Widget->AddMetadata(MakeShared<FCkFlexMeasureMetaData>(
                [Weak](const FCkFlexMeasureArgs& InArgs) -> FVector2D
                {
                    const TSharedPtr<FActionCard> Card = Weak.Pin();
                    return Card.IsValid() ? Card->Measure(InArgs) : FVector2D::ZeroVector;
                },
                [Weak](const float InWidth, const float InHeight)
                {
                    if (const TSharedPtr<FActionCard> Card = Weak.Pin())
                    { Card->NotifyArranged(InWidth, InHeight); }
                }));
        }

        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return _Widget.ToSharedRef(); }
        virtual auto GetPointerCaptures() const -> TArray<FCkUiPointerCapture> override
        {
            if (!_Widget.IsValid())
            { return {}; }
            const TOptional<FCkUiPointerCapture> Capture = _Widget->GetPendingCapture();
            return Capture.IsSet() ? TArray<FCkUiPointerCapture>{Capture.GetValue()} : TArray<FCkUiPointerCapture>{};
        }
        virtual void BeginPointerCaptureTransfer(const FCkUiPointerCapture& InCapture) noexcept override
        {
            if (_Widget.IsValid())
            { _Widget->BeginCaptureTransfer(InCapture); }
        }
        virtual void EndPointerCaptureTransfer(const FCkUiPointerCapture&, const bool InRestored) noexcept override
        {
            if (_Widget.IsValid())
            { _Widget->EndCaptureTransfer(InRestored); }
        }
        virtual void BeginFocusTransfer() noexcept override
        {
            if (_Widget.IsValid())
            { _Widget->BeginFocusTransfer(); }
        }
        virtual void EndFocusTransfer() noexcept override
        {
            if (_Widget.IsValid())
            { _Widget->EndFocusTransfer(); }
        }
        virtual void ReleaseTransientInteraction() override
        {
            if (_Widget.IsValid())
            { _Widget->ReleaseOwnedCapture(); }
        }
        virtual void ReleaseOwnerInteraction() override
        {
            _Active = false;
            ReleaseTransientInteraction();
        }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    private:
        class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FPreparedUpdate(TSharedRef<FActionCard> InCard, FConfiguration InConfiguration)
                : _Card(MoveTemp(InCard)), _Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override
            {
                _Card->_Configuration = MoveTemp(_Configuration);
                _Card->_Widget->ApplyVisual(_Card->_Configuration.Visual);
            }
        private:
            TSharedRef<FActionCard> _Card;
            FConfiguration _Configuration;
        };

        auto SlotMeasure() const -> TSharedPtr<FCkFlexMeasureMetaData>
        {
            TSharedPtr<SWidget> Target = _Configuration.Content;
            if (!Target.IsValid())
            { return {}; }
            TSharedPtr<FCkFlexMeasureMetaData> Result = Target->GetMetaData<FCkFlexMeasureMetaData>();
            const FChildren* Children = Target->GetChildren();
            if (!Result.IsValid() && Children != nullptr && Children->Num() == 1)
            {
                Target = ConstCastSharedRef<SWidget>(Children->GetChildAt(0));
                Result = Target->GetMetaData<FCkFlexMeasureMetaData>();
            }
            return Result;
        }

        auto Measure(const FCkFlexMeasureArgs& InArgs) const -> FVector2D
        {
            if (!_Configuration.Content.IsValid())
            { return FVector2D::ZeroVector; }
            const float Padding = _Configuration.Visual.Padding * 2.0f;
            FCkFlexMeasureArgs Inner = InArgs;
            if (Inner.WidthMode != YGMeasureModeUndefined)
            { Inner.AvailableWidth = FMath::Max(0.0f, Inner.AvailableWidth - Padding); }
            if (Inner.HeightMode != YGMeasureModeUndefined)
            { Inner.AvailableHeight = FMath::Max(0.0f, Inner.AvailableHeight - Padding); }
            const TSharedPtr<FCkFlexMeasureMetaData> Slot = SlotMeasure();
            const FVector2D Size = Slot.IsValid() ? Slot->Measure(Inner) : _Configuration.Content->GetDesiredSize();
            return FVector2D(Size.X + Padding, Size.Y + Padding);
        }

        void NotifyArranged(const float InWidth, const float InHeight)
        {
            if (const TSharedPtr<FCkFlexMeasureMetaData> Slot = SlotMeasure())
            {
                const float Padding = _Configuration.Visual.Padding * 2.0f;
                Slot->NotifyArranged(FMath::Max(0.0f, InWidth - Padding), FMath::Max(0.0f, InHeight - Padding));
            }
        }

        void Dispatch()
        {
            if (!CanDispatch())
            { return; }
            const FSimpleDelegate Action = _Configuration.Action;
            Action.ExecuteIfBound();
        }

        auto CanDispatch() const -> bool
        {
            return _Active && _Configuration.CanDispatchEvents.Get(false) && _Configuration.Enabled.Get(true);
        }

        TSharedPtr<SActionCard> _Widget;
        FConfiguration _Configuration;
        bool _Active = true;
    };

    auto FActionCard::PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
        -> TUniquePtr<ICkUiPreparedWidgetUpdate>
    {
        FConfiguration Configuration;
        if (!MakeConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        if (Configuration.Content != _Configuration.Content)
        {
            OutFailure = TEXT("Action card content slot cannot change for a retained id.");
            return {};
        }
        if (_Widget.IsValid() && _Widget->HasPendingPress() && Configuration.ActionName != _Configuration.ActionName)
        {
            OutFailure = TEXT("Action card action cannot change during an active press.");
            return {};
        }
        return MakeUnique<FPreparedUpdate>(ConstCastSharedRef<FActionCard>(AsShared()), MoveTemp(Configuration));
    }
}

auto FCkUiActionCard::Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("action-card");
    Registration.Schema.Properties = {
        {TEXT("action"), ECkUiCustomPropertyKind::Action, true},
        {TEXT("selected"), ECkUiCustomPropertyKind::BoolBinding, false},
        {TEXT("enabled"), ECkUiCustomPropertyKind::BoolBinding, false},
    };
    Registration.Schema.Slots = {{TEXT("content"), true}};
    Registration.Schema.StyleProperties = {
        {TEXT("-ck-action-card-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-hover-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-pressed-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-disabled-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-selected-background"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-border-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-selected-border-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-focus-border-color"), ECkUiCustomStyleKind::Color},
        {TEXT("-ck-action-card-radius"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-action-card-border-width"), ECkUiCustomStyleKind::Length},
        {TEXT("-ck-action-card-padding"), ECkUiCustomStyleKind::Length},
    };
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure)
        -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_ui_action_card::FConfiguration Configuration;
        if (!ck_ui_action_card::MakeConfiguration(InArguments, Configuration, OutFailure))
        { return {}; }
        const TSharedRef<ck_ui_action_card::FActionCard> Card = MakeShared<ck_ui_action_card::FActionCard>(MoveTemp(Configuration));
        Card->Initialize(InArguments.Id);
        return Card;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
