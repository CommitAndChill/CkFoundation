#include "Widgets/SCkProceduralAction_Timeline.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "SSimpleTimeSlider.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionTimeline"

namespace ck_procedural_action_timeline
{
    constexpr auto Gutter = 110.0f;
    constexpr auto SectionRowHeight = 30.0f;
    constexpr auto LaneHeight = 20.0f;
    constexpr auto PickPixels = 6.0f;
    constexpr auto DragPixels = 3.0f;
    constexpr auto FrameSeconds = 1.0 / 60.0;
}

// --------------------------------------------------------------------------------------------------------------------

class SCkProceduralAction_TimelineLanes : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_TimelineLanes) {}
    SLATE_END_ARGS()

    enum class EHit : uint8 { None, Empty, SectionBody, SectionEdge, Key, Marker, WindowStart, WindowEnd, WindowBody, Pose };

    auto
        Construct(
            const FArguments&,
            SCkProceduralAction_Timeline* InTimeline)
        -> void
    { _Timeline = InTimeline; }

    auto
        ComputeDesiredSize(
            float) const
        -> FVector2D override
    { return FVector2D{400.0, RowTop(RowCount())}; }

    auto
        OnPaint(
            const FPaintArgs& InArgs,
            const FGeometry& InGeometry,
            const FSlateRect& InCullingRect,
            FSlateWindowElementList& OutElements,
            int32 InLayer,
            const FWidgetStyle& InStyle,
            bool InParentEnabled) const
        -> int32 override;
    auto
        OnMouseButtonDown(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent)
        -> FReply override;
    auto
        OnMouseMove(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent)
        -> FReply override;
    auto
        OnMouseButtonUp(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent)
        -> FReply override;
    auto
        OnMouseWheel(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent)
        -> FReply override;
    auto
        OnCursorQuery(
            const FGeometry& InGeometry,
            const FPointerEvent& InEvent) const
        -> FCursorReply override;

private:
    auto RowCount() const -> int32 { return _Timeline->_Channels.Num() + _Timeline->_EventLanes + 2; }
    auto RowTop(int32 InRow) const -> float
    {
        using namespace ck_procedural_action_timeline;
        return InRow <= 0 ? 0.0f : SectionRowHeight + LaneHeight * (InRow - 1);
    }
    auto RowHeight(int32 InRow) const -> float
    { return InRow == 0 ? ck_procedural_action_timeline::SectionRowHeight : ck_procedural_action_timeline::LaneHeight; }
    auto EventsRow() const -> int32 { return _Timeline->_Channels.Num() + 1; }
    auto PosesRow() const -> int32 { return _Timeline->_Channels.Num() + _Timeline->_EventLanes + 1; }
    auto IsEventRow(int32 InRow) const -> bool { return InRow >= EventsRow() && InRow < PosesRow(); }
    auto RowAt(float InY) const -> int32
    {
        for (auto Row = 0; Row < RowCount(); ++Row)
        {
            if (InY >= RowTop(Row) && InY < RowTop(Row) + RowHeight(Row))
            { return Row; }
        }
        return INDEX_NONE;
    }
    auto ToX(const FGeometry& InGeometry, double InSeconds) const -> float
    {
        const auto View = _Timeline->Get_ViewRange();
        const auto Width = InGeometry.GetLocalSize().X - ck_procedural_action_timeline::Gutter;
        return ck_procedural_action_timeline::Gutter
            + static_cast<float>((InSeconds - View.GetLowerBoundValue()) / FMath::Max(View.Size<double>(), 1.0e-6) * Width);
    }
    auto ToSeconds(const FGeometry& InGeometry, float InX) const -> double
    {
        const auto View = _Timeline->Get_ViewRange();
        const auto Width = FMath::Max(InGeometry.GetLocalSize().X - ck_procedural_action_timeline::Gutter, 1.0f);
        return View.GetLowerBoundValue() + (InX - ck_procedural_action_timeline::Gutter) / Width * View.Size<double>();
    }
    auto HitTest(const FGeometry& InGeometry, const FVector2f& InLocal, int32& OutIndex) const -> EHit;
    auto Click() -> void;
    auto Commit() -> void;
    auto Get_DragSeconds(int32 InIndex, EHit InKind, double InFallback) const -> double
    { return _Dragging && _Hit == InKind && _HitIndex == InIndex ? _DragSeconds : InFallback; }
    // How far a dragged window has moved, snapped on its start and kept inside the clip.
    auto Get_WindowShift(int32 InIndex) const -> double
    {
        if (NOT _Dragging || _Hit != EHit::WindowBody || _HitIndex != InIndex)
        { return 0.0; }
        const auto& Event = _Timeline->_Events[InIndex];
        const auto Start = _Timeline->Snap(Event.Start + _DragSeconds - _PressSeconds);
        return FMath::Clamp(Start - Event.Start, -Event.Start, _Timeline->Get_Total() - Event.End);
    }

    SCkProceduralAction_Timeline* _Timeline = nullptr;
    EHit _Hit = EHit::None;
    int32 _HitIndex = INDEX_NONE;
    FVector2f _PressLocal = FVector2f::ZeroVector;
    double _PressSeconds = 0.0;
    double _DragSeconds = 0.0;
    bool _Dragging = false;
};

