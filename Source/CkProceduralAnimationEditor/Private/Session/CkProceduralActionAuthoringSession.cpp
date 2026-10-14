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
    _DerivedFrom = FCk_ProceduralActionEditor_Subject{};
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
    const auto Subject = ck::procedural_action_editor::Get_Subject(*_Clip);
    const auto SubjectChanged = NOT FCk_ProceduralActionEditor_Subject::StaticStruct()->CompareScriptStruct(
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
    ToRawFraction(
        FCk_Time InTime,
        FName& OutPhaseId,
        double& OutFraction) const
    -> bool
{
    if (_Timing.IsEmpty() || NOT FMath::IsFinite(InTime.Get_Seconds()))
    { return false; }
    for (const auto& Phase : _Timing)
    {
        const auto End = Phase.Get_Start() + Phase.Get_Duration();
        if (InTime.Get_Seconds() <= End.Get_Seconds() + 1.0e-9 || &Phase == &_Timing.Last())
        {
            OutPhaseId = Phase.Get_Id();
            OutFraction = FMath::Clamp((InTime - Phase.Get_Start()).Get_Seconds() / Phase.Get_Duration().Get_Seconds(), 0.0, 1.0);
            return true;
        }
    }
    return false;
}

auto
    FCk_ProceduralActionAuthoringSession::
    SnapToFrame(
        FCk_Time InTime)
    -> FCk_Time
{ return FCk_Time{FMath::RoundToDouble(InTime.Get_Seconds() * FramesPerSecond) / FramesPerSecond}; }

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
    _Compiled = {};
    _Diagnostics.Reset();
    _AppearanceError.Reset();
    _CompiledValid = _Clip.IsValid()
        && ck::procedural_action::Compile(*_Clip.Get(), _Compiled, _Diagnostics);
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

    auto Start = ck::procedural_action::FEvaluatedPose{};
    _HasStart = EvaluateAt(FCk_Time{}, Start);

    const auto ContentHash = _CompiledValid ? TOptional<uint64>{_Compiled.Get_ContentHash()} : TOptional<uint64>{};
    const auto Subject = _Clip.IsValid() ? ck::procedural_action_editor::Get_Subject(*_Clip.Get()) : FCk_ProceduralActionEditor_Subject{};
    const auto SubjectChanged = NOT FCk_ProceduralActionEditor_Subject::StaticStruct()->CompareScriptStruct(
        &_RevisionSubject, &Subject, PPF_None);
    if (_Revision == 0 || ContentHash != _RevisionContentHash || SubjectChanged || _RevisionConfig.Get() != _PreviewConfig.Get())
    {
        ++_Revision;
        _RevisionContentHash = ContentHash;
        _RevisionSubject = Subject;
        _RevisionConfig = _PreviewConfig.Get();
    }
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
        && InTime >= FCk_Time{} && InTime <= Get_Duration()
        && ck::procedural_action::Evaluate(_Compiled, _Initial, InTime, OutPose);
}

auto
    FCk_ProceduralActionAuthoringSession::
    Get_StatusText(
        FCk_Time InTime) const
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

    if (NOT _HasStart)
    {
        return LOCTEXT("MissingInitialPose",
            "Clip validated, but the authored preview cannot start: bind every slot to a limb of the subject in Channels.");
    }

    return FText::Format(LOCTEXT("AuthoredStatus",
        "Authored requested-space pose at {0}s / {1}s. No solved, contact or accepted evidence."),
        FText::AsNumber(InTime.Get_Seconds()), FText::AsNumber(Get_Duration().Get_Seconds()));
}

#undef LOCTEXT_NAMESPACE
