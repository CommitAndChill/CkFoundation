# Follow-up progress

Updated 2026-10-12 09:40 (Fable orchestrator). State: **design audited and corrected; package rewritten; Phase 1 dispatched to an Opus executor**.

| Phase | State | Evidence required before advancing |
|---|---|---|
| 1 Compiler view, progress, builder, mirror | Accepted 2026-10-12 (B-P1-1 attributed pre-existing by A/B) | B1–B3, T1–T3, R1, B4 native/AS rows; `ProceduralAnimation.Action` verdicts vs R48. |
| 2 Presets, queries, recovery, failure context | Accepted 2026-10-12 after orchestrator diff review (P2b 159/157/2, only the inherited editor reds; Presets 4/4; B-P2-1 attributed to machine load by A/B, B-P2-2 narrowed to the ReceiptOverflow row) | P1, Q1, D1; `ProceduralAnimation` wider pattern. |
| 3 Managed playback | Accepted 2026-10-12 after orchestrator review of the dispatcher and creation path (P3 171/169/2, only the inherited editor reds; B-P3-1 budget range corrected in DESIGN §11.4) | M1–M8 + AS + BP rows; explicit rows unchanged. |
| 4 Pose-first editor | Accepted 2026-10-12 after orchestrator review (4a 53/51/2, P4b 189/187/2 on `.Action`, P4c 58/56/2 on `.Action.Editor`; only the inherited reds; solved capture accepts a satisfied seek, refuses Finished and stale; `[EDITOR-VERIFY]` exercises written for the human) | E1–E7 rows + `[EDITOR-VERIFY]` write-up. |
| 5 Runtime mirror, examples, docs | Accepted 2026-10-12 after orchestrator review of the expansion (P5b 175/173/2, only the inherited editor reds; AS rows and Articulation 12/12 green alone; nit for Phase 7: the recovery drain log prints the AcquireResult ordinal because the enum is not a UENUM) | R2, C3, B4 docs; no project names in framework. |
| 6 Mars crawler migration | Accepted 2026-10-12 after orchestrator review (baseline CrawlerMelee 35/35 → final 38/38 exit 0; PlayerCombat 8/8; approved behaviour changes applied, each assertion change reasoned in-test; entity script owns the clip, debugger page updated, both approved out-of-fence) | C1, C2; CrawlerMelee baseline captured first. |
| 7 Final evidence | Executed 2026-10-12; F1 **Failed** (dossier [FINAL_REVIEW.md](FINAL_REVIEW.md)). Final gate A 460/455/5 (NEW = only the two inherited editor reds); CrawlerMelee 38/38; PlayerCombat 8/8; Performance.Builder 4/4 measured. The Game build compiled every module but failed at link (`LNK1318`, PDB limit), and it is AS-on. The cooked/AS-off route is Blocked. `[EDITOR-VERIFY]` exercises are owed. | F1; dossier. |

## Baseline (recorded by the orchestrator 2026-10-12)

- Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b`, CkGameplayDebugger `f343e17b`, all `feature/procedural-anim-ext`.
- Dirty paths not ours: `Plugins/Monolith/Source/MonolithEditor/Private/MonolithPieObjectActions.cpp` (M), `MonolithPieFunctionFrame.h`, `Tests/MonolithPieFunctionFrameTests.cpp` (??) — a sibling session's; untouched. CkFoundation: only `docs/campaigns/procedural-actions-ergonomics/` untracked.
- Toolbox: `CkAuto/UnrealToolbox.exe` (flags verified via `--help` 2026-10-12: `--build --target --test --test-pattern --no-live --discover-fresh --parallel --editor-args --known-reds --print-population --skip-renderer-tests`). No UnrealEditor process running at 09:20.
- `ProceduralAnimation.Action` pattern: R48 (`Saved/Logs/ProceduralActions-Commit-R48.log`, 2026-10-11 18:50): 132 / 131 passed / 1 failed / 0 contaminated / 3m40s; failure `Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture` (NEW, on no list). Inferred, not confirmed: R48 ran on the same source as HEAD `b23668049` (commit 18:40, log 18:50); Phase 1's gate confirms by reproducing 131/1 on the untouched rows.
- Known reds that intersect the wider `ProceduralAnimation` pattern: Mars root `Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact` (HOST-COUPLED); CkTests list: `Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep`, `..._PillarCrossingRenderedTraversal`, `..._PillarsEngageSteppedTops` (flaky), `..._PostsCentipedeReturns`, `..._SpiderInteractiveRosterDiagnostics` (functional-test rows under `Project.Functional Tests...`).
- `CrawlerMelee`: no fresh baseline; Phase 6 captures one before its first edit.

## Execution order note (orchestrator, 2026-10-12)

Phases run 1 → 2 → 3 → 5 → 4 → 6 → 7: Phase 5 (runtime mirror, examples, docs) precedes the editor phase because its gate is headless and it completes the programmer-facing deliverable; Phase 4 (editor) and Phase 6 (crawler) follow.

## Gate record (append per phase)

Template: date / phase / source hashes (`git -C <repo> diff --stat` + HEAD) / exact command / actual exit / `=== Test summary ===` block / baseline delta by failing name / confirmed findings fixed / findings dismissed with evidence / remaining uncertainty / next bounded step.

### Phase 1 gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; everything below is dirty, nothing staged or committed).

`git -C Plugins/CkFoundation diff --stat` (tracked files):
```
 .../Action/CkProceduralAction_Data.h               |  50 +++++
 .../Core/CkProceduralActionEvaluation.cpp          |  21 +--
 .../Core/CkProceduralActionValidation.cpp          | 210 +++++++++++++++------
 .../Core/CkProceduralActionValidation.h            |  46 ++++-
 4 files changed, 248 insertions(+), 79 deletions(-)
```
CkFoundation untracked: `Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionBuilder/` (`CkProceduralActionBuilder_Data.h`, `CkProceduralActionBuilder_Utils.h/.cpp`), `Core/CkProceduralActionProgress.h/.cpp`; generator output `Script/Generated/utils_procedural_action_builder.as` (not hand-edited).

`git -C Plugins/CkTests diff --stat`:
```
 Script/Generated/CkTests_AutoTestActors.as | 25 +++++++++++++++++++++++++
 1 file changed, 25 insertions(+)
