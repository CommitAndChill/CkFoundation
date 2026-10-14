#pragma once

#include "CkProceduralAnimation/Debug/CkProceduralAction_Debug.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolvedCapture.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolverWorld.h"

class FCk_ProceduralActionAuthoringSession;

// The playback sources behind the editor's transport: the authored session or the production Solver world. The Solver
// simulates forward one fixed step per editor frame and keeps every solved frame, so scrubbing back and replaying read the
// cache and never rebuild the world; it simulates only as far as the requested time needs. Times are clip times: the
// action's entry blend precedes time zero, and frames after the clip's end show the subject returning to its gait. The
// editor model polls it; it holds no selection.
class CKPROCEDURALANIMATIONEDITOR_API FCkProceduralActionEditorModes
{
public:
    enum class EMode : uint8 { Authored, Solver };

    struct FSolvedFrame
    {
        FCk_ProceduralAction_DebugSubject Subject;
        FCk_ProceduralActionSolvedSnapshot Capture;
    };

    explicit FCkProceduralActionEditorModes(
        TWeakPtr<FCk_ProceduralActionAuthoringSession> InSession);

    // The 60 Hz Solver step nearest InTime; false when the time cannot be represented as a step.
    static auto
        ToStep(
            FCk_Time InTime,
            int64& OutStep)
        -> bool;

    auto
        EnterAuthored()
        -> void;
    // Builds a fresh production world for the session's current revision and drops the solved frames.
    auto
        StartSolver()
        -> bool;
    // In Solver mode, a new session revision restarts the Solver; a revision that already failed to start is not retried.
    auto
        SourceChanged()
        -> void;
    // The time to display; the Solver simulates forward to it when it is past the solved frames.
    auto
        Show(
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
        GetStatusText() const
        -> FText;
    // The time of the displayed solved frame.
    auto
        GetTime() const
        -> FCk_Time;
    auto
        GetDuration() const
        -> FCk_Time;
    // The clip time of the last solved frame; past the clip's duration once the action has ended.
    auto
        GetSolvedTime() const
        -> FCk_Time;
    // No frame will be added: the Solver finished, failed or was released.
    auto
        GetIsComplete() const
        -> bool;
    auto
        GetFrameCount() const
        -> int32
    {
        return _Frames.Num();
    }
    auto
        GetDisplayedFrame() const
        -> const FSolvedFrame*;
    auto
        GetSolver() const
        -> const FCk_ProceduralActionSolverWorld*
    {
        return _Solver.Get();
    }

private:
    auto
        SetError(
            FString InError)
        -> bool;
    auto
        RecordFrames()
        -> void;
    auto
        Get_FrameIndex(
            int64 InClipStep) const
        -> int64;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TUniquePtr<FCk_ProceduralActionSolverWorld> _Solver;
    TArray<FSolvedFrame> _Frames;
    EMode _Mode = EMode::Authored;
    FString _Error;
    uint64 _SourceRevision = 0;
    int64 _RequestedStep = 0;
    // The frame at clip time zero, found once the action reports its first accepted time.
    TOptional<int64> _ClipStartFrame;
    bool _Closed = false;
    bool _PieStopObserved = false;
};
