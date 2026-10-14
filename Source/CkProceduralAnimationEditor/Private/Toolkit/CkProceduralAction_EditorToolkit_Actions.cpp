#include "CkProceduralAnimationEditor/CkProceduralAction_EditorToolkit.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModes.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSelectionProxy.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolvedCapture.h"

#include "Widgets/SCkProceduralAction_Viewport.h"

namespace ck_procedural_action_editor_toolkit_actions
{
    template <typename TItems>
    auto
        MakeId(
            const TCHAR* InPrefix,
            const TItems& InItems)
        -> FName
    {
        for (auto Index = 1;; ++Index)
        {
            const auto Id = FName{*FString::Printf(TEXT("%s%d"), InPrefix, Index)};
            if (NOT InItems.ContainsByPredicate([Id](const auto& InItem) { return InItem.Get_Id() == Id; }))
            { return Id; }
        }
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    TogglePlay()
    -> void
{
    if (_Closed)
    { return; }
    const auto Play = NOT _Model->Get_IsPlaying();
    const auto AtSolvedEnd = Get_IsSolved() && _Modes->GetIsComplete() && _Modes->GetFrameCount() > 0
        && _Model->Get_Time() >= _Modes->GetSolvedTime();
    if (Play && AtSolvedEnd)
    { _Model->SetTime(FCk_Time{}); }
    _Model->SetPlaying(Play);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RestartSolver()
    -> bool
{
    if (_Closed)
    { return false; }
    _Session->SetPlaying(false);
    _Model->SetPreviewMode(ECk_ProceduralActionEditor_PreviewMode::Solved);
    return Finish(_Modes->StartSolver(), TEXT("The production preview is ready."), _Modes->GetStatusText().ToString());
}

auto
    FCk_ProceduralAction_EditorToolkit::
    StepFrames(
        int32 InFrames)
    -> void
{
    if (_Closed)
    { return; }
    _Model->SetPlaying(false);
    const auto Duration = Get_IsSolved() ? _Modes->GetDuration() : _Session->GetDuration();
    const auto Seconds = FMath::Clamp(_Model->Get_Time().Get_Seconds() + InFrames / 60.0, 0.0, Duration.Get_Seconds());
    _Model->SetTime(FCk_Time{FMath::RoundToDouble(Seconds * 60.0) / 60.0});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RecordPose()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return false; }
    auto PoseId = FGuid{};
    auto Error = FString{};
    auto Key = FCk_ProceduralActionKeyTime{};
    const auto& Channels = _Model->Get_RecordChannels();
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    if (Get_IsSolved())
    {
        const auto* Frame = _Modes->GetDisplayedFrame();
        if (Frame == nullptr)
        { return Finish(false, {}, TEXT("No solved frame at the playhead yet; wait for the Solver to reach it, then record.")); }
        if (NOT _Session->ElapsedToKey(_Modes->GetTime(), Key))
        { return Finish(false, {}, TEXT("The Solver time does not fall inside a section of this clip.")); }
        const auto Target = FCk_ProceduralActionSolvedCaptureTarget{_Session->GetPreviewConfig(), _Session->GetRevision(),
            Key.Get_PhaseId(), Key.Get_KeyTime(), Channels};
        if (NOT Commands.CaptureSolvedPose(Frame->Capture, Target, PoseId, Error))
        { return Finish(false, {}, Error); }
    }
    else
    {
        if (NOT _Session->ElapsedToKey(_Model->Get_Time(), Key))
        { return Finish(false, {}, TEXT("The playhead is not inside a section.")); }
        auto Evaluated = ck::procedural_action::FEvaluatedPose{};
        const auto HasPose = _Session->EvaluateAt(_Model->Get_Time(), Evaluated);
        auto Pose = FCk_ProceduralAction_PoseKey{Key.Get_KeyTime()};
        for (const auto Channel : Channels)
        {
            if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
            {
                Pose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
                Pose.Set_Body(HasPose ? Evaluated.Get_Body() : FTransform::Identity);
                continue;
            }
            const auto* Limb = HasPose ? Evaluated.Get_Limbs().FindByPredicate([Channel](const auto& InLimb) { return InLimb.Get_Slot() == Channel; }) : nullptr;
            auto Target = FCk_ProceduralAction_LimbPoseTarget{Channel, Limb != nullptr ? Limb->Get_Position() : FVector::ZeroVector};
            if (Limb != nullptr && Limb->Get_HasOrientation())
            { Target.Set_OrientationMode(ECk_ProceduralAction_ValueMode::Set).Set_Orientation(Limb->Get_Orientation()); }
            if (Limb != nullptr && Limb->Get_HasBend())
            { Target.Set_BendMode(ECk_ProceduralAction_ValueMode::Set).Set_Bend(Limb->Get_BendDirection()); }
            Pose.Get_Limbs().Add(Target);
        }
        if (NOT Commands.RecordPose(Key.Get_PhaseId(), Pose, NAME_None, PoseId, Error))
        { return Finish(false, {}, Error); }
    }
    _Model->SelectPose(PoseId, Key.Get_PhaseId());
    _Model->NotifyContentChanged();
    return Finish(true, FString::Printf(TEXT("Recorded the pose at %s %.2f."), *Key.Get_PhaseId().ToString(), Key.Get_KeyTime().Get_Value()), {});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Get_SectionAt(
        FCk_Time InTime,
        FName& OutPhaseId,
        double& OutFraction) const
    -> bool
{
    const auto& Timing = _Session->GetTiming();
    for (const auto& Phase : Timing)
    {
        const auto End = Phase.Get_Start() + Phase.Get_Duration();
        if (InTime <= End || &Phase == &Timing.Last())
        {
            OutPhaseId = Phase.Get_Id();
            OutFraction = FMath::Clamp((InTime - Phase.Get_Start()).Get_Seconds() / Phase.Get_Duration().Get_Seconds(), 0.0, 1.0);
            return true;
        }
    }
    return false;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    AddSection()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return false; }
    const auto Id = ck_procedural_action_editor_toolkit_actions::MakeId(TEXT("Section"), Clip->Get_Phases());
    auto Error = FString{};
    if (NOT FCk_ProceduralActionAuthoringCommands{Clip}.AddPhase(Id, FCk_Time{0.5}, Error))
    { return Finish(false, {}, Error); }
    _Model->NotifyContentChanged();
    _Model->SelectSection(Id);
    return Finish(true, FString::Printf(TEXT("Added %s."), *Id.ToString()), {});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    AddSlot()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return false; }
    const auto Id = ck_procedural_action_editor_toolkit_actions::MakeId(TEXT("Slot"), Clip->Get_Slots());
    auto Error = FString{};
    if (NOT FCk_ProceduralActionAuthoringCommands{Clip}.AddSlot(Id, Error))
    { return Finish(false, {}, Error); }
    _Model->SetRecorded(Id, true);
    _Model->NotifyContentChanged();
    return Finish(true, FString::Printf(TEXT("Added %s; bind it to a limb in Channels."), *Id.ToString()), {});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    AddMarkerAtPlayhead()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    auto PhaseId = FName{};
    auto Fraction = 0.0;
    if (Clip == nullptr || NOT Get_SectionAt(_Model->Get_Time(), PhaseId, Fraction))
    { return Finish(false, {}, TEXT("Add a section before adding events.")); }
    const auto Id = ck_procedural_action_editor_toolkit_actions::MakeId(TEXT("Marker"), Clip->Get_Markers());
    auto Error = FString{};
    if (NOT FCk_ProceduralActionAuthoringCommands{Clip}.AddMarker(Id, PhaseId, Fraction, Error))
    { return Finish(false, {}, Error); }
    _Model->NotifyContentChanged();
    _Model->SelectEvent(Id);
    return Finish(true, FString::Printf(TEXT("Added %s."), *Id.ToString()), {});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    AddWindowAtPlayhead()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    auto PhaseId = FName{};
    auto Fraction = 0.0;
    if (Clip == nullptr || NOT Get_SectionAt(_Model->Get_Time(), PhaseId, Fraction))
    { return Finish(false, {}, TEXT("Add a section before adding events.")); }
    if (Fraction >= 1.0)
    { return Finish(false, {}, TEXT("A window opens at the playhead and closes at its section's end; move the playhead before the end.")); }
    const auto Id = ck_procedural_action_editor_toolkit_actions::MakeId(TEXT("Window"), Clip->Get_Windows());
    auto Error = FString{};
    if (NOT FCk_ProceduralActionAuthoringCommands{Clip}.AddWindow(Id, PhaseId, Fraction, PhaseId, 1.0, Error))
    { return Finish(false, {}, Error); }
    _Model->NotifyContentChanged();
    _Model->SelectEvent(Id);
    return Finish(true, FString::Printf(TEXT("Added %s."), *Id.ToString()), {});
}

auto
    FCk_ProceduralAction_EditorToolkit::
    ApplyStartingShape(
        ECk_ProceduralAction_Recipe InRecipe)
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return false; }
    auto Slots = TArray<FName>{};
    auto Limb = FName{};
    if (InRecipe == ECk_ProceduralAction_Recipe::Reach || InRecipe == ECk_ProceduralAction_Recipe::Sweep)
    {
        if (const auto* Config = _Session->GetPreviewConfig(); Config != nullptr && NOT Config->Get_LimbBindings().IsEmpty())
        { Limb = Config->Get_LimbBindings()[0].Get_LimbId(); }
        Slots.Add(Limb.IsNone() ? FName{TEXT("Limb")} : Limb);
    }
    auto Recipe = FCk_ProceduralAction_RecipeSpec{InRecipe, Slots};
    if (NOT Slots.IsEmpty())
    { Recipe.Set_Space(ECk_ProceduralAction_TrackSpace::LimbRestNormalized); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    auto Error = FString{};
    auto Applied = Commands.CreateFromRecipe(Recipe, Error);
    if (Applied && NOT Limb.IsNone())
    { Applied = Commands.RetargetSlot(_Session->GetPreviewConfig(), Slots[0], Limb, Error); }
    if (Applied)
    {
        for (const auto Slot : Slots)
        { _Model->SetRecorded(Slot, true); }
        _Model->NotifyContentChanged();
    }
    return Finish(Applied, TEXT("Applied the starting shape."), Error);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    DeleteSelection()
    -> bool
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return false; }
    const auto& Selection = _Model->Get_Selection();
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    auto Error = FString{};
    auto Deleted = false;
    if (Selection.Get_PoseId().IsValid())
    {
        Deleted = Commands.DeletePose(Selection.Get_PoseId(), Error);
        if (Deleted)
        { _Model->ClearPose(); }
    }
    else if (Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Key)
    { Deleted = Commands.DeleteKey(Selection.Get_PhaseId(), Selection.Get_Channel(), Selection.Get_KeyTime().Get_Value(), Error); }
    else if (Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Event)
    {
        Deleted = Commands.RemoveEvent(Selection.Get_EventId(), Error);
        if (Deleted)
        { _Model->SelectClip(); }
    }
    else if (Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Section)
    {
        if (Clip->Get_Phases().Num() <= 1)
        { return Finish(false, {}, TEXT("A clip keeps at least one section.")); }
        Deleted = Commands.RemovePhase(Selection.Get_PhaseId(), Error);
    }
    else
    { return Finish(false, {}, TEXT("Select a section, key, pose or event to delete.")); }
    if (Deleted)
    { _Model->NotifyContentChanged(); }
    return Finish(Deleted, TEXT("Deleted."), Error);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SetScenario(
        UCk_ProceduralActionPreview_Config* InScenario)
    -> void
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return; }
    auto Subject = Clip->Get_EditorSubject();
    Subject.Set_Scenario(TSoftObjectPtr<UObject>{InScenario});
    auto Error = FString{};
    Finish(FCk_ProceduralActionAuthoringCommands{Clip}.SetEditorSubject(Subject, Error), TEXT("The scenario is set."), Error);
    _Session->UseDerivedPreview();
    if (_Viewport.IsValid())
    { _Viewport->RefreshAppearance(); }
    _Model->NotifyContentChanged();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    FrameSubject()
    -> void
{
    if (_Viewport.IsValid())
    { _Viewport->FrameSubject(); }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnProxyEdited(
        FName InProperty)
    -> void
{
    auto* Clip = _Closed ? nullptr : _Session->GetClip();
    if (Clip == nullptr)
    { return; }
    const auto& Proxy = *_SelectionProxy;
    const auto& Selection = _Model->Get_Selection();
    const auto Is = [InProperty](FName InName) { return InProperty == InName; };
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    auto Error = FString{};
    auto Done = false;
    if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _SectionName)))
    {
        const auto OldPhaseId = Selection.Get_PhaseId();
        Done = Commands.RenamePhase(OldPhaseId, Proxy._SectionName, Error);
        if (Done)
        { _Model->RenameSection(OldPhaseId, Proxy._SectionName); }
    }
    else if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _Duration)))
    { Done = Commands.SetPhaseDuration(Selection.Get_PhaseId(), FCk_Time{Proxy._Duration}, Error); }
    else if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _Easing)))
    { Done = Commands.SetPhaseEasing(Selection.Get_PhaseId(), Proxy._Easing, Error); }
    else if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _Subject)))
    {
        Done = Commands.SetEditorSubject(Proxy._Subject, Error);
        _AppearanceRefreshPending = Done;
        _SourceRefreshPending = Done;
    }
    else if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _ReachPolicy)))
    { Done = Commands.SetReachPolicy(Selection.Get_PhaseId(), Selection.Get_Channel(), Proxy._ReachPolicy, Error); }
    else if (Is(GET_MEMBER_NAME_CHECKED(UCk_ProceduralActionSelectionProxy, _PoseName)))
    {
        const auto* Group = Clip->Get_EditorPoseGroups().FindByPredicate([&Selection](const FCk_ProceduralAction_PoseGroup& InGroup)
        {
            return InGroup.Get_PhaseId() == Selection.Get_PhaseId() && InGroup.Get_Channels().Contains(Selection.Get_Channel())
                && ck::procedural_action_editor::IsSamePoseTime(InGroup.Get_Time().Get_Value(), Selection.Get_KeyTime().Get_Value());
        });
        if (Group != nullptr)
        { Done = Commands.RenamePose(Group->Get_Id(), Proxy._PoseName, Error); }
        else
        { Error = TEXT("No pose is named at this key; Record one first."); }
    }
    else if (Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Key)
    {
        auto Pose = FCk_ProceduralAction_PoseKey{Selection.Get_KeyTime()};
        if (Selection.Get_Channel() == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        {
            Pose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
            Pose.Set_Body(FTransform{Proxy._Orientation.Quaternion(), Proxy._Position});
        }
        else
        {
            auto Target = FCk_ProceduralAction_LimbPoseTarget{Selection.Get_Channel(), Proxy._Position};
            Target.Set_OrientationMode(Proxy._OrientationMode).Set_Orientation(Proxy._Orientation.Quaternion());
            Target.Set_BendMode(Proxy._BendMode).Set_Bend(Proxy._BendDirection.GetSafeNormal());
            Pose.Get_Limbs().Add(Target);
        }
        Done = Commands.SetPoseKeys(Selection.Get_PhaseId(), Pose, Error);
    }
    else
    { return; }
    if (Done)
    { _Model->NotifyContentChanged(); }
    _DetailsRefreshPending = true;
    Finish(Done, FString::Printf(TEXT("Edited %s."), *InProperty.ToString()), Error);
}
