#include "CkProceduralAnimationEditor/CkProceduralActionEditorModes.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

namespace ck_procedural_action_editor_modes
{
    constexpr auto FixedStep = ck::time::Hz(60);
}

FCkProceduralActionEditorModes::
    FCkProceduralActionEditorModes(
        TWeakPtr<FCk_ProceduralActionAuthoringSession> InSession)
    : _Session(MoveTemp(InSession))
{
}

auto
    FCkProceduralActionEditorModes::
    ToStep(
        FCk_Time InTime,
        int64& OutStep)
    -> bool
{
    const auto Seconds = InTime.Get_Seconds();
    if (NOT FMath::IsFinite(Seconds))
    { return false; }
    if (Seconds <= 0.0)
    {
        OutStep = 0;
        return true;
    }
    const auto StepCount = InTime / ck_procedural_action_editor_modes::FixedStep;
    if (NOT FMath::IsFinite(StepCount)
        || StepCount >= static_cast<double>(MAX_int64))
    { return false; }
    OutStep = FMath::RoundToInt64(StepCount);
    return true;
}

auto
    FCkProceduralActionEditorModes::
    SetError(
        FString InError)
    -> bool
{
    _Error = MoveTemp(InError);
    return false;
}

auto
    FCkProceduralActionEditorModes::
    EnterAuthored()
    -> void
{
    if (_Closed)
    { return; }
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    _Frames.Reset();
    _Mode = EMode::Authored;
    _Error.Reset();
}

auto
    FCkProceduralActionEditorModes::
    StartSolver()
    -> bool
{
    if (_Closed)
    { return false; }
    _Mode = EMode::Solver;
    _PieStopObserved = false;
    _Frames.Reset();
    _ClipStartFrame.Reset();
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid())
    { return SetError(TEXT("The clip editor closed.")); }
    _SourceRevision = Session->GetRevision();
    if (NOT Session->IsCompiled())
    { return SetError(TEXT("The clip does not compile; fix the Issues to see the solved preview.")); }
    if (Session->GetPreviewConfig() == nullptr)
    { return SetError(TEXT("Choose a Subject to see the solved preview.")); }
    _Solver = MakeUnique<FCk_ProceduralActionSolverWorld>();
    if (NOT _Solver->StartClip(Session->GetPreviewConfig(), Session->GetClip(),
        Session->GetPreviewConfig()->Get_SlotBindings(), Session->GetRevision()))
    { return SetError(_Solver->GetError()); }
    _Error.Reset();
    return true;
}

auto
    FCkProceduralActionEditorModes::
    SourceChanged()
    -> void
{
    if (_Closed || _Mode == EMode::Authored)
    { return; }
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetRevision() == _SourceRevision)
    { return; }
    StartSolver();
}

auto
    FCkProceduralActionEditorModes::
    Show(
        FCk_Time InTime)
    -> bool
{
    if (_Closed || _Mode != EMode::Solver)
    { return false; }
    auto Step = int64{0};
    if (NOT ToStep(InTime, Step))
    { return SetError(TEXT("That time cannot be represented as a 60 Hz Solver step. Choose a shorter time.")); }
    _RequestedStep = Step;
    return true;
}

auto
    FCkProceduralActionEditorModes::
    RecordFrames()
    -> void
{
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return; }
    const auto Subject = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ControlSnapshot(_Solver->GetSubject().ConvertToHandle());
    if (NOT Subject.Get_Available())
    { return; }
    const auto Frame = FSolvedFrame{Subject, FCk_ProceduralActionSolvedSnapshot::Make(*_Solver)};
    while (_Frames.Num() <= _Solver->GetStep())
    { _Frames.Add(Frame); }
    if (NOT _ClipStartFrame.IsSet())
    {
        const auto Action = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ActionSnapshot(_Solver->GetAction());
        auto ClipStep = int64{0};
        if (Action.Get_Available() && Action.Get_AcceptedTime() > FCk_Time{} && ToStep(Action.Get_AcceptedTime(), ClipStep))
        { _ClipStartFrame = FMath::Max(int64{0}, static_cast<int64>(_Frames.Num() - 1) - ClipStep); }
    }
}

