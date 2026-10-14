#include "Session/CkProceduralActionEditorModes.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "UObject/UObjectGlobals.h"
#endif

namespace ck_procedural_action_editor_modes
{
    constexpr auto FixedStep = ck::time::Hz(60);

    auto
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
        const auto StepCount = InTime / FixedStep;
        if (NOT FMath::IsFinite(StepCount)
            || StepCount >= static_cast<double>(MAX_int64))
        { return false; }
        OutStep = FMath::RoundToInt64(StepCount);
        return true;
    }

    auto
        LastAcceptedRecord(
            const FCk_ProceduralActionRecordedStore& InRecording,
            FCk_Time InMaximumTime)
        -> const FCk_ProceduralAction_Record*
    {
        const auto& Records = InRecording.Get_Records();
        for (auto Index = Records.Num() - 1; Index >= 0; --Index)
        {
            const auto& Record = Records[Index];
            if (Record.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose
                && Record.Get_Receipt().Get_HasAcceptedPose()
                && Record.Get_Receipt().Get_Interval().Get_ToTime()
                    <= InMaximumTime + FCk_Time{1.0e-6})
            { return &Record; }
        }
        return nullptr;
    }
}

FCkProceduralActionEditorModes::
    FCkProceduralActionEditorModes(
        TWeakPtr<FCk_ProceduralActionAuthoringSession> InSession)
    : _Session(MoveTemp(InSession))
{
}

auto
    FCkProceduralActionEditorModes::
    SetError(
        FString InError)
    -> bool
{
    _Error = MoveTemp(InError);
    _Playing = false;
    return false;
}

auto
    FCkProceduralActionEditorModes::
    EnterAuthored()
    -> void
{
    if (_Closed)
    { return; }
    _Playing = false;
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    _Mode = EMode::Authored;
    _PendingSeekStep.Reset();
    _ScrubTargetStep.Reset();
    _Scrubbing = false;
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
    _Playing = false;
    _PieStopObserved = false;
    _PendingSeekStep.Reset();
    _ScrubTargetStep.Reset();
    _Scrubbing = false;
    _Solver.Reset();
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || NOT Session->IsCompiled())
    { return SetError(TEXT("Compile the authored clip before starting Solver.")); }
    if (Session->GetPreviewConfig() == nullptr)
    { return SetError(TEXT("Select a separate preview subject config before starting Solver.")); }
    _Solver = MakeUnique<FCk_ProceduralActionSolverWorld>();
    if (NOT _Solver->StartClip(Session->GetPreviewConfig(), Session->GetClip(),
        Session->GetPreviewConfig()->Get_SlotBindings(), Session->GetRevision()))
    { return SetError(_Solver->GetError()); }
    _SourceRevision = Session->GetRevision();
    _Invalidated = false;
    _Error.Reset();
    return true;
}

auto
    FCkProceduralActionEditorModes::
    EnterRecorded()
    -> bool
{
    if (_Closed)
    { return false; }
    const auto PreviousMode = _Mode;
    _Mode = EMode::Recorded;
    _Playing = false;
    _PendingSeekStep.Reset();
    _ScrubTargetStep.Reset();
    _Scrubbing = false;
    if (PreviousMode == EMode::Solver && _Solver.IsValid())
    {
        _Recorded = MakeUnique<FCk_ProceduralActionRecordedStore>(_Solver->GetRecording());
        _RecordedSourceRevision = _SourceRevision;
        _RecordedGeometryInvalidated = false;
        _Solver->Reset();
        _Solver.Reset();
        _SelectedRecordIndex = _Recorded->Get_Records().Num() - 1;
    }
    const auto Session = _Session.Pin();
    if (_Recorded.IsValid() && (NOT Session.IsValid()
        || Session->GetRevision() != _RecordedSourceRevision))
    { _RecordedGeometryInvalidated = true; }
    if (NOT _Recorded.IsValid())
    { return SetError(TEXT("No production receipt recording is available.")); }
    if (_RecordedGeometryInvalidated)
    { return SetError(TEXT("Recorded receipts belong to the previous clip/preview revision. Geometry hidden; restart Solver to record this revision.")); }
    _Playing = false;
    _Error.Reset();
    return true;
}