auto
    SCkProceduralAction_TimelineLanes::
    HitTest(
        const FGeometry& InGeometry,
        const FVector2f& InLocal,
        int32& OutIndex) const
    -> EHit
{
    using namespace ck_procedural_action_timeline;
    OutIndex = INDEX_NONE;
    const auto Row = RowAt(InLocal.Y);
    if (Row == INDEX_NONE || InLocal.X < Gutter)
    { return EHit::None; }
    const auto Near = [&](double InSeconds, float InPixels) { return FMath::Abs(ToX(InGeometry, InSeconds) - InLocal.X) <= InPixels; };
    const auto& Timeline = *_Timeline;
    if (Row == 0)
    {
        for (auto Index = 0; Index < Timeline._Sections.Num(); ++Index)
        {
            const auto& Section = Timeline._Sections[Index];
            if (Near(Section.Start + Section.Duration, 4.0f))
            { OutIndex = Index; return EHit::SectionEdge; }
        }
        for (auto Index = 0; Index < Timeline._Sections.Num(); ++Index)
        {
            const auto& Section = Timeline._Sections[Index];
            if (InLocal.X >= ToX(InGeometry, Section.Start) && InLocal.X <= ToX(InGeometry, Section.Start + Section.Duration))
            { OutIndex = Index; return EHit::SectionBody; }
        }
        return EHit::Empty;
    }
    if (IsEventRow(Row))
    {
        const auto Lane = Row - EventsRow();
        for (auto Index = 0; Index < Timeline._Events.Num(); ++Index)
        {
            const auto& Event = Timeline._Events[Index];
            if (Event.Lane == Lane && NOT Event.IsWindow && Near(Event.Start, PickPixels))
            { OutIndex = Index; return EHit::Marker; }
        }
        for (auto Index = 0; Index < Timeline._Events.Num(); ++Index)
        {
            const auto& Event = Timeline._Events[Index];
            if (Event.Lane == Lane && Event.IsWindow && Near(Event.Start, PickPixels))
            { OutIndex = Index; return EHit::WindowStart; }
            if (Event.Lane == Lane && Event.IsWindow && Near(Event.End, PickPixels))
            { OutIndex = Index; return EHit::WindowEnd; }
        }
        for (auto Index = 0; Index < Timeline._Events.Num(); ++Index)
        {
            const auto& Event = Timeline._Events[Index];
            if (Event.Lane == Lane && Event.IsWindow && InLocal.X > ToX(InGeometry, Event.Start) && InLocal.X < ToX(InGeometry, Event.End))
            { OutIndex = Index; return EHit::WindowBody; }
        }
        return EHit::Empty;
    }
    if (Row == PosesRow())
    {
        for (auto Index = 0; Index < Timeline._Poses.Num(); ++Index)
        {
            const auto X = ToX(InGeometry, Timeline._Poses[Index].Seconds);
            if (InLocal.X >= X - PickPixels && InLocal.X <= X + 60.0f)
            { OutIndex = Index; return EHit::Pose; }
        }
        return EHit::Empty;
    }
    for (auto Index = 0; Index < Timeline._Keys.Num(); ++Index)
    {
        if (Timeline._Keys[Index].Row == Row && Near(Timeline._Keys[Index].Seconds, PickPixels))
        { OutIndex = Index; return EHit::Key; }
    }
    return EHit::Empty;
}

