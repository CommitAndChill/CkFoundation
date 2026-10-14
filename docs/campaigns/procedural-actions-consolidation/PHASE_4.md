# Phase 4 — Editor consolidation

One executor session, editor module + CkTestsEditor rows. Target: one refresh per edit, one
playhead, one converter, validated curve writes, no uncalled commands, undo gaps closed, the
untested behaviours tested. Expected size: from ~11.5k to ≤9.5k lines.

## Entry criteria

Phase 3 committed; baseline `--test-pattern ProceduralAnimation.Action.Editor` recorded.

## Step 1 — Red tests (CkTestsEditor)

- `...Editor.Refresh.OneSolverBuildPerEdit`: mount in Solved mode; count `UWorld` creations with
  `CkProceduralActionSolverWorld` name across one gizmo release (hook `FWorldDelegates::OnPostWorldCreation`)
  → exactly 1. RED on entry (2 today).
- `...Editor.Time.OnePlayhead`: after `Model->SetTime(t)` and one ticker, the transport, timeline and
  session all report `t` (session via `GetTime` until removed, then via the model only).
- `...Editor.Curves.RejectsDuplicateKeyTimes`: two rich keys that map to one key time in a steep eased
  section → the edit is refused with the compiler's `KeyOrder` diagnostic, the asset unchanged.
- `...Editor.Undo.SubjectChangeRefreshesAppearance` and `...Editor.Undo.RenameSectionKeepsSelection`.
- Behaviour rows: rotate gizmo (`WM_Rotate` writes orientation), bend gizmo, window body drag,
  `DeleteKey`, `RemoveEvent`, `RenamePose` (label), `SetPhaseEasing`, `SetReachPolicy` via the proxy.

## Step 2 — One refresh path

- Delete the nine synchronous `NotifyContentChanged()` calls in `Toolkit_Actions.cpp` and the ones in
  `Viewport.cpp:513`, `Timeline.cpp:642`, `Channels.cpp` (2) — every content edit already reaches the
  toolkit through `CommitDraft → PostEditChange → OnObjectPropertyChanged → _SourceRefreshPending`.
  Keep `NotifyContentChanged` for model-only changes (record channels).
- `AuthoringSession::Refresh` bumps `_Revision` only when the clip's content hash changed or the
  subject changed (compare `Get_ContentHash()` before/after compile; keep a `_SubjectRevision`).
- `Modes::SourceChanged` unchanged (it keys on the revision).

## Step 3 — One playhead, one converter

- `AuthoringSession` loses `_Time`/`_Playing`/`SetPlaying`/`Advance`; `EvaluateAt(FCk_Time)` is the only
  evaluation entry; the toolkit's authored playback advances `Model::_Time` itself.
- `AuthoringSession` gains `ToRawFraction(FCk_Time, FName& OutPhase, double& OutFraction)` and
  `SnapToFrame(FCk_Time)`; delete the three raw-fraction converters (`Toolkit_Actions.cpp:131-149`,
  `Timeline.cpp:734-750`, `Transport.cpp:144`) and the four 60 Hz step-math sites (`Modes::ToStep`
  stays as the solver's step mapping; the UI uses the session).

## Step 4 — Validated curve writes, command cuts, undo

- `SetBodyKeys`/`SetPositionKeys` run the compiler on the draft (`Write_Into` compiles again or the
  commands call `Compile` and refuse with the diagnostic). Remove `IsNormalized`/`IsLater`/`HasEventId`
  re-implementations where the compiler already reports them (keep refusals the compiler cannot
  express, e.g. "window end must follow start" if the compiler is silent on it — check first).
- Wire `RemoveSlot` (Delete on a selected slot in Channels), `SetSlotSpace` (Channels slot dropdown),
  `ReorderPhase` (drag a section). Delete `AddLimbTrack`, `AddBodyKey`, `AddPositionKey`,
  `SetPositionKey`, `SetBendKey`, `CapturePose`, `UpdatePose`, `DuplicatePose`, `MovePose` and their
  helpers (Phase 3 removed their reason to exist).
- `PostUndo`: also set `_AppearanceRefreshPending` when the subject changed; after undo of a rename,
  reselect by the restored name (the model listens to `Refresh` and validates its selection).
- Delete the legacy preview pairing (`Preview_Config.cpp:370-426`, `Toolkit.cpp:89-97`) — Phase 3's
  migration superseded it — and fold `GuidedCreation` into the factory's `ConfigureProperties`.
- `DeleteKey` on a missing key reports failure, not "Deleted."

## Step 5 — Doctrine sweep

22 same-line `if` bodies in `Commands.cpp`, the ones in `Modes.cpp`/`SolverWorld.cpp`; rename
`FCkProceduralActionEditorModes` → `FCk_ProceduralActionEditorModes`, `FCkProceduralAction_CurveAdapter`
→ `FCk_ProceduralAction_CurveAdapter`, `FCkProceduralActionViewportClient` → `FCk_…`; `GetX` → `Get_X`
on Session/Modes/Solver (41 sites; mechanical); `float` seconds on the proxy → `FCk_Time`; the
breadcrumb comment `SolverWorld.h:15` reworded; `DestroyWorld` guards `GEngine` and calls
`GameInstance->Shutdown()` on the failed-create path.

## Exit criteria

- All Step-1 rows green; editor lane at baseline otherwise.
- `rg -c "NotifyContentChanged\(\)" Source/CkProceduralAnimationEditor` → ≤4 (model-only callers).
- `rg -n "_Playing|_Time;" Source/CkProceduralAnimationEditor/Public/CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h` → 0.
- `wc -l` over the module ≤ 9,500.
- `[EDITOR-VERIFY]` list in VALIDATION.md executed by the maintainer.