auto
    FCkProceduralActionEditorModes::
    SourceChanged()
    -> void
{
    if (_Closed)
    { return; }
    const auto Session = _Session.Pin();
    if (_Mode == EMode::Authored)
    {
        if (_Recorded.IsValid() && (NOT Session.IsValid()
            || Session->GetRevision() != _RecordedSourceRevision))
        { _RecordedGeometryInvalidated = true; }
        return;
    }
    const auto PinnedRevision = _Mode == EMode::Recorded
        ? _RecordedSourceRevision : _SourceRevision;
    if (Session.IsValid() && Session->GetRevision() == PinnedRevision)
    { return; }
    _Playing = false;
    _PendingSeekStep.Reset();
    _ScrubTargetStep.Reset();
    _Scrubbing = false;
    if (_Mode == EMode::Recorded)
    {
        _RecordedGeometryInvalidated = true;
        _Error = TEXT("Recorded receipts belong to the previous clip/preview revision. Geometry hidden; restart Solver to record this revision.");
        return;
    }
    _Invalidated = true;
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    _Error = TEXT("Clip or preview subject changed. Restart Solver to use the new revision.");
}

auto
    FCkProceduralActionEditorModes::
    PlayPause()
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || _Invalidated || NOT _Solver.IsValid()
        || _Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Error
        || _Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Finished)
    { return false; }
    if (NOT _Playing && NOT _Solver->Resume())
    { return false; }
    _Playing = NOT _Playing;
    return true;
}

auto
    FCkProceduralActionEditorModes::
    Frame()
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || _Invalidated || NOT _Solver.IsValid())
    { return false; }
    _Playing = false;
    _PendingSeekStep = (_PendingSeekStep.IsSet()
        ? _PendingSeekStep.GetValue() : _Solver->GetStep()) + 1;
    return true;
}

auto
    FCkProceduralActionEditorModes::
    ResetTime()
    -> bool
{
    return Seek(FCk_Time{});
}

auto
    FCkProceduralActionEditorModes::
    BeginScrub()
    -> void
{
    if (_Closed || _Mode != EMode::Solver || _Invalidated || NOT _Solver.IsValid())
    { return; }
    _Playing = false;
    _Solver->CancelSeek();
    _PendingSeekStep.Reset();
    _ScrubTargetStep = _Solver->GetStep();
    _Scrubbing = true;
}

auto
    FCkProceduralActionEditorModes::
    EndScrub()
    -> void
{
    if (NOT _Scrubbing)
    { return; }
    _Scrubbing = false;
    if (NOT _Closed && _Mode == EMode::Solver && NOT _Invalidated && _Solver.IsValid()
        && _ScrubTargetStep.IsSet())
    { _PendingSeekStep = _ScrubTargetStep.GetValue(); }
    _ScrubTargetStep.Reset();
}

auto
    FCkProceduralActionEditorModes::
    Seek(
        FCk_Time InTime)
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || _Invalidated || NOT _Solver.IsValid())
    { return false; }
    int64 Target = 0;
    if (NOT ck_procedural_action_editor_modes::ToStep(InTime, Target))
    { return SetError(TEXT("Seek time cannot be represented as a 60 Hz Solver step. Choose a shorter time.")); }
    _Playing = false;
    _Error.Reset();
    if (_Scrubbing)
    { _ScrubTargetStep = Target; }
    else
    { _PendingSeekStep = Target; }
    return true;
}