auto
    SCkProceduralAction_TimelineLanes::
    OnPaint(
        const FPaintArgs&,
        const FGeometry& InGeometry,
        const FSlateRect&,
        FSlateWindowElementList& OutElements,
        int32 InLayer,
        const FWidgetStyle&,
        bool) const
    -> int32
{
    using namespace ck_procedural_action_timeline;
    const auto& Timeline = *_Timeline;
    const auto Model = Timeline._Model.Pin();
    if (NOT Model.IsValid())
    { return InLayer; }
    const auto* White = FAppStyle::GetBrush("WhiteBrush");
    const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
    const auto Size = InGeometry.GetLocalSize();
    const auto& Selection = Model->Get_Selection();
    const auto Box = [&](int32 InAtLayer, float InX, float InY, float InWidth, float InHeight, const FLinearColor& InColor)
    {
        FSlateDrawElement::MakeBox(OutElements, InAtLayer, InGeometry.ToPaintGeometry(FVector2f{FMath::Max(InWidth, 1.0f), InHeight},
            FSlateLayoutTransform{FVector2f{InX, InY}}), White, ESlateDrawEffect::None, InColor);
    };
    const auto Text = [&](int32 InAtLayer, float InX, float InY, const FString& InText, const FLinearColor& InColor)
    {
        FSlateDrawElement::MakeText(OutElements, InAtLayer, InGeometry.ToPaintGeometry(FVector2f{200.0f, 14.0f},
            FSlateLayoutTransform{FVector2f{InX, InY}}), InText, Font, ESlateDrawEffect::None, InColor);
    };
    const auto Diamond = [&](int32 InAtLayer, float InX, float InY, float InSize, const FLinearColor& InColor)
    {
        FSlateDrawElement::MakeRotatedBox(OutElements, InAtLayer, InGeometry.ToPaintGeometry(FVector2f{InSize, InSize},
            FSlateLayoutTransform{FVector2f{InX - InSize * 0.5f, InY - InSize * 0.5f}}), White, ESlateDrawEffect::None,
            UE_HALF_PI * 0.5f, TOptional<FVector2f>{}, FSlateDrawElement::RelativeToElement, InColor);
    };
    const auto Line = [&](int32 InAtLayer, const FVector2f& InFrom, const FVector2f& InTo, const FLinearColor& InColor, float InThickness)
    {
        FSlateDrawElement::MakeLines(OutElements, InAtLayer, InGeometry.ToPaintGeometry(), TArray<FVector2f>{InFrom, InTo},
            ESlateDrawEffect::None, InColor, true, InThickness);
    };

    for (auto Row = 0; Row < RowCount(); ++Row)
    {
        Box(InLayer, 0.0f, RowTop(Row), Size.X, RowHeight(Row), Row % 2 == 0 ? CkStyle::Bg1() : CkStyle::Bg2());
        const auto Name = Row == 0 ? FString{TEXT("Sections")} : Row == EventsRow() ? FString{TEXT("Events")} : IsEventRow(Row) ? FString{}
            : Row == PosesRow() ? FString{TEXT("Poses")}
            : Timeline._Channels[Row - 1] == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName() ? FString{TEXT("Body")} : Timeline._Channels[Row - 1].ToString();
        const auto Selected = Row > 0 && Row < EventsRow() && Timeline._Channels[Row - 1] == Selection.Get_Channel();
        Text(InLayer + 1, 6.0f, RowTop(Row) + 3.0f, Name, Selected ? CkStyle::TextStrong() : CkStyle::TextDim());
    }
    const auto Bottom = RowTop(RowCount());
    const auto ClipEnd = Timeline.Get_Total();
    if (Timeline.Get_Extent() > ClipEnd + 1.0e-6)
    {
        const auto Left = ToX(InGeometry, ClipEnd);
        Box(InLayer + 1, Left, 0.0f, ToX(InGeometry, Timeline.Get_Extent()) - Left, Bottom, CkStyle::OverlayOf(CkStyle::Text(), 0.06f));
        Text(InLayer + 3, Left + 5.0f, 4.0f, TEXT("after the clip: back to the gait"), CkStyle::TextDim());
    }
    for (auto Index = 0; Index < Timeline._Sections.Num(); ++Index)
    {
        const auto& Section = Timeline._Sections[Index];
        const auto End = Get_DragSeconds(Index, EHit::SectionEdge, Section.Start + Section.Duration);
        const auto Left = ToX(InGeometry, Section.Start);
        const auto Right = ToX(InGeometry, End);
        const auto IsSelected = Section.Id == Selection.Get_PhaseId();
        Box(InLayer + 1, Left + 1.0f, 3.0f, Right - Left - 2.0f, SectionRowHeight - 6.0f, IsSelected ? CkStyle::Selection() : CkStyle::AccentDim());
        auto Shape = TArray<FVector2f>{};
        for (const auto& Point : Section.EasingShape)
        { Shape.Add(FVector2f{FMath::Lerp(Left, Right, Point.X), SectionRowHeight - 5.0f - Point.Y * (SectionRowHeight - 10.0f)}); }
        FSlateDrawElement::MakeLines(OutElements, InLayer + 2, InGeometry.ToPaintGeometry(), Shape, ESlateDrawEffect::None, CkStyle::OverlayOf(CkStyle::Text(), 0.35f), true, 1.0f);
        Text(InLayer + 3, Left + 5.0f, 4.0f, FString::Printf(TEXT("%s  %.2f s"), *Section.Id.ToString(), End - Section.Start), CkStyle::TextStrong());
        if (Section.CarriedStart)
        {
            Diamond(InLayer + 3, Left + 4.0f, SectionRowHeight * 0.5f, 9.0f, CkStyle::Text());
            Diamond(InLayer + 4, Left + 4.0f, SectionRowHeight * 0.5f, 5.0f, IsSelected ? CkStyle::Selection() : CkStyle::AccentDim());
        }
        Line(InLayer + 2, FVector2f{Right, 0.0f}, FVector2f{Right, Bottom}, CkStyle::BorderStrong(), 1.0f);
    }
    for (auto Index = 0; Index < Timeline._Keys.Num(); ++Index)
    {
        const auto& Key = Timeline._Keys[Index];
        const auto IsSelected = Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Key && Selection.Get_PhaseId() == Key.PhaseId
            && Selection.Get_Channel() == Key.Channel && ck::procedural_action_editor::IsSamePoseTime(Selection.Get_KeyTime().Get_Value(), Key.KeyTime);
        Diamond(InLayer + 3, ToX(InGeometry, Get_DragSeconds(Index, EHit::Key, Key.Seconds)), RowTop(Key.Row) + LaneHeight * 0.5f, 9.0f,
            IsSelected ? CkStyle::Selection() : CkStyle::Text());
    }
    for (auto Index = 0; Index < Timeline._Events.Num(); ++Index)
    {
        const auto& Event = Timeline._Events[Index];
        const auto EventsTop = RowTop(EventsRow() + Event.Lane);
        const auto IsSelected = Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Event && Selection.Get_EventId() == Event.Id;
        const auto Color = IsSelected ? CkStyle::Selection() : CkStyle::Warn();
        if (NOT Event.IsWindow)
        {
            const auto X = ToX(InGeometry, Get_DragSeconds(Index, EHit::Marker, Event.Start));
            Diamond(InLayer + 3, X, EventsTop + LaneHeight * 0.5f, 9.0f, Color);
            Text(InLayer + 3, X + 7.0f, EventsTop + 3.0f, Event.Id.ToString(), CkStyle::TextDim());
            continue;
        }
        const auto Shift = Get_WindowShift(Index);
        const auto Left = ToX(InGeometry, Get_DragSeconds(Index, EHit::WindowStart, Event.Start + Shift));
        const auto Right = ToX(InGeometry, Get_DragSeconds(Index, EHit::WindowEnd, Event.End + Shift));
        Box(InLayer + 2, Left, EventsTop + 4.0f, Right - Left, LaneHeight - 8.0f, IsSelected ? CkStyle::Selection() : CkStyle::InfoDim());
        Text(InLayer + 3, Left + 4.0f, EventsTop + 3.0f, Event.Id.ToString(), CkStyle::Text());
    }
    // Poses at one time (a section's end and the next one's start) share a tick and a label; a label stops before the next tick.
    const auto PosesTop = RowTop(PosesRow());
    auto Ordered = TArray<const SCkProceduralAction_Timeline::FPose*>{};
    for (const auto& Pose : Timeline._Poses)
    { Ordered.Add(&Pose); }
    Ordered.Sort([](const SCkProceduralAction_Timeline::FPose& InA, const SCkProceduralAction_Timeline::FPose& InB) { return InA.Seconds < InB.Seconds; });
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    for (auto Index = 0; Index < Ordered.Num();)
    {
        const auto X = ToX(InGeometry, Ordered[Index]->Seconds);
        auto Label = FString{};
        auto IsSelected = false;
        auto Next = Index;
        for (; Next < Ordered.Num() && FMath::Abs(ToX(InGeometry, Ordered[Next]->Seconds) - X) < 1.0f; ++Next)
        {
            Label += (Label.IsEmpty() ? FString{} : FString{TEXT(" / ")}) + Ordered[Next]->Label;
            IsSelected |= Selection.Get_PoseId() == Ordered[Next]->Id;
        }
        const auto Room = (Next < Ordered.Num() ? ToX(InGeometry, Ordered[Next]->Seconds) : Size.X) - X - 8.0f;
        if (FontMeasure->Measure(Label, Font).X > Room)
        {
            const auto Last = FontMeasure->FindLastWholeCharacterIndexBeforeOffset(Label, Font, FMath::Max(0, FMath::FloorToInt(Room) - 8));
            Label = Last >= 0 ? Label.Left(Last + 1) + TEXT("...") : FString{};
        }
        Line(InLayer + 2, FVector2f{X, PosesTop + 2.0f}, FVector2f{X, PosesTop + LaneHeight - 2.0f}, IsSelected ? CkStyle::Selection() : CkStyle::Ok(), 2.0f);
        Text(InLayer + 3, X + 4.0f, PosesTop + 3.0f, Label, IsSelected ? CkStyle::TextStrong() : CkStyle::Text());
        Index = Next;
    }
    const auto PlayheadX = ToX(InGeometry, Model->Get_Time().Get_Seconds());
    Line(InLayer + 5, FVector2f{PlayheadX, 0.0f}, FVector2f{PlayheadX, Bottom}, CkStyle::Err(), 1.5f);
    return InLayer + 6;
}

