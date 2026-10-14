#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

#include "UObject/StrongObjectPtr.h"

class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionAuthoringSession
{
public:
    explicit FCk_ProceduralActionAuthoringSession(
        UCk_ProceduralAction_Data* InClip);

    auto
        SetPreviewConfig(
            UCk_ProceduralActionPreview_Config* InConfig)
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

private:
    TStrongObjectPtr<UCk_ProceduralAction_Data> _Clip;
    TStrongObjectPtr<UCk_ProceduralActionPreview_Config> _PreviewConfig;
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
