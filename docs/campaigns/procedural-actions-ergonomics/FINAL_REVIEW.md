# Procedural actions ergonomics: final review dossier

2026-10-12, Phase 7 (Opus executor). Every count and log path below is copied from [PROGRESS.md](PROGRESS.md), where each record keeps the full summary block. Nothing has been committed, pushed, stashed or staged. HEADs: Mars `6a43f51`, CkFoundation `b23668049`, CkTests `96ef615b`, all on `feature/procedural-anim-ext`.

## Verdict (narrowest supported claim)

- **Supported by the final source (frozen 16:20, plus one measurement test file):**
  - The focused gates show no unexplained failure. The `ProceduralAnimation` pattern ran 460 tests: 455 passed and 5 failed. Three of those are listed reds; the other two are the inherited editor reds, attributed pre-existing by an A/B run in Phase 1.
  - `CrawlerMelee` 38/38 and `PlayerCombat` 8/8.
  - Every campaign row passes in C++, AngelScript and the Blueprint VM.
  - The cost measurements ran.
- **Not supported:**
  - The native **Game** build. Every module compiled, but the link failed with `LNK1318` on the PDB.
  - **AS-off** and **cooked/packaged** behaviour: not verified (Blocked).
  - Every `[EDITOR-VERIFY]` exercise: none has been observed by a human.
  - The inherited mounted red `MountedToolkit.TwoTabsModesAndCapture` stays an explicit **release blocker** (PHASE_7 exit rule).
- VALIDATION **F1 is therefore Failed** (the Game build), and the campaign is not releasable as complete.

## 1. File manifest (final, from `git status --short`)

Paths below are abbreviated: `PA/` = `Source/CkProceduralAnimation/Public/CkProceduralAnimation/`, `PAE/` = `Source/CkProceduralAnimationEditor/`, `UT/` = `Source/CkTests/Private/UnitTests/CkProceduralAnimation/`, `ED/` = `Source/CkTestsEditor/Private/CkProceduralAnimation/`.

### Mars (`D:/Repositories/CkRepos/Orion`): 24 tracked files, +734/-537
Campaign (Phase 6):
```
 M Script/Debugger/Pages/Mars_DebugPage_Monsters.as
 M Script/ECS/Crawler/Mars_Crawler_{ActionAdapter,Attack,Feature,Processor_AttackResolve,Processor_AttackTick,Utils}.as
 M Script/Tests/CrawlerMelee/Mars_AutoTestRig_CrawlerMelee.as
 M Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_{ActionScopeLoss,CancelBeforeCommit,Displacement,EveryExit,HitchAndZeroTime,InvalidSpec,PhaseSequence,TerminalStrikeHit}.as
 M Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as
?? Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_{CancelReturnsThroughAction,ClipDrivesPhases,OffenseWindowFromReceipt}.as
```
Generator and populator output (never hand-edited; regenerated at editor boot): `Script/Generated/Mars_{AutoTestActors,EntitySpawnParams,ScriptProcessorDrivers}.as` and `Content/Mars/Maps/AutoTests_Mars_MAP.umap`. Gitlinks: `Plugins/CkFoundation` and `Plugins/CkTests`, dirty. **Not this campaign's and left untouched:** the `Plugins/Monolith` gitlink, a sibling session's dirty `MonolithEditor` files. A gate also rewrites `Config/DefaultGameplayTags.ini`; never stage it.