auto
    SCkProceduralAction_TimelineLanes::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent)
    -> FReply
{
    const FVector2f Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    if (InEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        const auto& OnAddEvent = _Timeline->_OnAddEvent;
        const auto& OnDelete = _Timeline->_OnDelete;
        if (NOT OnAddEvent)
        { return FReply::Unhandled(); }
        auto Menu = FMenuBuilder{true, nullptr};
        _Hit = HitTest(InGeometry, Local, _HitIndex);
        if (OnDelete && _Hit != EHit::None && _Hit != EHit::Empty)
        {
            Click();
            const auto& Events = _Timeline->_Events;
            const auto What = _Hit == EHit::SectionBody || _Hit == EHit::SectionEdge
                ? FText::Format(LOCTEXT("DeleteSection", "Delete section {0}"), FText::FromName(_Timeline->_Sections[_HitIndex].Id))
                : _Hit == EHit::Marker
                    ? FText::Format(LOCTEXT("DeleteMarker", "Delete marker {0}"), FText::FromName(Events[_HitIndex].Id))
                    : _Hit == EHit::Key ? LOCTEXT("DeleteKey", "Delete key")
                        : _Hit == EHit::Pose ? LOCTEXT("DeletePose", "Delete pose")
                            : FText::Format(LOCTEXT("DeleteWindow", "Delete window {0}"), FText::FromName(Events[_HitIndex].Id));
            Menu.AddMenuEntry(What, FText::GetEmpty(), FSlateIcon{}, FUIAction{FExecuteAction::CreateLambda([OnDelete]() { OnDelete(); })});
            Menu.AddSeparator();
        }
        _Hit = EHit::None;
        Menu.AddMenuEntry(LOCTEXT("AddMarker", "Add marker at playhead"), FText::GetEmpty(), FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([OnAddEvent]() { OnAddEvent(false); })});
        Menu.AddMenuEntry(LOCTEXT("AddWindow", "Add window from playhead"), FText::GetEmpty(), FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([OnAddEvent]() { OnAddEvent(true); })});
        FSlateApplication::Get().PushMenu(SharedThis(this), InEvent.GetEventPath() != nullptr ? *InEvent.GetEventPath() : FWidgetPath{},
            Menu.MakeWidget(), InEvent.GetScreenSpacePosition(),
            FPopupTransitionEffect{FPopupTransitionEffect::ContextMenu});
        return FReply::Handled();
    }
    if (InEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }
    _Hit = HitTest(InGeometry, Local, _HitIndex);
    if (_Hit == EHit::None)
    { return FReply::Unhandled(); }
    _PressLocal = Local;
    _Dragging = false;
    _DragSeconds = ToSeconds(InGeometry, Local.X);
    _PressSeconds = _DragSeconds;
    if (_Hit == EHit::Empty)
    {
        if (const auto Model = _Timeline->_Model.Pin())
        { Model->SetTime(FCk_Time{_Timeline->Snap(_DragSeconds)}); }
    }
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

