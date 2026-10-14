# Phase 1 — compiler view, progress curves, builder and mirror

**Entry.** CkFoundation `b23668049`, CkTests `96ef615b`, Mars `6a43f51` on `feature/procedural-anim-ext` (verify with `git -C Plugins/CkFoundation rev-parse --short HEAD` etc.); source clean except the untracked campaign docs and the sibling's `Plugins/Monolith` edits. Read DESIGN §§3, 5, 6, 8 (mirror), 10, 11.1–11.3, 11.6 and AUDIT. Load `ck-macros-and-codegen`, `ck-angelscript-interop`, `ck-tests-authoring-and-running`. Read `Action/CkProceduralAction_Data.h`, `Core/CkProceduralActionValidation.h/.cpp`, `Core/CkProceduralActionEvaluation.cpp` and `Private/Authoring/CkProceduralActionAuthoringCommands.cpp:710-784` in full before editing.

## Steps

1. **Tests first (red).** Author the native rows below in CkTests; they must compile against the §11 signatures and fail until the work lands. Each row is one `IMPLEMENT_SIMPLE_AUTOMATION_TEST` in the file named in DESIGN §11.6, flags `EditorContext | EngineFilter` (headless, no world needed):
   - `Ck.ProceduralAnimation.Action.Builder.RejectsDuplicatePhaseAndSlot`, `.RejectsOccupiedKeyTime`, `.PoseKeyIsAtomic` (a pose with one unknown slot writes nothing), `.BodyOnlyDraftBuildsAndCompiles`, `.MultiLimbDraftBuildsAndCompiles`, `.BuildIntoFailureLeavesDestinationUnchanged` (compare the five fields before/after with `CompareScriptStruct`), `.DraftFromDataRoundTrips` (Make_Draft_FromData → Build_Into a second object → identical compiled hash), `.RecipesCompile` (all four), `.ContentLimitsRejectBeforeAllocation`.
   - `Ck.ProceduralAnimation.Action.Progress.OldClipsEvaluateIdentically` (load the two shipped example assets; for every phase with `_ProgressMode == Preset` assert `EvaluateProgress(Phase, t) == UCk_Utils_TweenEasing_UE::Apply(ToTween(Easing), t)` to 1e-12 over 257 samples per preset; assert `Compile(asset)` and `Compile(MakeClipSource(Make_Draft_FromData(asset)))` produce equal `Get_ContentHash()` and equal `Evaluate` output at every key, boundary and 7 interior times per phase; no baked hash constant), `.CustomCurveIsMonotoneAndBounded` (random nondecreasing keys, 1024 samples, f non-decreasing, f(0)=0, f(1)=1, plateau held exactly), `.RejectsReversalDuplicateNaNAndNonLinearEasing`, `.SteepCurveCrossesSeveralKeysAndWindowsInOrder` (one `SampleInterval` over a steep segment: crossed events strictly ordered by elapsed time, each window enter/exit exactly once).
   - `Ck.ProceduralAnimation.Action.Mirror.XZUsesYNormal`, `.ReflectsPositionsTangentsBendsAndOrientation` (orientation: `ReflectRotation` of a known quaternion equals the matrix form `S R S` to 1e-6, unit length), `.RejectsOccupiedDestinationMissingSourceAndRestNormalized`, `.SourceUnchanged`.
   - AS: `CkAutoTest_ProceduralAnimation_ActionBuilderPublicSurface.as` (build a Reach recipe draft, add a pose key, `Build_Into` a `NewObject` clip, `Validate_Data` Valid, `Create_FromLoadedClip` reaches a terminal); `CkAutoTest_ProceduralAnimation_ActionAssetDefinition.as` with a sibling `CkProceduralAnimation_ActionFixture_Assets.as` declaring `asset Ck_ProceduralAction_AssetRouteFixture of UCk_ProceduralAction_Data` whose initializer fills `_BodyChannel`, `_Slots`, `_Phases`: first try the generated setters (`Set_Slots(Slots);`, the same calls `Mars_AutoTest_CrawlerMelee_LongLimbLoaded.as:67-110` makes on a `NewObject`), then direct field assignment if the setters are not callable there; the test calls `Validate_Data` on it. **Gate:** if neither form compiles at editor boot (the asset's fields are `BlueprintReadOnly`, so this is plausible), record the exact script error in PROGRESS, keep the test, and mark the route Blocked in VALIDATION B4; do not change property specifiers to make it pass.
2. **Compiler view** (§11.3): add `FClipSource`, both `MakeClipSource`, the view-taking `Compile`; the asset `Compile` forwards. Zero behaviour change; the existing `Core` tests stay green.
3. **Progress curves** (§11.1, §11.3, §6): add `ECk_ProceduralAction_ProgressMode`, `FCk_ProceduralAction_ProgressKey`, `FCk_ProceduralAction_ProgressCurve`, `FCk_ProceduralAction_Phase::_Progress`; `Core/CkProceduralActionProgress.h/.cpp`; `FCompiledPhase::_Progress`. In `Compile`: validate (Custom ⇒ `_Easing == Linear`, 2..64 keys, strictly ascending times, nondecreasing values, endpoints exact, finite), build the monotone curve, replace `InverseEasing` with `InvertProgress`, hash keys only in Custom mode. In `Evaluate`: `Progress = EvaluateProgress(Phase, Raw)`.
4. **Builder** (§11.1, §11.2): data header, Utils class, pure helpers, `Mirror_Slot`. Every function validates fully before any mutation; on failure the draft is untouched (work on a local copy only when the operation spans several arrays). Reports use `MakePublicReport(Invalid, false, Diagnostics)` with field paths matching the compiler vocabulary (`Phases[2].LimbTracks[0].PositionKeys[3].Time`).
5. **Comment audit**, then build + test:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P1.log --project="D:/Repositories/CkRepos/Orion"
   ```

## Decision gates

- After step 1: the build must FAIL to link or the new tests must FAIL (red). If the new tests pass before step 2, they test nothing → STOP and fix the tests.
- After step 5: expected `=== Test summary ===` with every new row passing, every previously selected `ProceduralAnimation.Action` row keeping its R48 verdict, and the only failure `MountedToolkit.TwoTabsModesAndCapture`. Read the `[population]` block: the pattern must select the new rows (grep their names in the log). Zero selected new rows → `--discover-fresh` once; still zero → STOP.
  - A new failure outside the new rows → run it alone (`--test --no-live --known-reds off --test-pattern "<full path>"`); red alone → STOP, record; green alone → record as load-sensitive and re-run its lane group once.
  - Hash mismatch in `OldClipsEvaluateIdentically` → the view or the hash order changed; restore field order, do not re-record the hash.
  - Anything else → STOP, record in PROGRESS blockers.

## Exit

New rows pass under the pattern above; `Ck.ProceduralAnimation.Core.*` and `Action.*` keep their verdicts; `rg -n 'Ergonomics' Plugins/CkTests/Source Plugins/CkTests/Script` returns 0 hits; `rg -n 'InverseEasing' Plugins/CkFoundation/Source` returns 0 hits (replaced by `InvertProgress`). PROGRESS updated with command, exit code, summary block, manifest and file hashes (`git -C Plugins/CkFoundation diff --stat`).

## Fences

- Do not add a transient `UCk_ProceduralAction_Data` inside `Build_Into`; compile the draft through `FClipSource`.
- Do not change `_CompatibilityVersion`; do not resave the example assets.
- Do not clamp or sort author input; reject with a field path.
- Do not write the `CK_ENSURE_IF_NOT { } if (NOT Valid) { }` double shape in new code (the module's existing lines stay).
- Do not touch `Private/Authoring/CkProceduralActionAuthoringCommands.cpp` in this phase (Phase 4 delegates `MirrorSlot`).
- No `TOptional` in reflected structs; use the `ECk_ProceduralAction_ValueMode` + value pairs.
