#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Math/ValueRange/CkValueRange.h"
#include "CkCore/Time/CkTime.h"

#include "Delegates/Delegate.h"
#include "Misc/Guid.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECk_ProceduralActionEditor_SelectionKind : uint8
{
    None,
    Clip,
    Section,
    Channel,
    Key,
    Event
};

// Solved plays the production solver; Authored plays the requested targets straight from the clip.
enum class ECk_ProceduralActionEditor_PreviewMode : uint8
{
    Solved,
    Authored
};

// --------------------------------------------------------------------------------------------------------------------

/** What the author is working on. _Kind names the item last chosen; the section, channel and pose stay set as the context
 *  every view shares (a channel is shown in the selected section, a key is the selected channel's key in that section). */
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionEditorSelection
{
    CK_GENERATED_BODY(FCk_ProceduralActionEditorSelection);

private:
    ECk_ProceduralActionEditor_SelectionKind _Kind = ECk_ProceduralActionEditor_SelectionKind::None;
    FName _PhaseId;
    FName _Channel;
    FCk_FloatRange_0to1 _KeyTime;
    FName _EventId;
    FGuid _PoseId;

public:
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_PhaseId);
    CK_PROPERTY(_Channel);
    CK_PROPERTY(_KeyTime);
    CK_PROPERTY(_EventId);
    CK_PROPERTY(_PoseId);
};

// --------------------------------------------------------------------------------------------------------------------

/** The one editor state the clip editor's views share: the selection, the playhead and transport, and the preview mode.
 *  Views read it and change it through its methods; every change bumps Get_Revision and broadcasts OnChanged once. An
 *  edit to the clip itself bumps Get_ContentRevision too, so a view rebuilds what the clip feeds only when the clip changed. */
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionEditorModel
{
public:
    auto
        Get_Selection() const
        -> const FCk_ProceduralActionEditorSelection&
    { return _Selection; }
    auto
        Get_SelectedPhase() const
        -> FName
    { return _Selection.Get_PhaseId(); }
    auto
        Get_SelectedChannel() const
        -> FName
    { return _Selection.Get_Channel(); }
    auto
        Get_SelectedPose() const
        -> const FGuid&
    { return _Selection.Get_PoseId(); }
    // The selected channel as a limb slot: none when nothing or the body is selected.
    auto
        Get_SelectedSlot() const
        -> FName;

    auto
        SelectClip()
        -> void;
    auto
        SelectSection(
            FName InPhaseId)
        -> void;
    auto
        SelectChannel(
            FName InChannel)
        -> void;
    auto
        SelectTrack(
            FName InPhaseId,
            FName InChannel)
        -> void;
    auto
        SelectKey(
            FName InPhaseId,
            FName InChannel,
            FCk_FloatRange_0to1 InKeyTime)
        -> void;
    auto
        SelectEvent(
            FName InEventId)
        -> void;
    auto
        SelectPose(
            const FGuid& InPoseId,
            FName InPhaseId)
        -> void;
    auto
        ClearPose()
        -> void;
    // Follows a section the clip renamed, keeping the rest of the selection.
    auto
        RenameSection(
            FName InOldPhaseId,
            FName InNewPhaseId)
        -> void;

    auto
        Get_Time() const
        -> FCk_Time
    { return _Time; }
    auto
        Get_IsPlaying() const
        -> bool
    { return _Playing; }
    auto
        Get_IsLooping() const
        -> bool
    { return _Looping; }
    auto
        Get_PreviewMode() const
        -> ECk_ProceduralActionEditor_PreviewMode
    { return _PreviewMode; }

    // A time the author asked for (scrub, step, reset). Playback sources follow it.
    auto
        SetTime(
            FCk_Time InTime)
        -> void;
    // A time a playback source reached on its own; it is shown but nothing seeks to it.
    auto
        FollowSourceTime(
            FCk_Time InTime)
        -> void;
    auto
        SetPlaying(
            bool InPlaying)
        -> void;
    auto
        SetLooping(
            bool InLooping)
        -> void;
    auto
        SetPreviewMode(
            ECk_ProceduralActionEditor_PreviewMode InMode)
        -> void;
    auto
        Get_TimeRequestRevision() const
        -> uint64
    { return _TimeRequestRevision; }

    // The channels Record covers (the body channel by its pose-group name, slots by id).
    auto
        Get_RecordChannels() const
        -> const TArray<FName>&
    { return _RecordChannels; }
    auto
        Get_IsRecorded(
            FName InChannel) const
        -> bool
    { return _RecordChannels.Contains(InChannel); }
    auto
        SetRecorded(
            FName InChannel,
            bool InRecorded)
        -> void;
    auto
        SetRecordChannels(
            const TArray<FName>& InChannels)
        -> void;

    auto
        NotifyContentChanged()
        -> void;

    auto
        Get_Revision() const
        -> uint64
    { return _Revision; }
    auto
        Get_ContentRevision() const
        -> uint64
    { return _ContentRevision; }
    auto
        Get_SelectionRevision() const
        -> uint64
    { return _SelectionRevision; }

    FSimpleMulticastDelegate OnChanged;

private:
    auto
        DoSetSelection(
            const FCk_ProceduralActionEditorSelection& InSelection)
        -> void;
    auto
        DoBroadcast()
        -> void;

    FCk_ProceduralActionEditorSelection _Selection;
    TArray<FName> _RecordChannels;
    FCk_Time _Time;
    ECk_ProceduralActionEditor_PreviewMode _PreviewMode = ECk_ProceduralActionEditor_PreviewMode::Solved;
    uint64 _Revision = 0;
    uint64 _ContentRevision = 0;
    uint64 _SelectionRevision = 0;
    uint64 _TimeRequestRevision = 0;
    bool _Playing = false;
    bool _Looping = false;
};

// --------------------------------------------------------------------------------------------------------------------
