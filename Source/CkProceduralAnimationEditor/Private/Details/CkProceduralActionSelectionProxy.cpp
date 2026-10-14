#include "CkProceduralAnimationEditor/CkProceduralActionSelectionProxy.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"

namespace ck_procedural_action_selection_proxy
{
    auto
        AtTime(
            double InTime)
    {
        return [InTime](const auto& InKey)
        { return ck::procedural_action_editor::IsSamePoseTime(InKey.Get_Time().Get_Value(), InTime); };
    }

    auto
        StartsFromCurrentPose(
            const FCk_ProceduralAction_Phase& InPhase)
        -> bool
    {
        const auto AtStart = AtTime(0.0);
        if (InPhase.Get_BodyKeys().ContainsByPredicate(AtStart))
        { return false; }
        return NOT InPhase.Get_LimbTracks().ContainsByPredicate([&AtStart](const FCk_ProceduralAction_LimbTrack& InTrack)
            { return InTrack.Get_PositionKeys().ContainsByPredicate(AtStart); });
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_ProceduralActionSelectionProxy::
    Load(
        const FCk_ProceduralActionAuthoringSession& InSession,
        const FCk_ProceduralActionEditorModel& InModel)
    -> void
{
    using namespace ck_procedural_action_selection_proxy;
    const auto* Clip = InSession.GetClip();
    if (Clip == nullptr)
    {
        _Shows = ECk_ProceduralActionSelectionProxy_Shows::Nothing;
        return;
    }
    const auto& Selection = InModel.Get_Selection();
    _Subject = Clip->Get_EditorSubject();
    _Length = static_cast<float>(InSession.GetDuration().Get_Seconds());
    _Markers = Clip->Get_Markers();
    _Windows = Clip->Get_Windows();

    const auto PhaseId = Selection.Get_PhaseId();
    const auto* Phase = Clip->Get_Phases().FindByPredicate([PhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == PhaseId; });
    if (Phase != nullptr)
    {
        _SectionName = PhaseId;
        _Duration = static_cast<float>(Phase->Get_Duration().Get_Seconds());
        _Easing = Phase->Get_Easing();
        _StartsFromCurrentPose = StartsFromCurrentPose(*Phase);
    }

    switch (Selection.Get_Kind())
    {
        case ECk_ProceduralActionEditor_SelectionKind::Key:
            _Shows = Phase != nullptr ? ECk_ProceduralActionSelectionProxy_Shows::Key : ECk_ProceduralActionSelectionProxy_Shows::Clip;
            break;
        case ECk_ProceduralActionEditor_SelectionKind::Section:
        case ECk_ProceduralActionEditor_SelectionKind::Channel:
            _Shows = Phase != nullptr ? ECk_ProceduralActionSelectionProxy_Shows::Section : ECk_ProceduralActionSelectionProxy_Shows::Clip;
            break;
        case ECk_ProceduralActionEditor_SelectionKind::Event:
            _Shows = ECk_ProceduralActionSelectionProxy_Shows::Event;
            break;
        default:
            _Shows = ECk_ProceduralActionSelectionProxy_Shows::Clip;
            break;
    }

    if (_Shows == ECk_ProceduralActionSelectionProxy_Shows::Event)
    {
        const auto EventId = Selection.Get_EventId();
        auto At = TOptional<FCk_ProceduralAction_TimeRef>{};
        if (const auto* Marker = _Markers.FindByPredicate([EventId](const FCk_ProceduralAction_Marker& InMarker) { return InMarker.Get_Id() == EventId; }))
        { At = Marker->Get_At(); }
        else if (const auto* Window = _Windows.FindByPredicate([EventId](const FCk_ProceduralAction_Window& InWindow) { return InWindow.Get_Id() == EventId; }))
        { At = Window->Get_Start(); }
        _EventName = EventId;
        _EventSection = At.IsSet() ? At->Get_PhaseId() : NAME_None;
        auto Start = FCk_Time{};
        const auto* EventPhase = At.IsSet() ? Clip->Get_Phases().FindByPredicate([&At](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == At->Get_PhaseId(); }) : nullptr;
        _EventTime = EventPhase != nullptr && InSession.Get_PhaseStart(At->Get_PhaseId(), Start)
            ? static_cast<float>(Start.Get_Seconds() + EventPhase->Get_Duration().Get_Seconds() * At->Get_Time().Get_Value()) : 0.0f;
        return;
    }
    if (_Shows != ECk_ProceduralActionSelectionProxy_Shows::Key)
    { return; }

    const auto Channel = Selection.Get_Channel();
    const auto KeyTime = Selection.Get_KeyTime();
    const auto KeyedHere = AtTime(KeyTime.Get_Value());
    auto Elapsed = FCk_Time{};
    _Channel = Channel;
    _KeyTime = InSession.KeyToElapsed(PhaseId, KeyTime, Elapsed) ? static_cast<float>(Elapsed.Get_Seconds()) : 0.0f;
    _OrientationMode = ECk_ProceduralAction_ValueMode::Keep;
    _BendMode = ECk_ProceduralAction_ValueMode::Keep;
    _Position = FVector::ZeroVector;
    _Orientation = FRotator::ZeroRotator;
    _BendDirection = FVector::UpVector;
    const auto* Group = Clip->Get_EditorPoseGroups().FindByPredicate([PhaseId, Channel, &KeyTime](const FCk_ProceduralAction_PoseGroup& InGroup)
    {
        return InGroup.Get_PhaseId() == PhaseId && InGroup.Get_Channels().Contains(Channel)
            && ck::procedural_action_editor::IsSamePoseTime(InGroup.Get_Time().Get_Value(), KeyTime.Get_Value());
    });
    _PoseName = Group != nullptr ? Group->Get_Label() : NAME_None;
    if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
    {
        _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
        if (const auto* Key = Phase->Get_BodyKeys().FindByPredicate(KeyedHere))
        {
            _Position = Key->Get_Translation();
            _Orientation = Key->Get_Rotation().Rotator();
            _OrientationMode = ECk_ProceduralAction_ValueMode::Set;
        }
        return;
    }
    if (const auto* Slot = Clip->Get_Slots().FindByPredicate([Channel](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == Channel; }))
    { _Space = Slot->Get_Space(); }
    const auto* Track = Phase->Get_LimbTracks().FindByPredicate([Channel](const FCk_ProceduralAction_LimbTrack& InTrack)
        { return InTrack.Get_Slot() == Channel; });
    if (Track == nullptr)
    { return; }
    _ReachPolicy = Track->Get_ReachPolicy();
    if (const auto* Key = Track->Get_PositionKeys().FindByPredicate(KeyedHere))
    { _Position = Key->Get_Value(); }
    if (const auto* Key = Track->Get_OrientationKeys().FindByPredicate(KeyedHere))
    {
        _OrientationMode = ECk_ProceduralAction_ValueMode::Set;
        _Orientation = Key->Get_Value().Rotator();
    }
    if (const auto* Key = Track->Get_BendKeys().FindByPredicate(KeyedHere))
    {
        _BendMode = ECk_ProceduralAction_ValueMode::Set;
        _BendDirection = Key->Get_Value();
    }
}

auto
    UCk_ProceduralActionSelectionProxy::
    Get_IsCategoryShown(
        FName InCategory) const
    -> bool
{
    if (InCategory == FName{TEXT("Key")})
    { return _Shows == ECk_ProceduralActionSelectionProxy_Shows::Key; }
    if (InCategory == FName{TEXT("Section")})
    { return _Shows == ECk_ProceduralActionSelectionProxy_Shows::Key || _Shows == ECk_ProceduralActionSelectionProxy_Shows::Section; }
    if (InCategory == FName{TEXT("Clip")})
    { return _Shows == ECk_ProceduralActionSelectionProxy_Shows::Clip; }
    if (InCategory == FName{TEXT("Event")})
    { return _Shows == ECk_ProceduralActionSelectionProxy_Shows::Event; }
    return false;
}

auto
    UCk_ProceduralActionSelectionProxy::
    PostEditChangeProperty(
        FPropertyChangedEvent& InEvent)
    -> void
{
    Super::PostEditChangeProperty(InEvent);
    if (InEvent.ChangeType == EPropertyChangeType::Interactive || NOT OnEdited)
    { return; }
    const auto Name = InEvent.MemberProperty != nullptr ? InEvent.MemberProperty->GetFName() : InEvent.GetPropertyName();
    OnEdited(Name);
}

// --------------------------------------------------------------------------------------------------------------------
