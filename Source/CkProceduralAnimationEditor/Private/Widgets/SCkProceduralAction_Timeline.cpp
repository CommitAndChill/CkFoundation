#include "Widgets/SCkProceduralAction_Timeline.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionTimeline"

auto
    SCkProceduralAction_Timeline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _OnChanged = InArgs._OnChanged;
    _OnSelectionChanged = InArgs._OnSelectionChanged;

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(STextBlock).Text(LOCTEXT("TimelineTitle", "Phases, slots, and event lanes"))]
        + SVerticalBox::Slot().AutoHeight()
        [SAssignNew(_Rows, SVerticalBox)]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(STextBlock).Text(LOCTEXT("AuthoringToolsTitle", "Authoring tools: create, key, and bind"))]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_NewPhaseId, SEditableTextBox).HintText(LOCTEXT("PhaseName", "New phase ID"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SNumericEntryBox<double>)
                .MinValue(0.001).MaxValue(60.0).MinSliderValue(0.001).MaxSliderValue(5.0)
                .Value_Lambda([this]() -> TOptional<double> { return _NewPhaseDuration; })
                .OnValueChanged_Lambda([this](double InValue) { _NewPhaseDuration = InValue; })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("AddPhase", "Add phase"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("AddPhase"))); })]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_NewSlotId, SEditableTextBox).HintText(LOCTEXT("SlotName", "New limb slot ID"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("AddSlot", "Add slot"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("AddSlot"))); })]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_NewEventId, SEditableTextBox).HintText(LOCTEXT("EventName", "New marker ID"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("AddMarker", "Marker at cursor"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("AddMarker"))); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("AddWindow", "Window from cursor"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("AddWindow"))); })]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("AddTrack", "Add selected slot track"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("AddTrack"))); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("Body0", "Body key 0"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("BodyStart"))); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("Body1", "Body key 1"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("BodyEnd"))); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("Position0", "Endpoint key 0"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("PositionStart"))); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("Position1", "Endpoint key 1"))
                .OnClicked_Lambda([this]() { return Apply(FName(TEXT("PositionEnd"))); })]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_RenamedSlotId, SEditableTextBox)
                .HintText(LOCTEXT("RenameSlotId", "New ID for selected slot"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("RenameSlot", "Rename selected slot"))
                .ToolTipText(LOCTEXT("RenameSlotTip", "Rename the clip slot, its tracks, and the selected preview binding together. Other preview configs and runtime consumers must be rebound explicitly."))
                .OnClicked_Lambda([this]() { return ApplyRenameSlot(); })]]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(SCheckBox)
            .ToolTipText(LOCTEXT("SnapEventsTip", "Snap a dragged marker or window edge to phase time 0 or 1 within 0.02."))
            .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                { _SnapEventEdges = InState == ECheckBoxState::Checked; })
            [SNew(STextBlock).Text(LOCTEXT("SnapEvents", "Snap event edges to phase boundaries"))]]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(STextBlock).Text(LOCTEXT("TrackSpace", "Selected track space:"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("BodyLocal", "Body local"))
                .OnClicked_Lambda([this]()
                    { return ApplyTrackSpace(ECk_ProceduralAction_TrackSpace::BodyLocal); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("RootAtStart", "Root at start"))
                .OnClicked_Lambda([this]()
                    { return ApplyTrackSpace(ECk_ProceduralAction_TrackSpace::RootAtStart); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("LimbRestNormalized", "Rest normalized"))
                .OnClicked_Lambda([this]()
                    { return ApplyTrackSpace(ECk_ProceduralAction_TrackSpace::LimbRestNormalized); })]]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_MirrorDestination, SEditableTextBox)
                .HintText(LOCTEXT("MirrorDestination", "Mirror selected slot to declared destination slot"))]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("MirrorX", "Mirror X"))
                .OnClicked_Lambda([this]() { return ApplyMirror(EAxis::X); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("MirrorY", "Mirror Y"))
                .OnClicked_Lambda([this]() { return ApplyMirror(EAxis::Y); })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("MirrorZ", "Mirror Z"))
                .OnClicked_Lambda([this]() { return ApplyMirror(EAxis::Z); })]]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SAssignNew(_RetargetPicker, SComboButton)
                .OnGetMenuContent_Lambda([this]() { return BuildLimbPicker(); })
                .ButtonContent()
                [SNew(STextBlock).Text_Lambda([this]() -> FText
                {
                    return _PickedRetargetLimb.IsNone()
                        ? LOCTEXT("PickRetargetLimb", "Choose preview limb")
                        : FText::FromName(_PickedRetargetLimb);
                })]]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("RetargetSlot", "Bind selected slot"))
                .ToolTipText(LOCTEXT("RetargetTip", "Update the separate preview config mapping. Set a track's coordinate mode before keying; Solver validates actual reach and acceptance."))
                .OnClicked_Lambda([this]() { return ApplyRetarget(); })]]
        + SVerticalBox::Slot().AutoHeight().Padding(3.0f)
        [SAssignNew(_Message, STextBlock).AutoWrapText(true).Text_Lambda([this]() { return GetErrorText(); })]
        ]
    ];
    _NewPhaseId->SetTag(FName{TEXT("CkActionEditorNewPhaseId")});
    _NewSlotId->SetTag(FName{TEXT("CkActionEditorNewSlotId")});
    _RenamedSlotId->SetTag(FName{TEXT("CkActionEditorRenamedSlotId")});
    _NewEventId->SetTag(FName{TEXT("CkActionEditorNewEventId")});
    _MirrorDestination->SetTag(FName{TEXT("CkActionEditorMirrorDestination")});
    _Message->SetTag(FName{TEXT("CkActionEditorTimelineMessage")});
    RebuildRows();
}

