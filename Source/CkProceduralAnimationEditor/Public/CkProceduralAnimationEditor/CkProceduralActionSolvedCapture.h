#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

class FCk_ProceduralActionSolverWorld;
class UCk_ProceduralActionPreview_Config;

// --------------------------------------------------------------------------------------------------------------------

// A copy of the Solver evidence a solved capture may read. Ready: the production preview is paused at a step with no
// seek or preparation pending. Settled: the copied accepted pose is the subject's latest settled revision, so the
// displayed geometry is the subject's current pose rather than an older record.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionSolvedSnapshot
{
    CK_GENERATED_BODY(FCk_ProceduralActionSolvedSnapshot);

private:
    bool _Ready = false;
    bool _Settled = false;
    uint64 _PreviewRevision = 0;
    FCk_ProceduralArticulation_Pose _Pose;

public:
    CK_PROPERTY(_Ready);
    CK_PROPERTY(_Settled);
    CK_PROPERTY(_PreviewRevision);
    CK_PROPERTY(_Pose);

    static auto
        Make(
            const FCk_ProceduralActionSolverWorld& InSolver)
        -> FCk_ProceduralActionSolvedSnapshot;
};

// --------------------------------------------------------------------------------------------------------------------

// Where a solved capture writes and what it must match: the preview subject the Solver ran, the authoring session
// revision it ran against, the pose time and the channels (slots, or the body channel name) to capture.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionSolvedCaptureTarget
{
    CK_GENERATED_BODY(FCk_ProceduralActionSolvedCaptureTarget);

private:
    const UCk_ProceduralActionPreview_Config* _Config = nullptr;
    uint64 _SessionRevision = 0;
    FName _PhaseId;
    FCk_FloatRange_0to1 _Time;
    TArray<FName> _Channels;
    FName _Label;

public:
    auto
        Get_Config() const
        -> const UCk_ProceduralActionPreview_Config*
    { return _Config; }

    CK_PROPERTY_GET(_SessionRevision);
    CK_PROPERTY_GET(_PhaseId);
    CK_PROPERTY_GET(_Time);
    CK_PROPERTY_GET(_Channels);
    CK_PROPERTY(_Label);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionSolvedCaptureTarget, _Config, _SessionRevision, _PhaseId, _Time, _Channels);
};

// --------------------------------------------------------------------------------------------------------------------