auto
    SCkProceduralAction_TimelineLanes::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent)
    -> FReply
{
    if (NOT HasMouseCapture())
    { return FReply::Unhandled(); }
    const FVector2f Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    _Dragging |= FVector2f::Distance(Local, _PressLocal) > ck_procedural_action_timeline::DragPixels;
    if (NOT _Dragging)
    { return FReply::Handled(); }
    auto Seconds = _Timeline->Snap(ToSeconds(InGeometry, Local.X));
    if (_Hit == EHit::Key && _Timeline->_Keys.IsValidIndex(_HitIndex))
    {
        const auto& Key = _Timeline->_Keys[_HitIndex];
        if (const auto* Section = _Timeline->_Sections.FindByPredicate([&Key](const auto& InSection) { return InSection.Id == Key.PhaseId; }))
        { Seconds = FMath::Clamp(Seconds, Section->Start, Section->Start + Section->Duration); }
    }
    else if (_Hit == EHit::SectionEdge && _Timeline->_Sections.IsValidIndex(_HitIndex))
    { Seconds = FMath::Max(Seconds, _Timeline->_Sections[_HitIndex].Start + ck_procedural_action_timeline::FrameSeconds); }
    else if (_Hit == EHit::Empty || _Hit == EHit::SectionBody || _Hit == EHit::Pose)
    { Seconds = FMath::Clamp(Seconds, 0.0, _Timeline->Get_Extent()); }
    else
    { Seconds = FMath::Clamp(Seconds, 0.0, _Timeline->Get_Total()); }
    _DragSeconds = Seconds;
    if (_Hit == EHit::Empty || _Hit == EHit::SectionBody || _Hit == EHit::Pose)
    {
        if (const auto Model = _Timeline->_Model.Pin())
        { Model->SetTime(FCk_Time{Seconds}); }
    }
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

auto
    SCkProceduralAction_TimelineLanes::
    OnMouseButtonUp(
        const FGeometry&,
        const FPointerEvent& InEvent)
    -> FReply
{
    if (NOT HasMouseCapture() || InEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }
    if (_Dragging)
    { Commit(); }
    else
    { Click(); }
    _Dragging = false;
    _Hit = EHit::None;
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCkProceduralAction_TimelineLanes::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent)
    -> FReply
{
    const FVector2f Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    const auto Pivot = ToSeconds(InGeometry, Local.X);
    const auto Scale = InEvent.GetWheelDelta() > 0.0f ? 0.9 : 1.0 / 0.9;
    const auto View = _Timeline->Get_ViewRange();
    _Timeline->_ViewRange = TRange<double>{Pivot - (Pivot - View.GetLowerBoundValue()) * Scale, Pivot + (View.GetUpperBoundValue() - Pivot) * Scale};
    _Timeline->_ViewFollowsExtent = false;
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

auto
    SCkProceduralAction_TimelineLanes::
    OnCursorQuery(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent) const
    -> FCursorReply
{
    auto Index = int32{INDEX_NONE};
    const FVector2f Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
    const auto Hit = HasMouseCapture() ? _Hit : HitTest(InGeometry, Local, Index);
    switch (Hit)
    {
        case EHit::SectionEdge:
        case EHit::WindowStart:
        case EHit::WindowEnd:
            return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
        case EHit::Key:
        case EHit::Marker:
        case EHit::WindowBody:
            return FCursorReply::Cursor(_Dragging ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
        default:
            return FCursorReply::Unhandled();
    }
}

auto
    SCkProceduralAction_TimelineLanes::
    Click()
    -> void
{
    auto& Timeline = *_Timeline;
    const auto Model = Timeline._Model.Pin();
    const auto Session = Timeline._Session.Pin();
    if (NOT Model.IsValid() || NOT Session.IsValid())
    { return; }
    // A pose stays selected across other selections; clear it so Delete acts on what was just clicked.
    if (_Hit != EHit::Pose && _Hit != EHit::Empty && _Hit != EHit::None)
    { Model->ClearPose(); }
    switch (_Hit)
    {
        case EHit::SectionBody:
        case EHit::SectionEdge:
            Model->SelectSection(Timeline._Sections[_HitIndex].Id);
            break;
        case EHit::Key:
        {
            const auto& Key = Timeline._Keys[_HitIndex];
            Model->SelectKey(Key.PhaseId, Key.Channel, FCk_FloatRange_0to1{Key.KeyTime});
            Model->SetTime(FCk_Time{Key.Seconds});
            break;
        }
        case EHit::Marker:
        case EHit::WindowStart:
        case EHit::WindowEnd:
        case EHit::WindowBody:
            Model->SelectEvent(Timeline._Events[_HitIndex].Id);
            Model->SetTime(FCk_Time{_Hit == EHit::WindowEnd ? Timeline._Events[_HitIndex].End : Timeline._Events[_HitIndex].Start});
            break;
        case EHit::Pose:
        {
            const auto& Pose = Timeline._Poses[_HitIndex];
            Model->SelectPose(Pose.Id, Pose.PhaseId);
            Model->SetTime(FCk_Time{Pose.Seconds});
            break;
        }
        default:
            break;
    }
}

auto
    SCkProceduralAction_TimelineLanes::
    Commit()
    -> void
{
    auto& Timeline = *_Timeline;
    const auto Session = Timeline._Session.Pin();
    const auto Model = Timeline._Model.Pin();
    auto* Clip = Session.IsValid() ? Session->GetClip() : nullptr;
    if (Clip == nullptr || NOT Model.IsValid())
    { return; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    auto Error = FString{};
    auto Done = false;
    auto What = FString{};
    switch (_Hit)
    {
        case EHit::SectionEdge:
        {
            const auto& Section = Timeline._Sections[_HitIndex];
            Done = Commands.SetPhaseDuration(Section.Id, FCk_Time{_DragSeconds - Section.Start}, Error);
            What = FString::Printf(TEXT("%s lasts %.2f s."), *Section.Id.ToString(), _DragSeconds - Section.Start);
            break;
        }
        case EHit::Key:
        {
            const auto& Key = Timeline._Keys[_HitIndex];
            const auto* Timing = Session->GetTiming().FindByPredicate([&Key](const ck::procedural_action::FCompiledPhase& InPhase)
                { return InPhase.Get_Id() == Key.PhaseId; });
            if (Timing == nullptr)
            { return; }
            const auto Raw = (_DragSeconds - Timing->Get_Start().Get_Seconds()) / Timing->Get_Duration().Get_Seconds();
            const auto To = ck::procedural_action::EvaluateProgress(*Timing, FMath::Clamp(Raw, 0.0, 1.0));
            if (ck::procedural_action_editor::IsSamePoseTime(To, Key.KeyTime))
            { return; }
            Done = Commands.MoveKey(Key.PhaseId, Key.Channel, Key.KeyTime, To, Error);
            What = TEXT("Moved the key.");
            if (Done)
            { Model->SelectKey(Key.PhaseId, Key.Channel, FCk_FloatRange_0to1{To}); }
            break;
        }
        case EHit::WindowBody:
        {
            const auto& Event = Timeline._Events[_HitIndex];
            const auto Shift = Get_WindowShift(_HitIndex);
            auto StartPhase = FName{};
            auto EndPhase = FName{};
            auto StartFraction = 0.0;
            auto EndFraction = 0.0;
            if (FMath::IsNearlyZero(Shift) || NOT Timeline.Get_SectionAt(Event.Start + Shift, StartPhase, StartFraction)
                || NOT Timeline.Get_SectionAt(Event.End + Shift, EndPhase, EndFraction))
            { return; }
            Done = Commands.MoveWindow(Event.Id, FCk_ProceduralAction_TimeRef{StartPhase, FCk_FloatRange_0to1{StartFraction}},
                FCk_ProceduralAction_TimeRef{EndPhase, FCk_FloatRange_0to1{EndFraction}}, Error);
            What = FString::Printf(TEXT("Moved %s."), *Event.Id.ToString());
            break;
        }
        case EHit::Marker:
        case EHit::WindowStart:
        case EHit::WindowEnd:
        {
            auto PhaseId = FName{};
            auto Fraction = 0.0;
            if (NOT Timeline.Get_SectionAt(_DragSeconds, PhaseId, Fraction))
            { return; }
            const auto Id = Timeline._Events[_HitIndex].Id;
            constexpr auto StartEdge = true;
            Done = _Hit == EHit::Marker ? Commands.MoveMarker(Id, PhaseId, Fraction, Error)
                : Commands.MoveWindowEdge(Id, _Hit == EHit::WindowStart ? StartEdge : NOT StartEdge, PhaseId, Fraction, Error);
            What = FString::Printf(TEXT("Moved %s."), *Id.ToString());
            break;
        }
        default:
            return;
    }
    if (Done)
    { Model->NotifyContentChanged(); }
    Timeline.Report(Done, What, Error);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAction_Timeline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _Model = InArgs._Model;
    _OnResult = InArgs._OnResult;
    _OnAddEvent = InArgs._OnAddEvent;
    _OnDelete = InArgs._OnDelete;
    _Length = InArgs._Length;
    SetTag(FName{TEXT("CkActionEditor.Timeline")});
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SBox).WidthOverride(ck_procedural_action_timeline::Gutter)]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SSimpleTimeSlider)
                .Tag(FName{TEXT("CkActionEditor.Ruler")})
                .ViewRange_Lambda([this]() { return Get_ViewRange(); })
                .ClampRange_Lambda([this]() { return TRange<double>{0.0, Get_Extent()}; })
                .ScrubPosition_Lambda([this]() -> double
                {
                    const auto Model = _Model.Pin();
                    return Model.IsValid() ? Model->Get_Time().Get_Seconds() : 0.0;
                })
                .OnScrubPositionChanged_Lambda([this](double InSeconds, bool)
                {
                    if (const auto Model = _Model.Pin())
                    { Model->SetTime(FCk_Time{Snap(FMath::Clamp(InSeconds, 0.0, Get_Extent()))}); }
                })
                .OnViewRangeChanged_Lambda([this](TRange<double> InRange)
                {
                    _ViewRange = InRange;
                    _ViewFollowsExtent = false;
                })
            ]
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [SAssignNew(_Lanes, SCkProceduralAction_TimelineLanes, this)]
        ]
    ];
    Refresh();
}