### CkFoundation (`Plugins/CkFoundation`): 44 tracked files, +4844/-496
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
?? PA/ActionBuilder/CkProceduralActionBuilder_Data.h, CkProceduralActionBuilder_Utils.{h,cpp}
?? PA/ActionPlayer/CkProceduralActionPlayer_{Managed,Mirror}.cpp
?? PA/Core/CkProceduralActionProgress.{h,cpp}
?? PAE/Private/Asset/CkProceduralActionGuidedCreation.cpp
?? PAE/Private/Authoring/CkProceduralAction{PoseGroups,SolvedCapture}.cpp
?? PAE/Private/Widgets/SCkProceduralAction_ProgressPlot.{h,cpp}
?? PAE/Public/CkProceduralAnimationEditor/CkProceduralAction{GuidedCreation,PoseGroups,SolvedCapture}.h
?? docs/campaigns/procedural-actions-ergonomics/   (this campaign's docs)
```
`Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` is edited but matched by `.gitignore:49 *.md`, so it appears in neither list (see §8).

### CkTests (`Plugins/CkTests`): 7 tracked files, +719/-7
```
 M Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ArticulationPublicSurface.as
 M Script/Generated/CkTests_AutoTestActors.as                 (generator output)
 M UT/Debugger/Test_ProceduralActionDebugger_Pie.cpp
 M ED/Test_ProceduralAction_{BlueprintParity,MountedToolkit,PieCoexist,SaveReopen}.cpp
?? Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_{ActionAssetDefinition,ActionBuilderPublicSurface,ActionManagedPublicSurface,BorrowingSweep,BuilderSweep,NoncombatManaged}.as
?? Script/CkProceduralAnimation/CkProceduralAnimation_ActionFixture_Assets.as
?? UT/Action/CkTest_ProceduralActionManagedListener.h
?? UT/Action/Test_ProceduralAction_{Builder,BuilderCost,Diagnostics,Managed,Mirror,MirrorRuntime,Progress}.cpp
?? UT/ActionPlayer/Test_ProceduralActionPlayer_Presets.cpp
?? UT/Articulation/Test_ProceduralArticulation_Query.cpp
?? UT/CkTest_ProceduralWalkerFixture.h
?? ED/Test_ProceduralAction_{GuidedCreation,PoseCapture,PoseWorkspace,SolvedCapture}.cpp
?? Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level/{0/Y3,1/V4,4/4H,4/81,4/V7,9/7N}/   (populator output from gates; never stage)
```

## 2. Gates: baseline → final

| Pattern | Baseline (source, log) | Final (log) | Failing names at final |
|---|---|---|---|
| `ProceduralAnimation.Action` | R48 pre-campaign 132 / 131 / 1 (`Saved/Logs/ProceduralActions-Commit-R48.log`) | inside the gate A row below; the last standalone run was P3b 191 / 189 / 2 (`Saved/Logs/ProceduralErgonomics-P3b.log`) | `MountedToolkit.TwoTabsModesAndCapture` (in R48), `SaveReopen.FocusedAssetIdentity` (green in R48, red in its renderer group since Phase 1 run 3; A/B with Phase 1 reverted also red: `ProceduralErgonomics-P1-AB-Baseline2.log`) |
| `ProceduralAnimation` (wide) | no pre-change run; first wide run P2 425 / 417 / 8; P5 444 / 435 / 9 | **gate A of record: 460 / 455 / 5, exit 1** (`Saved/Logs/ProceduralErgonomics-Final-PA.log`) | NEW: the two above. Listed: `SurfaceMotion.SnapshotShowsThePendingContact` (root, HOST-COUPLED), `Ck_AutoTest_ProceduralAnimation_PillarCrossingRenderedTraversal`, `..._PostsCentipedeReturns` (CkTests list) |
| `CrawlerMelee` | P6 baseline 35 / 35 (`ProceduralErgonomics-P6-Baseline.log`) | **38 / 38, exit 0** (`Saved/Logs/ProceduralErgonomics-Final-Crawler.log`) | none |
| `PlayerCombat` | no pre-change run (P6: 8 / 8) | **8 / 8, exit 0** (`Saved/Logs/ProceduralErgonomics-Final-PlayerCombat.log`) | none |
| `ProceduralAnimation.Debugger` | no pre-change run | P3b-Debugger 9 / 9, exit 0 (`Saved/Logs/ProceduralErgonomics-P3b-Debugger.log`); the new row `ManagedStreamKeepsRecording` also passes in the final gate A | none |
| `ProceduralAnimation.Action.Performance` | new rows | **4 / 4, exit 0** (`Saved/Logs/ProceduralErgonomics-Final-Perf.log`) | none |

Final gate A in detail:
- Population: `selects 460 of them (8 renderer-only)`. 0 `Angelscript: Error` lines.
- Known-reds block: `Now passing` for `Gait.LandingProbeLiftsASwingOntoAStep` (CkTests list), and `Listed flaky, passed` for `PillarsEngageSteppedTops` and `SpiderInteractiveRosterDiagnostics`.
- History of earlier Phase 7 runs: the pre-fix run was 459 / 454 / 5 with the same NEW pair. The post-fix run was 460 / 454 / 6 with `NoncombatManaged` NEW and red alone (`ProceduralErgonomics-Final-NoncombatAlone.log`). That test still carried the old seal contract; the orchestrator corrected it (option a, test only) and the final gate is green on it.
- The toolbox rotates `Saved/Logs`. Copies of every Phase 7 log are in the Phase 7 session scratchpad.

Three environments, all green in the final gate:
- **Native C++:**
  - `Action.Builder` ×10, `Action.Progress` ×4, `Action.Mirror` ×7 (including `Runtime.*` ×3), `Action.Managed` ×10, `Action.Diagnostics` ×4.
  - `ActionPlayer.Presets` ×4, `Articulation.Query` ×4.
  - Editor rows: `PoseCapture.*` ×6, `SolvedCapture.*` ×3, `RecipeCreationCancelLeavesNoAssets`, `PairedSaveFailureReportsWhichAsset`, `MirrorDelegatesToBuilder`, `MountedToolkit.PoseWorkspace.*` ×2.
- **AngelScript:** `Ck_AutoTest_ProceduralAnimation_{ActionBuilderPublicSurface, ActionAssetDefinition, ActionManagedPublicSurface, NoncombatManaged, BuilderSweep, BorrowingSweep, ArticulationPublicSurface}`.
- **Blueprint VM:** `Action.Editor.BlueprintParity.{ManagedPlayLoadedVm, MirrorVm}`, plus the pre-existing `ArticulationNodes` and `LoadedClipVm`.

No full gate was run, by the maintainer's standing sizing rule (focused patterns only).

## 3. Build and runtime evidence

- **Editor builds:** Development, SHARED lock, green on the final source (gate A) and again with the measurement file (Perf run).
- **Game build** (`--build --target=Game`, no `--config`, `Saved/Logs/ProceduralErgonomics-Final-Game.log`): **exit 1**.
  - Toolbox lines: `Auto config resolved to Development`, `Build lock: EXCLUSIVE (Shared build environment)`, ShaderCompileWorker `Result: Succeeded`.
  - Then `[358/359] Link [x64] Mars.exe`, `LINK : fatal error LNK1318: Unexpected PDB error; LIMIT (12) '...\Binaries\Win64\Mars.pdb'`, `Result: Failed (OtherCompilationError)`, 1151 s.
  - **No compile error:** all 357 compile actions passed, including the CkProceduralAnimation and CkTests runtime modules with `WITH_EDITOR=0`.
  - Prime suspect (inferred): the monolithic PDB crossed the linker's size limit. The last good link on disk is `Mars.exe` from 2026-10-08, before the campaign.
  - Not run: a page-size relink, or an A/B without the campaign diff.
  - **This build is AS-on, not AS-off.** `Mars.uproject` enables `Angelscript` with no target restriction, so `CkBuildConfig.Build.cs:53-61` sets `WITH_ANGELSCRIPT_CK=1` (inferred from the rules).
- **Cooked/AS-off route:** **Blocked**. `Plugins/CkTests/Tools/ProceduralActionsSmoke/README.md` says the template "has not yet been built, cooked, or run". The route costs two builds of a second project plus a UAT cook. Not run, on the orchestrator's decision.
- **PIE runtime:** every AS and PIE row above ran in headless PIE editors. A rendered mounted pass ran the 8 renderer-only rows.

## 4. Measured costs

Source: `Test_ProceduralAction_BuilderCost.cpp`, run once.
- **Conditions:** Development editor, headless PIE. 3 concurrent editors (toolbox auto-size), so each row shared the machine with two others. 3 repetitions inside each pure row.
- **No targets.** A before/after on the same source is impossible: the old code had no builder, custom progress, runtime mirror or managed dispatch. Preset vs CustomCurve is the same-source comparison the brief asked for instead.

| Measured | Rep 1 / 2 / 3 |
|---|---|
| `Make_FromRecipe` (Reach) | 7.3 / 6.6 / 6.0 µs |
| 64 × `Add_PoseKey` | 62.4 / 60.4 / 54.8 µs (0.86-0.98 µs per key) |
| `Build_Into` (70 keys, compiles then writes) | 45.3 / 39.4 / 37.2 µs |
| `Compile`, Preset easing | 34.9 / 34.6 / 34.3 µs |
| `Compile`, CustomCurve (5 keys × 3 phases, same position keys) | 85.6 / 84.8 / 83.7 µs (≈ 2.4× Preset; load-time cost only) |
| `Evaluate` per sample, Preset / CustomCurve | 96.6 / 99.8 / 97.1 ns and 97.0 / 99.1 / 97.1 ns (no measurable difference) |
| `EvaluateProgress` per sample, Preset / CustomCurve | 4.8-5.8 ns / 9.1-9.9 ns |
| `ExpandMirrors` 1 mirror (70 → 140 keys) | 5.05 / 5.00 / 5.10 µs per call (max 21.5) |
| `ExpandMirrors` 8 mirrors (70 → 630 keys) | 34.7 / 33.7 / 34.4 µs per call (max 93.5) |

**Managed dispatch, steady state** (one run): 4 managed clips, one player, one records listener each, 300 frames.
- Records: 1200 delivered in 1200 callbacks (4.0 per frame).
- Store before play: 0 streams, 0 record slots, 16 bytes.
- Store maximum during measurement: 8 record slots, 80 pose transforms, 24576 bytes.
- Store after cancel and release: 0 streams, 0 slots, 0 transforms, 3280 bytes. Array capacity is retained; it was reached 2 polls after completion.
- Frame interval: managed 16.67 ms avg (16.27-17.07), a frame-paced regime. Idle 20.38 ms avg (2.94-986.5, one hitch). Because the pacing differs, **no dispatcher cost is derived from them**.
- Allocation count and dispatcher self-time: **unavailable**. The harness has no allocator counter; a `STATGROUP`/Insights capture of the `ManagedDispatch` processor is the follow-up.
- **Allocation sites (by code reading):**
  - Per page, the dispatcher copies the page records and listener arrays (`ActionPlayer/CkProceduralActionPlayer_Managed.cpp`).
  - The receipt store keeps its record and pose capacity after streams are reclaimed (the 3280 bytes above).
  - Compile, recipe and mirror expansion allocate only on build/play, not per frame (DESIGN §10).

## 5. Decisions taken, by phase (details in PROGRESS)

- **Phase 1** ([gate record](PROGRESS.md#phase-1-gate-record-2026-10-12-opus-executor)):
  - `Compile` takes an `FClipSource` view shared by the asset and the draft (one compiler).
  - Custom progress is a monotone Fritsch-Carlson Hermite. It is hashed only in `CustomCurve` mode, so old clips keep their content hashes.
  - The AS asset route uses the generated setters.
  - Carry-over into Phase 2: `Preset` with keys rejects (`ProgressKeysInactive`).
- **Phase 2:**
  - `Make_CountOnlySupport` returns the invalid request after its ensure, so `Add` rejects it.
  - The sampling-budget row was replaced by `ReceiptOverflowCarriesPendingAndBudget` (B-P2-2, option A narrowed).
  - The BodyPose red was attributed to machine load by A/B (B-P2-1).
- **Phase 3:**
  - `_ManagedRecordsPerTick` is validated `1..MaxOrdinaryRecordsPerInstance+2`, independent of `RecordsPerInstance` (B-P3-1).
  - Dispatcher order: snapshot, then read, copy, callbacks, revalidate, consume; terminal, then completion, then release.
  - Fair rotation; `PlayerLost` comes from EndPlay.
  - **Review fixes:** reads unsealed; the seal is a stream bit set at reservation (`SealManaged`); EndPlay destroys lost actions; `DeliveryStatus::Gap` and the page gap fields were removed. See the [Phase 3 review fixes record](PROGRESS.md#phase-3-review-fixes-gate-record-2026-10-12-opus-executor).
- **Phase 5:**
  - `ExpandMirrors` takes the slot bindings (rest frames are per limb).
  - It runs at all three bind sites; anchor sources reject.
  - The hash folds in mirrors only when they exist.
- **Phase 4:**
  - Pose groups live on the clip (one undo covers keys and metadata).
  - Pose commands write through `Write_Into`, not compile-gated (Q-P4-1, option B).
  - Settledness is `SubjectPoseRevision == Get_SettledRevision` (Q-P4-2).
  - A satisfied seek counts as Ready (B-P4b-1, option B).
  - Guided creation uses a modal choice; paired save reports per asset.
- **Phase 6:**
  - The entity script owns the clip (D-P6-1).
  - `CancelSeconds` became the dedicated return; the `PoseTimeout` watchdog was removed.
  - Phases follow the accepted PhaseId; `EntryBlend = 0`.
  - D-P6-1..11 list the rest; the approved assertion changes are each reasoned in their test.
- **Phase 7:** `NoncombatManaged` was corrected to the new seal contract (orchestrator, option a); the measurement file was added.

## 6. Rejected designs

These come from DESIGN §3, AUDIT, and the forks decided during execution.

- **From DESIGN §3 and AUDIT:**
  - A Slate-only builder: excludes AS and native users.
  - A second pose asset or player: duplicates authority.
  - Replacing receipts with signals: loses retained evidence.
  - Name-based automatic retargeting: can move the wrong appendage.
  - CountOnly as the borrower default: hides a consequential policy.
  - Unrestricted progress curves: can reverse time or window order.
  - Direct pose tweening: bypasses settlement.
  - A transient `UCk_ProceduralAction_Data` per `Build_Into`: GC churn and a second code path (AUDIT X6).
  - Managed records as CK signals: they retain and replay their payload (AUDIT X10).
  - A campaign-named test namespace (AUDIT X3).
  - A new `BuildReport` type (AUDIT X7).
  - Unconditional progress hashing (AUDIT X11).
- **Forks decided during execution:**
  - Compile-gated pose commands (Q-P4-1 A/C).
  - Requiring an unsatisfied seek or accepting `Finished` for solved capture (B-P4b-1 A/C).
  - Making `SampleInterval` report the budget step, or capturing context on pre-staging rejection (B-P2-2 B/C).
  - DESIGN 11.4's original `1..RecordsPerInstance+2` budget range (B-P3-1).
  - A managed `Gap` delivery status, and reads sealed for managed identities (review fixes).
  - Restoring "seal lifts at completion" (the post-fix stop, option b).

## 7. What still speaks the old contract

- `ECk_ProceduralAction_DeliveryStatus::Gap` and `FCk_ProceduralAction_ManagedPage::_FirstMissingSequence`/`_LastMissingSequence` were removed. Any Blueprint graph or out-of-tree AS that referenced them breaks on its next open or compile. In-tree callers were updated.
- `UCk_Utils_ProceduralActionPlayer_UE::Get_IsManagedDelivery` is now true until the stream is reclaimed, which is after release and completion. Callers that tested it right after the completion callback see `true`, as `NoncombatManaged` did.
- `ECk_ProceduralAction_ReadResult::ManagedDelivery` is kept but is returned by nothing.
- Phase 6 crawler: `EMars_Crawler_AttackCancel::PoseTimeout` and `PoseTimeoutSeconds` are gone. `Get_AttackPhaseElapsed`, `Get_AttackPhaseDuration` and `Get_AttackPendingSampleId` were removed (the debugger page was updated). A saved crawler spec that still carries the old fields loads them as ignored data (inferred).

## 8. Outstanding limits

1. **No `[EDITOR-VERIFY]` exercise has been performed by a human.** That covers VALIDATION exercises 1-5 and the clicks written in PROGRESS "Editor verification owed to the human" (body-only lean, dedicated-limb sweep with mirror, undo/redo and save/reopen, two clips beside PIE). It also covers the Phase 6 Sandbox Room 5 crawler check. E1, E5 and E7, and the human halves of E2 and E4, rest on these.
2. **Managed observation gap (debugger).** The managed dispatcher acknowledges records in the frame they are published. An observer reading through `Read_Receipts` therefore typically sees gap pages and the terminal, not the records. Seen by the Phase 3 executor and the Phase 7 executor; `ManagedStreamKeepsRecording` asserts only that recording keeps running, and that its last read result is Succeeded, Pending or Gap. Recommended follow-up: a bounded ring of recent managed pages on the player, for observation only.
3. **Recovery drain log** (`LimbControl/CkProceduralLimbControl_Processor.cpp`). It prints `ECk_ProceduralLimbControl_AcquireResult` as its integer ordinal, because that enum is not a UENUM and its formatter does not link.
4. **The sampling-budget failure context is untested.** The `SettledMotionExceededSamplingLimits` capture in `ActionPlayer/CkProceduralActionPlayer_Observe.cpp` is code-reviewed only: the public API cannot drive it deterministically (B-P2-2).
5. **Inherited reds and flaky candidates.** No entry was added; each list entry is a separate commit, and plugin-list entries go by PR in CkTests.
   - `Ck.ProceduralAnimation.Action.Editor.SaveReopen.FocusedAssetIdentity`: red in its renderer group in every gate since Phase 1 run 3, and in the Phase 1 A/B with the campaign reverted; green alone (`P1-FocusedAlone`, `P4b-FocusedAlone`). Recommended: `flaky` in `Plugins/CkTests/AutomationGate.json`. Reason: lane-order leak after the `TwoTabs` timeout.
   - `Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture`: red in R48 (pre-campaign), in the A/B, and in every gate, with `timed out at stage 2: Expanded Preview Placement has no visible Root Frame`. Recommended: run it alone once; if red, a `red` entry in the CkTests list. It is a **release blocker** until fixed or listed.
   - `Ck.ProceduralAnimation.BodyPose.PresentationLagIsBoundedThroughSustainedRotation`: red in the P2 wide gate and red alone under load; green alone on an idle machine in both A/B arms, and green in all three Phase 7 gates. Recommended: `flaky` (CkTests list). Reason: steadiness is measured against real delta time.
   - `Project.Functional Tests...Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal`: red in P5, green alone and green in the Phase 7 gates. Recommended: `flaky` (CkTests list).
   - Plugin-list hygiene, by PR in CkTests: `Gait.LandingProbeLiftsASwingOntoAStep` reports `Now passing`; the `flaky` entries `PillarsEngageSteppedTops` and `SpiderInteractiveRosterDiagnostics` passed.
6. **`PROCEDURAL_ACTIONS.md` is git-ignored** (`Plugins/CkFoundation/.gitignore:49 *.md`). Its Phase 5 edits exist on disk only. Neither a commit nor the rollback below covers it, and git holds no earlier version.
7. **Game link failure, AS-off unverified, cooked Blocked** (§3).
8. **Measurement limits** (§4): one run of the binary, three lanes loading the machine, no allocation count, no dispatcher self-time.
9. **Narrow evidence:**
   - Solved-capture settledness has been seen on a paused live Solver for one clip and one subject; walking subjects are unproven (Q-P4-2).
   - `TerminalStrikeHit`'s one-frame prediction (P6 remaining uncertainty) has not been observed failing.
10. **Adjacent smells flagged, not fixed** (AUDIT item 6 and P2):
    - The retired double-shape ensures in `Action/CkProceduralAction_Utils.cpp`.
    - `Request_RecoverLimbs` rejects without `CK_ENSURE_IF_NOT`.
    - `UCk_Utils_ProceduralActionPlayer_UE::Add` returns `{}` silently when `Attach` rejects.
    - Cosmetic: after a capture, the toolkit's selection message replaces the capture message.
    - Three gym species names (`SmallCrawler4`, `Crawler4/6/8`) remain in `Source/CkProceduralAnimation/Claude.md`; B-P5-4 scoped them out.
11. **The independent adversarial review** ran in a separate session. Its four confirmed defects were fixed and re-gated (Phase 3 review fixes record). Its full list of dismissed claims is not recorded in this directory.

## 9. Rollback (commands to run by hand; none has been run)

Each line names its own paths, so nothing outside this campaign is touched. Leave `Plugins/Monolith` alone; it belongs to a sibling session.

Mars:
```powershell
git -C D:/Repositories/CkRepos/Orion checkout -- Script/Debugger/Pages/Mars_DebugPage_Monsters.as Script/ECS/Crawler/Mars_Crawler_ActionAdapter.as Script/ECS/Crawler/Mars_Crawler_Attack.as Script/ECS/Crawler/Mars_Crawler_Feature.as Script/ECS/Crawler/Mars_Crawler_Processor_AttackResolve.as Script/ECS/Crawler/Mars_Crawler_Processor_AttackTick.as Script/ECS/Crawler/Mars_Crawler_Utils.as Script/Tests/CrawlerMelee/Mars_AutoTestRig_CrawlerMelee.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_ActionScopeLoss.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_CancelBeforeCommit.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_Displacement.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_EveryExit.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_HitchAndZeroTime.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_InvalidSpec.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_PhaseSequence.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_TerminalStrikeHit.as Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as Script/Generated/Mars_AutoTestActors.as Script/Generated/Mars_EntitySpawnParams.as Script/Generated/Mars_ScriptProcessorDrivers.as Content/Mars/Maps/AutoTests_Mars_MAP.umap
git -C D:/Repositories/CkRepos/Orion clean -n -- Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_CancelReturnsThroughAction.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_ClipDrivesPhases.as Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_OffenseWindowFromReceipt.as
```
CkFoundation:
```powershell
git -C D:/Repositories/CkRepos/Orion/Plugins/CkFoundation checkout -- Script/CkProceduralAnimation/Examples/CkProceduralAction_Noncombat.as Source/CkProceduralAnimation/Claude.md Source/CkProceduralAnimation/Private/CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h Source/CkProceduralAnimation/Public/CkProceduralAnimation Source/CkProceduralAnimationEditor docs/campaigns/procedural-actions/API.md docs/campaigns/procedural-actions/AUTHORING.md docs/campaigns/procedural-actions/GETTING_STARTED.md
git -C D:/Repositories/CkRepos/Orion/Plugins/CkFoundation clean -n -- Script/CkProceduralAnimation/Examples/CkProceduralAction_Builder.as Script/CkProceduralAnimation/Examples/CkProceduralAction_Sweep.as Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionBuilder Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Managed.cpp Source/CkProceduralAnimation/Public/CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Mirror.cpp Source/CkProceduralAnimation/Public/CkProceduralAnimation/Core/CkProceduralActionProgress.cpp Source/CkProceduralAnimation/Public/CkProceduralAnimation/Core/CkProceduralActionProgress.h Source/CkProceduralAnimationEditor docs/campaigns/procedural-actions-ergonomics
```
(Every tracked file under `Source/CkProceduralAnimation/Public/CkProceduralAnimation` and `Source/CkProceduralAnimationEditor` that differs from HEAD is a campaign file, per §1. Check `git -C ... status --short -- <dir>` first if another session may have touched those directories. `PROCEDURAL_ACTIONS.md` cannot be restored from git.)

CkTests:
```powershell
git -C D:/Repositories/CkRepos/Orion/Plugins/CkTests checkout -- Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ArticulationPublicSurface.as Script/Generated/CkTests_AutoTestActors.as Source/CkTests/Private/UnitTests/CkProceduralAnimation/Debugger/Test_ProceduralActionDebugger_Pie.cpp Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_BlueprintParity.cpp Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_MountedToolkit.cpp Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_PieCoexist.cpp Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_SaveReopen.cpp
git -C D:/Repositories/CkRepos/Orion/Plugins/CkTests clean -n -- Script/CkProceduralAnimation Source/CkTests/Private/UnitTests/CkProceduralAnimation Source/CkTestsEditor/Private/CkProceduralAnimation Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level
```
Review each `clean -n` listing, then rerun it with `-f` and the same pathspecs to delete. Deletion is permanent, and that step is the maintainer's. The editor must be closed: the repo's `PreToolUse` hook blocks `git clean` and `checkout` while an editor holds `Saved/Logs/Mars.log`.

## 10. VALIDATION rows

Evidence log for every automated row unless stated: `Saved/Logs/ProceduralErgonomics-Final-PA.log` (final gate A, 460/455/5).

| ID | Status | Evidence |
|---|---|---|
| B1 | Verified | `Action.Builder.{BodyOnlyDraftBuildsAndCompiles, MultiLimbDraftBuildsAndCompiles}`. The builder lives in the runtime module, with no editor dependency. |
| B2 | Verified | `Action.Builder.{RejectsDuplicatePhaseAndSlot, RejectsOccupiedKeyTime, PoseKeyIsAtomic, ContentLimitsRejectBeforeAllocation}` |
| B3 | Verified (failed-build half); survival of the compiled clip rests on the pre-existing `Action.LoadLifetime` rows (inferred coverage) | `Action.Builder.BuildIntoFailureLeavesDestinationUnchanged`, `DraftFromDataRoundTrips` |
| B4 | Verified for native, AS transient, AS asset and BP VM; Blocked for mounted BP authoring (`[EDITOR-VERIFY]`) | native Builder rows; `Ck_AutoTest_ProceduralAnimation_{ActionBuilderPublicSurface, ActionAssetDefinition}`; `BlueprintParity.{ManagedPlayLoadedVm, MirrorVm}` |
| T1 | Verified | `Action.Progress.OldClipsEvaluateIdentically` |
| T2 | Verified at runtime; editor agreement by construction (the plot calls the same `EvaluateProgress`) | `Action.Progress.{CustomCurveIsMonotoneAndBounded, RejectsReversalDuplicateNaNAndNonLinearEasing}` |
| T3 | Verified | `Action.Progress.SteepCurveCrossesSeveralKeysAndWindowsInOrder` |
| P1 | Verified | `ActionPlayer.Presets.*` ×4; `Ck_AutoTest_ProceduralAnimation_BorrowingSweep` (blocked return reaches `SupportLost / Failed / ReturnProbeAttemptsExhausted`) |
| Q1 | Verified | `Articulation.Query.*` ×4, `ArticulationPublicSurface` |
| D1 | Verified, except the sampling-budget context (Blocked, §8.4) | `Action.Diagnostics.*` ×4 |
| R1 | Verified | `Action.Mirror.{XZUsesYNormal, ReflectsPositionsTangentsBendsAndOrientation, RejectsOccupiedDestinationMissingSourceAndRestNormalized, SourceUnchanged}` |
| R2 | Verified | `Action.Mirror.Runtime.*` ×3, `BlueprintParity.MirrorVm` |
| M1 | Verified | `Action.Managed.NoCallbackBeforePlayReturns`, `InvalidScopeReturnsFailedReportWithoutCallback` |
| M2 | Verified | `Action.Managed.DrainsWithoutListenersAndReturnsStorageToBaseline`; `Performance.Builder.ManagedDispatchSteadyState` (0 slots after release; `Saved/Logs/ProceduralErgonomics-Final-Perf.log`) |
| M3 | Verified | `Action.Managed.RecordsArriveInOrderWithWindowsAndTerminal` |
| M4 | Verified (new contract: reads open, consume sealed) | `Action.Managed.PublicConsumeIsSealedReadsStayObservable` |
| M5 | Verified | `Action.Managed.ListenerReentrancy` |
| M6 | Verified | `Action.Managed.{ScopeLostSuppressesDeadRecipientWhileDrainContinues, PlayerLostFiresOnceFromEndPlay}` |
| M7 | Verified | `Action.Managed.BudgetFairnessAndZeroDeltaT` |
| M8 | Verified | `Action.Managed.LateSubscriptionHasNoReplay` |
| E1 | Blocked (`[EDITOR-VERIFY]` exercise 1); automated proxy green | `MountedToolkit.PoseWorkspace.CaptureAndPlay` (17 routed author actions, P4b), `PoseWorkspace.AdvancedHidesRawDetails` |
| E2 | Verified automated; Blocked for the mounted human pass | `AuthoringCommands.PoseCapture.{CaptureUpdateDuplicateMoveDelete, UndoRedoRestoresTracksAndGroups, SaveReopenKeepsIds}` |
| E3 | Verified | `AuthoringCommands.SolvedCapture.{RejectsStalePartialAndMalformed, ConvertsToDeclaredSpace, AcceptsSatisfiedSeek}` |
| E4 | Verified automated; Blocked for human observation | `PoseWorkspace.CaptureAndPlay`, `MountedToolkit.SharedPreviewRefresh` |
| E5 | **Failed** (inherited `TwoTabsModesAndCapture` red, `FocusedAssetIdentity` red in group); `PieCoexist.IndependentWorlds` green; exercise 4 Blocked | final gate A |
| E6 | Verified | `AuthoringCommands.{RecipeCreationCancelLeavesNoAssets, PairedSaveFailureReportsWhichAsset}`, `PoseCapture.OldClipDerivesTemporaryGroups` |
| E7 | Blocked (needs continuous human observation) | none |
| C1 | Verified | `Mars_AutoTest_CrawlerMelee_{ClipDrivesPhases, OffenseWindowFromReceipt}`; `rg PhaseElapsed Script/ECS/Crawler` = 0 (P6); `Saved/Logs/ProceduralErgonomics-Final-Crawler.log` |
| C2 | Verified | CrawlerMelee 35/35 → 38/38, approved changes reasoned in-test; `Saved/Logs/ProceduralErgonomics-Final-Crawler.log`; PlayerCombat 8/8 (`Saved/Logs/ProceduralErgonomics-Final-PlayerCombat.log`) |
| C3 | Verified | `Ck_AutoTest_ProceduralAnimation_{BuilderSweep, BorrowingSweep, NoncombatManaged}` |
| C4 | Verified | `AuthoringCommands.PoseCapture.CreateFromRecipeMatchesBuilder` (content-hash equality, `Test_ProceduralAction_PoseCapture.cpp:552`) |
| F1 | **Failed** | Focused gates and the measured cost report are done. The Game build failed at link (`Saved/Logs/ProceduralErgonomics-Final-Game.log`) and is AS-on, not AS-off. The cooked route is Blocked. The independent review's defects were fixed and re-gated, but its dismissals are not recorded here. |