auto
    FCkProceduralActionEditorModes::
    Get_FrameIndex(
        int64 InClipStep) const
    -> int64
{
    return InClipStep + _ClipStartFrame.Get(0);
}

auto
    FCkProceduralActionEditorModes::
    Tick()
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || NOT _Solver.IsValid())
    { return false; }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->GetWorld() == nullptr)
    {
        const auto NeedsRefresh = NOT _PieStopObserved;
        _PieStopObserved = true;
        return NeedsRefresh;
    }
    const auto NeedsFrames = _Frames.IsEmpty() || NOT _ClipStartFrame.IsSet() || Get_FrameIndex(_RequestedStep) >= _Frames.Num();
    if (NOT NeedsFrames || GetIsComplete())
    { return false; }
    const auto Advanced = _Solver->TickOncePerEditorFrame();
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Error)
    { return SetError(_Solver->GetError()); }
    if (Advanced)
    { RecordFrames(); }
    return Advanced;
}

auto
    FCkProceduralActionEditorModes::
    Close()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    _Frames.Reset();
    _Session.Reset();
}

auto
    FCkProceduralActionEditorModes::
    GetStatusText() const
    -> FText
{
    if (NOT _Error.IsEmpty())
    { return FText::FromString(_Error); }
    if (_Mode == EMode::Authored)
    { return FText::FromString(TEXT("AUTHORED: requested geometry only.")); }
    if (NOT _Solver.IsValid())
    { return FText::FromString(TEXT("SOLVER: not started.")); }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->GetWorld() == nullptr)
    { return FText::FromString(_Solver->GetError()); }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return FText::FromString(TEXT("SOLVER: preparing the subject and its floor.")); }
    return FText::FromString(FString::Printf(TEXT("SOLVER: %d frames solved%s; %d receipts, %d gaps."),
        _Frames.Num(), GetIsComplete() ? TEXT(", complete") : TEXT(""),
        _Solver->GetRecording().Get_Records().Num(), _Solver->GetRecording().Get_Gaps().Num()));
}

auto
    FCkProceduralActionEditorModes::
    GetDisplayedFrame() const
    -> const FSolvedFrame*
{
    if (_Mode != EMode::Solver || _Frames.IsEmpty())
    { return nullptr; }
    return &_Frames[FMath::Clamp(Get_FrameIndex(_RequestedStep), int64{0}, static_cast<int64>(_Frames.Num() - 1))];
}

auto
    FCkProceduralActionEditorModes::
    GetTime() const
    -> FCk_Time
{
    if (_Mode != EMode::Solver || _Frames.IsEmpty())
    { return FCk_Time{}; }
    const auto Index = FMath::Clamp(Get_FrameIndex(_RequestedStep), int64{0}, static_cast<int64>(_Frames.Num() - 1));
    return ck_procedural_action_editor_modes::FixedStep * static_cast<double>(FMath::Max(int64{0}, Index - _ClipStartFrame.Get(0)));
}

auto
    FCkProceduralActionEditorModes::
    GetSolvedTime() const
    -> FCk_Time
{
    const auto LastClipStep = static_cast<int64>(_Frames.Num() - 1) - _ClipStartFrame.Get(0);
    return _Frames.IsEmpty() ? FCk_Time{} : ck_procedural_action_editor_modes::FixedStep * static_cast<double>(FMath::Max(int64{0}, LastClipStep));
}

auto
    FCkProceduralActionEditorModes::
    GetIsComplete() const
    -> bool
{
    if (NOT _Solver.IsValid())
    { return true; }
    const auto Status = _Solver->GetStatus();
    return Status == FCk_ProceduralActionSolverWorld::EStatus::Finished
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Error
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Idle;
}

auto
    FCkProceduralActionEditorModes::
    GetDuration() const
    -> FCk_Time
{
    const auto Session = _Session.Pin();
    const auto ClipDuration = Session.IsValid() ? Session->GetDuration() : FCk_Time{};
    if (_Mode != EMode::Solver)
    { return ClipDuration; }
    return GetSolvedTime() > ClipDuration ? GetSolvedTime() : ClipDuration;
}
