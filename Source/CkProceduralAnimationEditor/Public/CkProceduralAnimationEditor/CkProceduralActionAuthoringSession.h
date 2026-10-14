#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

#include "UObject/StrongObjectPtr.h"

// A clip time inside one section: the section and the key time there. A key time is the section's eased progress at that
// elapsed time (its preset easing or custom curve), the time keys are sampled at; markers and windows stay on raw time.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionKeyTime
{
    CK_GENERATED_BODY(FCk_ProceduralActionKeyTime);

private:
    FName _PhaseId;
    FCk_FloatRange_0to1 _KeyTime;

public:
    CK_PROPERTY_GET(_PhaseId);
    CK_PROPERTY_GET(_KeyTime);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionKeyTime, _PhaseId, _KeyTime);
};

/** The compiled view of one clip on one preview subject. It holds no playhead: the editor model owns the time, and every
 *  view evaluates the clip at the model's time through EvaluateAt. Get_Revision changes only when what a preview plays
 *  changes (the compiled content, its subject or its preview config), so an edit that rebuilds nothing restarts nothing. */
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionAuthoringSession
{
public:
    // The preview's step: the Solver simulates and the transport steps at this rate.
    static constexpr auto FramesPerSecond = 60.0;

    explicit FCk_ProceduralActionAuthoringSession(
        UCk_ProceduralAction_Data* InClip);

    // Previews the clip on InConfig as given; the clip's editor subject is ignored until UseDerivedPreview.
    auto
        SetPreviewConfig(
            UCk_ProceduralActionPreview_Config* InConfig)
        -> void;
    // Previews the clip on the preview derived from its editor subject, derived again whenever the subject changes.
    auto
        UseDerivedPreview()
        -> void;
    auto
        Refresh()
        -> void;
    // The authored pose at InTime; false when the clip does not compile or InTime is outside it.
    auto
        EvaluateAt(
            FCk_Time InTime,
            ck::procedural_action::FEvaluatedPose& OutPose) const
        -> bool;

    auto
        Get_Clip() const
        -> UCk_ProceduralAction_Data*
    { return _Clip.Get(); }
    auto
        Get_PreviewConfig() const
        -> UCk_ProceduralActionPreview_Config*
    { return _PreviewConfig.Get(); }
    auto
        Get_CompiledClip() const
        -> const ck::procedural_action::FCompiledClip&
    { return _Compiled; }
    // Each section's timing and easing; valid whenever the section durations are, even when the clip does not compile.
    auto
        Get_Timing() const
        -> const TArray<ck::procedural_action::FCompiledPhase>&
    { return _Timing; }
    auto
        Get_Diagnostics() const
        -> const TArray<ck::procedural_action::FDiagnostic>&
    { return _Diagnostics; }
    auto
        Get_Revision() const
        -> uint64
    { return _Revision; }
    auto
        Get_Duration() const
        -> FCk_Time
    { return _Compiled.Get_Duration(); }
    auto
        Get_IsCompiled() const
        -> bool
    { return _CompiledValid; }
    // The clip compiles, its preview is valid, and the preview can start it.
    auto
        Get_CanPlay() const
        -> bool
    { return _CompiledValid && _HasStart && _AppearanceError.IsEmpty(); }
    auto
        Get_StatusText(
            FCk_Time InTime) const
        -> FText;

    // A time at a section boundary belongs to the section that ends there (its key at 1), never to the next section's start;
    // only time 0 is the first section's start. Both need valid section durations, not a compiling clip.
    auto
        ElapsedToKey(
            FCk_Time InElapsed,
            FCk_ProceduralActionKeyTime& OutKey) const
        -> bool;
    auto
        KeyToElapsed(
            FName InPhaseId,
            FCk_FloatRange_0to1 InKeyTime,
            FCk_Time& OutElapsed) const
        -> bool;
    // The section InTime falls in and the raw, uneased fraction there, the time markers and windows are placed at. A time
    // past the last section falls in it at 1.
    auto
        ToRawFraction(
            FCk_Time InTime,
            FName& OutPhaseId,
            double& OutFraction) const
        -> bool;
    auto
        Get_PhaseStart(
            FName InPhaseId,
            FCk_Time& OutStart) const
        -> bool;
    // The preview frame nearest InTime.
    static auto
        SnapToFrame(
            FCk_Time InTime)
        -> FCk_Time;

private:
    auto
        RefreshTiming()
        -> void;
    auto
        RefreshDerivedPreview()
        -> void;

private:
    TStrongObjectPtr<UCk_ProceduralAction_Data> _Clip;
    TStrongObjectPtr<UCk_ProceduralActionPreview_Config> _PreviewConfig;
    TOptional<FCk_ProceduralActionEditor_Subject> _DerivedFrom;
    TArray<ck::procedural_action::FCompiledPhase> _Timing;
    ck::procedural_action::FCompiledClip _Compiled;
    ck::procedural_action::FInitialPose _Initial;
    TArray<ck::procedural_action::FDiagnostic> _Diagnostics;
    FString _AppearanceError;
    // What the current revision was built from: a refresh that finds all three unchanged keeps the revision.
    TOptional<uint64> _RevisionContentHash;
    FCk_ProceduralActionEditor_Subject _RevisionSubject;
    TWeakObjectPtr<UCk_ProceduralActionPreview_Config> _RevisionConfig;
    uint64 _Revision = 0;
    bool _CompiledValid = false;
    bool _HasStart = false;
};