auto
    FCkProceduralActionEditorModes::
    Tick()
    -> bool
{
    if (_Closed || _Mode != EMode::Solver || _Invalidated || NOT _Solver.IsValid())
    { return false; }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->GetWorld() == nullptr)
    {
        _Playing = false;
        _PendingSeekStep.Reset();
        _ScrubTargetStep.Reset();
        _Scrubbing = false;
        const auto NeedsRefresh = NOT _PieStopObserved;
        _PieStopObserved = true;
        return NeedsRefresh;
    }
    if (_Scrubbing)
    { return false; }
    auto SeekServiced = false;
    if (_PendingSeekStep.IsSet())
    {
        const auto Target = _PendingSeekStep.GetValue();
        _PendingSeekStep.Reset();
        _Solver->CancelSeek();
        if (NOT _Solver->SeekToStep(Target))
        { return SetError(_Solver->GetError()); }
        SeekServiced = true;
    }
    if (NOT _Playing && _Solver->GetStatus() != FCk_ProceduralActionSolverWorld::EStatus::Seeking
        && _Solver->GetStatus() != FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return SeekServiced; }
    const auto Advanced = _Solver->TickOncePerEditorFrame();
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Error)
    { SetError(_Solver->GetError()); }
    else if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Finished)
    { _Playing = false; }
    return SeekServiced || Advanced;
}

auto
    FCkProceduralActionEditorModes::
    Close()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    _Playing = false;
    _PendingSeekStep.Reset();
    _ScrubTargetStep.Reset();
    _Scrubbing = false;
    if (_Solver.IsValid())
    { _Solver->Reset(); _Solver.Reset(); }
    _Recorded.Reset();
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
    if (_Scrubbing && _Mode == EMode::Solver)
    { return FText::FromString(TEXT("SOLVER: scrubbing; release to seek the production world.")); }
    if (_Mode == EMode::Recorded)
    {
        return FText::FromString(_Recorded.IsValid()
            ? FString::Printf(TEXT("RECORDED: %d copied records, %d gaps. Read-only; source world released."),
                _Recorded->Get_Records().Num(), _Recorded->Get_Gaps().Num())
            : TEXT("RECORDED: no copied evidence."));
    }
    if (NOT _Solver.IsValid())
    { return FText::FromString(TEXT("SOLVER: restart required.")); }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled
        && _Solver->GetWorld() == nullptr)
    { return FText::FromString(_Solver->GetError()); }
    if (_Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Preparing)
    { return FText::FromString(TEXT("SOLVER: preparing the walking subject and trusted floor support.")); }
    const auto PreviewStatus = _Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Finished
        ? FString{TEXT("complete")}
        : _Solver->GetObservationHorizonStep() > 0
            ? FString::Printf(TEXT("observing until step %lld"), _Solver->GetObservationHorizonStep())
            : FString{TEXT("observing")};
    return FText::FromString(FString::Printf(
        TEXT("SOLVER: production GamePreview step %lld; action %s; preview %s; %d copied receipts, %d gaps%s."),
        _Solver->GetStep(), _Solver->GetIsActionTerminal() ? TEXT("terminal") : TEXT("active"),
        *PreviewStatus,
        _Solver->GetRecording().Get_Records().Num(), _Solver->GetRecording().Get_Gaps().Num(),
        _Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Seeking ? TEXT("; seeking") : TEXT("")));
}

auto
    FCkProceduralActionEditorModes::
    GetTime() const
    -> FCk_Time
{
    return _Mode == EMode::Solver && _Solver.IsValid()
        ? ck_procedural_action_editor_modes::FixedStep
            * static_cast<double>(_Scrubbing && _ScrubTargetStep.IsSet()
                ? _ScrubTargetStep.GetValue() : _Solver->GetStep())
        : FCk_Time{};
}

auto
    FCkProceduralActionEditorModes::
    GetRequestedStep() const
    -> TOptional<int64>
{
    return _Scrubbing ? _ScrubTargetStep : _PendingSeekStep;
}

auto
    FCkProceduralActionEditorModes::
    GetDuration() const
    -> FCk_Time
{
    const auto Session = _Session.Pin();
    if (_Mode == EMode::Solver && _Solver.IsValid()
        && _Solver->GetObservationHorizonStep() > 0)
    {
        return ck_procedural_action_editor_modes::FixedStep
            * static_cast<double>(_Solver->GetObservationHorizonStep());
    }
    return Session.IsValid() ? Session->GetDuration() : FCk_Time{};
}

