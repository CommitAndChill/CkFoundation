#pragma once

#include "Widgets/SCompoundWidget.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "Math/Axis.h"

class FCk_ProceduralActionAuthoringSession;
class SEditableTextBox;
class STextBlock;
class SComboButton;
class SVerticalBox;

class SCkProceduralAction_Timeline : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_Timeline) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_EVENT(FSimpleDelegate, OnChanged)
        SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    auto
        Refresh()
        -> void;
    auto
        GetSelectedPhase() const
        -> FName
    {
        return _SelectedPhase;
    }
    auto
        GetSelectedSlot() const
        -> FName
    {
        return _SelectedSlot;
    }
    auto
        SelectTrack(
            FName InPhaseId,
            FName InSlotId)
        -> void;

private:
    enum class EDragEdge : uint8 { None, Marker, WindowStart, WindowEnd };

    auto
        RebuildRows()
        -> void;
    auto
        Apply(
            FName InOperation)
        -> FReply;
    auto
        ApplyPhase(
            FName InOperation,
            FName InPhaseId)
        -> FReply;
    auto
        ApplySlot(
            FName InSlotId)
        -> FReply;
    auto
        ApplyRenameSlot()
        -> FReply;
    auto
        ApplyTrackSpace(
            ECk_ProceduralAction_TrackSpace InSpace)
        -> FReply;
    auto
        ApplyMirror(
            EAxis::Type InPlaneNormal)
        -> FReply;
    auto
        ApplyRetarget()
        -> FReply;
    auto
        BuildLimbPicker()
        -> TSharedRef<SWidget>;
    auto
        GetCurrentPhaseTime() const
        -> double;
    auto
        GetErrorText() const
        -> FText;
    auto
        BeginEventDrag(
            FName InId,
            FName InPhaseId,
            EDragEdge InEdge,
            double InValue)
        -> void;
    auto
        ChangeEventDrag(
            float InValue)
        -> void;
    auto
        EndEventDrag()
        -> void;
    auto
        RebuildAfterEventDrag(
            double InCurrentTime,
            float InDeltaTime)
        -> EActiveTimerReturnType;
    auto
        GetEventValue(
            FName InId,
            EDragEdge InEdge,
            double InAuthoredValue) const
        -> float;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    FSimpleDelegate _OnChanged;
    FSimpleDelegate _OnSelectionChanged;
    TSharedPtr<SVerticalBox> _Rows;
    TSharedPtr<SEditableTextBox> _NewPhaseId;
    TSharedPtr<SEditableTextBox> _NewSlotId;
    TSharedPtr<SEditableTextBox> _RenamedSlotId;
    TSharedPtr<SEditableTextBox> _NewEventId;
    TSharedPtr<SEditableTextBox> _MirrorDestination;
    TSharedPtr<STextBlock> _Message;
    TSharedPtr<SComboButton> _RetargetPicker;
    FName _PickedRetargetLimb = NAME_None;
    double _NewPhaseDuration = 0.5;
    FName _SelectedPhase = NAME_None;
    FName _SelectedSlot = NAME_None;
    FName _DraggedEvent = NAME_None;
    FName _DragPhase = NAME_None;
    EDragEdge _DragEdge = EDragEdge::None;
    double _DragValue = 0.0;
    bool _SnapEventEdges = false;
    FString _Error;
};
