#pragma once

#include "CkProceduralAnimationEditor/CkProceduralActionRecordedStore.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolverWorld.h"

class FCk_ProceduralActionAuthoringSession;

// Toolkit-local transport. Recorded is a copied value snapshot and never owns the source world.
class FCkProceduralActionEditorModes
{
public:
    enum class EMode : uint8 { Authored, Solver, Recorded };

    explicit FCkProceduralActionEditorModes(
        TWeakPtr<FCk_ProceduralActionAuthoringSession> InSession);

    auto
        EnterAuthored()
        -> void;
    auto
        StartSolver()
        -> bool;
    auto
        EnterRecorded()
        -> bool;
    auto
        SourceChanged()
        -> void;
    auto
        PlayPause()
        -> bool;
    auto
        Frame()
        -> bool;
    auto
        ResetTime()
        -> bool;
    auto
        BeginScrub()
        -> void;
    auto
        EndScrub()
        -> void;
    auto
        Seek(
            FCk_Time InTime)
        -> bool;
    auto
        Tick()
        -> bool;
    auto
        Close()
        -> void;

    auto
        GetMode() const
        -> EMode
    {
        return _Mode;
    }
    auto
        GetIsPlaying() const
        -> bool
    {
        return _Playing;
    }
    auto
        GetStatusText() const
        -> FText;
    auto
        GetTime() const
        -> FCk_Time;
    auto
        GetRequestedStep() const
        -> TOptional<int64>;
    auto
        GetDuration() const
        -> FCk_Time;
    auto
        GetSolver() const
        -> const FCk_ProceduralActionSolverWorld*
    {
        return _Solver.Get();
    }
    auto
        GetRecorded() const
        -> const FCk_ProceduralActionRecordedStore*
    {
        return _Recorded.Get();
    }
    auto
        GetSelectedRecord() const
        -> const FCk_ProceduralAction_Record*;
    auto
        GetSelectedRecordIndex() const
        -> int32
    {
        return _SelectedRecordIndex;
    }
    auto
        SelectRecord(
            int32 InIndex)
        -> bool;
    auto
        GetDisplayedRecord() const
        -> const FCk_ProceduralAction_Record*;
    auto
        GetDisplayedPose() const
        -> const FCk_ProceduralArticulation_Pose*;
    auto
        GetRecordedGeometryInvalidated() const
        -> bool
    {
        return _RecordedGeometryInvalidated;
    }

private:
    auto
        FindSolverRecord() const
        -> const FCk_ProceduralAction_Record*;
    auto
        SetError(
            FString InError)
        -> bool;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TUniquePtr<FCk_ProceduralActionSolverWorld> _Solver;
    TUniquePtr<FCk_ProceduralActionRecordedStore> _Recorded;
    EMode _Mode = EMode::Authored;
    FString _Error;
    int32 _SelectedRecordIndex = INDEX_NONE;
    uint64 _SourceRevision = 0;
    uint64 _RecordedSourceRevision = 0;
    TOptional<int64> _PendingSeekStep;
    TOptional<int64> _ScrubTargetStep;
    bool _Scrubbing = false;
    bool _Playing = false;
    bool _Closed = false;
    bool _Invalidated = false;
    bool _RecordedGeometryInvalidated = false;
    bool _PieStopObserved = false;
};
