#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"
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

class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionAuthoringSession
{
public:
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
    auto
        SetTime(
            FCk_Time InTime)
        -> void;
    auto
        EvaluateAt(
            FCk_Time InTime,
            ck::procedural_action::FEvaluatedPose& OutPose) const
        -> bool;
    auto
        Advance(
            FCk_Time InDelta)
        -> void;
    auto
        SetPlaying(
            bool InPlaying)
        -> void;

    auto
        GetClip() const
        -> UCk_ProceduralAction_Data*
    {
        return _Clip.Get();
    }
    auto
        GetPreviewConfig() const
        -> UCk_ProceduralActionPreview_Config*
    {
        return _PreviewConfig.Get();
    }
    auto
        GetCompiledClip() const
        -> const ck::procedural_action::FCompiledClip&
    {
        return _Compiled;
    }
    auto
        GetPose() const
        -> const ck::procedural_action::FEvaluatedPose&
    {
        return _Pose;
    }
    // Each section's timing and easing; valid whenever the section durations are, even when the clip does not compile.
    auto
        GetTiming() const
        -> const TArray<ck::procedural_action::FCompiledPhase>&
    {
        return _Timing;
    }
    auto
        GetDiagnostics() const
        -> const TArray<ck::procedural_action::FDiagnostic>&
    {
        return _Diagnostics;
    }
    auto
        GetTime() const
        -> FCk_Time
    {
        return _Time;
    }
    auto
        GetRevision() const
        -> uint64
    {
        return _Revision;
    }
    auto
        GetDuration() const
        -> FCk_Time
    {
        return _Compiled.Get_Duration();
    }
    auto
        IsCompiled() const
        -> bool
    {
        return _CompiledValid;
    }
    auto
        HasPose() const
        -> bool
    {
        return _HasPose;
    }
    auto
        IsPlaying() const
        -> bool
    {
        return _Playing;
    }
    auto
        CanPlay() const
        -> bool
    {
        return _CompiledValid && _HasPose && _AppearanceError.IsEmpty();
    }
    auto
        GetStatusText() const
        -> FText;

    // Pose groups of the current revision: the clip's persisted groups plus temporary ones derived for unclaimed key
    // times. Rebuilt by Refresh, never per frame.
    auto
        GetPoseGroups() const
        -> const TArray<FCk_ProceduralActionPoseGroupView>&
    {
        return _PoseGroups;
    }
    auto
        FindPoseGroup(
            const FGuid& InId) const
        -> const FCk_ProceduralActionPoseGroupView*;

    // The staging pose holds the requested targets the author is editing. It belongs to the session: nothing here writes
    // the clip or the Solver world, and only an explicit capture command turns it into keys.
    auto
        SetStagingTime(
            FName InPhaseId,
            FCk_FloatRange_0to1 InTime)
        -> void;
    auto
        SetStagingBody(
            const FTransform& InBody)
        -> void;
    auto
        SetStagingLimb(
            const FCk_ProceduralAction_LimbPoseTarget& InTarget)
        -> void;
    // Participants are the channels a whole-pose capture must cover; staged targets outside them are released.
    auto
        SetParticipants(
            const TArray<FName>& InParticipants)
        -> void;
    auto
        LoadStagingFromPoseGroup(
            const FGuid& InId,
            FString& OutError)
        -> bool;
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
    auto
        Get_PhaseStart(
            FName InPhaseId,
            FCk_Time& OutStart) const
        -> bool;

    auto
        GetStagingPhaseId() const
        -> FName
    {
        return _StagingPhaseId;
    }
    auto
        GetStagingPose() const
        -> const FCk_ProceduralAction_PoseKey&
    {
        return _StagingPose;
    }
    auto
        GetParticipants() const
        -> const TArray<FName>&
    {
        return _Participants;
    }

private:
    auto
        EvaluateCurrent()
        -> void;
    auto
        RefreshTiming()
        -> void;
    auto
        RefreshDerivedPreview()
        -> void;

private:
    TStrongObjectPtr<UCk_ProceduralAction_Data> _Clip;
    TStrongObjectPtr<UCk_ProceduralActionPreview_Config> _PreviewConfig;
    TOptional<FCk_ProceduralAction_EditorSubject> _DerivedFrom;
    TArray<ck::procedural_action::FCompiledPhase> _Timing;
    ck::procedural_action::FCompiledClip _Compiled;
    ck::procedural_action::FInitialPose _Initial;
    ck::procedural_action::FEvaluatedPose _Pose;
    TArray<ck::procedural_action::FDiagnostic> _Diagnostics;
    TArray<FCk_ProceduralActionPoseGroupView> _PoseGroups;
    FCk_ProceduralAction_PoseKey _StagingPose;
    FName _StagingPhaseId;
    TArray<FName> _Participants;
    FString _AppearanceError;
    FCk_Time _Time;
    uint64 _Revision = 0;
    bool _CompiledValid = false;
    bool _HasPose = false;
    bool _Playing = false;
};
