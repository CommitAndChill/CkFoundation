# Phase 4 — pose-first editor

**Entry.** Phases 1–3 recorded. Load `ck-slate-tools`. Read DESIGN §4, §5 (editor commands wrap the draft operations), §8 (mirror), §10, and the whole `Source/CkProceduralAnimationEditor/` module (8.2k lines: toolkit, timeline, viewport, curve adapter, commands, session, modes, solver world, preview config). Read `Plugins/CkTests/Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_MountedToolkit.cpp` and `Test_ProceduralAction_AuthoringCommands.cpp` for the fixture style. Keep the inherited `TwoTabsModesAndCapture` red recorded separately from this phase's new fixtures; do not attempt to fix it unless its cause is a line this phase changes.

This phase is editor-only Slate work and is the largest; it may be executed as two sessions, 4a (steps 1–4: tests, data, commands, session) and 4b (steps 5–7: workspace, guided creation, gate), each with its own gate on the `ProceduralAnimation.Action.Editor` pattern. Its mounted rows run in the renderer pass. Agents cannot observe the mounted tool: every observable row is a `NonNullRHI` automation test or an `[EDITOR-VERIFY]` step for the human in VALIDATION.

## Steps

1. **Tests first (red)** in `CkTestsEditor`:
   - `Ck.ProceduralAnimation.Action.Editor.AuthoringCommands.PoseCapture.*`: capture/update/duplicate/move/delete through the new commands; pose group IDs stable; unrelated keys/events untouched; undo/redo restores both tracks and metadata; save/reopen keeps IDs (`Test_ProceduralAction_SaveReopen.cpp` pattern).
   - `.AuthoringCommands.SolvedCapture.RejectsStalePartialAndMalformed` and `.ConvertsToDeclaredSpace`.
   - `.AuthoringCommands.MirrorDelegatesToBuilder`: the command and `UCk_Utils_ProceduralActionBuilder_UE::Mirror_Slot` produce identical phases.
   - `.AuthoringCommands.RecipeCreationCancelLeavesNoAssets` and `.PairedSaveFailureReportsWhichAsset`.
   - Mounted (`NonNullRHI`): `.MountedToolkit.PoseWorkspace.CaptureAndPlay` (open BodyPulse, capture a pose from the inspector, play solved preview, observe continuous recording without per-frame capture), `.MountedToolkit.PoseWorkspace.AdvancedHidesRawDetails`.
2. **Data**: `FCk_ProceduralAction_PoseGroup` (`FGuid _Id; FName _Label; FName _PhaseId; FCk_FloatRange_0to1 _Time; TArray<FName> _Channels; ECk_ProceduralAction_PoseProvenance _Provenance {Requested, Solved}`) and `UCk_ProceduralAction_Data::_EditorPoseGroups` under `WITH_EDITORONLY_DATA`; the compiler ignores it (not in `FClipSource`).
3. **Commands**: new `FCk_ProceduralActionAuthoringCommands` members `CapturePose`, `UpdatePose`, `DuplicatePose`, `MovePose`, `DeletePose`, `CaptureSolvedPose`, `CreateFromRecipe`; each builds a draft via `Make_Draft_FromData`, applies builder functions, `Build_Into` under one `FScopedTransaction` with `Modify()`, then updates `_EditorPoseGroups` in the same transaction. `MirrorSlot` becomes a thin wrapper over `Mirror_Slot`. Solved capture reads `FCk_ProceduralActionSolverWorld` state and rejects unless `Ready`, settled, and `GetRecording().Get_Provenance()` matches the session revision.
4. **Session**: session-owned requested targets (`FCk_ProceduralActionAuthoringSession` gains a staging pose: body + per-slot targets, participant set) and the derived temporary groups for old clips (matching key times).
5. **Workspace**: toolkit layout per DESIGN §4; subject tree, pose timeline rows (grouped keys), selected-pose inspector, curve panel wired to `FCk_ProceduralAction_ProgressCurve` through the curve adapter (samples from `EvaluateProgress`), issue summary from the last compile diagnostics, Advanced section holding the raw Details views and the recorded/support diagnostics. Default Play = Solver mode; Requested preview = Authored mode, labelled.
6. **Guided creation**: factory/asset-definition flow offering subject (choose/create preview config) and recipe, with name/path chosen before writes; cancel creates nothing; paired save reports per asset.
7. Comment audit, then:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action.Editor --output=Saved/Logs/ProceduralErgonomics-P4.log --project="D:/Repositories/CkRepos/Orion"
   ```

## Decision gates

- Red first, as before.
- After step 7: new editor rows pass; `Renderer-only: N test(s) ran` names the mounted rows; the only failure is the inherited `TwoTabsModesAndCapture`. Exit 81 → renderer editor did not come up; re-run once; still 81 → STOP (do not pass `--skip-renderer-tests` for an editor phase).
- A mounted row fails on a routed click → capture the fixture's failure screenshot path from the log and the hit-path diagnostic, record both, try one bounded fixture correction; a second failure → STOP.
- Anything else → STOP.

## Exit

Rows pass; `[EDITOR-VERIFY]` steps 1, 2 and 4 of VALIDATION are written up with exact clicks for the human; PROGRESS updated with the observed step count of the scripted body/reach/sweep exercise (no invented time target).

## Fences

- No persisted second set of transforms in pose groups.
- No editor dependency added to the runtime module.
- No per-frame widget reconstruction; refresh on revision change.
- Do not relax `CanPlay`; invalid drafts stay saveable, Play stays disabled with the field-linked reason.
- Do not touch the runtime `Mirror_Slot` geometry; if the editor needs a different frame, STOP and record.