```
(generator output for the two new AS tests). CkTests untracked: `Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_{Builder,Progress,Mirror}.cpp`, `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_{ActionBuilderPublicSurface,ActionAssetDefinition}.as`, `Script/CkProceduralAnimation/CkProceduralAnimation_ActionFixture_Assets.as`, and populator output `Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level/4/{81,V7}/` (gate side effect; never stage).

**Red before implementation.** The native rows reference `FCk_ProceduralAction_ClipDraft`, `UCk_Utils_ProceduralActionBuilder_UE`, `EvaluateProgress`, `BuildMonotoneProgress`, `MakeClipSource`, `ReflectRotation`, `FCk_ProceduralAction_ProgressCurve`, `ECk_ProceduralAction_ProgressMode`; `rg --no-ignore` over CkFoundation `Source` + `Script` at 09:39, after the tests were written and before any implementation, found 0 files for each. Red by construction (cannot compile); not observed with a build, because the package allowed one build.

**Command (3 invocations of the same line; first two fixed my own errors).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P1.log --project="D:/Repositories/CkRepos/Orion"
```
1. Exit 1, build failed: `CkProceduralActionBuilder_Utils.cpp(302)` C2672, `auto Occupied = INDEX_NONE;` deduces the anonymous enum and cannot bind `int32&`. Fixed with `int32{INDEX_NONE}`.
2. Exit 1, build green, 151 / 148 / 3: `Action.Builder.RecipesCompile` (Sweep apex built with `FQuat(Up, pi/2)`, whose polynomial SinCos error exceeds the test's 1e-9; replaced with the exact quarter/half-turn component swap) and `Ck_AutoTest_ProceduralAnimation_ActionBuilderPublicSurface` (my fixture bound one part to a two-segment limb; articulation `Add` ensured; fixed to one part per segment as `Mars_AutoTest_CrawlerMelee_LongLimbLoaded.as:27-42` does), plus the inherited red.
3. Final run, actual exit **1**:
```
=== Test summary ===
Total: 151
Passed: 149
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 3m 12s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 5 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population: `--test-pattern 'ProceduralAnimation.Action' selects 151 of them (5 renderer-only)`; 151 = R48's 132 + 19 new rows. Logs archived in the session scratchpad as `ProceduralErgonomics-P1-attempt{1,2,3}.log`; the final one is also `Saved/Logs/ProceduralErgonomics-P1.log`.

**Delta vs R48 (132 / 131 / 1).** Inherited: `MountedToolkit.TwoTabsModesAndCapture`, same message as R48 (`timed out at stage 2: Expanded Preview Placement has no visible Root Frame`). New: `SaveReopen.FocusedAssetIdentity` fails `Expected 'routed Slate click adds a phase' to be true` (`Test_ProceduralAction_SaveReopen.cpp:353`, `ClickWidget` on the Add phase button returned false). Every other pre-existing row keeps its R48 verdict.

**New rows, all selected and passed (final run):** `Ck.ProceduralAnimation.Action.Builder.{RejectsDuplicatePhaseAndSlot, RejectsOccupiedKeyTime, PoseKeyIsAtomic, BodyOnlyDraftBuildsAndCompiles, MultiLimbDraftBuildsAndCompiles, BuildIntoFailureLeavesDestinationUnchanged, DraftFromDataRoundTrips, RecipesCompile, ContentLimitsRejectBeforeAllocation}`, `Ck.ProceduralAnimation.Action.Progress.{OldClipsEvaluateIdentically, CustomCurveIsMonotoneAndBounded, RejectsReversalDuplicateNaNAndNonLinearEasing, SteepCurveCrossesSeveralKeysAndWindowsInOrder}`, `Ck.ProceduralAnimation.Action.Mirror.{XZUsesYNormal, ReflectsPositionsTangentsBendsAndOrientation, RejectsOccupiedDestinationMissingSourceAndRestNormalized, SourceUnchanged}`, `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_{ActionBuilderPublicSurface, ActionAssetDefinition}`.

**AS asset route (VALIDATION B4).** Works: `asset Ck_ProceduralAction_AssetRouteFixture of UCk_ProceduralAction_Data` (in `ck_procedural_action_fixture_assets`) fills the fields through the generated setters (`Set_BodyChannel`, `Set_Slots`, `Set_Phases`) inside the initializer; `ActionAssetDefinition` passed in runs 2 and 3. Direct field assignment was not needed and not tried. Precedent that made this likely: `Plugins/CkTests/Script/CkVisualLod/CkVisualLod_GymAssets.as` calls `Set_RenderingInfo` on a `BlueprintReadOnly` field.

**Exit greps.** `rg -n 'Ergonomics' Plugins/CkTests/Source Plugins/CkTests/Script`: 0. `rg -n 'InverseEasing' Plugins/CkFoundation/Source`: 0.

**Not run.** No full gate, no wider `ProceduralAnimation` pattern, no Blueprint `[EDITOR-VERIFY]`, no A/B build with the change reverted.

### Phase 2 gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; everything is dirty on top of the Phase-1 diff, nothing staged or committed).

`git -C Plugins/CkFoundation diff --stat` (tracked files, Phase 1 + Phase 2 together; Phase 2 adds every line outside `Core/CkProceduralActionEvaluation.cpp` and the Phase-1 hunks of `Action/CkProceduralAction_Data.h`, `Core/CkProceduralActionValidation.{h,cpp}`):
```
 .../LimbControl/CkProceduralLimbControl_Control.h  |   9 +
 .../Action/CkProceduralAction_Data.h               |  50 +++++
 .../Action/CkProceduralAction_Fragment.cpp         |  27 ++-
 .../Action/CkProceduralAction_Fragment.h           |   9 +
 .../Action/CkProceduralAction_Fragment_Data.h      |  64 +++++-
 .../Action/CkProceduralAction_Receipt.h            |   4 +
 .../Action/CkProceduralAction_Utils.cpp            |  45 +++++
 .../Action/CkProceduralAction_Utils.h              |  10 +
 .../CkProceduralActionPlayer_Fragment_Data.h       |  31 +++
 .../CkProceduralActionPlayer_Observe.cpp           |  51 +++++
 .../CkProceduralActionPlayer_Processor.cpp         |   1 +
 .../CkProceduralActionPlayer_Service.cpp           |  40 +++-
 .../CkProceduralActionPlayer_Utils.cpp             |  44 +++++
 .../ActionPlayer/CkProceduralActionPlayer_Utils.h  |  22 +++
 .../CkProceduralArticulation_Utils.cpp             |  80 ++++++++
 .../Articulation/CkProceduralArticulation_Utils.h  |  28 +++
 .../Core/CkProceduralActionEvaluation.cpp          |  21 +-
 .../Core/CkProceduralActionReceiptStore.cpp        |  27 ++-
 .../Core/CkProceduralActionReceiptStore.h          |  13 ++
 .../Core/CkProceduralActionValidation.cpp          | 217 +++++++++++++++------
 .../Core/CkProceduralActionValidation.h            |  46 ++++-
 .../CkProceduralLimbControl_Control.cpp            |  50 ++++-
 .../LimbControl/CkProceduralLimbControl_Fragment.h |  10 +-
 .../CkProceduralLimbControl_Fragment_Data.h        |  15 ++
 .../CkProceduralLimbControl_Processor.cpp          |  26 ++-
 25 files changed, 841 insertions(+), 99 deletions(-)
```
CkFoundation untracked: unchanged from Phase 1 (no new untracked source in Phase 2). Generator output under `Script/Generated/` regenerated by the test boot (not hand-edited).

`git -C Plugins/CkTests diff --stat`:
```
 ...roceduralAnimation_ArticulationPublicSurface.as | 50 ++++++++++++++++++++++
 Script/Generated/CkTests_AutoTestActors.as         | 25 +++++++++++   (Phase-1 generator output)
 2 files changed, 75 insertions(+)
```
CkTests untracked, new in Phase 2: `Source/CkTests/Private/UnitTests/CkProceduralAnimation/{CkTest_ProceduralWalkerFixture.h, ActionPlayer/Test_ProceduralActionPlayer_Presets.cpp, Articulation/Test_ProceduralArticulation_Query.cpp, Action/Test_ProceduralAction_Diagnostics.cpp}`; edited Phase-1 file `Action/Test_ProceduralAction_Progress.cpp` (carry-over row). Populator output under `Content/__ExternalActors__/` is a gate side effect (never stage).

**Red before implementation.** Tests written first; `rg --no-ignore -l` over CkFoundation `Source` + `Script` at 10:36:57, before any implementation, found 0 files for each of `FCk_ProceduralAction_FailureContext`, `FCk_ProceduralActionPlayer_BorrowingSpec`, `Make_DedicatedPlayerSpec`, `Make_BorrowingPlayerSpec`, `Make_CountOnlySupport`, `Get_IsAnyLimbControlled`, `Get_IsAnyLimbReturning`, `Get_UnplantedLimbIds`, `Request_RecoverUnplantedLimbs`, `FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs`, `Get_ResultText`, `ProgressKeysInactive`. Red by construction (cannot compile); not observed with a build.

**Command (run 1, the phase gate).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-P2.log --project="D:/Repositories/CkRepos/Orion"
```
Build green on the first attempt. Actual exit **1**.
```
=== Test summary ===
Total: 425
Passed: 417
Failed: 8
Skipped: 0
Contaminated: 0
Duration: 10m 42s
Failed tests (8):
  Ck.ProceduralAnimation.Action.Diagnostics.SamplingBudgetCarriesObservedAndBudget
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics
Renderer-only: 35 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (4):
  Ck.ProceduralAnimation.Action.Diagnostics.SamplingBudgetCarriesObservedAndBudget
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation
Still red (listed) (3):
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns  [Plugins/CkTests]
Listed flaky, failed, not re-run (1):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics  [Plugins/CkTests]
Now passing (listed red in a plugin's file) — not pruned from here; if it holds in the plugin's home project, open a PR there (1):
  Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep  [Plugins/CkTests]
Listed flaky, passed — if it keeps passing, remove the entry (1):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarsEngageSteppedTops  [Plugins/CkTests]
NOTE: the run is already failing: listed-flaky failures were not re-run
```
Logs archived in the session scratchpad: `ProceduralErgonomics-P2-run1.log`, `ProceduralErgonomics-P2-BodyPoseAlone.log`, `ProceduralErgonomics-P2-Diagnostics.log`.

**Delta against the recorded baseline.** Inherited and expected: `MountedToolkit.TwoTabsModesAndCapture`, `SaveReopen.FocusedAssetIdentity` (Phase-1 B-P1-1 attribution, still on no list), and the listed `SurfaceMotion.SnapshotShowsThePendingContact`, `PillarCrossingRenderedTraversal`, `PostsCentipedeReturns`, `SpiderInteractiveRosterDiagnostics` (flaky). New, not attributed: `BodyPose.PresentationLagIsBoundedThroughSustainedRotation` (B-P2-1). New, this phase's own row: `Diagnostics.SamplingBudgetCarriesObservedAndBudget` (B-P2-2). Note: this wider pattern had no recorded pre-change run; only the `ProceduralAnimation.Action` subset had a baseline (R48 / Phase 1).

**New rows, selected and passed (run 1):** `Ck.ProceduralAnimation.ActionPlayer.Presets.{DedicatedSpecAddsAndMatchesTuning, BorrowingSpecCarriesSupportAndReturn, CountOnlyBelowOneEnsures, BorrowingWithInsufficientContactsRejectsAtAdmission}`, `Ck.ProceduralAnimation.Articulation.Query.{ControlledAndReturningMatchObservations, UnplantedIdsInTopologyOrder, RecoverUnplantedEmptySetSucceeds, RecoverUnplantedUsesDrainTimeSet}`, `Ck.ProceduralAnimation.Action.Diagnostics.{ReturnTimeoutCarriesElapsedAndLimit, UnknownReasonRendersIdentifier, ContextRidesTerminalRecord}`, `Ck.ProceduralAnimation.Action.Progress.RejectsReversalDuplicateNaNAndNonLinearEasing` (with the Preset-with-keys carry-over), `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_ArticulationPublicSurface` (extended). Failed: `Diagnostics.SamplingBudgetCarriesObservedAndBudget`.

**Follow-up runs.**
1. `--test --no-live --known-reds off --test-pattern "Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation"` (same binary as run 1): exit 1, 1 / 0 / 1, red alone (`body turned steadily over at least 20 samples (5)`, worst trail 29.79 deg at sample 5).
2. Test-only fix to the SamplingBudget row (target at 95 % of reach), then `--build --target=Editor --test --test-pattern ProceduralAnimation.Action.Diagnostics --output=Saved/Logs/ProceduralErgonomics-P2-Diagnostics.log`: build green, exit 1, 4 / 3 / 1, same failure (`Outcome InputTimeout`, context without a budget). Diagnosed by reading code: see B-P2-2.

**Exit greps.** `Script/Generated/utils_procedural_articulation.as` contains `Request_RecoverUnplantedLimbs` (line 8) and `Get_UnplantedLimbIds`; `utils_procedural_action_player.as` contains `Make_CountOnlySupport`; `utils_procedural_action.as` contains `Get_ResultText`. Run-1 log: 0 `Angelscript: Error` lines.

**Admission check (step 2).** `FCk_ProceduralLimbControl_Control::Attach` already rejects `CountOnly` with 0 contacts through `ck::procedural_support::ValidatePolicy` (`Core/CkProceduralSupportRegion.cpp:50`, `_MinimumTrustedContacts > 0` and `<= 64`), atomically (nothing is added before the check). No change made. Adjacent smell, not fixed: `UCk_Utils_ProceduralActionPlayer_UE::Add` returns `{}` without an ensure when `Attach` rejects (`ActionPlayer/CkProceduralActionPlayer_Utils.cpp`, the line after the double-shape ensure), so an invalid locomotion policy is rejected silently.

**Not run.** No full gate, no Blueprint `[EDITOR-VERIFY]`, no A/B with the phase reverted (stash is outside this executor's permissions).

**Phase 2 is blocked** by B-P2-1 and B-P2-2.

### Phase 2 closing record (2026-10-12, Opus executor, after the orchestrator's decisions)

**Changes since the record above.** (1) `Make_CountOnlySupport` returns the requested `{CountOnly, InMinimumTrustedContacts}` unchanged after its ensure fires instead of the Disabled default, so `Add`/`Attach` rejects it; `CountOnlyBelowOneEnsures` now asserts CountOnly with the invalid count (0, -2, 65) and that `Add` with the borrowing spec built from `Make_CountOnlySupport(0)` returns an invalid handle and leaves no player on the subject. (2) `Diagnostics.SamplingBudgetCarriesObservedAndBudget` is deleted and replaced by `Diagnostics.ReceiptOverflowCarriesPendingAndBudget` (player Capacity `_RecordsPerInstance = 4`, manual action submitting in-budget batches that nobody consumes until the fifth publication overflows). Its first run failed only `The terminal record is retained`: `FReceiptStore::Read` answers `InvalidCursor` to a page larger than `RecordsPerInstance + 2` (`Core/CkProceduralActionReceiptStore.cpp:607-610`) and the test helper asked for 64; the helper now pages at the player's `RecordsPerInstance + 2`. Test-only fix, no framework change.

**Known gap for the Phase 7 dossier.** The `SamplingBudgetExceeded` failure context in `ActionPlayer/CkProceduralActionPlayer_Observe.cpp` (`EndpointTravelMicrometres` / `SegmentAngleMillidegrees`, settled step first) is implemented but has no deterministic public-API fixture: the predicted-step gate in `ActionPlayer/CkProceduralActionPlayer_Service.cpp:1210-1222` (`QualifiesOther`, gated at 1271-1273, submit failed at 1369-1385) rejects an over-budget manual batch before staging, so the settled-motion check is reached only when a prediction within budget settles outside it. It is covered by code review only; `SampleInterval` and the pre-staging rejection contract are unchanged by decision.

**Command (P2b).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P2b.log --project="D:/Repositories/CkRepos/Orion"
```
Attempt 1 (archived as `ProceduralErgonomics-P2b-attempt1.log` in the session scratchpad): exit 1, 159 / 156 / 3, the new overflow row failing on the page size above plus the two inherited editor reds; it also reported the deleted SamplingBudget row as absent from a stale test list. Attempt 2, after the helper fix, actual exit **1**:
```
=== Test summary ===
Total: 159
Passed: 157
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 3m 14s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 5 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population `--test-pattern 'ProceduralAnimation.Action' selects 159 of them (5 renderer-only)`. The only failures are the two inherited editor reds (B-P1-1 attribution). 0 `Angelscript: Error` lines. Every Phase 1 and Phase 2 `.Action` row passed, including `Action.Diagnostics.{ReceiptOverflowCarriesPendingAndBudget, ReturnTimeoutCarriesElapsedAndLimit, UnknownReasonRendersIdentifier, ContextRidesTerminalRecord}`, `Action.Progress.RejectsReversalDuplicateNaNAndNonLinearEasing`, `Action.Builder.*`, `Action.Mirror.*`, `Ck_AutoTest_ProceduralAnimation_{ActionBuilderPublicSurface, ActionAssetDefinition}`.

**Presets on the final binary (test only).** `Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --no-live --test-pattern ProceduralAnimation.ActionPlayer.Presets --output=Saved/Logs/ProceduralErgonomics-P2b-Presets.log --project="D:/Repositories/CkRepos/Orion"`, actual exit **0**:
```
=== Test summary ===
Total: 4
Passed: 4
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 33s
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
(no listed test ran in this run, and there were no new failures)
```
Passed: `ActionPlayer.Presets.{BorrowingSpecCarriesSupportAndReturn, BorrowingWithInsufficientContactsRejectsAtAdmission, CountOnlyBelowOneEnsures, DedicatedSpecAddsAndMatchesTuning}`.

**Not re-run on the final binary.** The `Articulation.Query.*` rows and the `ArticulationPublicSurface` AS test passed in run 1 of the record above; no framework source changed since except `Make_CountOnlySupport`'s ensure body, which those rows do not reach with an invalid count.

### Phase 3 gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; everything is dirty on top of the Phase 1 + 2 diff, nothing staged or committed).

`git -C Plugins/CkFoundation diff --stat` (tracked files, Phases 1-3 together; Phase 3 adds the hunks in `Action/CkProceduralAction_{Fragment.h, Fragment_Data.h, Receipt.h, Utils.h, Utils.cpp}`, `ActionPlayer/CkProceduralActionPlayer_{Fragment.h, Fragment_Data.h, Processor.h, Utils.h, Utils.cpp}`, `Core/CkProceduralActionReceiptStore.{h,cpp}`):
```
 .../LimbControl/CkProceduralLimbControl_Control.h  |   9 +
 .../Action/CkProceduralAction_Data.h               |  50 +++++
 .../Action/CkProceduralAction_Fragment.cpp         |  27 ++-
 .../Action/CkProceduralAction_Fragment.h           |  16 ++
 .../Action/CkProceduralAction_Fragment_Data.h      | 155 ++++++++++++++-
 .../Action/CkProceduralAction_Receipt.h            |  45 +++++
 .../Action/CkProceduralAction_Utils.cpp            | 154 ++++++++++++++-
 .../Action/CkProceduralAction_Utils.h              |  65 +++++-
 .../CkProceduralActionPlayer_Fragment.h            |  51 +++++
 .../CkProceduralActionPlayer_Fragment_Data.h       |  49 +++++
 .../CkProceduralActionPlayer_Observe.cpp           |  51 +++++
 .../CkProceduralActionPlayer_Processor.cpp         |   1 +
 .../CkProceduralActionPlayer_Processor.h           |  68 +++++++
 .../CkProceduralActionPlayer_Service.cpp           |  40 +++-
 .../CkProceduralActionPlayer_Utils.cpp             | 199 +++++++++++++++++++
 .../ActionPlayer/CkProceduralActionPlayer_Utils.h  |  68 +++++++
 .../CkProceduralArticulation_Utils.cpp             |  80 ++++++++
 .../Articulation/CkProceduralArticulation_Utils.h  |  28 +++
 .../Core/CkProceduralActionEvaluation.cpp          |  21 +-
 .../Core/CkProceduralActionReceiptStore.cpp        |  44 ++++-
 .../Core/CkProceduralActionReceiptStore.h          |  20 ++
 .../Core/CkProceduralActionValidation.cpp          | 217 +++++++++++++++------
 .../Core/CkProceduralActionValidation.h            |  46 ++++-
 .../CkProceduralLimbControl_Control.cpp            |  50 ++++-
 .../LimbControl/CkProceduralLimbControl_Fragment.h |  10 +-
 .../CkProceduralLimbControl_Fragment_Data.h        |  15 ++
 .../CkProceduralLimbControl_Processor.cpp          |  26 ++-
 27 files changed, 1500 insertions(+), 105 deletions(-)
```
CkFoundation untracked, new in Phase 3: `Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Managed.cpp` (both processors). Generator output `Script/Generated/utils_procedural_action{,_player}.as` regenerated by the test boot (not hand-edited).

`git -C Plugins/CkTests diff --stat`:
```
 ...roceduralAnimation_ArticulationPublicSurface.as |  50 ++++++
 Script/Generated/CkTests_AutoTestActors.as         |  38 ++++
 .../Test_ProceduralAction_BlueprintParity.cpp      | 197 +++++++++++++++++++++
 3 files changed, 285 insertions(+)
```
(Phase 3: the BlueprintParity row and the generator's wrapper for the new AS test.) CkTests untracked, new in Phase 3: `Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/{Test_ProceduralAction_Managed.cpp, CkTest_ProceduralActionManagedListener.h}`, `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ActionManagedPublicSurface.as`; populator output `Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level/0/Y3/` (gate side effect; never stage).

**Red before implementation.** Tests written first. `rg --no-ignore -l` over CkFoundation `Source` + `Script` at 11:46:13, after the tests and before any implementation, found 0 files for each of `Play_Loaded`, `FCk_ProceduralAction_ManagedStart`, `FCk_ProceduralAction_ManagedPage`, `FCk_ProceduralAction_ManagedCompletion`, `ECk_ProceduralAction_DeliveryStatus`, `FCk_Delegate_ProceduralAction_OnManagedRecords`, `BindTo_OnManagedRecords`, `Get_IsManagedDelivery`, `Get_AcknowledgedThrough`, `_ManagedRecordsPerTick`, `FProcessor_ProceduralActionPlayer_ManagedDispatch`, `FFragment_ProceduralActionPlayer_Managed`, `ManagedDelivery`. Red by construction (cannot compile); not observed with a build.

**Command (2 invocations of the same line).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P3.log --project="D:/Repositories/CkRepos/Orion"
```
1. Build green on the first attempt. Exit 1, 171 / 166 / 5 (archived as `ProceduralErgonomics-P3-attempt1.log` in the session scratchpad). Three of my own rows failed on test defects, no framework defect: `InvalidScopeReturnsFailedReportWithoutCallback` and `PublicConsumeAndReadAreSealed` - `Expected ('Warning') level log message or higher matching '...' to occur 2 (1) times ... but it was found 4 (2) time(s)` (each Ck ensure logs its message twice; the expectation now uses 0 = at least once); `ListenerReentrancy` - `Expected 'PlayAnother: the nested play started' to be true` (the row checked `ck::IsValid` on the nested action after its completion had already released it; it now checks the start report and identity). One bounded test-only fix, no source change.
2. Final run, actual exit **1**:
```
=== Test summary ===
Total: 171
Passed: 169
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 4m 2s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 5 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Archived as `ProceduralErgonomics-P3-attempt2.log` in the session scratchpad; also `Saved/Logs/ProceduralErgonomics-P3.log`. 0 `Angelscript: Error` lines. Population 171 = P2b's 159 + 12 new rows.

**Delta against P2b (159 / 157 / 2).** Failing names identical: the two inherited editor reds (B-P1-1 attribution). Every P2b row passed again, including the explicit-delivery rows `Action.Clip.*`, `Action.Manual.*`, `Action.ReceiptStore*` and `Action.PlayerBudgetValidation`.

**New rows, selected and passed (final run):** `Ck.ProceduralAnimation.Action.Managed.{NoCallbackBeforePlayReturns, InvalidScopeReturnsFailedReportWithoutCallback, DrainsWithoutListenersAndReturnsStorageToBaseline, RecordsArriveInOrderWithWindowsAndTerminal, PublicConsumeAndReadAreSealed, ListenerReentrancy, ScopeLostSuppressesDeadRecipientWhileDrainContinues, PlayerLostFiresOnceFromEndPlay, BudgetFairnessAndZeroDeltaT, LateSubscriptionHasNoReplay}`, `Ck.ProceduralAnimation.Action.Editor.BlueprintParity.ManagedPlayLoadedVm`, `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_ActionManagedPublicSurface`.

**Exit grep.** `rg -n 'Get_IsManagedDelivery' Plugins/CkFoundation/Script/Generated`: `utils_procedural_action_player.as:66` and `:68`.

**Decisions taken inside the package's latitude.**
- `_ManagedRecordsPerTick` is validated as `1..(receipt_limits::MaxOrdinaryRecordsPerInstance + 2)`, independent of `RecordsPerInstance`; taken with orchestrator approval (DESIGN 11.4's `1..RecordsPerInstance+2` would have rejected existing capacities with `RecordsPerInstance` 1, 2 and 4). The per-identity page stays `min(remaining budget, RecordsPerInstance + 2)`.
- Per-pass order for each identity: `Get_AcknowledgedThrough` -> `Read` -> copy the page, the terminal result/context and both listener arrays -> records listeners in bind order, then the accepted-pose listeners per accepted record -> revalidate the player (valid, not pending destroy, both fragments) -> `Consume` through the page -> on the terminal page: mark the entry completed, fire the completion, `Request_Release` the action if it is valid, has the fragment and is not pending destroy, then remove the entry if the player is still dispatchable. An empty, non-terminal page calls nobody and consumes no budget.
- Rotation: identities are walked in ascending order from `_NextDispatchIndex`; the next pass starts after the last identity serviced, counted in the next pass's order (identities completed this pass are subtracted).
- `PlayerLost` comes from `FProcessor_ProceduralActionPlayer_ManagedEndPlay`: it moves the entries out, fires once per entry not marked completed, in identity order, with `Result{}` (Outcome None) and an empty context. An entry whose completion was already fired (its recipient destroyed the player) is marked completed first and is skipped.
- `Get_IsManagedDelivery` is true from `Play`'s return until the entry is removed; during the completion callback it is still true (reads stay sealed), afterwards false.
- Listeners: at most 8 per kind per identity (ensure + no add beyond); a duplicate bind is a no-op; binding an unbound delegate, an unknown, explicit or already-completed identity ensures; unbinding a completed or unknown identity is a quiet no-op (a late unbind after completion is a normal race). Unbinding inside a callback takes effect from the next page (the page in flight was copied).
- `ManagedStart._Report` is `Valid` whenever an action was created (including a loaded clip that will complete `Rejected`); `Invalid` with one `StartRejected` diagnostic and no identity otherwise.
- The managed entry is `ck::FProceduralActionManagedDelivery` (named like its sibling `FProceduralActionPendingRecovery`) and also holds the action handle, set right after entity creation, so the dispatcher can release it.
- `Play` (soft) shares `DoCreate_FromClip` with `Create_FromClip`, as `Play_Loaded` shares `DoCreate_Loaded` with `Create_FromLoadedClip`.

**Not run.** No full gate, no wider `ProceduralAnimation` pattern, no Blueprint `[EDITOR-VERIFY]`, no A/B with the phase reverted. `M2` asserts storage returns to baseline within 4 frame polls after completion; the AddInfo with the measured count is in the automation report, not the log.

### Phase 5 gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; everything is dirty on top of the Phase 1-3 diff, nothing staged or committed). Phase 4 (editor) has not run; `Source/CkProceduralAnimationEditor` untouched.

`git -C Plugins/CkFoundation diff --stat` (tracked files, Phases 1-3 + 5 together; Phase 5 adds the hunks in `Action/CkProceduralAction_Fragment_Data.h` (`_Mirrors`), `Action/CkProceduralAction_Fragment.h` (`FProceduralActionClipParams::_Mirrors`), `Action/CkProceduralAction_Utils.cpp` (mirror count check, expansion on the loaded path, `MirrorRejected`), `ActionPlayer/CkProceduralActionPlayer_ClipBinding.h`, `_Service.cpp` (expansion on the soft-load path), `_Utils.cpp` (expansion in `Get_AdmissionReport`), `Core/CkProceduralActionValidation.h` (`ExpandMirrors` declaration + friend), `Script/.../CkProceduralAction_Noncombat.as`, `Source/CkProceduralAnimation/Claude.md`, `docs/campaigns/procedural-actions/{API,AUTHORING,GETTING_STARTED}.md`):
```
 33 files changed, 1742 insertions(+), 152 deletions(-)
```
(per-file list in the executor report). CkFoundation untracked, new in Phase 5: `Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Mirror.cpp`, `Script/CkProceduralAnimation/Examples/CkProceduralAction_{Builder,Sweep}.as`. Also edited, but invisible to git: `Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` is matched by `.gitignore:49 *.md` and was never tracked (B-P5-3).

`git -C Plugins/CkTests diff --stat`:
```
 ...roceduralAnimation_ArticulationPublicSurface.as |  50 +++   (Phase 2)
 Script/Generated/CkTests_AutoTestActors.as         |  77 +++++ (generator output, 3 new wrappers this phase)
 .../Test_ProceduralAction_BlueprintParity.cpp      | 345 +++++++ (Phase 3 row + Phase 5 MirrorVm row)
 3 files changed, 472 insertions(+)
```
CkTests untracked, new in Phase 5: `Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_MirrorRuntime.cpp`, `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_{NoncombatManaged,BuilderSweep,BorrowingSweep}.as`; populator output under `Content/__ExternalActors__/` (gate side effect; never stage).

**Red before implementation.** Tests written first. `rg --no-ignore -l` over CkFoundation `Source` + `Script` at 12:27:45, after the tests and before any implementation, found 0 files for each of `_Mirrors`, `Set_Mirrors`, `Get_Mirrors`, `ExpandMirrors`, `MirrorRejected`, `PlayManagedGesture`, `ck_procedural_action_builder::`, `namespace ck_procedural_action_builder`, `namespace ck_procedural_action_sweep`, `BuildSweep`, `StartSweep`, `AddBorrowingSweepPlayer`, `DescribeOutcome`, `RecoverUnplanted\b`. Red by construction (cannot compile / AS cannot resolve); not observed with a build.

**Command (3 invocations of the same line).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-P5.log --project="D:/Repositories/CkRepos/Orion"
```
1. Exit 1, build failed, one test-only error: `Test_ProceduralAction_MirrorRuntime.cpp(251)` C2678, `FCk_FloatRange_0to1` has no `operator==`; fixed by comparing `Get_Value()`. Framework code compiled clean.
2. Exit 1, 444 / 437 / 7. My rows red: `Mirror.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding` (`Expected '<case>: the stream holds only the terminal' to be true` for all 6 cases: a rejected loaded clip's stream is WindowsClosed summary + Terminal, `FReceiptStore::Finish` always closes windows first; my assertion was wrong) and `Ck_AutoTest_ProceduralAnimation_BorrowingSweep` (`step 16/17 'the leg is recovered': condition 'Check_Recovered' never became true`). One bounded test-only fix each: the row now asserts "terminal present, no AcceptedPose"; BorrowingSweep waits for `Get_TrustedContactCount(Gait) >= 3` before recovering (the C++ `Query.RecoverUnplantedUsesDrainTimeSet` precondition) and logs its recovery state.
3. Final run, actual exit **1**:
```
=== Test summary ===
Total: 444
Passed: 435
Failed: 9
Skipped: 0
Contaminated: 0
Duration: 10m 5s
Failed tests (9):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.Action.Mirror.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_BorrowingSweep
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics
Renderer-only: 6 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (5):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.Action.Mirror.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_BorrowingSweep
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal
Still red (listed) (3):
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns  [Plugins/CkTests]
Listed flaky, failed, not re-run (1):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics  [Plugins/CkTests]
Now passing (listed red in a plugin's file) — not pruned from here; if it holds in the plugin's home project, open a PR there (1):
  Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep  [Plugins/CkTests]
Listed flaky, passed — if it keeps passing, remove the entry (1):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarsEngageSteppedTops  [Plugins/CkTests]
NOTE: the run is already failing: listed-flaky failures were not re-run
```
Population `--test-pattern 'ProceduralAnimation' selects 444 of them (6 renderer-only)` (P2 wide run: 425). 0 `Angelscript: Error` lines. Logs archived in the session scratchpad as `ProceduralErgonomics-P5-attempt{1,2,3}.log`; the final one is also `Saved/Logs/ProceduralErgonomics-P5.log`.

**Delta against the baselines.** `.Action` subset after Phase 3 (171 / 169, reds `MountedToolkit.TwoTabsModesAndCapture`, `SaveReopen.FocusedAssetIdentity`): both still red (B-P1-1 attribution), no other pre-existing `.Action` row changed verdict. Wide-pattern listed reds (`SurfaceMotion.SnapshotShowsThePendingContact`, `PillarCrossingRenderedTraversal`, `PostsCentipedeReturns`, flaky `SpiderInteractiveRosterDiagnostics`): same set as P2 run 1. New in this run and not mine: `SpiderCourseTraversal` (`Ledge completed two route traversals with every leg replanted (1 traversals, 8/8 legs)`), **green alone** (`Saved/Logs/ProceduralErgonomics-P5-SpiderAlone.log`, `--test --no-live --known-reds off --skip-renderer-tests`, exit 0, 1/1); no gait, surface-motion or rig source changed in Phase 5, so it reads as load-sensitive in the 6-lane run (a `flaky` candidate for the CkTests list, by PR there). New and mine: the two rows below (B-P5-1, B-P5-2).

**New rows (final run).** Passed: `Ck.ProceduralAnimation.Action.Mirror.Runtime.{ExpandsBeforeClaimAndKeepsSourceUnchanged, RestNormalizedConvertsThroughSubjectFrames}`, `Ck.ProceduralAnimation.Action.Editor.BlueprintParity.MirrorVm`, `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_{NoncombatManaged, BuilderSweep}`. Failed: `Mirror.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding` (B-P5-1), `Ck_AutoTest_ProceduralAnimation_BorrowingSweep` (B-P5-2). In run 2 every framework assertion of the rejects row passed (Rejected/`MirrorRejected`, no compiled clip, no claim, asset unchanged, advisory report names `MirrorDestination`, 65 mirrors reject before reserving); BorrowingSweep passed 8 of 9 assertions in run 3, including the short sweep (`Outcome: Completed; Recovery: Succeeded; Reason: ClipCompleted`) and the floor-loss diagnosis (`Outcome: SupportLost; Recovery: Failed; Reason: ReturnProbeAttemptsExhausted`, `Get_IsWaitingForRecovery` true).

**BodyPose row alone.** `--test --no-live --known-reds off --skip-renderer-tests --test-pattern "Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation"` (`Saved/Logs/ProceduralErgonomics-P5-BodyPoseAlone.log`), run immediately after the wide gate: exit 1, 1/0/1, `body turned steadily over at least 20 samples (5)`, worst trail 29.7952 deg at sample 5, rate change 187.73 deg/s. It **passed** inside the wide run. Same signature and same circumstance (started while the gate's editors were shutting down) as P2's red-alone run, which the orchestrator's A/B attributed to machine load (B-P2-1). Not re-run on an idle machine.

**Exit grep.** `rg -n 'Mars|Crawler|whip' Plugins/CkFoundation/Script Plugins/CkFoundation/docs/campaigns/procedural-actions Plugins/CkFoundation/Source/CkProceduralAnimation` is **not** 0: 26 hits remain, none in a file this phase created and none in a line it wrote (B-P5-4). The one hit in a file this phase edited (`API.md`, "Mars imports") was reworded.

**Decisions taken inside the package's latitude.**
- `ExpandMirrors` takes the slot bindings: `ExpandMirrors(const FCompiledClip&, const TArray<FCk_ProceduralAction_MirrorSpec>&, const TArray<FCk_ProceduralAction_SlotBinding>&, const FCk_Handle_ProceduralArticulation&, FCompiledClip&, TArray<FDiagnostic>&)`. The locked signature has no bindings, but rest frames are per limb and the slot-to-limb map lives only in the bindings. Declared in `Core/CkProceduralActionValidation.h` (friend of `FCompiledClip`, forward-declared types, no new include), defined in `ActionPlayer/CkProceduralActionPlayer_Mirror.cpp`.
- Where it runs: a service helper `ExpandClipMirrors(Subject, ClipParams, InOutClip, Diagnostics*)` (no-op without mirrors, so existing plays pay no copy) runs between `Compile` and `BindCompiledClip` on all three binding sites: `DoCreate_Loaded` (loaded path, `Create_FromLoadedClip` and `Play_Loaded`), the service's soft-load branch (`Create_FromClip` and `Play`), and `Get_AdmissionReport`. Mirrors ride `FProceduralActionClipParams::_Mirrors` (non-constructor `CK_PROPERTY`) so the deferred path sees them.
- Rest-normalized conversion: each slot's frame is `RestEndpointLocal * AttachmentLocal` of its bound limb with its summed segment length, read through `UCk_Utils_ProceduralArticulation_UE::Get_Limb` + `UCk_Utils_ProceduralLimb_UE::Try_GetLayout`, exactly what `CaptureClipInitial`/`ResolveClipFrame` use. Position: source frame (scaled by its chain) -> body frame -> `ReflectVector` -> destination frame (divided by its chain). Position tangents scale the same way without the offset; bend values and tangents rotate only (stay unit); orientation `R_D^-1 * ReflectRotation(R_S * q)`; an explicit rest target maps as position + rotation. With identity frames this reduces exactly to the draft geometry, which is what BodyLocal and RootAtStart use. Reflection is in the body frame (presentation if any, else root), the frame the rest frames are relative to.
- AnchorAtStart sources reject (`MirrorSpace`): DESIGN §8 says runtime "may" convert them, but an anchor is a caller-supplied world frame with no body mirror plane.
- Rejections (codes `MirrorDestination`, `MirrorSource`, `MirrorSpace`, `MirrorBinding`, `MirrorPlane`, `ContentLimit`): destination None / equal to source / declared by the clip / repeated; source undeclared or never tracked; destination binding missing, to a limb the subject lacks, or to the source's limb; rest-normalized frames unreadable; > 64 slots after expansion, tracks per phase, total keys. Sources must be clip slots (no chained mirrors). The action finishes `Rejected` with reason `MirrorRejected`, no ensure (same as `ClipBindingsRejected`). More than 64 mirrors is rejected at `Create_*`/`Play*` with the existing ensure (`InvalidField = Play.Mirrors`).
- The destination slot copies the source's initial source, terminal-orientation flag, space and anchor; tracks keep interpolation, reach and clearance policies; destination tracks are appended per phase; critical times and events are unchanged (same key times). The content hash folds in each mirror (source, destination, plane) only when mirrors exist, so unmirrored plays keep their revision.
- Native mirror rows live in a new `Test_ProceduralAction_MirrorRuntime.cpp` (PIE, `WITH_EDITOR`), not `Test_ProceduralAction_Mirror.cpp` (pure, no world); the test names are the package's.
- The BP VM row (`BlueprintParity.MirrorVm`) drives the builder's `Mirror_Slot` through a ref draft variable and checks that a Make `LoadedPlaySpec` node exposes `_Mirrors`; it does not run a runtime mirrored play in the VM.
- The BorrowingSweep AS test composes its own static floor box (so it can remove and restore it) and the walker from the gym's `ck::ProceduralGym_Rig4` / `ck::ProceduralGym_Gait` and `ck_procedural_gym::BodyClearance`, as `NoncombatCarrierReach` does; it does not use `FCkProceduralAnimationGym_Fixture.Create`, which spawns its own crawlers and has no partial floor removal.

**Not run.** No full gate, no Blueprint `[EDITOR-VERIFY]`, no A/B with the phase reverted.

### Phase 5 re-gate record (2026-10-12, Opus executor, after the orchestrator's decisions)

**Changes since the Phase 5 gate record.**
- B-P5-1: `Test_ProceduralAction_MirrorRuntime.cpp` now calls `AddExpectedError(MirrorCountEnsure, EAutomationExpectedErrorFlags::Contains, 0, false)`, so the pattern is not a regex. Test only.
- B-P5-2: `CkAutoTest_ProceduralAnimation_BorrowingSweep.as` now waits until Leg1-3 each publish a `Planted` foot with `Trusted` contact before it requests recovery, matching `Gait/Test_ProceduralGait_RecoveryPie.cpp` ("Three independent gait contacts trust the restored floor"). It logs the recovery completion, the unplanted set and Leg0's control state. `LimbControl/CkProceduralLimbControl_Processor.cpp`: the `RecoverUnplantedLimbs` drain's failed path now logs at Log level, naming the subject, the gathered limb IDs and the `ECk_ProceduralLimbControl_AcquireResult`, written as its integer value because that enum is not a UENUM and `{}` formatting it does not link. No change to the admission rules.
- Dirty tree after this re-gate: CkFoundation `33 files changed, 1752 insertions(+), 152 deletions(-)` (Phase 5 adds `LimbControl/CkProceduralLimbControl_Processor.cpp` since the last record); CkTests `3 files changed, 472 insertions(+)` plus the untracked files listed before.

**Command (2 invocations of the same line).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P5b.log --project="D:/Repositories/CkRepos/Orion"
```
1. Exit 1, link failure: `LNK2019 unresolved external symbol StaticEnum<enum ECk_ProceduralLimbControl_AcquireResult>`. That enum is a plain enum class (`Private/.../LimbControl/CkProceduralLimbControl_Control.h:8`), so its declared formatter cannot resolve `StaticEnum`. Fixed by logging `static_cast<int32>(Acquired)`.
2. Final run, actual exit **1**:
```
=== Test summary ===
Total: 175
Passed: 173
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 3m 19s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 5 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population `--test-pattern 'ProceduralAnimation.Action' selects 175 of them (5 renderer-only)`, which is Phase 3's 171 plus the 4 new native rows. 0 `Angelscript: Error` lines. The only failing names are the two inherited editor reds (B-P1-1). Passed: `Action.Mirror.Runtime.{ExpandsBeforeClaimAndKeepsSourceUnchanged, RejectsOccupiedDestinationMissingSourceIncompatibleBinding, RestNormalizedConvertsThroughSubjectFrames}`, `Action.Editor.BlueprintParity.MirrorVm`, and all Phase 1-3 `.Action` rows.

**Test-only runs on the same binary** (`--test --no-live`, actual exit **0** for each):
- `--test-pattern Ck_AutoTest_ProceduralAnimation_BorrowingSweep` (`Saved/Logs/ProceduralErgonomics-P5b-Borrowing.log`): Total 1, Passed 1, Failed 0, Contaminated 0, 31s. Test log: short sweep `Outcome: Completed; Recovery: Succeeded; Reason: ClipCompleted`; floor-loss sweep `Outcome: SupportLost; Recovery: Failed; Reason: ReturnProbeAttemptsExhausted`; `RecoverUnplantedLimbs completed ECk_Request_OperationResult::Succeeded (0); unplanted now [] control=ECk_ProceduralLimbControl_State::Returning (2) reason=ExplicitRecovery`; then Leg0 back to `Default` with trusted support.
- `--test-pattern ProceduralAnimation.Articulation` (`..._P5b-Articulation.log`): selects 12; Total 12, Passed 12, Failed 0, Contaminated 0, 37s.
- `--test-pattern Ck_AutoTest_ProceduralAnimation_NoncombatManaged` (`..._P5b-Noncombat.log`): Total 1, Passed 1, 29s.
- `--test-pattern Ck_AutoTest_ProceduralAnimation_BuilderSweep` (`..._P5b-BuilderSweep.log`): Total 1, Passed 1, 29s.
Each printed `(no listed test ran in this run, and there were no new failures)`.

**B-P5-2 root cause.** The AS row asked for recovery on the wrong signal. Its precondition `utils_procedural_gait::Get_TrustedContactCount(Gait) >= 3` counts the published foot contact of *every* live leg (`Gait/CkProceduralGait_Utils.cpp:210-214`), including the unplanted, borrowed Leg0. While a foot is not planted, its contact reports the trust of the swing target, not of a plant. So the count read 4 before Leg1-3 had re-planted on trusted ground. Recovery of a locomotor limb takes a recovery lease against the player's support policy (`LimbControl/CkProceduralLimbControl_Control.cpp:171`, `TryAcquireRecoveryLease(..., Locomotion.Get_Support(), ...)`), and a rejected lease returns `SupportUnavailable` (`:181`). With RecoveryPie's precondition (each other leg planted on trusted contact) the same request succeeds. Confirmed by the passing run. Inferred, not observed: the earlier failure's AcquireResult was `SupportUnavailable`; it was not logged then, and the new drain log did not fire in the passing run. The drain shares `TryRecover` with `Request_RecoverLimbs` (`LimbControl/CkProceduralLimbControl_Processor.cpp`, both lambdas), so this is no framework defect.

**Exit grep (scoped, orchestrator decision 4).** `rg -n 'Mars|Crawler|whip' Plugins/CkFoundation/Source/CkProceduralAnimation Plugins/CkFoundation/Script Plugins/CkFoundation/docs/campaigns/procedural-actions/{GETTING_STARTED,API,AUTHORING}.md` returns three lines, all in `Source/CkProceduralAnimation/Claude.md` (lines 50, 106, 214). All three are gym species fixture names (`SmallCrawler4`, `Crawler4/6/8`), ruled out of scope. No other hits: 0 project names.

**Attributions recorded (orchestrator decision 5).**
- `Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal`: red in the Phase 5 wide run, green alone, and no phase touched gait, surface-motion or rig code. Same shape as the listed `PillarCrossingRenderedTraversal`. Load-sensitive; a `flaky` candidate for the CkTests list, by PR there. Not ours.
- `Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation`: red alone right after the wide gate (5 steady samples, worst trail 29.7952 deg), green inside it. Matches the Phase 2 machine-load attribution (B-P2-1). No further runs.

**Not run.** No full gate and no wide `ProceduralAnimation` pattern after these changes (the changes are test files plus one log line in the recovery drain). No Blueprint `[EDITOR-VERIFY]`.

### Phase 4a gate record (2026-10-12, Opus executor; steps 1-4 of PHASE_4)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; all dirty on top of Phases 1-3 and 5, nothing staged or committed).

`git -C Plugins/CkFoundation diff --stat` for the files this phase touched (Data.h carries Phase 1's progress fields too):
```
 .../Action/CkProceduralAction_Data.h               | 113 +++
 .../CkProceduralActionAuthoringCommands.cpp        | 842 +++++++++++++++++++--
 .../Session/CkProceduralActionAuthoringSession.cpp |  90 +++
 .../CkProceduralActionAuthoringCommands.h          |  59 ++
 .../CkProceduralActionAuthoringSession.h           |  62 ++
 5 files changed, 1108 insertions(+), 58 deletions(-)
```
Whole CkFoundation tree: `37 files changed, 2810 insertions(+), 210 deletions(-)` (was 33 / 1752 / 152 after Phase 5). CkFoundation untracked, new in 4a: `Source/CkProceduralAnimationEditor/Public/CkProceduralAnimationEditor/{CkProceduralActionPoseGroups.h, CkProceduralActionSolvedCapture.h}`, `Source/CkProceduralAnimationEditor/Private/Authoring/{CkProceduralActionPoseGroups.cpp, CkProceduralActionSolvedCapture.cpp}`.
`git -C Plugins/CkTests diff --stat`: `3 files changed, 472 insertions(+)` (unchanged from Phase 5; no tracked CkTests file edited in 4a). CkTests untracked, new in 4a: `Source/CkTestsEditor/Private/CkProceduralAnimation/{Test_ProceduralAction_PoseCapture.cpp, Test_ProceduralAction_SolvedCapture.cpp}`.

**Red before implementation.** Tests written first. A data-header edit made before the tests was reverted (byte-identical to the pre-phase diff, checked by diffing the hunk sets) before the red check. `rg --no-ignore -l -F` over CkFoundation `Source` + `Script` at 13:34:35, after the tests and before any implementation, found 0 files for each of `FCk_ProceduralAction_PoseGroup`, `ECk_ProceduralAction_PoseProvenance`, `_EditorPoseGroups`, `Get_EditorPoseGroups`, `UpdatePose`, `DuplicatePose`, `MovePose`, `DeletePose`, `CaptureSolvedPose`, `CreateFromRecipe`, `FCk_ProceduralActionSolvedSnapshot`, `FCk_ProceduralActionSolvedCaptureTarget`, `CkProceduralActionSolvedCapture.h`, `CkProceduralActionPoseGroups.h`, `FCk_ProceduralActionPoseGroupView`, `DerivePoseGroups`, `ReadPoseKey`, `SetStagingTime`, `SetStagingBody`, `SetStagingLimb`, `SetParticipants`, `LoadStagingFromPoseGroup`, `GetStagingPose`, `GetPoseGroups`; `CapturePose` matched only CkJolt's unrelated `DoCapturePoses_AnyThread`. Red by construction (cannot compile); not observed with a build. `MirrorDelegatesToBuilder` compiles against the old code; it is red by reasoning only (the old command rejected any orientation keys, `Mirror supports body/root-local position and bend tracks without orientation`), not observed.

**Command (3 invocations of the same line).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action.Editor --output=Saved/Logs/ProceduralErgonomics-P4a.log --project="D:/Repositories/CkRepos/Orion"
```
1. Exit 1, build failed: `CkProceduralActionAuthoringCommands.cpp(100,43): error C2676: binary '!=': 'const ck::procedural_action::EDiagnosticSeverity'` (the internal compiler diagnostic has its own severity enum). Fixed with two `IsError` overloads.
2. Exit 1, 53 / 50 / 3: my `PoseCapture.SaveReopenKeepsIds` killed its lane editor (`Fatal error: LongPackageNameToFilename failed to convert '/CkActionPoseGroups_.../Clip'. Path does not map to any roots`): the test converted the package path before registering its mount point. One bounded test-only fix (register first).
3. Final run, actual exit **1**:
```
=== Test summary ===
Total: 53
Passed: 51
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 1m 40s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 4 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population: `--test-pattern 'ProceduralAnimation.Action.Editor' selects 53 of them (4 renderer-only)`; the pre-build cache of run 2 said 44, so baseline 44 + 9 new rows. The run also printed `main: editor exited abnormally (exit=0xFF (decimal 255)) but AFTER completing every test — results kept`; the same line is in `ProceduralErgonomics-P5b.log` and `-P1-EditorGroup.log`, so it predates this phase. Logs archived in the session scratchpad as `ProceduralErgonomics-P4a-attempt{1,2,3}.log`; the final one is also `Saved/Logs/ProceduralErgonomics-P4a.log`.

**Delta against the baseline.** Baseline for this pattern: 44 rows, red = the two inherited editor reds (B-P1-1). After: 53 rows, red = the same two names. Every other pre-existing `.Action.Editor` row passed, including `AuthoringCommands.MirrorAndSpace` (the old mirror row, now running through the delegating command), `AuthoringCommands.{StableIds, WindowBoundaries, TracksAndUndo, EventDrag, RetargetMapping, GizmoKeys, RenameSlot}`, `MountedToolkit.SharedPreviewRefresh`, `PieCoexist.IndependentWorlds`, `CurveAdapter.*`, `SolverWorld.*`.

**New rows, all selected and passed (final run):** `Ck.ProceduralAnimation.Action.Editor.AuthoringCommands.PoseCapture.{CaptureUpdateDuplicateMoveDelete, UndoRedoRestoresTracksAndGroups, SaveReopenKeepsIds, OldClipDerivesTemporaryGroups, StagingIsSessionOwned, CreateFromRecipeMatchesBuilder}`, `...AuthoringCommands.SolvedCapture.{RejectsStalePartialAndMalformed, ConvertsToDeclaredSpace}`, `...AuthoringCommands.MirrorDelegatesToBuilder`.

**Not yet written (4b, step 1 rows that test 4b steps):** `AuthoringCommands.RecipeCreationCancelLeavesNoAssets`, `AuthoringCommands.PairedSaveFailureReportsWhichAsset`, `MountedToolkit.PoseWorkspace.CaptureAndPlay`, `MountedToolkit.PoseWorkspace.AdvancedHidesRawDetails`. No mounted fixture drives a pose exercise yet, so the step count of the scripted body/reach/sweep exercise is **not observed**.

**Decisions taken inside the package's latitude (vetoable).**
- *Pose-group ID and undo model.* `_EditorPoseGroups` lives on the clip object, so `Clip->Modify()` snapshots keys and metadata together and one undo restores both. Capture and Duplicate mint `FGuid::NewGuid()`; Update and Move keep the ID; Delete removes the entry. Old clips are never written on open: temporary groups are derived per (phase, exact key time) from unclaimed body keys and limb position keys, with an ID derived from `StrCrc32(phase name)` and the time's bit pattern, so it is stable across sessions and machines. The first explicit edit of a temporary group (Update, Move) persists it under that derived ID. Recipe creation persists one group per recipe key time with fresh GUIDs.
- *Group membership.* A channel joins a group by a body key (body) or a position key (slot) at exactly the group time (same exact equality as the builder's occupancy check); orientation and bend keys at that time ride with their slot. The body channel is the reserved name `(Body)` (`FCk_ProceduralAction_PoseGroup::Get_BodyChannelName`). A persisted group with a channel lacking its key, or sharing a key with another group, is reported broken with a channel-naming reason; Update/Duplicate/Move refuse a broken group, Delete accepts it.
- *Compile gate.* Pose commands write through `Build_Into`, which writes only a compiling draft; the command compiles the draft first, so a refused edit opens no transaction and dirties nothing. Consequence: an edit that removes a phase's 0 or 1 key (moving or deleting an endpoint pose) is refused with `KeyEndpoints`. See question Q-P4-1.
- *Mirror.* `MirrorSlot` keeps its signature (`EAxis` normal: X->YZ, Y->XZ, Z->XY), copies the clip into a draft, runs `Mirror_Slot`, and writes `_Phases` back in one transaction. It does **not** go through `Build_Into`, so it still works on a draft that does not compile yet (the existing `MirrorAndSpace` row mirrors a one-key track). It now mirrors orientation keys too (the builder's `q' = (w, -S v)`) and rejects what the builder rejects (codes `MirrorSpace`, `MirrorDestination`, ...). Runtime `Mirror_Slot` geometry untouched.
- *Move/Duplicate tangents.* `Add_PoseKey` writes zero tangents; Move and Duplicate copy the source keys' arrive/leave tangents onto the new keys (body, position, bend), so a moved Catmull-Rom key keeps its shape. Tracks emptied by Move/Delete stay (they carry the slot frame).
- *Capture contract.* `CapturePose(phase, pose, participants, label)` requires the pose's channels to equal the participant set (missing participant and extra channel both refuse, naming them); an occupied time refuses with "use Update Pose ... or Duplicate". Update requires exactly the group's channels and writes at the group's time.
- *Solved capture: revision and settlement.* `FCk_ProceduralActionSolvedSnapshot::Make(SolverWorld)` copies: Ready = `EStatus::Ready` and no pending seek (`GetSeekTargetStep() == INDEX_NONE`); the last `AcceptedPose` record's `CurrentPose`; Settled = that record's `Stamp.SubjectPoseRevision == UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(subject)` (the copied pose is the subject's latest settled pose); PreviewRevision = `GetRecording().Get_Provenance().Get_PreviewRevision()`. The command refuses unless Ready, Settled and PreviewRevision equals the target's session revision. **Inferred, not confirmed against a live Solver:** that the stamp's `SubjectPoseRevision` and `Get_SettledRevision` count the same revisions and that the Observe publication lands in the same tick the subject settles; the unit rows build snapshots by hand plus one idle `FCk_ProceduralActionSolverWorld`. A Finished (terminal) preview is refused as not Ready.
- *Solved conversion frames* mirror the runtime's (`ActionPlayer/CkProceduralActionPlayer_Service.cpp` space switch): BodyLocal = the solved pose's root frame; RootAtStart = the preview config's root frame; AnchorAtStart and LimbRestNormalized through the editor's `ResolveSlotFrame` with the body world = solved body presentation when present, else the config's presentation offset applied to the solved root. Rest-normalized positions are divided by the chain length. A slot's frame is the one its tracks use (target phase first), BodyLocal when it has none. Orientation is written only when the slot requires terminal orientation or its track already has orientation keys. Body capture inverts `ComposeProceduralBodyPoseAction` against the config's presentation offset and is refused for a walking subject. Any missing channel (no binding, no limb pose, no body presentation) or malformed geometry refuses the whole capture, naming every affected channel.
- *Recipe creation* refuses a clip with any authored content (phases, slots, enabled body, markers, windows).
- *Session staging.* `SetStagingTime/Body/Limb`, `SetParticipants` (drops staged targets outside the set), `LoadStagingFromPoseGroup` (key values, not interpolated samples). Pose groups are rebuilt in `Refresh()` only (revision change), never per frame.

**Questions for the orchestrator (no workaround invented).**
- **Q-P4-1 (architecture): compile-gated pose commands vs. incremental authoring.** PHASE_4 step 3 mandates `Build_Into` for every pose command, and `Build_Into` writes only a compiling draft. So (a) the first capture into a phase with no keys (the Blank recipe's `Motion` phase) is refused (`KeyEndpoints`), (b) no pose command works on a clip that is already invalid for an unrelated reason, while DESIGN §4 says invalid drafts stay saveable. Recipes other than Blank work (they key 0 and 1). Options: (A) keep the gate and make Blank seed endpoint keys for its listed channels; (B) let pose commands write an uncompiled draft directly (as `MirrorSlot` now does) and leave `Build_Into` to code callers; (C) keep the gate and document that pose authoring starts from a recipe or from 0/1 keys added on the timeline.
- **Q-P4-2: settledness evidence.** Confirm that `SubjectPoseRevision == Get_SettledRevision` is the intended "settled" test, or name the signal the Solver should expose instead; 4b's mounted row is the first place it meets a live Solver.

**Not run.** No mounted pose rows (4b), no wider `ProceduralAnimation` pattern, no full gate, no `[EDITOR-VERIFY]`. Comment audit of the 4a diff done (no breadcrumbs; the comments kept are contract/why lines).

**Next bounded step (4b, fresh session).** Steps 5-7: workspace layout, guided creation, the two remaining command rows and the two mounted `NonNullRHI` rows, then the gate with `--output=Saved/Logs/ProceduralErgonomics-P4b.log`. Risk to plan for: moving the raw Details views into a collapsed Advanced area changes the preconditions of the existing mounted rows (`MountedToolkit.*` finds the preview Details as the second `SDetailsView`; `SaveReopen.FocusedAssetIdentity` clicks the timeline's Add phase button), so 4b must expand Advanced in those fixtures or keep those widgets reachable.

### Phase 4b gate record (2026-10-12, Opus executor; steps 5-7 of PHASE_4 plus the orchestrator's Q-P4-1/Q-P4-2 decisions)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged; everything dirty on top of Phases 1-3, 5 and 4a; nothing staged or committed). Monolith's dirty paths untouched.

`git -C Plugins/CkFoundation diff --stat` for the tracked files this phase touched (several carry earlier phases' hunks too):
```
 .../Private/Asset/CkProceduralAction_Factory.cpp   |   55 +-
 .../CkProceduralActionAuthoringCommands.cpp        |  889 ++++++++-   (4a + 4b)
 .../Session/CkProceduralActionEditorModes.cpp      |    4 +-
 .../Toolkit/CkProceduralAction_EditorToolkit.cpp   | 1968 +++++++++++++++++---
 .../Toolkit/CkProceduralAction_EditorToolkit.h     |  176 ++
 .../Widgets/SCkProceduralAction_Timeline.cpp       |    3 +-
 .../Private/Widgets/SCkProceduralAction_Timeline.h |    2 +
 .../CkProceduralActionAuthoringCommands.h          |   74 +    (4a + 4b)
 .../CkProceduralAction_Factory.h                   |   21 +
 docs/campaigns/procedural-actions/API.md           |   88 +-     (Phase 5 + one 4b row)
 docs/campaigns/procedural-actions/AUTHORING.md     |    8 +-      (Phase 5 + the 4b workspace paragraph)
```
Whole CkFoundation tree: `44 files changed, 4818 insertions(+), 496 deletions(-)` (was 37 / 2810 / 210 after 4a). CkFoundation untracked, new in 4b: `Source/CkProceduralAnimationEditor/Public/CkProceduralAnimationEditor/CkProceduralActionGuidedCreation.h`, `Source/CkProceduralAnimationEditor/Private/Asset/CkProceduralActionGuidedCreation.cpp`, `Source/CkProceduralAnimationEditor/Private/Widgets/SCkProceduralAction_ProgressPlot.{h,cpp}`; `ActionBuilder/CkProceduralActionBuilder_Utils.{h,cpp}` (untracked since Phase 1) gained `Write_Into`.
`git -C Plugins/CkTests diff --stat`: `6 files changed, 624 insertions(+), 7 deletions(-)` (tracked 4b edits: `Test_ProceduralAction_MountedToolkit.cpp` +62, `Test_ProceduralAction_SaveReopen.cpp` +95, `Test_ProceduralAction_PieCoexist.cpp` 1 line). CkTests untracked, new in 4b: `Source/CkTestsEditor/Private/CkProceduralAnimation/{Test_ProceduralAction_GuidedCreation.cpp, Test_ProceduralAction_PoseWorkspace.cpp}`; edited untracked: `Test_ProceduralAction_Builder.cpp` (+1 row), `Test_ProceduralAction_PoseCapture.cpp` (one assertion block, see below).

**Orchestrator decisions implemented.**
- *Q-P4-1 -> option B.* `UCk_Utils_ProceduralActionBuilder_UE::Write_Into(Draft, Destination)` (runtime builder, same category and report as `Build_Into`): rejects only a null destination (`NullData`) or `CompatibilityVersion != 1` (`Version`), then copies the five authored fields without compiling; its contract comment says it is the incremental authoring path and that Play/`Build_Into` stay compile-gated. `CommitDraft` (every pose command, `MirrorSlot`, and the new `SetPhaseDuration`/`SetPhaseTiming`) now writes through `Write_Into` under one transaction; the compile pre-check is gone. B-P4-1 is resolved by this.
- *Q-P4-2, confirmed in code:* `FCkProceduralActionEditorModes::StartSolver` passes `Session->GetRevision()` to `StartClip` (`Private/Session/CkProceduralActionEditorModes.cpp:113-114`); `StartClip` stores it (`Private/Session/CkProceduralActionSolverWorld.cpp:132`); `CreateAction` stamps it into the recording's `FProvenance` (`CkProceduralActionSolverWorld.cpp:390-394`, also on every seek rebuild); `FCk_ProceduralActionSolvedSnapshot::Make` reads `Recording.Get_Provenance().Get_PreviewRevision()` (`Private/Authoring/CkProceduralActionSolvedCapture.cpp:15`). So the snapshot's revision is the session revision the Solver started from. Runtime evidence for the settled test (the 4a inference): in both gate runs the mounted row's first paused capture on a live BodyPulse Solver was accepted (`Solved capture accepted on attempt 1`), i.e. `Stamp.SubjectPoseRevision == Get_SettledRevision(subject)` held on a paused live Solver. One clip, one subject; not proof for walking subjects.

**Red before implementation.** Tests written first. `rg --no-ignore -l -F` over CkFoundation `Source` + `Script` at 14:11:30, after the four test files existed and before any implementation, found 0 files for each of `Write_Into`, `CkProceduralActionGuidedCreation.h`, `FCk_ProceduralActionCreation_Choice`, `ECk_ProceduralActionCreation_Subject`, `Get_PreviewSiblingPackageName`, `ConfigureFromChoice`, `Get_HasConfirmedChoice`, `DescribeSaveResults`, `FCk_ProceduralActionAssetSaveResult` and every widget tag the mounted rows look up (`CkActionEditorAdvanced`, `CkActionEditorAdvancedHeader`, `CkActionEditorPlay`, `CkActionEditorFrame`, `CkActionEditorCapturePose`, `CkActionEditorUpdatePose`, `CkActionEditorCaptureSolved`, `CkActionEditorPoseMessage`, `CkActionEditorStatus`, `CkActionEditorNudge_`, `CkActionEditorRecordedMode`, `CkActionEditorProgressPlot`, `CkActionEditorSubjectTree`, `CkActionEditorInspector`); `SaveAssets` matched only CkVat's unrelated `EPersistence::SaveAssets`, and no Solver status printed a gap count (the CaptureAndPlay row asserts ` 0 gaps`). Red by construction (cannot compile / cannot find widgets); not observed with a build.

**Command (2 invocations of the same line, plus one classification run).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P4b.log --project="D:/Repositories/CkRepos/Orion"
```
1. Build green first time. Exit 1, 189 / 186 / 3: the two inherited reds plus `Ck.ProceduralAnimation.Action.Editor.PieCoexist.IndependentWorlds` (`Action toolkit PIE coexistence timed out at stage 3 ... transportPause=0`). Cause, mine and confirmed from the fixture source: its `HasText`/`FindButton` match a button label *exactly* and it clicked `"Play"`; the workspace's button reads `Play solved preview`, so no button was found and nothing was clicked. Fixture correction (one line): click `"Play solved preview"`. Also added a diagnostic to `SaveReopen.FocusedAssetIdentity` (its marker assertion now prints the timeline's message, tagged `CkActionEditorTimelineMessage` in `SCkProceduralAction_Timeline`).
2. Final run, actual exit **1**:
```
=== Renderer pass complete: 5 passed, 2 failed ===
=== Test summary ===
Total: 189
Passed: 187
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 3m 38s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 7 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population: `--test-pattern 'ProceduralAnimation.Action' selects 189 of them (7 renderer-only)`. Renderer group: `PoseWorkspace.AdvancedHidesRawDetails`, `PoseWorkspace.CaptureAndPlay`, `MountedToolkit.SharedPreviewRefresh`, `MountedToolkit.TwoTabsModesAndCapture`, `PieCoexist.IndependentWorlds`, `SaveReopen.FocusedAssetIdentity`, `Ck.ProceduralAnimation.Debugger.PIE.LegActions`. The `main: editor exited abnormally (exit=0xFF) but AFTER completing every test — results kept` line appears again (predates this phase, see 4a). Logs: `Saved/Logs/ProceduralErgonomics-P4b.log` (final) and scratchpad copies `ProceduralErgonomics-P4b-attempt{1,2}.log`, renderer-editor logs `P4b-attempt{1,2}-renderer-Mars.log` (the fixtures' `AddInfo` lines are only in the renderer editor's own log).
3. Classification of the moved red, `--test --no-live --known-reds off --test-pattern "Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity"` -> `Saved/Logs/ProceduralErgonomics-P4b-FocusedAlone.log`: **1/1 passed, exit 0**, on the new layout (it expands Advanced, adds a phase, a marker and a window by routed input, saves with Ctrl+S and reopens).

**Delta against the baseline.** Baseline for this pattern after 4a: `.Action` 175 native/AS rows + `.Action.Editor` 53 (51 green, red = `MountedToolkit.TwoTabsModesAndCapture`, `SaveReopen.FocusedAssetIdentity`). After: 189 selected, red = the same two names; every other row passed, including all 4a rows, `AuthoringCommands.MirrorAndSpace`, `MirrorDelegatesToBuilder`, `MountedToolkit.SharedPreviewRefresh` and (after the label fix) `PieCoexist.IndependentWorlds`.
- `TwoTabsModesAndCapture`: identical failure text to every run since Phase 1 (`timed out at stage 2: Expanded Preview Placement has no visible Root Frame`). The updated fixture now gets through the new Advanced expansion first (its diagnostic shows both Details reachable, preview Details 632 x 321 px, `placement=1 root=0`), so the layout change is not the cause; the stage-2 reason is the inherited one. Not fixed.
- `FocusedAssetIdentity`: B-P1-1's in-group/alone split persists. Its failure point moved: before 4b the routed `Add phase` click missed; now `Add phase` succeeds and the routed `Marker at cursor` click does not reach the button (the timeline's message is still its idle hint, so `AddMarker` never ran). Green alone (run 3). The prime suspect stays B-P1-1's: state left by the timed-out `TwoTabsModesAndCapture` in the same editor (it times out 1 s before `PieCoexist`, which runs just before this row).

**New rows, all selected and passed (both runs):** `Ck.ProceduralAnimation.Action.Builder.WriteIntoAcceptsUncompilableDraft`, `Ck.ProceduralAnimation.Action.Editor.AuthoringCommands.RecipeCreationCancelLeavesNoAssets`, `...AuthoringCommands.PairedSaveFailureReportsWhichAsset`, `...MountedToolkit.PoseWorkspace.AdvancedHidesRawDetails` (NonNullRHI), `...MountedToolkit.PoseWorkspace.CaptureAndPlay` (NonNullRHI). `Renderer-only: 7 test(s) ran in a real-renderer editor` names the two mounted rows among them.

**CaptureAndPlay, observed (renderer log, both runs identical):** requested capture from the inspector at Windup + 2 frames; one Play click ran the solved preview to `step 37; action terminal; preview complete; 32 copied receipts, 0 gaps` over 38 observed editor frames; `Capture solved pose` at terminal was refused (`The Solver preview is not ready: pause it at a step with no seek or preparation pending.`); a re-run paused at step >= 4 was accepted on the first attempt (provenance Solved); after `Update pose` the next solved capture was refused (`The solved preview ran on revision 3, but the clip and preview are now at revision 5. Press Play solved preview to re-run it...`); a second re-run paused at step >= 6 was accepted again. **Observed step count of the scripted body exercise: 17 routed author actions (2 of them transport frame steps).** The reach and sweep parts of the exercise are not driven by any fixture: not observed.

**Existing fixtures updated (layout maintenance, no assertion weakened).**
- `MountedToolkit.TwoTabsModesAndCapture`: opens Advanced (header tag) before the Details stage and before the Recorded/Authored buttons, closes it before authored playback, ticks `Requested preview` before clicking Play (Play is now the solved preview), and selects the LimbNod slot through the subject tree's `CkActionEditorChannel_<Slot>` button instead of the old timeline's `Slot <Id>` row.
- `SaveReopen.FocusedAssetIdentity`: opens Advanced and scrolls the per-track tools into view before typing the phase ID and before the marker/window clicks; prints the timeline message on the marker assertion.
- `PieCoexist.IndependentWorlds`: clicks the exact label `Play solved preview`.
- `AuthoringCommands.PoseCapture.CaptureUpdateDuplicateMoveDelete` (4a): its last block asserted the old compile gate (moving the phase-start pose refused with `KeyEndpoints`). Under option B the move is written, so the block now asserts the move succeeds, the clip then fails `Compile` with `KeyEndpoints`, and the pose keeps its ID.

**Decisions taken inside the package's latitude (vetoable).**
- *Layout.* Header (subject picker, Create Preview Config, Save clip + preview with a per-asset report), transport (Play solved preview / Pause, `Requested preview (authored targets, not solved)` checkbox, Solver (restart), Reset, Frame, time slider), status line, then subject tree | viewport + handle buttons | selected-pose inspector over phase timing; below: pose timeline rows (124 px), pose actions with their message, issues (76 px) and the collapsed `Advanced` expandable area (340 px when open: clip Details | preview Details | the old one-by-one timeline tools | Authored/Recorded buttons, receipt stepping, recorded and support diagnostics, X/Y/Z key curves and plots). `Solver (restart)` stays on the main transport because three existing mounted rows and the PIE exercise use it.
- *Play.* Without the requested checkbox, Play starts (or restarts, when absent, invalidated, finished, errored or cancelled) the Solver and toggles play/pause; with it, Play enters Authored mode and plays the session. Enablement: compiled clip and a preview config (plus `CanPlay()` for the requested preview, unchanged). The disabled tooltip is the first error diagnostic (`Play is disabled until the clip compiles. <FieldPath> [<Code>]: expected ...; got ...`) or `Choose a preview subject...`.
- *Staging.* The pose time follows the transport (Reset, Frame and the slider in Authored mode); selecting a pose chip moves the transport to it and loads its key values (4a `LoadStagingFromPoseGroup`). The inspector edits staged body offset and limb endpoint positions (spin boxes plus -1/+1 nudges); orientation and bend keep whatever the loaded pose had (bend is edited with the viewport's `Move bend guide`, which edits the key directly as before). Ticking a channel adds it to the participants with an identity/zero staged value; unticking releases it. The clip opens with its first pose selected.
- *Solved capture target.* Phase and normalized time come from the Solver's current time; channels are the ticked participants; label from the `New pose label` field. With no live Solver for this revision, the button explains the revision mismatch from the revision the toolkit last started the Solver at (the 4a command's own revision check covers a live stale Solver).
- *Curve panel.* `SCkProceduralAction_ProgressPlot` samples `EvaluateProgress` of the selected phase's compiled phase at 65 points on selection/revision change; preset easing menu, `Custom curve` (seeded from the current curve at 0/0.25/0.5/0.75/1, easing set to Linear) or `Use preset easing`; interior key time/progress spin boxes; duration spin box. New commands `SetPhaseDuration` and `SetPhaseTiming` (both through `CommitDraft`). An uncompilable clip shows no curve and says why.
- *Issue summary.* Every compile diagnostic as a button (`<FieldPath> [<Code>]: expected ...; got ...`, warnings prefixed); click selects its phase (timing panel + old timeline) and slot.
- *Selection sync.* Selecting a phase, a pose or a subject channel also selects the matching track in the old timeline, so the viewport handles, the X/Y/Z curves and the Advanced tools follow the workspace selection.
- *Rebuilds.* Subject, pose, issue, inspector and timing rows are rebuilt on session revision change (every refresh path, plus an `OnTicker` revision check) and on selection/participant change; never per frame. The Play enablement check allocates nothing.
- *Guided creation.* `UCk_ProceduralAction_Factory::ConfigureProperties` shows a modal dialog (a structure Details view of `FCk_ProceduralActionCreation_Choice`: subject `UseExisting` / `NewBodyOnly` / `NewDedicatedLimbs` / `NewWalker`, existing preview, recipe spec) and returns `ConfigureFromChoice(result)`; cancel or an invalid choice (recipe the builder rejects, reuse without a preview) leaves no confirmed choice and creates nothing. The Content Browser names the asset after the dialog. `FactoryCreateNew` with a confirmed choice checks the sibling `<Clip>_Preview_DA` path is free before creating anything, fills the clip with the 4a `CreateFromRecipe` (pose groups included), creates the preview config for a new subject (`Standalone` for body-only/dedicated, `Walking` for walker; limbs, rig and gait are filled afterwards in its Details) and remembers the pairing; a refusal there shows a notification and returns null. A factory used from code without the dialog still creates a bare clip (existing `AuthoringSession.AssetFactories`). Creation saves nothing, so there is no partial-save cleanup to do; paired creation is two asset creations, not one undo step.
- *Paired save.* `ck::procedural_action_editor::SaveAssets` saves each asset on its own (`UPackage::SavePackage`), reporting per asset: missing object, transient package, unmounted path, read-only file, or engine save failure; `DescribeSaveResults` prints `Saved <path>` / `NOT saved <path>: <reason>`.
- *Solver status* now prints `N copied receipts, M gaps`.

**Comment audit of the 4b diff.** Done: kept only contract/why comments (`Write_Into` contract, `CommitDraft` why, factory and guided-creation contracts, workspace and plot class comments, fixture "why the Advanced area" notes); no phase/campaign breadcrumbs in shipped files or test names.

**Known nit (not fixed, cosmetic).** After a successful capture the toolkit selects the new pose, and the selection message replaces the command's success message (`Selected Pose 2: its key values are staged.` where `Captured ...` was meant). One-line follow-up in `FinishPoseCommand` callers.

**Not run.** No wider `ProceduralAnimation` pattern, no full gate, no `[EDITOR-VERIFY]`. A focused green on `ProceduralAnimation.Action` is not the full gate.

### Phase 4c gate record (2026-10-12, Opus executor; orchestrator decision on B-P4b-1 -> option B)

**Change.** `FCk_ProceduralActionSolvedSnapshot::Make` (`Source/CkProceduralAnimationEditor/Private/Authoring/CkProceduralActionSolvedCapture.cpp`): Ready is now `GetStatus() == EStatus::Ready && (GetSeekTargetStep() == INDEX_NONE || GetSeekTargetStep() == GetStep())`; a reached seek target is satisfied, not pending. `FCk_ProceduralActionSolverWorld` is unchanged (`_SeekTarget` is not cleared). A `Finished` preview is still not Ready, and the command's revision check is unchanged. New row `Ck.ProceduralAnimation.Action.Editor.AuthoringCommands.SolvedCapture.AcceptsSatisfiedSeek` in `Plugins/CkTests/Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_SolvedCapture.cpp`: a live BodyPulse Solver at revision 21 prepares, seeks to the first interior step from step 8 that is not on a body key time, ticks until the step is reached, and then asserts: Ready with the reached seek target still recorded; a target at revision 22 is refused naming the revision; the revision-21 capture is accepted with Solved provenance; after `Resume` to `Finished` the snapshot is not Ready and a capture is refused naming readiness and writes nothing. When a reached step has no settled accepted pose, the row steps the seek forward (at most 8 seeks) and logs it; observed: `satisfied seek captured at step 8 after 1 seek(s)`. `CaptureAndPlay` is unchanged (it already passes by pausing a re-run, and the new row covers the seek path).

**Command.**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action.Editor --output=Saved/Logs/ProceduralErgonomics-P4c.log --project="D:/Repositories/CkRepos/Orion"
```
Build green; actual exit **1**:
```
=== Renderer pass complete: 4 passed, 2 failed ===
=== Test summary ===
Total: 58
Passed: 56
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 2m 5s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 6 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population: `--test-pattern 'ProceduralAnimation.Action.Editor' selects 57 of them (6 renderer-only)`. The summary's 58 includes `Ck.ProceduralAnimation.Debugger.PIE.LegActions`, which the toolbox scheduled into the renderer group, as in P4b.

**Delta.** Against the 4a baseline (53/51/2) plus the four 4b editor rows and the new row: 58, red = the same two inherited names, with the same failure texts as P4b (TwoTabs at stage 2 `Expanded Preview Placement has no visible Root Frame`; FocusedAssetIdentity at the routed `Marker at cursor` click, green alone in P4b). All editor rows green otherwise, including `AcceptsSatisfiedSeek`, `RejectsStalePartialAndMalformed`, `ConvertsToDeclaredSpace`, both `PoseWorkspace` rows, `SharedPreviewRefresh` and `PieCoexist.IndependentWorlds`. Logs: `Saved/Logs/ProceduralErgonomics-P4c.log`, scratchpad copies `ProceduralErgonomics-P4c.log` and `P4c-renderer-Mars.log`; the row's `AddInfo` line is in `Saved/Logs/Mars_2.log` (the headless lane that ran it).

**Not run.** No `.Action` pattern after this change (it touches only the editor module and an editor test), no wider pattern, no full gate.

### Phase 6 baseline (2026-10-12 14:59, Opus executor, before any edit)

Source: Mars `6a43f51` (Script/ clean), CkFoundation/CkTests dirty with Phases 1-5 (editor built at P4c). No UnrealEditor running.

Command (test-only, no build):
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-P6-Baseline.log --project="D:/Repositories/CkRepos/Orion"
```
Exit **0**. Population: `--test-pattern 'CrawlerMelee' selects 35 of them (0 renderer-only)` (the package expected 34; `Mars_AutoTest_CrawlerMelee_LongLimbLive.as` and `_LongLimbLoaded.as` hold several rows each).
```
=== Test summary ===
Total: 35
Passed: 35
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 1m 40s
```
Failing names: none. Green names (35):
- `Mars_AutoTest_CrawlerMelee_AcquireAndLeash`
- `Mars_AutoTest_CrawlerMelee_ActionScopeLoss`
- `Mars_AutoTest_CrawlerMelee_AggressionToggle`
- `Mars_AutoTest_CrawlerMelee_ApproachAndPrepare`
- `Mars_AutoTest_CrawlerMelee_BlockedRoute`
- `Mars_AutoTest_CrawlerMelee_CancelBeforeCommit`
- `Mars_AutoTest_CrawlerMelee_Displacement`
- `Mars_AutoTest_CrawlerMelee_DodgeCommitment`
- `Mars_AutoTest_CrawlerMelee_EngagesAndRetries`
- `Mars_AutoTest_CrawlerMelee_EveryExit`
- `Mars_AutoTest_CrawlerMelee_HitExactlyOnce`
- `Mars_AutoTest_CrawlerMelee_HitchAndZeroTime`
- `Mars_AutoTest_CrawlerMelee_InterruptPriority`
- `Mars_AutoTest_CrawlerMelee_InvalidSpec`
- `Mars_AutoTest_CrawlerMelee_LongLimbBounds`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveCancel`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveClear`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveDamage`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveScopeLoss`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveSever`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveTargetLoss`
- `Mars_AutoTest_CrawlerMelee_LongLimbLiveWall`
- `Mars_AutoTest_CrawlerMelee_LongLimbLoadedBranch`
- `Mars_AutoTest_CrawlerMelee_LongLimbLoadedFastArc`
- `Mars_AutoTest_CrawlerMelee_LongLimbLoadedProjected`
- `Mars_AutoTest_CrawlerMelee_LongLimbTargetBinding`
- `Mars_AutoTest_CrawlerMelee_MissAndCooldown`
- `Mars_AutoTest_CrawlerMelee_Obstruction`
- `Mars_AutoTest_CrawlerMelee_PassiveAndNetworkGate`
- `Mars_AutoTest_CrawlerMelee_PhaseSequence`
- `Mars_AutoTest_CrawlerMelee_PrepareTimeout`
- `Mars_AutoTest_CrawlerMelee_QueryPrimitives`
- `Mars_AutoTest_CrawlerMelee_StaleGeneration`
- `Mars_AutoTest_CrawlerMelee_StationaryFacing`
- `Mars_AutoTest_CrawlerMelee_TerminalStrikeHit`

### Phase 6 gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51` (HEAD unchanged), every change below dirty, nothing staged or committed. No framework or C++ edit; `Plugins/*` untouched by this phase. Pattern of every run: `CrawlerMelee` (plus one extra consumer pattern, `PlayerCombat`, below).

**Out-of-fence edits approved by the orchestrator (2026-10-12) before they were made.** `Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as` (the clip owner the package names: `UPROPERTY() private UCk_ProceduralAction_Data _AttackClip`, built once in `DoConstruct`, handed to the kernel weakly) and `Script/Debugger/Pages/Mars_DebugPage_Monsters.as` (its two rows called the removed `Get_AttackPhaseElapsed`/`Get_AttackPhaseDuration`/`Get_AttackPendingSampleId`; they now show the action state and the last accepted PhaseId, sample and interval).

**Red before implementation.** Not observed with a run: the kernel edits were written before the three new test files (an order deviation from PHASE_6 step 1). Red by construction against the baseline tree instead: `git grep` over `HEAD -- Script` finds 0 occurrences of every symbol the new rows need (`Make_AttackClip`, `Get_AttackAcceptedInterval`, `Get_OffenseWindowId`, `Find_ClipPhase`, `Get_AttackAction`, `FMars_TESTONLY_CrawlerMelee_AcceptWatch`, `Watch_Accepts`, `FMars_Crawler_ClipPhase`, `AcceptedPhaseId`, `WindupEndpointAccepted`, `StrikeEndpointStaged`), so none of the three rows compiles on the baseline.

**Run 1** (the package's command, cached population):
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-P6.log --project="D:/Repositories/CkRepos/Orion"
```
Exit **1**; `selects 35` (the new rows were not yet discovered); 35 / 34 / 1; red: `Mars_AutoTest_CrawlerMelee_InvalidSpec`, `step 1/4 ... an easing outside the clip presets is rejected by field (got [])`. This migration's own new assertion: `Get_IsClipEasing` compared `int32(InEasing) <= int32(InOutCubic)`, which accepted `ECk_ProceduralAction_Easing(255)` (inferred: the cast reads the byte as signed, -1). One bounded fix: `Get_IsClipEasing` became an explicit membership test of the seven declared values. Log kept as `Saved/Logs/ProceduralErgonomics-P6-Run1.log`. Every pre-existing behaviour row was green in run 1, including all those with updated assertions.

**Final run** (same command plus `--discover-fresh` so the three generated test actors are discovered):
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --discover-fresh --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-P6.log --project="D:/Repositories/CkRepos/Orion"
```
Exit **0**. `[population] --test-pattern 'CrawlerMelee' selects 38 of them (0 renderer-only)`.
```
=== Test summary ===
Total: 38
Passed: 38
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 1m 32s
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
(no listed test ran in this run, and there were no new failures)
```
Failing names: none.

**Name diff against the baseline (35 green, 0 red).** All 35 baseline names green again; added and green: `Mars_AutoTest_CrawlerMelee_ClipDrivesPhases`, `Mars_AutoTest_CrawlerMelee_CancelReturnsThroughAction`, `Mars_AutoTest_CrawlerMelee_OffenseWindowFromReceipt`. Lane logs: the only `[Crawler]` lines are InvalidSpec's two expected rejections (AcquireRadius, translation-only), the same single ensure aggregate as the baseline; no `Angelscript` compile error (the 28 `AsErrorParser` matches in the toolbox log are test names printed by fresh discovery).

**Extra consumer check (not part of the package's gate).** `PlayerCombat.ProductionReceiver` drives the crawler's jab outside the CrawlerMelee pattern: `./CkAuto/UnrealToolbox.exe --test --test-pattern PlayerCombat --output=Saved/Logs/ProceduralErgonomics-P6-PlayerCombat.log --project="D:/Repositories/CkRepos/Orion"`, exit 0, 8 / 8. No pre-change baseline exists for this pattern.

**Exit grep.** `rg --no-ignore -n PhaseElapsed Script/ECS/Crawler` returns 0 hits.

**Assertions updated for an approved change (each carries a one-line reason in the test).**
- `PhaseSequence`: "Strike/Recover entered with the windup/strike key posed" (read from the removed requested offset) -> Strike/Recover are entered on their first accepted interval, which starts at the previous phase's end on the clip timeline; Cooldown on the interval that ends the clip; Cooldown and Idle find the presentation at neutral (1 cm). Committed sample range `> ToStrike.ack && <= ToRecover.ack` -> `>= ToStrike.ack && < ToRecover.ack` (the records now carry the first sample of the new phase). Reason: phases follow the accepted PhaseId (approved row 3).
- `HitchAndZeroTime`: "Strike spanned at least two ticks" -> at least one tick, and the windup/strike-key checks -> interval-start checks. Reason: the clip stops each accepted interval at a phase boundary; there is no posing tick plus acknowledged endpoint any more.
- `EveryExit`: (5) and (6) removed (the game's acknowledgement watchdog, `PoseTimeout`, approved row 2); (6b) now faults the resolver's `ActionStreamFailed` verdict instead of the removed `PendingSampleId` (no game acknowledgement exists to fault), same expected exit (`PoseFailed`, Cooldown, terminal drained); (7) "CancelRecover ended on an acknowledged sample of its own" -> "ended on the action's terminal record" (approved row 1: the return is the action's); (2) and (8) trigger `StrikeEntered` instead of the removed `StrikeSampleSubmitted`.
- `TerminalStrikeHit`: Recover frame `candidate + 1` -> `candidate + 2`, and the committed sample is the candidate's terminal Strike interval (`SawCandidateSampleId`, interval ends at the strike's end). Trigger `StrikeEndpointSubmitted` -> `StrikeEndpointStaged` (accepted clip time + this frame's delta reaches the strike's end). Reason: row 3.
- `ActionScopeLoss`: "action Ready with a sample in flight" -> "action Playing with an accepted sample". Reason: the jab is a clip action.
- `CancelBeforeCommit` (2, 3) and `Displacement`: trigger renamed to the new equivalent moment (`WindupEndpointAccepted`: the first Strike sample is staged this frame; `StrikeEntered`); assertions unchanged.
- `InvalidSpec`: two assertions added (easing outside the presets rejected by field name; enabled profile without `AttackClip` rejected); nothing changed.

**Decisions the package did not pin down.**
- D-P6-1 Clip cache: `FMars_Crawler_Combat` gains `TWeakObjectPtr<UCk_ProceduralAction_Data> AttackClip`; `UMars_Crawler_EntityScript` builds `_AttackClip` once in `DoConstruct` (`utils_crawler::Make_AttackClip(CrawlerSpec.Combat, this)`) and writes the weak pointer into the spec before `utils_crawler::Add`. The spec is copied into `FMars_Fragment_Crawler_Params` at `Add` and never mutated, so one spec revision = one construction; there is no cross-crawler cache. The entity script object is pinned by ObjectPooling for the entity's life (`FFragment_EntityScript::_Script` comment), so the UPROPERTY roots the clip. `Validate` on an enabled combat profile rejects a missing clip (last check, so earlier rules still report first).
- D-P6-2 `Make_AttackClip` signature is `(const FMars_Crawler_Combat&, UObject InOwner)`: the package's `FMars_Crawler_AttackSpec` does not exist; Combat holds Timing and Pose, and the owner is the clip's outer. A timing or pose that `Validate` rejects builds nothing (no ensure: `Add` reports that rejection); a builder rejection of a valid spec ensures with the first diagnostic.
- D-P6-3 `CancelRecover` mapping: an offensive cancel (not Dead/PoseFailed/Disabled) calls `Request_Cancel` once and enters `CancelRecover`; the action's dedicated return (`Transition.DedicatedReturn = CancelSeconds`, `ReturnTimeout` raised to it when smaller) moves the body; `CancelRecover` ends in Cooldown (CooldownSeconds) on the action's Terminal record, or as PoseFailed when the stream fails or the action is lost. Dead/PoseFailed/Disabled also cancel the action and go straight to Cooldown(0); Idle still waits for the acknowledged terminal and release.
- D-P6-4 Recover -> Cooldown is triggered by the `Offense` window's exit event carried in an accepted interval's `Events` (at Recover 1 = clip end), i.e. a window record, not the action state.
- D-P6-5 `EntryBlend = 0` in the tuning override: the player default (0.1 s) would delay the windup by 0.1 s, an unapproved timing change; with an identity-offset start the blend moves nothing.
- D-P6-6 The player's `PublicationTimeout` was `PoseTimeoutSeconds + 1` (2 s); with the field removed it is the framework default (1 s), and the clip's own publication watchdog replaces the game's pose watchdog.
- D-P6-7 Easing fields are now typed `ECk_ProceduralAction_Easing` (defaults unchanged: OutCubic, InQuad, InOutCubic); `Validate` rejects any value outside the seven declared ones, naming the field.
- D-P6-8 Library-initiated interruption: an offensive phase with the action `Cancelling` that the game did not request, or `Finished` with an outcome other than `Completed`, cancels as `PoseFailed` (the manual path's "action no longer Ready" rule).
- D-P6-9 The resolver applies the presentation-equals-receipt ensure to the page's latest accepted receipt only (an earlier receipt in the same page cannot match the live presentation); every accepted receipt still updates phase, window and sweep.
- D-P6-10 The game's `WindowClosed` latch (set by a cancel) still blocks sweeps and candidates from intervals staged before the cancel took effect, whatever windows they carry; generation alone would not have, because a post-cancel resolve stamps the new generation.
- D-P6-11 `EMars_Crawler_AttackCancel::PoseTimeout` removed with its watchdog (no other reader: `rg` over `Script` and `Plugins/*/Script`).

**`git status --short Script` (Mars):**
```
 M Script/Debugger/Pages/Mars_DebugPage_Monsters.as
 M Script/ECS/Crawler/Mars_Crawler_ActionAdapter.as
 M Script/ECS/Crawler/Mars_Crawler_Attack.as
 M Script/ECS/Crawler/Mars_Crawler_Feature.as
 M Script/ECS/Crawler/Mars_Crawler_Processor_AttackResolve.as
 M Script/ECS/Crawler/Mars_Crawler_Processor_AttackTick.as
 M Script/ECS/Crawler/Mars_Crawler_Utils.as
 M Script/Generated/Mars_AutoTestActors.as
 M Script/Generated/Mars_EntitySpawnParams.as
 M Script/Generated/Mars_ScriptProcessorDrivers.as
 M Script/Tests/CrawlerMelee/Mars_AutoTestRig_CrawlerMelee.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_ActionScopeLoss.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_CancelBeforeCommit.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_Displacement.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_EveryExit.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_HitchAndZeroTime.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_InvalidSpec.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_PhaseSequence.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_TerminalStrikeHit.as
 M Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as
?? Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_CancelReturnsThroughAction.as
?? Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_ClipDrivesPhases.as
?? Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_OffenseWindowFromReceipt.as
```
`Script/Generated/*` changed by the generator at editor startup (three test actors, spawn params, script processor drivers for the new test processor); not hand-edited. Gate side effects outside `Script/` (`Config/DefaultGameplayTags.ini`, `Content/__ExternalActors__/...`) are never to be staged.

**Remaining uncertainty.** `TerminalStrikeHit`'s trigger predicts this frame's staged interval from the accepted clip time plus the frame delta (play rate 1, no backlog inside Strike); a lane whose player delta differed from the script processor's would fire one frame early (the row would then fail on "candidate came from the strike's terminal interval", not pass silently). Not observed in the final run.

**[EDITOR-VERIFY]** Sandbox Room 5 crawler (`UMars_Sandbox_Crawler_EntityScript`), PIE: make it aggressive (the lever), let it jab you; the windup, strike and recovery should read as before. Break line of sight or hit it mid-windup: the body should ease back over about 0.15 s with no snap. Open the Monsters debugger page on the crawler: "Attack phase" shows the action state; "Attack accepted" shows the clip phase, sample and interval times advancing.

### Phase 7 frozen source (2026-10-12 15:33, Opus executor)

Editor closed (`Saved/Logs/Mars.log` write-probe `free`, no UnrealEditor/MarsEditor process). HEADs unchanged: Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b`, all `feature/procedural-anim-ext`. Nothing staged. From here on the only permitted source addition is the measurement file `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_BuilderCost.cpp` (PHASE_7 step 3); every other byte below is frozen. Snapshots (full `git diff` per repo plus SHA-1 of every untracked file) are in the session scratchpad as `freeze-diff{_,Plugins_CkFoundation,Plugins_CkTests}.patch` / `freeze-untracked*.sha1`; patch SHA-1s: Mars `9d235e20`, CkFoundation `0a5b886a`, CkTests `c081bf37`.

`git diff --stat` totals: Mars `24 files changed, 734 insertions(+), 537 deletions(-)` (incl. `Content/Mars/Maps/AutoTests_Mars_MAP.umap` binary and the `Plugins/{CkFoundation,CkTests,Monolith}` gitlinks); CkFoundation `44 files changed, 4818 insertions(+), 496 deletions(-)`; CkTests `6 files changed, 624 insertions(+), 7 deletions(-)`.

`git status --short`, Mars (`Plugins/Monolith` dirt is a sibling session's, untouched):
```
 M Content/Mars/Maps/AutoTests_Mars_MAP.umap
 m Plugins/CkFoundation
 m Plugins/CkTests
 m Plugins/Monolith
 M Script/Debugger/Pages/Mars_DebugPage_Monsters.as
 M Script/ECS/Crawler/Mars_Crawler_{ActionAdapter,Attack,Feature,Processor_AttackResolve,Processor_AttackTick,Utils}.as
 M Script/Generated/Mars_{AutoTestActors,EntitySpawnParams,ScriptProcessorDrivers}.as
 M Script/Tests/CrawlerMelee/Mars_AutoTestRig_CrawlerMelee.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_{ActionScopeLoss,CancelBeforeCommit,Displacement,EveryExit,HitchAndZeroTime,InvalidSpec,PhaseSequence,TerminalStrikeHit}.as
 M Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as
?? Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_{CancelReturnsThroughAction,ClipDrivesPhases,OffenseWindowFromReceipt}.as
```
CkFoundation (paths under `Source/CkProceduralAnimation/Public/CkProceduralAnimation/` abbreviated `PA/`, `Source/CkProceduralAnimationEditor/` abbreviated `PAE/`):
```
 M Script/CkProceduralAnimation/Examples/CkProceduralAction_Noncombat.as
 M Source/CkProceduralAnimation/Claude.md
 M Source/CkProceduralAnimation/Private/CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h
 M PA/Action/CkProceduralAction_{Data.h,Fragment.cpp,Fragment.h,Fragment_Data.h,Receipt.h,Utils.cpp,Utils.h}
 M PA/ActionPlayer/CkProceduralActionPlayer_{ClipBinding.h,Fragment.h,Fragment_Data.h,Observe.cpp,Processor.cpp,Processor.h,Service.cpp,Utils.cpp,Utils.h}
 M PA/Articulation/CkProceduralArticulation_Utils.{cpp,h}
 M PA/Core/CkProceduralActionEvaluation.cpp
 M PA/Core/CkProceduralActionReceiptStore.{cpp,h}
 M PA/Core/CkProceduralActionValidation.{cpp,h}
 M PA/LimbControl/CkProceduralLimbControl_{Control.cpp,Fragment.h,Fragment_Data.h,Processor.cpp}
 M PAE/Private/Asset/CkProceduralAction_Factory.cpp
 M PAE/Private/Authoring/CkProceduralActionAuthoringCommands.cpp
 M PAE/Private/Session/CkProceduralAction{AuthoringSession,EditorModes}.cpp
 M PAE/Private/Toolkit/CkProceduralAction_EditorToolkit.{cpp,h}
 M PAE/Private/Widgets/SCkProceduralAction_Timeline.{cpp,h}
 M PAE/Public/CkProceduralAnimationEditor/CkProceduralAction{AuthoringCommands,AuthoringSession}.h, CkProceduralAction_Factory.h
 M docs/campaigns/procedural-actions/{API,AUTHORING,GETTING_STARTED}.md
?? Script/CkProceduralAnimation/Examples/CkProceduralAction_{Builder,Sweep}.as
?? PA/ActionBuilder/   (CkProceduralActionBuilder_Data.h, CkProceduralActionBuilder_Utils.{h,cpp})
?? PA/ActionPlayer/CkProceduralActionPlayer_{Managed,Mirror}.cpp
?? PA/Core/CkProceduralActionProgress.{h,cpp}
?? PAE/Private/Asset/CkProceduralActionGuidedCreation.cpp
?? PAE/Private/Authoring/CkProceduralAction{PoseGroups,SolvedCapture}.cpp
?? PAE/Private/Widgets/SCkProceduralAction_ProgressPlot.{h,cpp}
?? PAE/Public/CkProceduralAnimationEditor/CkProceduralAction{GuidedCreation,PoseGroups,SolvedCapture}.h
?? docs/campaigns/procedural-actions-ergonomics/
```
(`Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` is edited but git-ignored, so absent from both lists.)
CkTests (`UT/` = `Source/CkTests/Private/UnitTests/CkProceduralAnimation/`, `ED/` = `Source/CkTestsEditor/Private/CkProceduralAnimation/`):
```
 M Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ArticulationPublicSurface.as
 M Script/Generated/CkTests_AutoTestActors.as
 M ED/Test_ProceduralAction_{BlueprintParity,MountedToolkit,PieCoexist,SaveReopen}.cpp
?? Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level/{0/Y3,1/V4,4/4H,4/81,4/V7,9/7N}/   (populator output; never stage)
?? Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_{ActionAssetDefinition,ActionBuilderPublicSurface,ActionManagedPublicSurface,BorrowingSweep,BuilderSweep,NoncombatManaged}.as
?? Script/CkProceduralAnimation/CkProceduralAnimation_ActionFixture_Assets.as
?? UT/Action/CkTest_ProceduralActionManagedListener.h
?? UT/Action/Test_ProceduralAction_{Builder,Diagnostics,Managed,Mirror,MirrorRuntime,Progress}.cpp
?? UT/ActionPlayer/   (Test_ProceduralActionPlayer_Presets.cpp)
?? UT/Articulation/Test_ProceduralArticulation_Query.cpp
?? UT/CkTest_ProceduralWalkerFixture.h
?? ED/Test_ProceduralAction_{GuidedCreation,PoseCapture,PoseWorkspace,SolvedCapture}.cpp
```

### Phase 7 pre-fix gate A (2026-10-12 15:35-15:44, Opus executor; superseded once the review's managed-delivery fixes land)

Ran on the frozen source above. The review then reported four managed-delivery defects to be fixed, so this is **pre-fix** evidence: it does not certify the final source and must be re-run after the fixes. Freeze re-checked after the run: `git diff` SHA-1 per repo unchanged (Mars `9d235e20`, CkFoundation `0a5b886a`, CkTests `c081bf37`), and no untracked file changed except this PROGRESS.md.

Command (editor closed, `Mars.log` probe `free`):
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-Final-PA.log --project="D:/Repositories/CkRepos/Orion"
```
Build `=== Build succeeded ===` (Development, auto-resolved; `Build lock: SHARED`). `[population] --test-pattern 'ProceduralAnimation' selects 459 of them (8 renderer-only)`. Actual exit **1**:
```
=== Test summary ===
Total: 459
Passed: 454
Failed: 5
Skipped: 0
Contaminated: 0
Duration: 8m 35s
Failed tests (5):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns
Renderer-only: 8 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Still red (listed) (3):
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns  [Plugins/CkTests]
Now passing (listed red in a plugin's file) — not pruned from here; if it holds in the plugin's home project, open a PR there (1):
  Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep  [Plugins/CkTests]
Listed flaky, passed — if it keeps passing, remove the entry (2):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarsEngageSteppedTops  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics  [Plugins/CkTests]
```
**Delta.** NEW failures are exactly the two inherited editor reds (B-P1-1), with the same texts as every gate since Phase 1: TwoTabs `timed out at stage 2: Expanded Preview Placement has no visible Root Frame` (`Test_ProceduralAction_MountedToolkit.cpp:728`); FocusedAssetIdentity `Expected 'mounted marker creation changes the authored clip (timeline says: Select a phase and slot below. Edit key values in Clip Details.)' to be true` (`Test_ProceduralAction_SaveReopen.cpp:478`). The other three reds are listed. No other new failure, so no solo re-runs were needed. Population 459 vs P5's 444 = the 4b/4c editor and builder rows. Both load-sensitive candidates passed this time (`BodyPose.PresentationLagIsBoundedThroughSustainedRotation`, `Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal`). 0 `Angelscript: Error` lines. All three environments are present by name and green: native (`Action.{Builder 10, Progress 4, Mirror 7, Managed 10, Diagnostics 4}`, `ActionPlayer.Presets 4`, `Articulation.Query 4`, editor `PoseCapture 6`, `SolvedCapture 3`, `PoseWorkspace 2`), AS (`Ck_AutoTest_ProceduralAnimation_{ActionBuilderPublicSurface, ActionAssetDefinition, ActionManagedPublicSurface, NoncombatManaged, BuilderSweep, BorrowingSweep, ArticulationPublicSurface}`), Blueprint VM (`Action.Editor.BlueprintParity.{ManagedPlayLoadedVm, MirrorVm}`). The pre-existing `editor exited abnormally (exit=0xFF) but AFTER completing every test — results kept` line appears for 4 editors (net 1, lane 2/6, lane 1/6, renderer), as in earlier phases. Log archived in the session scratchpad as `ProceduralErgonomics-Final-PA.log`.

**Stopped here on the orchestrator's instruction**: gate B, the measurement harness, the Game build, the cooked check and the dossier were not started. A draft of the measurement file is in the session scratchpad only (`Test_ProceduralAction_BuilderCost.cpp`, rows `Ck.ProceduralAnimation.Action.Performance.Builder.{RecipePoseKeysBuildInto, CompileAndEvaluatePresetVsCustomCurve, ExpandMirrorsOneAndEight, ManagedDispatchSteadyState}`); it is not in the tree and has never been compiled.

### Phase 7 frozen source (post-fix) (2026-10-12 16:06, Opus executor)

Taken after the Phase 3 review fixes (record below). Editor closed (`Mars.log` probe `free`, no UnrealEditor/MarsEditor process). HEADs unchanged (Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b`); nothing staged. The only addition permitted from here on is `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_BuilderCost.cpp` (step 4). Scratchpad snapshots `postfix-diff{_,Plugins_CkFoundation,Plugins_CkTests}.patch`, `postfix-untracked*.sha1`, `postfix-status*.txt`; patch SHA-1s: Mars `9d235e20` (unchanged), CkFoundation `3d199b55`, CkTests `d98ed5cd`.

`git diff --stat` totals: Mars `24 files changed, 734 insertions(+), 537 deletions(-)` (unchanged); CkFoundation `44 files changed, 4844 insertions(+), 496 deletions(-)`; CkTests `7 files changed, 719 insertions(+), 7 deletions(-)`.

Delta against the first freeze (`git status --short` plus untracked hashes): CkTests gained one tracked modification, ` M Source/CkTests/Private/UnitTests/CkProceduralAnimation/Debugger/Test_ProceduralActionDebugger_Pie.cpp`. Contents changed, with the same file lists: CkFoundation tracked `Action/CkProceduralAction_{Fragment_Data.h,Receipt.h,Utils.h,Utils.cpp}`, `ActionPlayer/CkProceduralActionPlayer_{Fragment.h,Utils.h,Utils.cpp}`, `Core/CkProceduralActionReceiptStore.{h,cpp}`, `docs/campaigns/procedural-actions/{API,GETTING_STARTED}.md`; CkFoundation untracked `ActionPlayer/CkProceduralActionPlayer_Managed.cpp` and this campaign's `AUDIT.md`, `DESIGN.md`, `PROGRESS.md`, `VALIDATION.md`; CkTests untracked `UT/Action/{Test_ProceduralAction_Managed.cpp, CkTest_ProceduralActionManagedListener.h}`, `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ActionManagedPublicSurface.as`. This matches the manifest in the Phase 3 review fixes record. Mars is byte-identical to the first freeze.

### Phase 7 final gate A (2026-10-12 16:07-16:16, Opus executor) — STOPPED: one new failure, red alone

Same command as the pre-fix run, on the post-fix freeze:
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-Final-PA.log --project="D:/Repositories/CkRepos/Orion"
```
`=== Build succeeded ===` (SHARED lock). `[population] --test-pattern 'ProceduralAnimation' selects 460 of them (8 renderer-only)` (pre-fix 459, plus `Debugger.ActionPie.ManagedStreamKeepsRecording`). Actual exit **1**:
```
=== Test summary ===
Total: 460
Passed: 454
Failed: 6
Skipped: 0
Contaminated: 0
Duration: 8m 29s
Failed tests (6):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_NoncombatManaged
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns
Renderer-only: 8 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (3):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_NoncombatManaged
Still red (listed) (3):
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns  [Plugins/CkTests]
Now passing (listed red in a plugin's file) — not pruned from here; if it holds in the plugin's home project, open a PR there (1):
  Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep  [Plugins/CkTests]
Listed flaky, passed — if it keeps passing, remove the entry (2):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarsEngageSteppedTops  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics  [Plugins/CkTests]
```
0 `Angelscript: Error` lines. The two inherited editor reds fail with their usual texts. Both load-sensitive candidates passed. All `Action.Managed.*` (10), `BlueprintParity` (4), `ActionManagedPublicSurface` and `ManagedStreamKeepsRecording` passed. Scratchpad copy: `ProceduralErgonomics-Final-PA-postfix.log` (pre-fix copy: `ProceduralErgonomics-Final-PA.log`).

**New failure: `Ck_AutoTest_ProceduralAnimation_NoncombatManaged`** (green in the pre-fix gate A on the same rows).
- Gate text: `FinishTest TestResult=Failed. Failed: step 4/4 'inspect the completion': Nothing is left for the caller to drain or release (1/8 assertions failed)`.
- Alone, `--test --no-live --known-reds off --test-pattern "Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_NoncombatManaged"`, `Saved/Logs/ProceduralErgonomics-Final-NoncombatAlone.log`: exit **1**, Total 1 / Passed 0 / Failed 1, the same assertion. **Red alone → STOP (not fixed).**
- Prime suspect, read from code, not confirmed by a run:
  - The assertion is `Plugins/CkTests/Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_NoncombatManaged.as:95-96`: `Get_IsManagedDelivery(_Player, Identity) == false` in the step right after the completion callback fired. That file is the Phase 5 example test, untracked.
  - Fix 2 of the review round moved `Get_IsManagedDelivery` from the dispatcher's map entry ("true from Play's return until the entry is removed", Phase 3 decision) to the stream bit (`FReceiptStore::Get_IsManagedDelivery`, `Core/CkProceduralActionReceiptStore.cpp:412-418`). The fixes record says that bit is false only "once the stream is reclaimed", and reclaim follows the release one or more frames after completion (`[ManagedDrain] store returned to baseline after 2 polls past completion`).
  - The fix round removed the same post-completion assertion from `ActionManagedPublicSurface.as` but not from this sibling. That would make this a contract change the test was not updated for, not a dispatcher defect.
- Discriminating options:
  - (a) Change the row's last step to wait until the identity is no longer retained (`Get_RetainedReceiptIdentities`) before asserting the seal is gone, or drop the assertion as `ActionManagedPublicSurface` did.
  - (b) Restore the old contract: `Get_IsManagedDelivery` returns false once the completion has fired, for example by also consulting the dispatcher entry's `_Completed`. That contradicts the fix round's "consume stays sealed until reclaim" and needs the orchestrator's call.

**Not run (STOP):** gate B (CrawlerMelee, PlayerCombat), step 4 (measurement file still only in the scratchpad), the Game build, the cooked-route record and FINAL_REVIEW.md.

### Phase 7 test correction (2026-10-12, orchestrator decision: option (a), applied by the orchestrator)

`Plugins/CkTests/Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_NoncombatManaged.as` (untracked, Phase 5) was correct under the old contract but not the new one. The post-completion `Get_IsManagedDelivery == false` assertion is now:
- a named wait step, `Add_Step_WaitUntil("the framework releases the action and reclaims its stream", n"Check_Reclaimed", 600)`, which waits until the identity leaves `Get_RetainedReceiptIdentities(_Player)`;
- followed by `Step_Verify`, which asserts that the action handle is invalid and `Get_IsManagedDelivery` is false.

This is the review-round contract: the managed seal lifts when the stream is reclaimed, not when the completion fires. Test only; no framework change.

### Phase 7 frozen source (final) (2026-10-12 16:20, Opus executor)

Editor closed (`Mars.log` probe `free`, no editor process). HEADs unchanged. Patch SHA-1s are identical to the post-fix freeze: Mars `9d235e20`, CkFoundation `3d199b55`, CkTests `d98ed5cd`. Diff stats are also unchanged (24/+734/-537, 44/+4844/-496, 7/+719/-7). The only content change since the post-fix freeze is `CkAutoTest_ProceduralAnimation_NoncombatManaged.as`, now SHA-1 `4737706b` (plus this PROGRESS.md). Scratchpad snapshots: `final-{status,diff,untracked}*`. Step 4 may still add `Test_ProceduralAction_BuilderCost.cpp`.

### Phase 7 final gate A, of record (2026-10-12 16:20-16:29, Opus executor)

```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-Final-PA.log --project="D:/Repositories/CkRepos/Orion"
```
`=== Build succeeded ===`. `selects 460 of them (8 renderer-only)`. Actual exit **1**:
```
=== Test summary ===
Total: 460
Passed: 455
Failed: 5
Skipped: 0
Contaminated: 0
Duration: 8m 5s
Failed tests (5):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns
Renderer-only: 8 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Still red (listed) (3):
  Ck.ProceduralAnimation.SurfaceMotion.SnapshotShowsThePendingContact
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PostsCentipedeReturns  [Plugins/CkTests]
Now passing (listed red in a plugin's file) — not pruned from here; if it holds in the plugin's home project, open a PR there (1):
  Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep  [Plugins/CkTests]
Listed flaky, passed — if it keeps passing, remove the entry (2):
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_PillarsEngageSteppedTops  [Plugins/CkTests]
  Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_ProceduralAnimation_SpiderInteractiveRosterDiagnostics  [Plugins/CkTests]
```
The NEW failures are exactly the two inherited editor reds, with their usual texts. `NoncombatManaged` passed, and so did both load-sensitive candidates. 0 `Angelscript: Error` lines. Every campaign row family passed: Builder 10, Progress 4, Mirror 7, Managed 10, Diagnostics 4, Presets 4, Query 4, PoseCapture 6, SolvedCapture 3, PoseWorkspace 2, BlueprintParity 4, AS `ActionBuilderPublicSurface`, `ActionAssetDefinition`, `ActionManagedPublicSurface`, `NoncombatManaged`, `BuilderSweep`, `BorrowingSweep`, `ArticulationPublicSurface`, and Debugger `ManagedStreamKeepsRecording`. Scratchpad copy: `ProceduralErgonomics-Final-PA-final.log`.

### Phase 7 gate B (2026-10-12 16:30-16:33, Opus executor; same binary, test only)

`./CkAuto/UnrealToolbox.exe --test --discover-fresh --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-Final-Crawler.log --project="D:/Repositories/CkRepos/Orion"`: `selects 38 of them (0 renderer-only)`, actual exit **0**:
```
=== Test summary ===
Total: 38
Passed: 38
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 1m 30s
=== Known reds: 81 listed (...) ===
(no listed test ran in this run, and there were no new failures)
```
`./CkAuto/UnrealToolbox.exe --test --test-pattern PlayerCombat --output=Saved/Logs/ProceduralErgonomics-Final-PlayerCombat.log --project="D:/Repositories/CkRepos/Orion"`: `selects 8 of them`, actual exit **0**:
```
=== Test summary ===
Total: 8
Passed: 8
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 38s
(no listed test ran in this run, and there were no new failures)
```
CrawlerMelee: Phase 6 baseline 35/35, final 38/38 (the 35 baseline names plus the three new rows). PlayerCombat: 8/8, the same as P6, with no pre-change baseline.

### Phase 7 measurements (2026-10-12 16:34-16:37, Opus executor)

**Added (the only source addition after the final freeze):** `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_BuilderCost.cpp` (untracked). It has four rows, `Ck.ProceduralAnimation.Action.Performance.Builder.{RecipePoseKeysBuildInto, CompileAndEvaluatePresetVsCustomCurve, ExpandMirrorsOneAndEight, ManagedDispatchSteadyState}`, in the style of `Test_ProceduralAction_Performance.cpp`: native `FPlatformTime::Seconds` timers, fixed counts, numbers reported through `AddInfo` and `ck::tests::Log` under `[CkAction BUILDERCOST]`. They assert only scenario validity (every operation Valid, all four clips Playing through the measured frames, records delivered, complete frame counts). There are no timing thresholds.

Command (`--discover-fresh` added so the new rows are discovered):
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation.Action.Performance --output=Saved/Logs/ProceduralErgonomics-Final-Perf.log --project="D:/Repositories/CkRepos/Orion"
```
`=== Build succeeded ===`; the build compiled `Test_ProceduralAction_BuilderCost.cpp` (an adaptive non-unity build, so it was outside the unity file). `selects 4 of them (0 renderer-only)`. Actual exit **0**:
```
=== Test summary ===
Total: 4
Passed: 4
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 43s
(no listed test ran in this run, and there were no new failures)
```
**Conditions.**
- Development editor build, headless (`-nullrhi`) PIE on `/Engine/Maps/Entry`, 8-core machine.
- The toolbox auto-sized to 3 concurrent editors, so the rows ran at the same time on three lanes: the two pure rows in `Mars.log`, ExpandMirrors in `Mars_2.log`, ManagedDispatch in `Mars_3.log`. Every number was therefore measured with two other editors loading the machine.
- One run of the binary. Repetitions inside the pure rows: 3.
- Lane logs are archived in the scratchpad as `Perf-lane-Mars{,_2,_3}.log`.

| Row / stage | Rep 1 | Rep 2 | Rep 3 |
|---|---|---|---|
| `Make_FromRecipe` (Reach, 3 phases), avg (min-max) over 20 | 7.3 µs (5.3-15.9) | 6.6 µs (5.4-15.6) | 6.0 µs (5.5-9.7) |
| 64 × `Add_PoseKey` (body Keep, one limb), avg total | 62.4 µs (0.98 µs/key) | 60.4 µs (0.94) | 54.8 µs (0.86) |
| `Build_Into` (70 position keys) | 45.3 µs (34.4-75.4) | 39.4 µs (34.7-55.7) | 37.2 µs (35.3-58.0) |
| `Compile` Preset, per call (200 calls) | 34.9 µs | 34.6 µs | 34.3 µs |
| `Compile` CustomCurve (5 keys per phase, same position keys), per call | 85.6 µs | 84.8 µs | 83.7 µs |
| `Evaluate` Preset, per sample (20000) | 96.6 ns | 99.8 ns | 97.1 ns |
| `Evaluate` CustomCurve, per sample | 97.0 ns | 99.1 ns | 97.1 ns |
| `EvaluateProgress` only, Preset / CustomCurve, per sample | 4.8 / 9.1 ns | 5.8 / 9.9 ns | 4.8 / 9.5 ns |
| `ExpandMirrors` 1 mirror (70 → 140 keys), per call (200) | 5.05 µs (max 15.4) | 5.00 µs (max 10.9) | 5.10 µs (max 21.5) |
| `ExpandMirrors` 8 mirrors (70 → 630 keys), per call | 34.7 µs (max 93.5) | 33.7 µs (max 40.1) | 34.4 µs (max 75.3) |

**ManagedDispatchSteadyState.** One run: four 60 s managed `Play_Loaded` clips on one 4-limb player, one records listener each, 300 frames, against 300 idle frames on the same player with no actions.
- Records: 1200 delivered in 1200 callbacks, 4.0 per frame (one record per clip per frame).
- Frame interval, managed segment: avg 16.67 ms, min 16.27, max 17.07. That is a frame-paced regime of about 60 Hz, so the dispatcher's own cost is below what this harness resolves.
- Frame interval, idle segment: avg 20.38 ms, min 2.94, max 986.5 (one hitch). The two segments ran under different frame pacing, so their difference is **not** a dispatcher cost and none is claimed.
- Store before play: 0 streams, 0 record slots, 0 pose transforms, 16 bytes allocated.
- Store maximum during the measured frames: 8 record slots, 80 pose transforms, 24576 bytes allocated.
- Store after cancel and release: 0 streams, 0 slots, 0 transforms, 3280 bytes allocated (capacity kept, not returned to 16), reached 2 polls after completion.
- Allocation count: unavailable; the harness has no allocator counter. Dispatcher self-time: unavailable without an Insights capture of the processor stat.

**Unchanged by this file:** no framework byte changed. After the run, the patch SHA-1s per repo are still the final freeze's (Mars `9d235e20`, CkFoundation `3d199b55`, CkTests `d98ed5cd`). The only new untracked file is `Test_ProceduralAction_BuilderCost.cpp`.

### Phase 7 Game build (2026-10-12 16:38-16:57, Opus executor) — FAILED at link (environment, inferred)

`Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Game --output=Saved/Logs/ProceduralErgonomics-Final-Game.log --project="D:/Repositories/CkRepos/Orion"` (no `--config`). Actual exit **1**. Lines:
```
=== Auto config resolved to Development (last built) ===
=== Build lock: EXCLUSIVE (Shared build environment) ===
=== Building ShaderCompileWorker ===            (Result: Succeeded)
Running UnrealBuildTool: ... Mars Win64 Development -project=".../Mars.uproject" -waitmutex -architecture=x64
[358/359] Link [x64] Mars.exe
LINK : fatal error LNK1318: Unexpected PDB error; LIMIT (12) 'D:\Repositories\CkRepos\Orion\Binaries\Win64\Mars.pdb'
Result: Failed (OtherCompilationError)
Total execution time: 1151.16 seconds
=== Build FAILED ===
```
- **Compile succeeded, link failed.** 0 `error C*` / `fatal error C*` lines: all 357 compile actions of the monolithic Game target succeeded, including every CkProceduralAnimation and CkTests runtime module with `WITH_EDITOR=0`. The single link action failed on the PDB, with only `LNK4217/LNK4286` warnings (Jolt symbol import) besides.
- **Prime suspect (inferred, not confirmed):** the monolithic `Mars.pdb` reached the MSVC PDB size limit (error class `LIMIT (12)`). The last good Game link on disk is `Binaries/Win64/Mars.exe` 2026-10-08 21:30 (`Mars.target` the same time), from before this campaign. No `Mars.pdb` is present now. This campaign's added runtime code and tests are a plausible push over the limit, but unproven.
- **Discriminating options (not run; both touch build configuration, which is outside this phase):**
  - (a) Relink with a larger PDB page size: UBT's Windows `PdbPageSize`, or `/PDBPAGESIZE:8192`.
  - (b) A/B: run the same Game link on the tree without the campaign diff.
- **Not AS-off.** `Mars.uproject` enables the `Angelscript` plugin, whose descriptor has no target restriction. `CkBuildConfig.Build.cs:53-61` therefore defines `WITH_ANGELSCRIPT_CK=1` for this Game target too (inferred from the rules; the define was not grepped from the compile). Even a successful Mars Game link would prove a native non-editor build, not an AS-off one. The only AS-off route in the tree is the smoke host below.

### Phase 7 cooked route — Blocked (orchestrator decision; not run)

`Plugins/CkTests/Tools/ProceduralActionsSmoke` exists, with `README.md`, `Prepare.ps1`, a template `.uproject` that disables `Angelscript`, and a runtime module that rejects `WITH_ANGELSCRIPT_CK`. Its documented sequence:
1. `Prepare.ps1` into a new `Saved/ProceduralActionsMatrix/ASOff-<guid>`.
2. Toolbox `--build --target=Editor` and `--build --target=Game` of that new project.
3. `RunUAT.bat ... BuildCookRun -skipbuild -skipbuildeditor -cook -stage -package`.
4. Run the staged exe until `PROCEDURAL_ACTIONS_NATIVE_SMOKE result=PASS ... WITH_EDITOR=0` and exit 0.

The README itself states that "This new template and preparation flow have not yet been built, cooked, or run". The route also costs two full builds of a second project plus a UAT cook. Recorded **Blocked** on that basis. AS-off compile, cook and packaged runtime are therefore unverified.

### Editor verification owed to the human (`[EDITOR-VERIFY]`)

Written against the widgets that now exist. Record for each: the number of clicks/drags/typed values you used, any stall, and screenshots of the result.

1. **Body-only lean (VALIDATION exercise 1).** Content Browser > right-click an empty folder > Animation > **Procedural Action Clip**. In **New Procedural Action**: Subject = `New Body Only`; Motion > Recipe = `Body Lean`, Amplitude Cm = 12, keep Prepare/Execute/Recover and Easing > **Create**; type the name `Lean_DA` and press Enter. Check that `Lean_DA` and `Lean_Preview_DA` appear (unsaved) only now; repeat once pressing **Cancel** in the dialog and check that nothing appears. The editor opens with pose `Prepare 0` selected. In the pose timeline click **Prepare 1** > inspector Body offset X: click **-1** until X reads the pullback you want (or type it) > **Update pose**. Click **Execute 1** > raise X > **Update pose**. To capture a new pose: click **Execute 0**, press **Frame** six times (the inspector's Pose time moves), set the body offset, type `Forward` in **New pose label** > **Capture pose** (a chip `Forward` appears). Click **Phase Execute (...)** > Phase timing: **Custom curve** > set key 2 progress to 0.8 > Enter; the plot bends and Issues still reads `No issues: the clip compiles.` Click **Phase Recover (...)** > Duration (s) = 0.6 > Enter. Press **Play solved preview** and watch one full pass (the status line ends `action terminal; preview complete; ... 0 gaps`). Press **Save clip + preview** and read the two `Saved ...` lines.
2. **Dedicated limb sweep (exercise 2).** Create a clip as above with Subject = `Use Existing`, Existing Preview = `Ck_ProceduralAction_LimbNod_Preview_DA`, Recipe = `Sweep`, Slots = the slot LimbNod's preview binds (Advanced > preview Details > Slot Bindings), Space = `BodyLocal` (the editor mirror reflects body-local and root-relative tracks; rest-normalized tracks mirror only at play time). Under Body and limbs tick that slot and click its row (it reads `SELECTED`). Click the apex chip **Execute 0.5**, **Move endpoint key**, scrub into Execute and drag the viewport widget outward; **Move bend guide** and drag. Press **Frame** to move to about Execute 0.25, set the endpoint values in the inspector > **Capture pose** (intermediate arc pose). Press **Play solved preview**, then either press **Pause** mid-Execute or scrub the time slider / press **Frame** to a mid-Execute step > **Capture solved pose** (the chip ends `(solved)`). Add a partner slot (Advanced > per-track tools > New limb slot ID > **Add slot**, then **Bind selected slot**), select the source slot under Body and limbs, **Choose the partner limb** > the new slot, plane `XZ plane (Y normal)` > **Mirror to selected limb**. Press Ctrl+Z repeatedly: each press undoes one command in reverse order (mirror, slot binding, added slot, solved pose, captured pose); Ctrl+Y restores them with the same labels. **Save clip + preview**, close the tab, reopen the clip: the chips and the `(solved)` provenance are still listed.
3. **Two clips beside PIE (exercise 4).** Open `Ck_ProceduralAction_BodyPulse_DA` and `Ck_ProceduralAction_LimbNod_DA` in two editor tabs. Start PIE (any map). In BodyPulse press **Play solved preview**; in LimbNod click a phase in the pose timeline and change **Duration (s)** in Phase timing. Confirm BodyPulse keeps playing with its status unchanged while LimbNod's status says the clip changed. In LimbNod press **Play solved preview** and, while the status reads `preparing` or during a slider scrub, close the LimbNod tab. Stop PIE. Check: no ensure/crash; no `ProceduralAction` warnings about leaked worlds in the Output Log; `obj gc` then `obj list class=World` shows no lingering `CkProceduralActionSolverWorld`; reopening LimbNod opens cleanly.

## Owed to the maintainer

- `Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity` qualifies for a `flaky` entry in `Plugins/CkTests/AutomationGate.json`: red in its renderer group in every gate since Phase 1 (P1 runs 3+, P1 A/B baseline, P2, P3, P4a, P4b, P4c), green alone (`ProceduralErgonomics-P1-FocusedAlone.log`, `ProceduralErgonomics-P4b-FocusedAlone.log`). A plugin-list change goes by PR in CkTests; not added.
- Cosmetic, editor: after a successful capture the toolkit selects the new pose, and the selection message (`Selected <label>: its key values are staged.`) replaces the command's success message. One-line follow-up in the `FinishPoseCommand` callers of `Private/Toolkit/CkProceduralAction_EditorToolkit.cpp`.

- `Plugins/CkFoundation/Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` is untracked because `Plugins/CkFoundation/.gitignore:49` ignores `*.md`. Phase 5's edits to it are on disk only. Whether to force-add it is a publishing decision; `.gitignore` is unchanged.

### Phase 3 review fixes gate record (2026-10-12, Opus executor)

**Source.** Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b` (HEADs unchanged). The tree now also carries Phases 4-6 dirty work from other sessions (`git -C Plugins/CkFoundation diff --stat`: 44 files, +4844/-496; CkTests: 7 files, +719/-7 across all phases); this round touched only the files below and none of Phases 4-6's except the two managed-delivery doc passages named.

**Fixes (the four review defects).**
1. Reads unsealed: `UCk_Utils_ProceduralActionPlayer_UE::Read_Receipts` no longer checks managed delivery; a read is non-consuming for every identity. `Request_ConsumeReceipts` stays sealed (ensure + `Failed_NotEnqueued`). `ECk_ProceduralAction_ReadResult::ManagedDelivery` is now returned by nothing; the value is kept and its comment says it names a refused consume and that reads never return it.
2. The seal is a stream bit: `FReceiptStore::FStream::ManagedDelivery`, set by the new `FReceiptStore::SealManaged(Identity)` (only before any record) which `DoCreate` calls right after reservation and before `Request_CreateEntity` (an unexpected refusal ensures and aborts the reservation); `FReceiptStore::Get_IsManagedDelivery(Identity) const` is false once the stream is reclaimed. `UCk_Utils_ProceduralActionPlayer_UE::Get_IsManagedDelivery` and the consume seal read the stream bit. `FProceduralActionManagedDelivery` lost `_Identity` (the map key is the identity) and keeps recipients, the action handle and `_Completed`.
3. `FProcessor_ProceduralActionPlayer_ManagedEndPlay` requests `Request_DestroyEntity` on every entry's action that is valid, has the action fragment and is not pending destroy (completed entries included: their release request can no longer succeed once the player is dying), then fires `PlayerLost` for the entries not marked completed, in identity order.
4. `ECk_ProceduralAction_DeliveryStatus::Gap` and `FCk_ProceduralAction_ManagedPage::_FirstMissingSequence`/`_LastMissingSequence` removed. The dispatcher treats only Succeeded/Pending as readable; any other read result ensures (except `StreamExpired`) and completes `Expired`.

**Manifest delta (this round).** CkFoundation: `Core/CkProceduralActionReceiptStore.{h,cpp}` (stream bit, `SealManaged`, `Get_IsManagedDelivery`), `Action/CkProceduralAction_Fragment_Data.h` (enum doc, `Gap` removed), `Action/CkProceduralAction_Receipt.h` (page fields removed), `Action/CkProceduralAction_Utils.{h,cpp}` (seal at reservation, `Play` doc), `ActionPlayer/CkProceduralActionPlayer_Fragment.h` (entry without identity), `ActionPlayer/CkProceduralActionPlayer_Utils.{h,cpp}` (reads unsealed, seal by stream bit), `ActionPlayer/CkProceduralActionPlayer_Managed.cpp` (Gap removed, EndPlay destroys lost actions); docs `docs/campaigns/procedural-actions/API.md` §11 (Delivery status, Sealed identity, Listeners) and §10 `None` row, `GETTING_STARTED.md` (delivery comment, seal paragraph). `PROCEDURAL_ACTIONS.md` had no managed gap or seal line. CkTests: `Action/Test_ProceduralAction_Managed.cpp` (M1 read and seal assertions, M4 renamed and rewritten, M6b independent scope and destruction, measured counts logged), `Action/CkTest_ProceduralActionManagedListener.h` (`ManagedDuringCompletion`, `ActionPendingDestroyAtCompletion`), `Debugger/Test_ProceduralActionDebugger_Pie.cpp` (new row), `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ActionManagedPublicSurface.as` (read stays Pending; post-completion seal assertion removed). M3, the BP parity row and the AS test referenced no gap field.

**Red before the fixes.** Tests changed first. At 15:50:32 `rg --no-ignore` over `Core/` found no `Get_IsManagedDelivery`, `SealManaged` or `ManagedDelivery` (the rewritten M4 calls `FReceiptStore::Get_IsManagedDelivery`: red by construction). The other new assertions (readable managed reads in M1/M4/AS, the debugger row's uninterrupted recording, M6b's destroyed action and surviving scope) were red by reasoning against the then-current code (`Read_Receipts` returned `ManagedDelivery`; `ManagedEndPlay` destroyed nothing); not observed with a build.

**Command 1.**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P3b.log --project="D:/Repositories/CkRepos/Orion"
```
Build green on the first attempt. Actual exit **1**:
```
=== Test summary ===
Total: 191
Passed: 189
Failed: 2
Skipped: 0
Contaminated: 0
Duration: 3m 24s
Failed tests (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
Renderer-only: 7 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
NEW FAILURES — not on the list, these fail the gate (2):
  Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture
  Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity
```
Population `selects 191 of them (7 renderer-only)` (P3's 171 plus the rows Phases 4-6 and this round added). Failing names identical to the pre-fix gate and P2b: only the two inherited editor reds. 0 `Angelscript: Error` lines. Passed: all ten `Ck.ProceduralAnimation.Action.Managed.*` rows including `PublicConsumeIsSealedReadsStayObservable` and `PlayerLostFiresOnceFromEndPlay`, `Action.Editor.BlueprintParity.ManagedPlayLoadedVm`, `...Ck_AutoTest_ProceduralAnimation_ActionManagedPublicSurface`, and `Ck.ProceduralAnimation.Debugger.ActionPie.ManagedStreamKeepsRecording` (matched by the pattern). Measured, from the log: `[ManagedDrain] store returned to baseline after 2 polls past completion`; `[ManagedPlayerLost] the lost action became invalid after 2 polls past completion`.

**Command 2 (test only, same binary).**
```
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --no-live --test-pattern ProceduralAnimation.Debugger --output=Saved/Logs/ProceduralErgonomics-P3b-Debugger.log --project="D:/Repositories/CkRepos/Orion"
```
Actual exit **0**:
```
=== Test summary ===
Total: 9
Passed: 9
Failed: 0
Skipped: 0
Contaminated: 0
Duration: 1m 32s
Renderer-only: 2 test(s) ran in a real-renderer editor
=== Known reds: 81 listed (37 in AutomationGate.json, 1 in Plugins/CkFoundation/AutomationGate.json, 6 in Plugins/CkGameplayDebugger/AutomationGate.json, 37 in Plugins/CkTests/AutomationGate.json) ===
(no listed test ran in this run, and there were no new failures)
```
Passed: `Ck.ProceduralAnimation.Debugger.History.{BoundedOrderedCopies, RejectsNonFiniteTime, HoldScrubLiveReset, ClampsCapacity, RejectsStaleAndMixedSamples}`, `Ck.ProceduralAnimation.Debugger.PIE.{LegActions, RealGymSnapshotAndLifecycle}`, `Ck.ProceduralAnimation.Action.Debugger.CopiedReceiptsAndGameplayGap`, `Ck.ProceduralAnimation.Debugger.ActionPie.ManagedStreamKeepsRecording`. No pre-fix run of this pattern exists, so "existing rows keep their verdicts" rests on all eight existing rows passing here.

**Decisions taken in this round.** `SealManaged` instead of a delivery argument on `Reserve`/`ReserveUnbound` (keeps their signatures and the receipt-store tests unchanged). `ManagedEndPlay` destroys before it fires `PlayerLost`, so a recipient already sees the action pending destroy. The debugger row asserts through public model state: `_ActionRecordingStopped` has no getter, so the row asserts the recording's last read result stays Succeeded/Pending/Gap and its `Terminal` turns true (a stopped recording cannot reach terminal). M6b asserts the action is pending destroy when `PlayerLost` fires and logs the frames until it is invalid (2 in this run) rather than asserting a frame count.

## Blockers

- **B-P1-1 (open, 2026-10-12): `Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity` is red in its renderer group, green alone.** Evidence: Success in R48 and in Phase-1 run 2; Fail in run 3 and in a group re-run (`--test --no-live --known-reds off --test-pattern "ProceduralAnimation.Action.Editor"`, `Saved/Logs/ProceduralErgonomics-P1-EditorGroup.log`: 42 / 40 / 2, same two names); Success alone (`--test --no-live --known-reds off --test-pattern "Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity"`, `Saved/Logs/ProceduralErgonomics-P1-FocusedAlone.log`, exit 0). Renderer-group order is identical in all four runs: SharedPreviewRefresh, TwoTabsModesAndCapture (fails by timeout), PieCoexist.IndependentWorlds, FocusedAssetIdentity. The only source change between run 2 (pass) and run 3 (fail) was the Sweep recipe apex arithmetic and the AS fixture part count, neither reachable from an editor Slate click. Prime suspect (inferred, not confirmed): state left by TwoTabsModesAndCapture's stage-2 timeout (an open toolkit window or tab over the hit-test point) makes the routed `Add phase` click miss; the Phase-1 reflected `_Progress` field on `FCk_ProceduralAction_Phase` (an extra Details row) is the secondary suspect. Discriminating options: (a) revert the Phase-1 diff (both submodules), rebuild, and re-run `ProceduralAnimation.Action.Editor`: red there makes it pre-existing and order-dependent; (b) on the current build, run `SaveReopen.FocusedAssetIdentity` together with `PieCoexist.IndependentWorlds` but without `TwoTabsModesAndCapture`: green there points at the TwoTabs leak. Per PHASE_1 "anything else -> STOP", Phase 2 is not started.
  - **Resolved 2026-10-12 (orchestrator), option (a) executed.** Phase-1 source stashed in both submodules (`Source` in CkFoundation; `Source`, `Script/CkProceduralAnimation`, `Script/Generated` in CkTests), rebuilt, `--test --no-live --discover-fresh --known-reds off --test-pattern ProceduralAnimation.Action.Editor` on the untouched tree: first attempt exit 81 (real-renderer discovery editor did not come up, 38/38 headless), retry `Saved/Logs/ProceduralErgonomics-P1-AB-Baseline2.log`: **42 / 40 / 2, the same two names** (`MountedToolkit.TwoTabsModesAndCapture`, `SaveReopen.FocusedAssetIdentity`). Pre-existing, in-group, green alone: not Phase 1's. Stashes popped; tree identical to the executor's final state. It qualifies for a `flaky` known-red entry in `Plugins/CkTests/AutomationGate.json` (red in a gate, green alone); that is a separate commit by the maintainer, not done here. Phase 1 is **accepted**.
  - Orchestrator review of the Phase-1 diff (2026-10-12): rotation reflection is the proper `(w, -S v)`; `Add_PoseKey` edits a local phase copy and commits at the end; `Build_Into` compiles the draft through `FClipSource` before writing. One carry-over for Phase 2: `ValidateProgress` tolerates non-empty `_Keys` in `Preset` mode, while `Get_HasConsistentTuningSelection` rejects a non-default inactive field; make `Preset` with keys reject (`ProgressKeysInactive`) and add the row to `RejectsReversalDuplicateNaNAndNonLinearEasing`.
- **B-P2-1 (open, 2026-10-12): `Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation` is red in the wider pattern and red alone.** Run 1: `body turned steadily over at least 20 samples (19)`, worst trail 30.84 deg at sample 1, rate change 262.90 deg/s at sample 2 (`Test_ProceduralBodyPose_TiltSteps.cpp:753`). Alone (`Saved/Logs/ProceduralErgonomics-P2-BodyPoseAlone.log`, exit 1): `(5)` steady samples, worst trail 29.79 deg. Per PHASE_2 "red alone -> STOP". Attribution unknown: neither phase edits BodyPose, gait or SurfaceMotion code (`git diff --stat` above), but this pattern had no pre-change baseline. Prime suspect (inferred): pre-existing, sample-timing sensitive (the steady-sample count varies 19 vs 5 between runs, which an ordering change in a processor this phase does not touch would not explain). Discriminating options: (a) stash both phases (both submodules), rebuild, run this row alone: red there makes it pre-existing; (b) run it alone twice more on the current binary to see whether it oscillates (an `OSCILLATES` red entry for `Plugins/CkTests/AutomationGate.json` by PR in the plugin's repo).
  - **Resolved 2026-10-12 (orchestrator), option (a) executed both ways.** Both phases stashed (CkFoundation `Source`; CkTests `Source`, `Script/CkProceduralAnimation`, `Script/Generated`), rebuilt, row alone with `--no-live --known-reds off --skip-renderer-tests` (the row is headless): `Saved/Logs/ProceduralErgonomics-P2-AB-Baseline.log` **1/1 pass**. Stashes popped, rebuilt, same command on the idle machine: `Saved/Logs/ProceduralErgonomics-P2-AB-WithPhases.log` **1/1 pass**. The executor's red alone run (`P2-BodyPoseAlone.log`, 5 steady samples) started right after the wide gate while its editors were shutting down; the test measures steadiness against real world delta time (`Test_ProceduralBodyPose_TiltSteps.cpp:517`), so it is machine-load sensitive. Not caused by Phases 1 or 2; a `flaky` candidate for the CkTests list (by PR there). Blocker closed.
- **B-P2-2 (open, 2026-10-12): `Ck.ProceduralAnimation.Action.Diagnostics.SamplingBudgetCarriesObservedAndBudget` cannot be driven as PHASE_2 specifies.** Both attempts (a 10-degree manual target at full reach, then at 95 % of reach) ended `InputTimeout` after 10 s with no budget in the context. Cause, read from code (not a guess): the player service predicts every manual candidate's step against the tuned sampling budget before staging and, when it exceeds, completes the submit `Failed` and drops the batch while the action stays `Ready` (`ActionPlayer/CkProceduralActionPlayer_Service.cpp`: `QualifiesOther` measures the predicted step against `Sampling.MaxEndpointTravelCm` at 1210-1222, `BatchQualified` gates on it at 1271-1273, the fallback fails the submit at 1369-1385). The measured `SettledMotionExceededSamplingLimits` path (`ActionPlayer/CkProceduralActionPlayer_Observe.cpp`) therefore fires only when a predicted step was within budget and the settled one was not, which the public API cannot produce deterministically; the clip pre-settlement `SamplingBudgetExceeded` path (`Service.cpp`, `Plan.CriticalFractions.IsEmpty()`) has no measurement to carry (`SampleInterval` reports only a bool). Architecture question: (A) keep the measured context on the Observe path and rewrite the row as a pure unit test of the measurement (a small internal helper exposed for tests), or test it through a settlement-divergence fixture; (B) make `SampleInterval` report the step that broke the budget so the clip path carries observed/budget, which touches the Phase-1 evaluation core; (C) also capture context for the pre-staging manual rejection, which today does not fail the action at all (a contract change). The failing row stays in the tree as written, with the 95 % target.
  - **Resolved 2026-10-12 (orchestrator decision, option A narrowed).** The row is deleted and replaced by `Ck.ProceduralAnimation.Action.Diagnostics.ReceiptOverflowCarriesPendingAndBudget` (passing in P2b); the Observe sampling-budget capture stays as written and is a known gap for Phase 7 (see the Phase 2 closing record).
- **B-P3-1 (resolved 2026-10-12, orchestrator decision): DESIGN 11.4 validated `_ManagedRecordsPerTick` as `1..RecordsPerInstance+2` with default 32.** `ValidateCapacity` is shared by `FReceiptStore::Configure` and player `Add`, so that range would have rejected existing explicit capacities (`Test_ProceduralActionReceiptStore.cpp:225` RPI 1, `:372` RPI 2, `Test_ProceduralAction_Diagnostics.cpp:245` RPI 4). Raised before implementation; the orchestrator approved option A: validate `1..(receipt_limits::MaxOrdinaryRecordsPerInstance + 2)` independent of `RecordsPerInstance`, keep default 32, keep the per-identity page cap `min(remaining budget, RecordsPerInstance + 2)`, change none of the three tests. Implemented in `Core/CkProceduralActionReceiptStore.cpp` `ValidateCapacity`; the three rows pass in P3.
- **B-P5-1 (resolved 2026-10-12, regex flag applied, row passes in P5b): `Ck.ProceduralAnimation.Action.Mirror.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding` red in the final run, test defect, cause confirmed from source.** The row has no failing assertion in run 3; the automation lists the ensure as an unexpected error: `CkEnsure: [PIE-ID -2] [Server] Valid / ProceduralAction Create_FromLoadedClip rejected clip or tuning [Play.Mirrors].` (the 65-mirror case, which the row expects). `AddExpectedError(MirrorCountEnsure, Contains, 0)` treats its pattern as a regex by default (`Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h:1873`, `bool IsRegex = true`), and `[Play.Mirrors]` is a character class, so the expectation never matches. Fix (not applied: the bounded fix/re-run for this row was already spent on the record-count assertion): pass `IsRegex = false`, i.e. `AddExpectedError(MirrorCountEnsure, EAutomationExpectedErrorFlags::Contains, 0, false)`, or drop the bracketed suffix from `MirrorCountEnsure`. Framework behaviour under this row passed in run 2.
- **B-P5-2 (resolved 2026-10-12, test sequencing; root cause in the Phase 5 re-gate record): `Ck_AutoTest_ProceduralAnimation_BorrowingSweep` cannot recover the unplanted leg.** Final-run text: `Failed: step 17/18 'the leg is recovered': condition 'Check_Recovered' never became true within 900 polls / 7.50s (floor 2.00s) (1/9 assertions failed)`. Test log (`Saved/Logs/Mars_2.log` of that run): after the floor returns and `Get_TrustedContactCount(Gait)` reaches 3 (it reads 4), `Request_RecoverUnplantedLimbs` completes **`Failed`** once, and `Leg0` stays `Unplanted`, reason `ReturnProbeAttemptsExhausted`, `TrustedSupport=false`, for the whole 7.5 s. Everything before it passes: short sweep `Completed / Succeeded`, the floor-loss sweep `SupportLost / Failed / ReturnProbeAttemptsExhausted` diagnosed through `Get_ResultText` and `Get_UnplantedLimbIds`. The bounded fix (wait for trusted support first) was applied and re-run; per the package, stopped. Prime suspect (inferred, not confirmed): the `TryRecover` admission behind `Request_RecoverUnplantedLimbs` rejects this state, e.g. the player's `CountOnly(3)` support counting the borrowed leg's own stale gait contact, or recovery requiring a probe budget the exhausted return used up; the C++ `Query.RecoverUnplantedUsesDrainTimeSet` also shows a recovery that completes `Failed` for one of its two limbs. Discriminating options: (a) read `FCk_ProceduralLimbControl_Control::TryRecover` / the RecoverUnplantedLimbs drain for the exact rejection reason and log it in the AS row; (b) replace the AS recovery step with `Request_RecoverLimbs(Leg0)` (the path `Gait.RecoveryPie` proves) to see whether only the unplanted-set request fails.
- **B-P5-3 (closed 2026-10-12, moved to "Owed to the maintainer"): `Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` is untracked and git-ignored** (`Plugins/CkFoundation/.gitignore:49 *.md`); the Phase 5 edits to it exist on disk only and will not appear in any diff or commit unless the file is force-added. `Claude.md` and the `docs/campaigns/procedural-actions/*.md` files are tracked.
- **B-P5-4 (closed 2026-10-12, grep scoped by orchestrator decision; scoped result in the re-gate record): the Phase 5 exit grep cannot reach 0 without rewriting history or a gym's species name.** 26 hits remain: `docs/campaigns/procedural-actions/{PROGRESS 13, REVIEW 3, RESEARCH 2, VALIDATION 2, PROMPT 1, CONSUMER_INTEGRATION 1, ARCHITECTURE 1}.md` (the earlier campaign's evidence records and its "no physical whip elasticity" disclaimer) and `Source/CkProceduralAnimation/Claude.md` 3 (the gym species names `SmallCrawler4`, `Crawler4/6/8`). None is in a file Phase 5 created or a line it wrote. Decision needed: scope the grep to the shipped workflow docs (`API`, `AUTHORING`, `GETTING_STARTED`, `PROCEDURAL_ACTIONS.md`, `Script/`), which return 0, or authorize rewriting those records.
- **B-P4-1 (open, 2026-10-12): pose commands are compile-gated by `Build_Into` (PHASE_4 step 3), which refuses the first capture into an unkeyed phase (Blank recipe) and every pose command on an already-invalid clip, against DESIGN §4 "invalid drafts remain saveable".** Implemented as the package says; not worked around. Options and evidence: Q-P4-1 in the Phase 4a gate record. Needs an architecture decision before 4b wires the Capture button.
  - **Resolved 2026-10-12 (orchestrator decision Q-P4-1 -> option B, implemented in 4b).** Pose, mirror and phase-timing commands write through `Write_Into`; `PoseCapture.CaptureUpdateDuplicateMoveDelete` now asserts the uncompilable move is written and reported by `Compile`.
- **B-P4b-1 (resolved 2026-10-12, orchestrator decision option B, implemented in P4c): the orchestrator's mounted script "wait for terminal, Capture solved pose -> accepted" contradicts the 4a snapshot rule.** `FCk_ProceduralActionSolvedSnapshot::Make` sets Ready only for `EStatus::Ready` with no seek target (`Private/Authoring/CkProceduralActionSolvedCapture.cpp:16-17`). A preview with no observation horizon goes `Finished` at terminal (`CkProceduralActionSolverWorld.cpp:711-712`), so a terminal capture is refused; observed in both gate runs (`The Solver preview is not ready: pause it at a step with no seek or preparation pending.`). A completed seek also leaves `_SeekTarget` set (`SeekToStep` sets it; only `Resume` clears it, `CkProceduralActionSolverWorld.cpp:487-495`), so after **Reset** or **Frame** in Solver mode a capture is refused until Play resumes. The mounted row therefore asserts the terminal refusal and captures after pausing a re-run (the VALIDATION wording "pause Solver at a settled step"). Options: (A) keep the rule and the row as written; (B) count a satisfied seek (`_SeekTarget <= _Step` with status Ready) as not pending, so Reset/Frame-paused poses are capturable; (C) also accept `Finished` when the last accepted record is still the settled revision. B and C change 4a semantics and need a row each.
- **Q-P4b-2 (closed 2026-10-12, orchestrator):** moved to "Owed to the maintainer"; not added.