auto
    SCkProceduralAction_Timeline::
    Get_Total() const
    -> double
{ return _Sections.IsEmpty() ? 0.0 : _Sections.Last().Start + _Sections.Last().Duration; }

auto
    SCkProceduralAction_Timeline::
    Get_Extent() const
    -> double
{ return FMath::Max(Get_Total(), _Length ? _Length() : 0.0); }

auto
    SCkProceduralAction_Timeline::
    Get_ViewRange() const
    -> TRange<double>
{
    const auto Extent = Get_Extent();
    if (NOT _ViewFollowsExtent || Extent <= 0.0)
    { return _ViewRange; }
    return TRange<double>{-0.02 * Extent, Extent * 1.04};
}

auto
    SCkProceduralAction_Timeline::
    Snap(
        double InSeconds) const
    -> double
{ return FMath::RoundToDouble(InSeconds / ck_procedural_action_timeline::FrameSeconds) * ck_procedural_action_timeline::FrameSeconds; }

auto
    SCkProceduralAction_Timeline::
    Get_SectionAt(
        double InSeconds,
        FName& OutPhaseId,
        double& OutFraction) const
    -> bool
{
    for (const auto& Section : _Sections)
    {
        if (InSeconds <= Section.Start + Section.Duration + 1.0e-9 || &Section == &_Sections.Last())
        {
            OutPhaseId = Section.Id;
            OutFraction = FMath::Clamp((InSeconds - Section.Start) / Section.Duration, 0.0, 1.0);
            return true;
        }
    }
    return false;
}