auto
    FCkProceduralActionEditorModes::
    SelectRecord(
        int32 InIndex)
    -> bool
{
    if (_Mode != EMode::Recorded || NOT _Recorded.IsValid()
        || NOT _Recorded->Get_Records().IsValidIndex(InIndex))
    { return false; }
    _SelectedRecordIndex = InIndex;
    return true;
}

auto
    FCkProceduralActionEditorModes::
    GetSelectedRecord() const
    -> const FCk_ProceduralAction_Record*
{
    return _Mode == EMode::Recorded && _Recorded.IsValid()
        ? _Recorded->Get_Records().IsValidIndex(_SelectedRecordIndex)
            ? &_Recorded->Get_Records()[_SelectedRecordIndex] : nullptr
        : nullptr;
}

auto
    FCkProceduralActionEditorModes::
    FindSolverRecord() const
    -> const FCk_ProceduralAction_Record*
{
    return _Solver.IsValid()
        ? ck_procedural_action_editor_modes::LastAcceptedRecord(
            _Solver->GetRecording(),
            ck_procedural_action_editor_modes::FixedStep
                * static_cast<double>(_Solver->GetStep())) : nullptr;
}

auto
    FCkProceduralActionEditorModes::
    GetDisplayedRecord() const
    -> const FCk_ProceduralAction_Record*
{
    if (_Mode == EMode::Solver)
    { return FindSolverRecord(); }
    if (_RecordedGeometryInvalidated)
    { return nullptr; }
    return GetSelectedRecord();
}

