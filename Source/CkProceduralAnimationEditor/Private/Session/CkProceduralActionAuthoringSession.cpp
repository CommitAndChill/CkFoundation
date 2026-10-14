#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreviewSpace.h"

#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionAuthoringSession"

FCk_ProceduralActionAuthoringSession::FCk_ProceduralActionAuthoringSession(
    UCk_ProceduralAction_Data* InClip)
    : _Clip(InClip)
{
    Refresh();
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetPreviewConfig(
        UCk_ProceduralActionPreview_Config* InConfig)
    -> void
{
    _DerivedFrom.Reset();
    _PreviewConfig.Reset(InConfig);
    Refresh();
}

auto
    FCk_ProceduralActionAuthoringSession::
    UseDerivedPreview()
    -> void
{
    _DerivedFrom = FCk_ProceduralAction_EditorSubject{};
    _PreviewConfig.Reset();
    Refresh();
}

auto
    FCk_ProceduralActionAuthoringSession::
    RefreshDerivedPreview()
    -> void
{
    if (NOT _DerivedFrom.IsSet() || NOT _Clip.IsValid())
    { return; }
    const auto& Subject = _Clip->Get_EditorSubject();
    const auto SubjectChanged = NOT FCk_ProceduralAction_EditorSubject::StaticStruct()->CompareScriptStruct(
        &_DerivedFrom.GetValue(), &Subject, PPF_None);
    if (_PreviewConfig.IsValid() && NOT SubjectChanged)
    { return; }
    _DerivedFrom = Subject;
    _PreviewConfig.Reset(ck::procedural_action_editor::DerivePreviewConfig(*_Clip.Get()));
}

auto
    FCk_ProceduralActionAuthoringSession::
    RefreshTiming()
    -> void
{
    _Timing.Reset();
    if (NOT _Clip.IsValid())
    { return; }
    auto Start = FCk_Time{};
    for (const auto& Phase : _Clip->Get_Phases())
    {
        const auto Duration = Phase.Get_Duration();
        if (NOT FMath::IsFinite(Duration.Get_Seconds()) || Duration <= FCk_Time{})
        {
            _Timing.Reset();
            return;
        }
        auto Timing = ck::procedural_action::FCompiledPhase{};
        Timing.Get_Id() = Phase.Get_Id();
        Timing.Get_Start() = Start;
        Timing.Get_Duration() = Duration;
        Timing.Get_Easing() = Phase.Get_Easing();
        _Timing.Add(MoveTemp(Timing));
        Start += Duration;
    }
}

auto
    FCk_ProceduralActionAuthoringSession::
    ElapsedToKey(
        FCk_Time InElapsed,
        FCk_ProceduralActionKeyTime& OutKey) const
    -> bool
{
    if (_Timing.IsEmpty() || NOT FMath::IsFinite(InElapsed.Get_Seconds()))
    { return false; }
    const auto Elapsed = FMath::Clamp(InElapsed.Get_Seconds(), 0.0,
        (_Timing.Last().Get_Start() + _Timing.Last().Get_Duration()).Get_Seconds());
    for (const auto& Phase : _Timing)
    {
        const auto End = (Phase.Get_Start() + Phase.Get_Duration()).Get_Seconds();
        if (Elapsed > End && &Phase != &_Timing.Last())
        { continue; }
        const auto Raw = FMath::Clamp((Elapsed - Phase.Get_Start().Get_Seconds()) / Phase.Get_Duration().Get_Seconds(), 0.0, 1.0);
        const auto KeyTime = Raw <= 0.0 || Raw >= 1.0 ? Raw : ck::procedural_action::EvaluateProgress(Phase, Raw);
        OutKey = FCk_ProceduralActionKeyTime{Phase.Get_Id(), FCk_FloatRange_0to1{KeyTime}};
        return true;
    }
    return false;
}

auto
    FCk_ProceduralActionAuthoringSession::
    KeyToElapsed(
        FName InPhaseId,
        FCk_FloatRange_0to1 InKeyTime,
        FCk_Time& OutElapsed) const
    -> bool
{
    const auto* Phase = _Timing.FindByPredicate([InPhaseId](const ck::procedural_action::FCompiledPhase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { return false; }
    const auto KeyTime = InKeyTime.Get_Value();
    const auto Raw = KeyTime <= 0.0 || KeyTime >= 1.0 ? KeyTime : ck::procedural_action::InvertProgress(*Phase, KeyTime);
    OutElapsed = Phase->Get_Start() + Phase->Get_Duration() * Raw;
    return true;
}

auto
    FCk_ProceduralActionAuthoringSession::
    Get_PhaseStart(
        FName InPhaseId,
        FCk_Time& OutStart) const
    -> bool
{
    const auto* Phase = _Timing.FindByPredicate([InPhaseId](const ck::procedural_action::FCompiledPhase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { return false; }
    OutStart = Phase->Get_Start();
    return true;
}

auto
    FCk_ProceduralActionAuthoringSession::
    Refresh()
    -> void
{
    _Playing = false;
    ++_Revision;
    _Compiled = {};
    _Diagnostics.Reset();
    _AppearanceError.Reset();
    _CompiledValid = _Clip.IsValid()
        && ck::procedural_action::Compile(*_Clip.Get(), _Compiled, _Diagnostics);
    _PoseGroups.Reset();
    if (_Clip.IsValid())
    { ck::procedural_action_editor::DerivePoseGroups(*_Clip.Get(), _PoseGroups); }
    RefreshTiming();
    RefreshDerivedPreview();

    _Initial = {};
    if (const auto* Config = _PreviewConfig.Get())
    {
        Config->ValidateAppearance(_AppearanceError);
        if (_AppearanceError.IsEmpty())
        { Config->ValidateSurface(_AppearanceError); }
        _Initial.Get_Body() = Config->Get_InitialBody();
        _Initial.Get_Limbs() = Config->Get_InitialLimbEndpoints();
        _Initial.Get_BendDirections() = Config->Get_InitialBendDirections();
        if (_CompiledValid)
        {
            // A captured slot starts where its foot stands in game; without an authored start the preview stands it at rest.
            for (const auto& Slot : _Compiled.Get_Slots())
            {
                const auto IsAuthoredRest = Slot.Get_InitialSource() == ECk_ProceduralAction_InitialSource::AuthoredRest;
                if (NOT IsAuthoredRest && _Initial.Get_Limbs().Contains(Slot.Get_Id()))
                { continue; }
                _Initial.Get_BendDirections().Remove(Slot.Get_Id());
                _Initial.Get_Limbs().Remove(Slot.Get_Id());
                auto Rest = FTransform::Identity;
                if (ck::procedural_action_editor::ResolveRestInTrackSpace(*Config, Slot, Rest))
                { _Initial.Get_Limbs().Add(Slot.Get_Id(), Rest); }
            }
        }
    }

    if (_CompiledValid)
    { _Time = FMath::Clamp(_Time, FCk_Time{}, GetDuration()); }
    else
    { _Time = FCk_Time{}; }
    EvaluateCurrent();
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetTime(
        FCk_Time InTime)
    -> void
{
    _Playing = false;
    const auto Seconds = InTime.Get_Seconds();
    if (NOT _CompiledValid || NOT FMath::IsFinite(Seconds))
    { return; }

    _Time = FMath::Clamp(InTime, FCk_Time{}, GetDuration());
    EvaluateCurrent();
}

auto
    FCk_ProceduralActionAuthoringSession::
    EvaluateAt(
        FCk_Time InTime,
        ck::procedural_action::FEvaluatedPose& OutPose) const
    -> bool
{
    const auto Seconds = InTime.Get_Seconds();
    return _CompiledValid && FMath::IsFinite(Seconds)
        && InTime >= FCk_Time{} && InTime <= GetDuration()
        && ck::procedural_action::Evaluate(_Compiled, _Initial, InTime, OutPose);
}

auto
    FCk_ProceduralActionAuthoringSession::
    Advance(
        FCk_Time InDelta)
    -> void
{
    const auto DeltaSeconds = InDelta.Get_Seconds();
    if (NOT _Playing || NOT CanPlay() || NOT FMath::IsFinite(DeltaSeconds)
        || DeltaSeconds <= 0.0)
    { return; }

    _Time = FMath::Min(GetDuration(), _Time + InDelta);
    EvaluateCurrent();
    if (_Time >= GetDuration())
    { _Playing = false; }
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetPlaying(
        bool InPlaying)
    -> void
{
    if (NOT InPlaying)
    { _Playing = false; return; }

    if (NOT CanPlay())
    { return; }

    if (_Time >= GetDuration())
    {
        _Time = FCk_Time{};
        EvaluateCurrent();
    }
    _Playing = true;
}

auto
    FCk_ProceduralActionAuthoringSession::
    GetStatusText() const
    -> FText
{
    if (NOT _CompiledValid)
    {
        auto Text = FString(TEXT("Invalid draft. Save is available; authored playback is disabled."));
        for (const auto& Diagnostic : _Diagnostics)
        {
            Text += TEXT("\n") + ck::procedural_action_editor::Describe(Diagnostic);
        }
        return FText::FromString(Text);
    }

    if (NOT _AppearanceError.IsEmpty())
    {
        return FText::FromString(FString::Printf(
            TEXT("Preview config is invalid: %s Playback is disabled; the draft can be saved."),
            *_AppearanceError));
    }

    if (NOT _HasPose)
    {
        return LOCTEXT("MissingInitialPose",
            "Clip validated, but the authored preview cannot start: bind every slot to a limb of the subject in Channels.");
    }

    return FText::Format(LOCTEXT("AuthoredStatus",
        "Authored requested-space pose at {0}s / {1}s. No solved, contact or accepted evidence."),
        FText::AsNumber(_Time.Get_Seconds()), FText::AsNumber(GetDuration().Get_Seconds()));
}

auto
    FCk_ProceduralActionAuthoringSession::
    FindPoseGroup(
        const FGuid& InId) const
    -> const FCk_ProceduralActionPoseGroupView*
{
    return _PoseGroups.FindByPredicate([&InId](const FCk_ProceduralActionPoseGroupView& InView)
        { return InView.Get_Group().Get_Id() == InId; });
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetStagingTime(
        FName InPhaseId,
        FCk_FloatRange_0to1 InTime)
    -> void
{
    _StagingPhaseId = InPhaseId;
    _StagingPose.Set_Time(InTime);
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetStagingBody(
        const FTransform& InBody)
    -> void
{
    _StagingPose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
    _StagingPose.Set_Body(InBody);
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetStagingLimb(
        const FCk_ProceduralAction_LimbPoseTarget& InTarget)
    -> void
{
    auto& Limbs = _StagingPose.Get_Limbs();
    if (auto* Existing = Limbs.FindByPredicate([&InTarget](const FCk_ProceduralAction_LimbPoseTarget& InCandidate)
        { return InCandidate.Get_Slot() == InTarget.Get_Slot(); }))
    {
        *Existing = InTarget;
        return;
    }
    Limbs.Add(InTarget);
}

auto
    FCk_ProceduralActionAuthoringSession::
    SetParticipants(
        const TArray<FName>& InParticipants)
    -> void
{
    _Participants = InParticipants;
    if (NOT _Participants.Contains(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()))
    { _StagingPose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Keep); }
    _StagingPose.Get_Limbs().RemoveAll([this](const FCk_ProceduralAction_LimbPoseTarget& InTarget)
        { return NOT _Participants.Contains(InTarget.Get_Slot()); });
}

auto
    FCk_ProceduralActionAuthoringSession::
    LoadStagingFromPoseGroup(
        const FGuid& InId,
        FString& OutError)
    -> bool
{
    const auto* View = FindPoseGroup(InId);
    if (View == nullptr || NOT _Clip.IsValid())
    {
        OutError = TEXT("The pose is not part of the current clip revision.");
        return false;
    }
    if (View->Get_IsBroken())
    {
        OutError = View->Get_BrokenReason();
        return false;
    }
    auto Pose = FCk_ProceduralAction_PoseKey{};
    if (NOT ck::procedural_action_editor::ReadPoseKey(_Clip->Get_Phases(), View->Get_Group(), Pose, OutError))
    { return false; }
    _StagingPose = MoveTemp(Pose);
    _StagingPhaseId = View->Get_Group().Get_PhaseId();
    _Participants = View->Get_Group().Get_Channels();
    return true;
}

auto
    FCk_ProceduralActionAuthoringSession::
    EvaluateCurrent()
    -> void
{
    _Pose = {};
    _HasPose = _CompiledValid && ck::procedural_action::Evaluate(
        _Compiled, _Initial, _Time, _Pose);
    if (NOT _HasPose)
    { _Playing = false; }
}

#undef LOCTEXT_NAMESPACE
