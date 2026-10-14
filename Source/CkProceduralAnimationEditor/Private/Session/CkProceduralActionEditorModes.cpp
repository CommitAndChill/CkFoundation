#include "CkProceduralAnimationEditor/CkProceduralActionEditorModes.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

namespace ck_procedural_action_editor_modes
{
    constexpr auto FixedStep = ck::time::Hz(60);
}

FCk_ProceduralActionEditorModes::
    FCk_ProceduralActionEditorModes(
        TWeakPtr<FCk_ProceduralActionAuthoringSession> InSession)
    : _Session(MoveTemp(InSession))
{
}

auto
    FCk_ProceduralActionEditorModes::
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
    FCk_ProceduralActionEditorModes::
    SetError(
        FString InError)
    -> bool
{
    _Error = MoveTemp(InError);
    return false;
}

auto
    FCk_ProceduralActionEditorModes::
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
    FCk_ProceduralActionEditorModes::
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
    _SourceRevision = Session->Get_Revision();
    if (NOT Session->Get_IsCompiled())
    { return SetError(TEXT("The clip does not compile; fix the Issues to see the solved preview.")); }
    if (Session->Get_PreviewConfig() == nullptr)
    { return SetError(TEXT("Choose a Subject to see the solved preview.")); }
    _Solver = MakeUnique<FCk_ProceduralActionSolverWorld>();
    if (NOT _Solver->StartClip(Session->Get_PreviewConfig(), Session->Get_Clip(),
        Session->Get_PreviewConfig()->Get_SlotBindings(), Session->Get_Revision()))
    { return SetError(_Solver->Get_Error()); }
    _Error.Reset();
    return true;
}

auto
    FCk_ProceduralActionEditorModes::
    SourceChanged()
    -> void
{
    if (_Closed || _Mode == EMode::Authored)
    { return; }
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->Get_Revision() == _SourceRevision)
    { return; }
    StartSolver();
}

auto
    FCk_ProceduralActionEditorModes::
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
    FCk_ProceduralActionEditorModes::
    RecordFrames()
    -> void
{
    if (_Solver->Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return; }
    const auto Subject = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ControlSnapshot(_Solver->Get_Subject().ConvertToHandle());
    if (NOT Subject.Get_Available())
    { return; }
    const auto Frame = FSolvedFrame{Subject, FCk_ProceduralActionSolvedSnapshot::Make(*_Solver)};
    while (_Frames.Num() <= _Solver->Get_Step())
    { _Frames.Add(Frame); }
    if (NOT _ClipStartFrame.IsSet())
    {
        const auto Action = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ActionSnapshot(_Solver->Get_Action());
        auto ClipStep = int64{0};
        if (Action.Get_Available() && Action.Get_AcceptedTime() > FCk_Time{} && ToStep(Action.Get_AcceptedTime(), ClipStep))
        { _ClipStartFrame = FMath::Max(int64{0}, static_cast<int64>(_Frames.Num() - 1) - ClipStep); }
    }
}

auto
    FCk_ProceduralActionEditorModes::
    Get_FrameIndex(
        int64 InClipStep) const
    -> int64
{
    return InClipStep + _ClipStartFrame.Get(0);
}

auto
    FCk_ProceduralActionEditorModes::
    Tick()
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || NOT _Solver.IsValid())
    { return false; }
    if (_Solver->Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->Get_World() == nullptr)
    {
        const auto NeedsRefresh = NOT _PieStopObserved;
        _PieStopObserved = true;
        return NeedsRefresh;
    }
    const auto NeedsFrames = _Frames.IsEmpty() || NOT _ClipStartFrame.IsSet() || Get_FrameIndex(_RequestedStep) >= _Frames.Num();
    if (NOT NeedsFrames || Get_IsComplete())
    { return false; }
    const auto Advanced = _Solver->TickOncePerEditorFrame();
    if (_Solver->Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Error)
    { return SetError(_Solver->Get_Error()); }
    if (Advanced)
    { RecordFrames(); }
    return Advanced;
}

auto
    FCk_ProceduralActionEditorModes::
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
    FCk_ProceduralActionEditorModes::
    Get_StatusText() const
    -> FText
{
    if (NOT _Error.IsEmpty())
    { return FText::FromString(_Error); }
    if (_Mode == EMode::Authored)
    { return FText::FromString(TEXT("AUTHORED: requested geometry only.")); }
    if (NOT _Solver.IsValid())
    { return FText::FromString(TEXT("SOLVER: not started.")); }
    if (_Solver->Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->Get_World() == nullptr)
    { return FText::FromString(_Solver->Get_Error()); }
    if (_Solver->Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return FText::FromString(TEXT("SOLVER: preparing the subject and its floor.")); }
    return FText::FromString(FString::Printf(TEXT("SOLVER: %d frames solved%s; %d receipts, %d gaps."),
        _Frames.Num(), Get_IsComplete() ? TEXT(", complete") : TEXT(""),
        _Solver->Get_Recording().Get_Records().Num(), _Solver->Get_Recording().Get_Gaps().Num()));
}

auto
    FCk_ProceduralActionEditorModes::
    Get_DisplayedFrame() const
    -> const FSolvedFrame*
{
    if (_Mode != EMode::Solver || _Frames.IsEmpty())
    { return nullptr; }
    return &_Frames[FMath::Clamp(Get_FrameIndex(_RequestedStep), int64{0}, static_cast<int64>(_Frames.Num() - 1))];
}

auto
    FCk_ProceduralActionEditorModes::
    Get_Time() const
    -> FCk_Time
{
    if (_Mode != EMode::Solver || _Frames.IsEmpty())
    { return FCk_Time{}; }
    const auto Index = FMath::Clamp(Get_FrameIndex(_RequestedStep), int64{0}, static_cast<int64>(_Frames.Num() - 1));
    return ck_procedural_action_editor_modes::FixedStep * static_cast<double>(FMath::Max(int64{0}, Index - _ClipStartFrame.Get(0)));
}

auto
    FCk_ProceduralActionEditorModes::
    Get_SolvedTime() const
    -> FCk_Time
{
    const auto LastClipStep = static_cast<int64>(_Frames.Num() - 1) - _ClipStartFrame.Get(0);
    return _Frames.IsEmpty() ? FCk_Time{} : ck_procedural_action_editor_modes::FixedStep * static_cast<double>(FMath::Max(int64{0}, LastClipStep));
}

auto
    FCk_ProceduralActionEditorModes::
    Get_IsComplete() const
    -> bool
{
    if (NOT _Solver.IsValid())
    { return true; }
    const auto Status = _Solver->Get_Status();
    return Status == FCk_ProceduralActionSolverWorld::EStatus::Finished
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Error
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        || Status == FCk_ProceduralActionSolverWorld::EStatus::Idle;
}

auto
    FCk_ProceduralActionEditorModes::
    Get_Duration() const
    -> FCk_Time
{
    const auto Session = _Session.Pin();
    const auto ClipDuration = Session.IsValid() ? Session->Get_Duration() : FCk_Time{};
    if (_Mode != EMode::Solver)
    { return ClipDuration; }
    return Get_SolvedTime() > ClipDuration ? Get_SolvedTime() : ClipDuration;
}