auto
    FCkProceduralActionEditorModes::
    GetDisplayedPose() const
    -> const FCk_ProceduralArticulation_Pose*
{
    const auto* Record = GetDisplayedRecord();
    return Record != nullptr && Record->Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose
        && Record->Get_Receipt().Get_HasAcceptedPose()
        ? &Record->Get_Receipt().Get_CurrentPose() : nullptr;
}

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_ProceduralAction_EditorModes_CoalescedSeekAndSource,
    "Ck.ProceduralAnimation.Action.Editor.Modes.CoalescedSeekAndSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralAction_EditorModes_CoalescedSeekAndSource::
    RunTest(
        const FString&)
    -> bool
{
    int64 PureStep = INDEX_NONE;
    TestTrue(TEXT("negative seek time maps to initial step"),
        ck_procedural_action_editor_modes::ToStep(FCk_Time{-0.25}, PureStep)
            && PureStep == 0);
    TestTrue(TEXT("ordinary seek time maps to 60 Hz step"),
        ck_procedural_action_editor_modes::ToStep(FCk_Time{0.2}, PureStep)
            && PureStep == 12);
    TestFalse(TEXT("huge finite seek time rejects before int64 conversion"),
        ck_procedural_action_editor_modes::ToStep(FCk_Time{1.0e300}, PureStep));
    TestEqual(TEXT("rejected pure conversion preserves previous step"), PureStep, int64{12});
    constexpr auto ClipPath = TEXT("/CkFoundation/ProceduralAnimation/Examples/"
        "Ck_ProceduralAction_BodyPulse_DA.Ck_ProceduralAction_BodyPulse_DA");
    constexpr auto PreviewPath = TEXT("/CkFoundation/ProceduralAnimation/Examples/"
        "Ck_ProceduralAction_BodyPulse_Preview_DA.Ck_ProceduralAction_BodyPulse_Preview_DA");
    auto* Clip = LoadObject<UCk_ProceduralAction_Data>(nullptr, ClipPath);
    auto* Preview = LoadObject<UCk_ProceduralActionPreview_Config>(nullptr, PreviewPath);
    if (NOT TestNotNull(TEXT("saved clip"), Clip)
        || NOT TestNotNull(TEXT("saved preview"), Preview))
    { return false; }
    auto Session = MakeShared<FCk_ProceduralActionAuthoringSession>(Clip);
    Session->SetPreviewConfig(Preview);
    auto Modes = FCkProceduralActionEditorModes{Session};
    if (NOT TestTrue(TEXT("production Solver starts"), Modes.StartSolver()))
    { Modes.Close(); return false; }
    const auto* WorldBefore = Modes.GetSolver()->GetWorld();
    const auto StepBefore = Modes.GetSolver()->GetStep();
    Modes.BeginScrub();
    TestTrue(TEXT("first captured drag value accepted"), Modes.Seek(FCk_Time{0.3}));
    TestFalse(TEXT("first captured drag frame does not service world"), Modes.Tick());
    TestTrue(TEXT("second captured drag value accepted"), Modes.Seek(FCk_Time{0.1}));
    TestFalse(TEXT("second captured drag frame does not service world"), Modes.Tick());
    TestTrue(TEXT("final captured drag value accepted"), Modes.Seek(FCk_Time{0.2}));
    TestFalse(TEXT("final captured drag frame does not service world"), Modes.Tick());
    TestTrue(TEXT("captured drag retains production world"),
        Modes.GetSolver()->GetWorld() == WorldBefore);
    TestEqual(TEXT("captured drag does not tick or rebuild"),
        Modes.GetSolver()->GetStep(), StepBefore);
    TestTrue(TEXT("captured drag displays target time"),
        FMath::IsNearlyEqual(Modes.GetTime().Get_Seconds(), 0.2));
    TestTrue(TEXT("captured drag reports deferred seek"),
        Modes.GetStatusText().ToString().Contains(TEXT("release to seek")));
    Modes.EndScrub();
    TestTrue(TEXT("release queues exactly latest drag target"),
        Modes.GetRequestedStep().IsSet()
            && Modes.GetRequestedStep().GetValue() == 12);
    TestFalse(TEXT("unrepresentable seek rejects without replacing queued target"),
        Modes.Seek(FCk_Time{1.0e300}));
    TestTrue(TEXT("queued target survives rejected seek"),
        Modes.GetRequestedStep().IsSet()
            && Modes.GetRequestedStep().GetValue() == 12);
    TestTrue(TEXT("transport reports rejected seek"),
        Modes.GetStatusText().ToString().Contains(TEXT("cannot be represented")));
    TestTrue(TEXT("valid seek clears rejected-seek diagnostic"),
        Modes.Seek(FCk_Time{0.2})
            && NOT Modes.GetStatusText().ToString().Contains(TEXT("cannot be represented")));
    TestTrue(TEXT("release services one queued seek"), Modes.Tick());
    TestFalse(TEXT("serviced seek queue clears"), Modes.GetRequestedStep().IsSet());
    TestEqual(TEXT("production Solver receives only the final seek target"),
        Modes.GetSolver()->GetSeekTargetStep(), int64{12});
    TestTrue(TEXT("Solver recording copies independently"), Modes.EnterRecorded());
    const auto* CopiedRecording = Modes.GetRecorded();
    TestNotNull(TEXT("copied recording remains available"), CopiedRecording);
    Modes.EnterAuthored();
    Session->Refresh();
    Modes.SourceChanged();
    TestFalse(TEXT("authored edit cannot reopen stale recorded geometry"),
        Modes.EnterRecorded());
    TestTrue(TEXT("recorded geometry invalidates on source revision"),
        Modes.GetRecordedGeometryInvalidated());
    TestNull(TEXT("stale recorded geometry cannot be displayed"), Modes.GetDisplayedRecord());
    TestTrue(TEXT("raw copied recording remains browsable for provenance"),
        Modes.GetRecorded() == CopiedRecording);
    TestTrue(TEXT("stale recording reports why geometry is hidden"),
        Modes.GetStatusText().ToString().Contains(TEXT("previous clip/preview revision")));
    Modes.Close();
    return NOT HasAnyErrors();
}

#endif