auto
    SCkProceduralAction_Timeline::
    Refresh()
    -> void
{
    _PickedRetargetLimb = NAME_None;
    RebuildRows();
}

auto
    SCkProceduralAction_Timeline::
    SelectTrack(
        FName InPhaseId,
        FName InSlotId)
    -> void
{
    _SelectedPhase = InPhaseId;
    _SelectedSlot = InSlotId;
    _PickedRetargetLimb = NAME_None;
    _OnSelectionChanged.ExecuteIfBound();
    RebuildRows();
}

auto
    SCkProceduralAction_Timeline::
    GetErrorText() const
    -> FText
{
    return _Error.IsEmpty()
        ? LOCTEXT("TimelineHint", "Select a phase and slot below. Edit key values in Clip Details.")
        : FText::FromString(_Error);
}

auto
    SCkProceduralAction_Timeline::
    GetCurrentPhaseTime() const
    -> double
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || NOT Session->IsCompiled())
    { return 0.0; }
    const auto* Phase = Session->GetCompiledClip().Get_Phases().FindByPredicate(
        [this](const ck::procedural_action::FCompiledPhase& InPhase)
        { return InPhase.Get_Id() == _SelectedPhase; });
    if (Phase == nullptr || Phase->Get_Duration().Get_Seconds() <= 0.0)
    { return 0.0; }
    return FMath::Clamp((Session->GetTime().Get_Seconds() - Phase->Get_Start().Get_Seconds())
        / Phase->Get_Duration().Get_Seconds(), 0.0, 1.0);
}

