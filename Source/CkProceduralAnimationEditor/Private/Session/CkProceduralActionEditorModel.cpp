#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"

#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_ProceduralActionEditorModel::
    Get_SelectedSlot() const
    -> FName
{
    const auto Channel = _Selection.Get_Channel();
    return Channel == ck::procedural_action_editor::Get_BodyChannelName() ? NAME_None : Channel;
}

auto
    FCk_ProceduralActionEditorModel::
    SelectClip()
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Clip);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SelectSection(
        FName InPhaseId)
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Section);
    Selection.Set_PhaseId(InPhaseId);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SelectChannel(
        FName InChannel)
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Channel);
    Selection.Set_Channel(InChannel);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SelectTrack(
        FName InPhaseId,
        FName InChannel)
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Channel);
    Selection.Set_PhaseId(InPhaseId);
    Selection.Set_Channel(InChannel);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SelectKey(
        FName InPhaseId,
        FName InChannel,
        FCk_FloatRange_0to1 InKeyTime)
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Key);
    Selection.Set_PhaseId(InPhaseId);
    Selection.Set_Channel(InChannel);
    Selection.Set_KeyTime(InKeyTime);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SelectEvent(
        FName InEventId)
    -> void
{
    auto Selection = _Selection;
    Selection.Set_Kind(ECk_ProceduralActionEditor_SelectionKind::Event);
    Selection.Set_EventId(InEventId);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    RenameSection(
        FName InOldPhaseId,
        FName InNewPhaseId)
    -> void
{
    if (_Selection.Get_PhaseId() != InOldPhaseId)
    { return; }
    auto Selection = _Selection;
    Selection.Set_PhaseId(InNewPhaseId);
    DoSetSelection(Selection);
}

auto
    FCk_ProceduralActionEditorModel::
    SetTime(
        FCk_Time InTime)
    -> void
{
    if (NOT FMath::IsFinite(InTime.Get_Seconds()))
    { return; }
    _Time = InTime;
    ++_TimeRequestRevision;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    FollowSourceTime(
        FCk_Time InTime)
    -> void
{
    if (NOT FMath::IsFinite(InTime.Get_Seconds()) || InTime == _Time)
    { return; }
    _Time = InTime;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    SetPlaying(
        bool InPlaying)
    -> void
{
    if (_Playing == InPlaying)
    { return; }
    _Playing = InPlaying;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    SetLooping(
        bool InLooping)
    -> void
{
    if (_Looping == InLooping)
    { return; }
    _Looping = InLooping;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    SetPreviewMode(
        ECk_ProceduralActionEditor_PreviewMode InMode)
    -> void
{
    if (_PreviewMode == InMode)
    { return; }
    _PreviewMode = InMode;
    _Playing = false;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    SetRecorded(
        FName InChannel,
        bool InRecorded)
    -> void
{
    if (_RecordChannels.Contains(InChannel) == InRecorded)
    { return; }
    if (InRecorded)
    { _RecordChannels.Add(InChannel); }
    else
    { _RecordChannels.Remove(InChannel); }
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    SetRecordChannels(
        const TArray<FName>& InChannels)
    -> void
{
    if (_RecordChannels == InChannels)
    { return; }
    _RecordChannels = InChannels;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    NotifyContentChanged()
    -> void
{
    ++_ContentRevision;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    DoSetSelection(
        const FCk_ProceduralActionEditorSelection& InSelection)
    -> void
{
    _Selection = InSelection;
    ++_SelectionRevision;
    DoBroadcast();
}

auto
    FCk_ProceduralActionEditorModel::
    DoBroadcast()
    -> void
{
    ++_Revision;
    OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------