auto
    SCkProceduralAction_Timeline::
    Report(
        bool InSucceeded,
        const FString& InSuccess,
        const FString& InError)
    -> void
{
    if (_OnResult)
    { _OnResult(InSucceeded, InSucceeded ? InSuccess : InError); }
}

auto
    SCkProceduralAction_Timeline::
    Refresh()
    -> void
{
    const auto Session = _Session.Pin();
    const auto* Clip = Session.IsValid() ? Session->GetClip() : nullptr;
    const auto PreviousTotal = Get_Total();
    _Sections.Reset();
    _Channels.Reset();
    _Keys.Reset();
    _Events.Reset();
    _Poses.Reset();
    if (Clip == nullptr)
    { return; }
    for (const auto& Timing : Session->GetTiming())
    {
        auto Section = FSection{Timing.Get_Id(), Timing.Get_Start().Get_Seconds(), Timing.Get_Duration().Get_Seconds(), Timing.Get_Easing()};
        for (auto Step = 0; Step <= 16; ++Step)
        {
            const auto Raw = Step / 16.0;
            Section.EasingShape.Add(FVector2f{static_cast<float>(Raw), static_cast<float>(ck::procedural_action::EvaluateProgress(Timing, Raw))});
        }
        _Sections.Add(MoveTemp(Section));
    }
    if (Clip->Get_BodyChannel().Get_Enabled())
    { _Channels.Add(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()); }
    for (const auto& Slot : Clip->Get_Slots())
    { _Channels.Add(Slot.Get_Id()); }
    const auto AddKey = [this, &Session](FName InPhaseId, FName InChannel, double InKeyTime)
    {
        auto Elapsed = FCk_Time{};
        const auto Row = _Channels.IndexOfByKey(InChannel) + 1;
        const auto Known = _Keys.ContainsByPredicate([&](const FKey& InKey)
            { return InKey.PhaseId == InPhaseId && InKey.Channel == InChannel && ck::procedural_action_editor::IsSamePoseTime(InKey.KeyTime, InKeyTime); });
        if (Row > 0 && NOT Known && Session->KeyToElapsed(InPhaseId, FCk_FloatRange_0to1{InKeyTime}, Elapsed))
        { _Keys.Add(FKey{InPhaseId, InChannel, InKeyTime, Elapsed.Get_Seconds(), Row}); }
    };
    for (const auto& Phase : Clip->Get_Phases())
    {
        auto* Section = _Sections.FindByPredicate([&Phase](const FSection& InSection) { return InSection.Id == Phase.Get_Id(); });
        const auto AtStart = [](const auto& InKey) { return ck::procedural_action_editor::IsSamePoseTime(InKey.Get_Time().Get_Value(), 0.0); };
        if (Section != nullptr)
        {
            Section->CarriedStart = NOT Phase.Get_BodyKeys().ContainsByPredicate(AtStart)
                && NOT Phase.Get_LimbTracks().ContainsByPredicate([&AtStart](const FCk_ProceduralAction_LimbTrack& InTrack)
                    { return InTrack.Get_PositionKeys().ContainsByPredicate(AtStart); });
        }
        if (Clip->Get_BodyChannel().Get_Enabled())
        {
            for (const auto& Key : Phase.Get_BodyKeys())
            { AddKey(Phase.Get_Id(), FCk_ProceduralAction_PoseGroup::Get_BodyChannelName(), Key.Get_Time().Get_Value()); }
        }
        for (const auto& Track : Phase.Get_LimbTracks())
        {
            for (const auto& Key : Track.Get_PositionKeys())
            { AddKey(Phase.Get_Id(), Track.Get_Slot(), Key.Get_Time().Get_Value()); }
            for (const auto& Key : Track.Get_OrientationKeys())
            { AddKey(Phase.Get_Id(), Track.Get_Slot(), Key.Get_Time().Get_Value()); }
            for (const auto& Key : Track.Get_BendKeys())
            { AddKey(Phase.Get_Id(), Track.Get_Slot(), Key.Get_Time().Get_Value()); }
        }
    }
    const auto ToSeconds = [this](const FCk_ProceduralAction_TimeRef& InAt) -> double
    {
        const auto* Section = _Sections.FindByPredicate([&InAt](const FSection& InSection) { return InSection.Id == InAt.Get_PhaseId(); });
        return Section == nullptr ? 0.0 : Section->Start + Section->Duration * InAt.Get_Time().Get_Value();
    };
    for (const auto& Marker : Clip->Get_Markers())
    { _Events.Add(FEvent{Marker.Get_Id(), false, ToSeconds(Marker.Get_At()), ToSeconds(Marker.Get_At())}); }
    for (const auto& Window : Clip->Get_Windows())
    { _Events.Add(FEvent{Window.Get_Id(), true, ToSeconds(Window.Get_Start()), ToSeconds(Window.Get_End())}); }
    // Overlapping events stack on lanes of their own, as notifies do in a montage; touching windows share one.
    auto Order = TArray<int32>{};
    for (auto Index = 0; Index < _Events.Num(); ++Index)
    { Order.Add(Index); }
    Order.Sort([this](int32 InA, int32 InB) { return _Events[InA].Start < _Events[InB].Start; });
    auto LaneEnds = TArray<double>{};
    for (const auto Index : Order)
    {
        auto& Event = _Events[Index];
        auto Lane = LaneEnds.IndexOfByPredicate([&Event](double InEnd)
            { return InEnd < Event.Start - 1.0e-6 || (Event.IsWindow && InEnd <= Event.Start + 1.0e-6); });
        if (Lane == INDEX_NONE)
        { Lane = LaneEnds.Add(0.0); }
        LaneEnds[Lane] = Event.End;
        Event.Lane = Lane;
    }
    _EventLanes = FMath::Max(1, LaneEnds.Num());
    for (const auto& View : Session->GetPoseGroups())
    {
        const auto& Group = View.Get_Group();
        auto Elapsed = FCk_Time{};
        if (Session->KeyToElapsed(Group.Get_PhaseId(), Group.Get_Time(), Elapsed))
        {
            _Poses.Add(FPose{Group.Get_Id(), Group.Get_PhaseId(),
                Group.Get_Label().ToString() + (View.Get_IsBroken() ? TEXT(" (broken)") : TEXT("")), Elapsed.Get_Seconds()});
        }
    }
    const auto Total = Get_Total();
    if (NOT FMath::IsNearlyEqual(Total, PreviousTotal) && Total > 0.0)
    { _ViewFollowsExtent = true; }
    if (_Lanes.IsValid())
    { _Lanes->Invalidate(EInvalidateWidgetReason::Layout); }
}

#undef LOCTEXT_NAMESPACE