auto
    SCkProceduralAction_Timeline::
    Apply(
        FName InOperation)
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    auto Succeeded = false;
    _Error.Reset();
    if (InOperation == FName(TEXT("AddPhase")))
    {
        Succeeded = Commands.AddPhase(FName{*_NewPhaseId->GetText().ToString()},
            FCk_Time{_NewPhaseDuration}, _Error);
    }
    else if (InOperation == FName(TEXT("AddSlot")))
    {
        Succeeded = Commands.AddSlot(FName{*_NewSlotId->GetText().ToString()}, _Error);
    }
    else if (InOperation == FName(TEXT("AddMarker")))
    {
        Succeeded = Commands.AddMarker(FName{*_NewEventId->GetText().ToString()},
            _SelectedPhase, GetCurrentPhaseTime(), _Error);
    }
    else if (InOperation == FName(TEXT("AddWindow")))
    {
        const auto Start = GetCurrentPhaseTime();
        Succeeded = Commands.AddWindow(FName{*_NewEventId->GetText().ToString()},
            _SelectedPhase, Start, _SelectedPhase, FMath::Min(1.0, Start + 0.1), _Error);
    }
    else if (InOperation == FName(TEXT("AddTrack")))
    {
        Succeeded = Commands.AddLimbTrack(_SelectedPhase, _SelectedSlot, _Error);
    }
    else if (InOperation == FName(TEXT("BodyStart")) || InOperation == FName(TEXT("BodyEnd")))
    {
        Succeeded = Commands.AddBodyKey(_SelectedPhase,
            InOperation == FName(TEXT("BodyStart")) ? 0.0 : 1.0,
            FTransform::Identity, _Error);
    }
    else if (InOperation == FName(TEXT("PositionStart")) || InOperation == FName(TEXT("PositionEnd")))
    {
        Succeeded = Commands.AddPositionKey(_SelectedPhase, _SelectedSlot,
            InOperation == FName(TEXT("PositionStart")) ? 0.0 : 1.0,
            FVector::ZeroVector, _Error);
    }
    if (Succeeded)
    {
        if (InOperation == FName(TEXT("AddPhase")))
        { _SelectedPhase = FName{*_NewPhaseId->GetText().ToString()}; }
        if (InOperation == FName(TEXT("AddSlot")))
        { _SelectedSlot = FName{*_NewSlotId->GetText().ToString()}; _PickedRetargetLimb = NAME_None; }
        _OnChanged.ExecuteIfBound();
        _OnSelectionChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    ApplyRenameSlot()
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    const auto NewId = FName{*_RenamedSlotId->GetText().ToString()};
    _Error.Reset();
    if (Commands.RenameSlot(Session->GetPreviewConfig(), _SelectedSlot, NewId, _Error))
    {
        _SelectedSlot = NewId;
        _PickedRetargetLimb = NAME_None;
        _OnChanged.ExecuteIfBound();
        _OnSelectionChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    ApplyTrackSpace(
        ECk_ProceduralAction_TrackSpace InSpace)
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    _Error.Reset();
    if (Commands.SetTrackSpace(_SelectedPhase, _SelectedSlot, InSpace, _Error))
    {
        _OnChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    ApplyMirror(
        EAxis::Type InPlaneNormal)
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    _Error.Reset();
    if (Commands.MirrorSlot(_SelectedSlot,
        FName{*_MirrorDestination->GetText().ToString()}, InPlaneNormal, _Error))
    {
        _OnChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    ApplyRetarget()
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    _Error.Reset();
    if (Commands.RetargetSlot(Session->GetPreviewConfig(), _SelectedSlot,
        _PickedRetargetLimb, _Error))
    {
        _OnChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    BuildLimbPicker()
    -> TSharedRef<SWidget>
{
    auto Rows = SNew(SVerticalBox);
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
    if (Config == nullptr)
    {
        Rows->AddSlot().AutoHeight()
        [SNew(STextBlock).Text(LOCTEXT("NoPreviewForPicker", "Select a preview config first."))];
    }
    else
    {
        auto HasChoices = false;
        const auto AddChoice = [this, Config, &Rows, &HasChoices](FName InId, int32 InLinkCount,
            double InReachCm, FString InCapability)
        {
            HasChoices = true;
            const auto HasVisualBinding = Config->Get_LimbBindings().ContainsByPredicate(
                [InId](const auto& InBinding) { return InBinding.Get_LimbId() == InId; });
            const auto UsedByOtherSlot = Config->Get_SlotBindings().ContainsByPredicate(
                [this, InId](const auto& InBinding)
                { return InBinding.Get_Slot() != _SelectedSlot && InBinding.Get_LimbId() == InId; });
            const auto Label = FString::Printf(TEXT("%s | %d links | %.1f cm reach | %s%s"),
                *InId.ToString(), InLinkCount, InReachCm, *InCapability,
                HasVisualBinding ? UsedByOtherSlot ? TEXT(" | bound to another slot") : TEXT("")
                    : TEXT(" | missing visual binding"));
            Rows->AddSlot().AutoHeight().Padding(2.0f)
            [SNew(SButton).Text(FText::FromString(Label))
                .IsEnabled(HasVisualBinding && NOT UsedByOtherSlot && NOT InId.IsNone())
                .OnClicked_Lambda([this, InId]() -> FReply
                {
                    _PickedRetargetLimb = InId;
                    if (_RetargetPicker.IsValid())
                    { _RetargetPicker->SetIsOpen(false); }
                    return FReply::Handled();
                })];
        };
        if (Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (const auto* Rig = Config->Get_WalkingRig().Get())
            {
                for (const auto& Leg : Rig->Get_Legs())
                {
                    double ReachCm = 0.0;
                    for (const auto Length : Leg.Get_Chain().Get_SegmentLengths())
                    { ReachCm += Length; }
                    AddChoice(Leg.Get_Id(), Leg.Get_Chain().Get_SegmentLengths().Num(),
                        ReachCm, TEXT("walking / borrowed"));
                }
            }
        }
        else
        {
            for (const auto& Limb : Config->Get_Layout().Get_Limbs())
            {
                double ReachCm = 0.0;
                for (const auto Length : Limb.Get_SegmentLengths())
                { ReachCm += Length; }
                AddChoice(Limb.Get_Id(), Limb.Get_SegmentLengths().Num(), ReachCm,
                    FString::Printf(TEXT("%s / %s"),
                        *UEnum::GetValueAsString(Limb.Get_Role()),
                        *UEnum::GetValueAsString(Limb.Get_Solver())));
            }
        }
        if (NOT HasChoices)
        {
            Rows->AddSlot().AutoHeight()
            [SNew(STextBlock).Text(LOCTEXT("NoPreviewLimbs", "Preview topology has no limbs."))];
        }
    }
    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [Rows];
}

auto
    SCkProceduralAction_Timeline::
    BeginEventDrag(
        FName InId,
        FName InPhaseId,
        EDragEdge InEdge,
        double InValue)
    -> void
{
    _DraggedEvent = InId;
    _DragPhase = InPhaseId;
    _DragEdge = InEdge;
    _DragValue = InValue;
}

auto
    SCkProceduralAction_Timeline::
    ChangeEventDrag(
        float InValue)
    -> void
{
    if (_DragEdge == EDragEdge::None)
    { return; }
    _DragValue = FMath::Clamp(static_cast<double>(InValue), 0.0, 1.0);
    if (_SnapEventEdges)
    {
        if (_DragValue <= 0.02) { _DragValue = 0.0; }
        if (_DragValue >= 0.98) { _DragValue = 1.0; }
    }
}

auto
    SCkProceduralAction_Timeline::
    EndEventDrag()
    -> void
{
    const auto Session = _Session.Pin();
    if (_DragEdge == EDragEdge::None || NOT Session.IsValid() || Session->GetClip() == nullptr)
    { _DragEdge = EDragEdge::None; return; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    _Error.Reset();
    auto Succeeded = false;
    if (_DragEdge == EDragEdge::Marker)
    { Succeeded = Commands.MoveMarker(_DraggedEvent, _DragPhase, _DragValue, _Error); }
    else
    {
        Succeeded = Commands.MoveWindowEdge(_DraggedEvent, _DragEdge == EDragEdge::WindowStart,
            _DragPhase, _DragValue, _Error);
    }
    _DragEdge = EDragEdge::None;
    _DraggedEvent = NAME_None;
    if (Succeeded)
    { _OnChanged.ExecuteIfBound(); }
    RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(
        this, &SCkProceduralAction_Timeline::RebuildAfterEventDrag));
}

auto
    SCkProceduralAction_Timeline::
    RebuildAfterEventDrag(
        double InCurrentTime,
        float InDeltaTime)
    -> EActiveTimerReturnType
{
    RebuildRows();
    return EActiveTimerReturnType::Stop;
}

auto
    SCkProceduralAction_Timeline::
    GetEventValue(
        FName InId,
        EDragEdge InEdge,
        double InAuthoredValue) const
    -> float
{
    return static_cast<float>(_DraggedEvent == InId && _DragEdge == InEdge
        ? _DragValue : InAuthoredValue);
}

auto
    SCkProceduralAction_Timeline::
    ApplyPhase(
        FName InOperation,
        FName InPhaseId)
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    auto Succeeded = false;
    _Error.Reset();
    const auto Index = Session->GetClip()->Get_Phases().IndexOfByPredicate(
        [InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (InOperation == FName(TEXT("Up")))
    { Succeeded = Commands.ReorderPhase(InPhaseId, Index - 1, _Error); }
    else if (InOperation == FName(TEXT("Down")))
    { Succeeded = Commands.ReorderPhase(InPhaseId, Index + 1, _Error); }
    else if (InOperation == FName(TEXT("Remove")))
    { Succeeded = Commands.RemovePhase(InPhaseId, _Error); }
    if (Succeeded)
    {
        if (_SelectedPhase == InPhaseId && InOperation == FName(TEXT("Remove")))
        { _SelectedPhase = NAME_None; }
        _OnChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    ApplySlot(
        FName InSlotId)
    -> FReply
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    _Error.Reset();
    if (Commands.RemoveSlot(InSlotId, _Error))
    {
        if (_SelectedSlot == InSlotId)
        { _SelectedSlot = NAME_None; _PickedRetargetLimb = NAME_None; }
        _OnChanged.ExecuteIfBound();
        RebuildRows();
    }
    return FReply::Handled();
}

auto
    SCkProceduralAction_Timeline::
    RebuildRows()
    -> void
{
    const auto Session = _Session.Pin();
    if (NOT _Rows.IsValid() || NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return; }
    const auto* Clip = Session->GetClip();
    _Rows->ClearChildren();
    for (const auto& Slot : Clip->Get_Slots())
    {
        const auto Id = Slot.Get_Id();
        const auto* Preview = Session->GetPreviewConfig();
        const auto* Binding = Preview == nullptr ? nullptr
            : Preview->Get_SlotBindings().FindByPredicate([Id](const auto& InBinding)
                { return InBinding.Get_Slot() == Id; });
        const auto BindingText = Binding != nullptr
            ? FText::FromName(Binding->Get_LimbId()) : LOCTEXT("Unbound", "unbound");
        _Rows->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(FText::Format(LOCTEXT("SlotRow", "Slot {0} → {1}{2}"),
                FText::FromName(Id), BindingText,
                Id == _SelectedSlot ? LOCTEXT("SelectedSuffix", "  SELECTED") : FText::GetEmpty()))
                .OnClicked_Lambda([this, Id]()
                {
                    _SelectedSlot = Id;
                    _PickedRetargetLimb = NAME_None;
                    _OnSelectionChanged.ExecuteIfBound();
                    RebuildRows();
                    return FReply::Handled();
                })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("RemoveSlot", "Remove"))
                .OnClicked_Lambda([this, Id]() { return ApplySlot(Id); })]
        ];
    }
    for (const auto& Phase : Clip->Get_Phases())
    {
        const auto Id = Phase.Get_Id();
        auto Row = SNew(SHorizontalBox);
        Row->AddSlot().AutoWidth()
        [SNew(SButton).Text(FText::Format(LOCTEXT("PhaseRow", "Phase {0} ({1}s)"),
            FText::FromName(Id), FText::AsNumber(Phase.Get_Duration().Get_Seconds())))
            .OnClicked_Lambda([this, Id]()
            {
                _SelectedPhase = Id;
                _OnSelectionChanged.ExecuteIfBound();
                RebuildRows();
                return FReply::Handled();
            })];
        Row->AddSlot().AutoWidth()
        [SNew(STextBlock).Text(Id == _SelectedPhase ? LOCTEXT("Selected", "  SELECTED") : FText::GetEmpty())];
        Row->AddSlot().AutoWidth()
        [SNew(SButton).Text(LOCTEXT("MovePhaseUp", "↑"))
            .OnClicked_Lambda([this, Id]() { return ApplyPhase(FName(TEXT("Up")), Id); })];
        Row->AddSlot().AutoWidth()
        [SNew(SButton).Text(LOCTEXT("MovePhaseDown", "↓"))
            .OnClicked_Lambda([this, Id]() { return ApplyPhase(FName(TEXT("Down")), Id); })];
        Row->AddSlot().AutoWidth()
        [SNew(SButton).Text(LOCTEXT("RemovePhase", "Remove"))
            .OnClicked_Lambda([this, Id]() { return ApplyPhase(FName(TEXT("Remove")), Id); })];
        _Rows->AddSlot().AutoHeight().Padding(2.0f)[Row];

        if (Phase.Get_BodyKeys().Num() > 0)
        {
            _Rows->AddSlot().AutoHeight()
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("BodyLane", "    Body: {0} keys"),
                FText::AsNumber(Phase.Get_BodyKeys().Num())))];
        }
        for (const auto& Track : Phase.Get_LimbTracks())
        {
            _Rows->AddSlot().AutoHeight()
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("TrackLane", "    {0} ({1}): {2} endpoint keys"),
                FText::FromName(Track.Get_Slot()),
                FText::FromString(UEnum::GetValueAsString(Track.Get_Space())),
                FText::AsNumber(Track.Get_PositionKeys().Num())))];
        }
    }
    _Rows->AddSlot().AutoHeight()
    [SNew(SButton).Text(LOCTEXT("SelectBody", "Select body curve"))
        .OnClicked_Lambda([this]()
        {
            _SelectedSlot = NAME_None;
            _OnSelectionChanged.ExecuteIfBound();
            RebuildRows();
            return FReply::Handled();
        })];
    for (const auto& Marker : Clip->Get_Markers())
    {
        const auto Id = Marker.Get_Id();
        const auto PhaseId = Marker.Get_At().Get_PhaseId();
        const auto Value = Marker.Get_At().Get_Time().Get_Value();
        _Rows->AddSlot().AutoHeight()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("MarkerLane", "Marker {0}: {1}"),
                FText::FromName(Id), FText::FromName(PhaseId)))]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SNew(SSlider).Value_Lambda([this, Id, Value]() { return GetEventValue(Id, EDragEdge::Marker, Value); })
                .ToolTipText(LOCTEXT("DragMarkerTip", "Drag within the named phase; one undoable edit is committed on release."))
                .OnMouseCaptureBegin_Lambda([this, Id, PhaseId, Value]()
                    { BeginEventDrag(Id, PhaseId, EDragEdge::Marker, Value); })
                .OnValueChanged_Lambda([this](float InValue) { ChangeEventDrag(InValue); })
                .OnMouseCaptureEnd_Lambda([this]() { EndEventDrag(); })]];
    }
    for (const auto& Window : Clip->Get_Windows())
    {
        const auto Id = Window.Get_Id();
        const auto StartPhase = Window.Get_Start().Get_PhaseId();
        const auto StartValue = Window.Get_Start().Get_Time().Get_Value();
        const auto EndPhase = Window.Get_End().Get_PhaseId();
        const auto EndValue = Window.Get_End().Get_Time().Get_Value();
        _Rows->AddSlot().AutoHeight()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("WindowLane", "Window {0}: [{1}, {2})"),
                FText::FromName(Id), FText::FromName(StartPhase), FText::FromName(EndPhase)))]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SNew(SSlider).Value_Lambda([this, Id, StartValue]()
                    { return GetEventValue(Id, EDragEdge::WindowStart, StartValue); })
                .ToolTipText(LOCTEXT("DragWindowStartTip", "Drag the inclusive start within its phase; release commits one undoable edit."))
                .OnMouseCaptureBegin_Lambda([this, Id, StartPhase, StartValue]()
                    { BeginEventDrag(Id, StartPhase, EDragEdge::WindowStart, StartValue); })
                .OnValueChanged_Lambda([this](float InValue) { ChangeEventDrag(InValue); })
                .OnMouseCaptureEnd_Lambda([this]() { EndEventDrag(); })]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [SNew(SSlider).Value_Lambda([this, Id, EndValue]()
                    { return GetEventValue(Id, EDragEdge::WindowEnd, EndValue); })
                .ToolTipText(LOCTEXT("DragWindowEndTip", "Drag the exclusive end within its phase; end must remain after start."))
                .OnMouseCaptureBegin_Lambda([this, Id, EndPhase, EndValue]()
                    { BeginEventDrag(Id, EndPhase, EDragEdge::WindowEnd, EndValue); })
                .OnValueChanged_Lambda([this](float InValue) { ChangeEventDrag(InValue); })
                .OnMouseCaptureEnd_Lambda([this]() { EndEventDrag(); })]];
    }
}

#undef LOCTEXT_NAMESPACE
