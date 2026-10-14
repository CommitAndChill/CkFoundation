# Progress — procedural actions consolidation

Executor: append a dated session block per session. Never improvise past a STOP; write the blocker
here and end the session.

## Phase 0 — maintainer commit of the working tree (not an executor phase)

CkFoundation, explicit pathspecs, two commits:
1. `feat(procedural-animation-editor): …` — every modified file under
   `Source/CkProceduralAnimationEditor/` plus the untracked
   `Private/Toolkit/CkProceduralAction_EditorToolkit_Actions.cpp`.
2. `fix(ism-renderer): no per-instance previous transforms` — `Source/CkIsmRenderer/Claude.md`,
   `…/Proxy/CkIsmProxy_Processor.cpp`, `…/Renderer/CkIsmRenderer_Processor.h`.
   And `fix(procedural-animation): skip a dead claimed leg when holding claims` —
   `Source/CkProceduralAnimation/Public/CkProceduralAnimation/Articulation/CkProceduralArticulation_Processor.cpp`
   **after** changing the guard to `if (NOT procedural_leg::Get_IsLive(Leg)) { continue; }` (drop the
   `ck::IsValid(Leg) &&`); Phase 1 replaces it.
CkTests: `Source/CkTestsEditor/Private/CkProceduralAnimation/{AuthoringCommands,EditorModes,PieCoexist,PoseWorkspace,SharedPreviewRefresh,AuthoringSession}.cpp`.
Host: `Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_PincerClawSeveredMidStrike.as`, the
regenerated `Script/Generated/Mars_AutoTestActors.as`, `Script/Generated/Mars_EntitySpawnParams.as`,
`Content/Mars/Maps/AutoTests_Mars_MAP.umap`, `Config/DefaultEditorPerProjectUserSettings.ini`, plus the two
gitlinks. Not `Plugins/Monolith`.

Recorded hashes: CkFoundation `f0f143371`, CkTests `2860ddf3`, Orion `1db7570`.

| Phase | Status | Session | Hashes (CkFoundation / CkTests / Orion) | Gate result |
|---|---|---|---|---|
| 0 | done (maintainer) | 2026-10-13 | f0f143371 / 2860ddf3 / 1db7570 | — |
| 1 | done | 2026-10-13 Opus | b244c586f + 20e61a914 / 721eb560 / 75f8fcf | Action 185/185, Articulation 12/12, CrawlerMelee 38/38 |
| 2 | done | 2026-10-13 Opus | see the Phase 2 session block | ProceduralAnimation 453/449 (4 listed), CrawlerMelee 38/38, Crawler 44/44, Jolt.Query 6/6 |
| 3 | done (resave owed) | 2026-10-13 Opus | see the Phase 3 session block | Action.Editor 61/61, CrawlerMelee 38/38 pre-resave, Action 184/184 |
| 4 | done | 2026-10-13 Opus | see the Phase 4 session block | Action.Editor 70/71 (1 env. red, green alone), Action 193/194 (same), CrawlerMelee 38/38 |
| 5 | done | 2026-10-14 Opus | see the Phase 5 session block | ProceduralAnimation 457/462 (3 listed + 2 green alone), CrawlerMelee 38/38 |
| Validation | done (focused) | 2026-10-14 Opus | see the Validation session block | full gate skipped by maintainer ruling |

## Blockers

- **B1 — RESOLVED 2026-10-13 by maintainer:** ignore the ISM renderer change (out of campaign scope); Orion
  pointers committed in `1db7570`. Original record follows.
- **B1 original record (2026-10-13, Phase 1 entry) — Phase 0 incomplete; CkFoundation tree dirty. STOP before Step 1.**
  Observed (Opus executor session, read-only; nothing built, nothing edited but this file):
  - `git status --short` under `Plugins/CkFoundation` prints ` M Source/CkIsmRenderer/Claude.md` and
    `?? docs/campaigns/procedural-actions-consolidation/` (this package). Entry criterion: clean tree.
  - The planned `fix(ism-renderer): no per-instance previous transforms` commit does not exist, and the
    code it was to carry is not in the working tree either: `CkIsmRenderer_Processor.h:130,138` still
    sets `SetHasPerInstancePrevTransforms(false)` / `(true)` by mobility, and
    `CkIsmProxy_Processor.cpp:298` still calls `SetPreviousTransformById`; both files last changed in
    `0ab8e2b73` (2026-10-01). The dirty `Claude.md` describes code that is absent ("false for every
    mobility", "do not call `SetPreviousTransformById`"). Either the processor edits were lost or the
    doc is ahead of an unwritten change — maintainer's call, not the executor's.
  - The claim-fix commit landed as `f0f143371` but keeps `ck::IsValid(Leg) &&` in the guard (Phase 0
    said drop it). Harmless: Step 2 deletes the block.
  - Phase-0 hashes are not recorded above (`____`), so the "`git log -1` is the Phase-0 commit"
    criterion cannot be checked. Current tips: CkFoundation `f0f143371`, CkTests `2860ddf3` (clean),
    both on `feature/procedural-anim-ext` and ahead of their upstreams (unpushed). Orion `5047689`
    records gitlinks CkFoundation `20f8d81f7` / CkTests `3d94cf15` — the two gitlink bumps of Phase 0
    are not committed. Host row `PincerClawSeveredMidStrike` is committed (`722e41e`).
  - Baseline gate not run (it follows the clean-tree check).
  To unblock: resolve the ISM commit (restore the processor edits and commit with the doc, or revert
  the doc), commit the two Orion gitlinks, fill in the Phase-0 hashes and set row 0 to done. Then
  re-run Phase 1 from Entry criteria.

- **B2 — RESOLVED 2026-10-13 by Fable ruling (Option A):** the host rule (Strike end key translation-only,
  `Mars_Crawler_Attack.as:155-201`) and the framework continuity rule test different things; `Make_DraftWithRotatedStrike`
  now also yaws Recover's time-0 body key so the derived clip stays continuous and still breaks the host rule.
  `CrawlerMelee` re-run on the final binary: 38/38, exit 0. Original record follows.
- **B2 original record (2026-10-13, Phase 1 exit gate) — `CrawlerMelee` 36/38: the new `PhaseBoundary` compile rule rejects a
  clip the host tests derive on purpose. STOP (gate branch not enumerated; host-test-vs-rule design call).**
  - Failing rows (final binary, `--test --no-live --test-pattern CrawlerMelee`):
    `Mars_AutoTest_CrawlerMelee_InvalidSpec` and `Mars_AutoTest_CrawlerMelee_PincerKeyValidation`, both
    `the derived clip compiles (got [PhaseBoundary at [Phases[2].BodyKeys] ...])`.
  - Cause: `Make_DraftWithRotatedStrike` (`Script/Tests/CrawlerMelee/Mars_AutoTestRig_CrawlerMelee.as:961`)
    yaws only the Strike phase's last body key by 30 degrees, so the next phase starts from a different body
    pose. The tests build that clip with `Build_DerivedClip`, which asserts it compiles, and then expect the
    host's own `utils_crawler::Validate_AttackClip` to reject it. Step 5's rule now rejects it at compile.
    The shipped reference clips compile (the same rows assert that and pass).
  - Options: (A, recommended) host test keeps the derived clip continuous: also yaw the following phase's
    time-0 body key, or drop that key so the phase starts from the carried pose; the host rule is still the one
    under test, and the framework rule stays as planned. (B) The test instead asserts the compile rejects the
    rotated strike with `PhaseBoundary`, and the host rule loses this case. (C) Narrow the framework rule (e.g.
    body translation only). A and B are host-only edits; C changes Step 5.
  - The other 36 CrawlerMelee rows pass, including `PincerClawSeveredMidStrike`.

## Open rulings for the maintainer (not blocking)

- `FCk_Handle_ProceduralLeg` vs `FCk_Handle_ProceduralLimb`: two typesafe handles for a gait leg and a
  layout appendage. Collapse or keep?
- Campaign docs under `docs/campaigns/` name the host project (71 pre-existing files do the same).
  Convention ruling, not a branch fault.
- (2026-10-13, Phase 2: it passed in the `ProceduralAnimation` gate, lane 6.) `Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal` is red on this branch with all branch runtime
  changes backed out and is not on the known-reds list; confirm it is red on `origin/dev` and list it
  from CkPlugins by PR, or treat as a regression to chase.
- Distances/angles stay `float` + unit suffix (`_MaxEndpointTravelCm`): no house type exists; add one
  or accept.

## Session 2026-10-13 — Phase 1 (Opus executor)

- Loaded: `ck-change-control`, `ck-tests-authoring-and-running`, `/build-test`, `ck-macros-and-codegen`,
  `ckecs-architecture-contract`; read PROMPT.md, PHASE_1.md, this file.
- Entry criteria: **failed** at the first check (CkFoundation tree dirty, Phase 0 incomplete) → STOP,
  recorded as B1 under Blockers.
- Not done: baseline gate, Step 1 red tests, Steps 2–5, exit gates. No source, test, or build change;
  no toolbox invocation; no commit. The only edit is this file.
- Next session: once B1 is cleared, start Phase 1 from Entry criteria (the baseline gate is part of it).

## Session 2026-10-13 (resumed after B1) — Phase 1 (Opus executor)

Stakes: medium-blast, reversible (runtime module + tests, nothing committed).

**Baseline** (entry, `--build --test --discover-fresh --test-pattern ProceduralAnimation.Action`, CkFoundation
`f0f143371`, toolbox v1.58): Total 177 / Passed 177 / Failed 0, no NEW failures.

**Gate 1 (red first).** All eight rows red on the pre-fix binary, each on its intended assertion:
Lifecycle 7/7 failed (`--build --test --discover-fresh --test-pattern ProceduralAnimation.Action.Lifecycle`),
`Compile.PhaseBoundaryDiscontinuityRejected` failed (`--test --no-live --test-pattern Compile.PhaseBoundary`).
`Request_ClearFault` was added as a stub that completed `Failed` so its row could compile and run red; Step 3
replaced it. `DetachedClaimedLegReleasesItsClaim` was later re-proven red by A/B in its final two-leg form
(fix disabled, old `HoldClaimedChannels` guard restored): fails on `A destroyed claimed leg leaves the rig
operational`; green with the fix.

**Exit gates on the final binary** (rebuilt after the A/B restore):
- `ProceduralAnimation.Action`: 185 / 185 / 0, exit 0 (baseline 177 + 8 new). Editor boots (no scheduler cycle
  from the new Service `RunAfter` edge).
- `ProceduralAnimation.Articulation`: 12 / 12 / 0, exit 0.
- `CrawlerMelee`: 38 / 36 / 2 → **B2**.
- Greps: `ReleaseDeadClaims` 3 hits; `Faulted;` in the articulation processor 13 before and after (PHASE_1 says
  10; HEAD already had 13; the criterion is "unchanged"); `Clear()` in the fragment header 1;
  `Delivery::Explicit;` in `CkProceduralAction_Fragment_Data.h` 0; `UE_LOG` in the module 0.
- Comment audit done: added comments are why/contract only, no campaign names.

**Changes (uncommitted).** CkFoundation: claims reconcile against limb liveness (`_LostLimbOwners`,
`ReleaseDeadClaims`, `LimbUnavailable` interruption, working-tree guard deleted); transient faults re-arm;
`FCk_Request_ProceduralArticulation_ClearFault` + `Request_ClearFault` (immediate mutator); capacity ensure;
`Auto` default delivery; unacknowledged terminal streams expire after 5 s with a Warning; the three forbidden ensure
shapes fixed; `UE_LOG` → `procedural_animation::Display`; Service runs after `FProcessor_ProceduralRig_Update`;
player EndPlay finishes live actions `SubjectLost`; repeat cancel/release complete `Succeeded`; `PhaseBoundary`
compile diagnostic; 121 same-line `if` bodies split. Editor: the solver world pins `Explicit` (it consumes
receipts). CkTests: `Test_ProceduralAction_Lifecycle.cpp` (7 rows), the PhaseBoundary row, a generic
`OnRequestCompleted` on the reentry listener, and `Explicit` pinned in the rows that read receipts or results
after the finish (Clip ×6 sites, ClockCadence, ActionDebugger, BlueprintParity VM, AS BuilderPublicSurface and
LoadedClipPublicSurface); PlaySpecAccessors and the AS default assertion now expect `Auto`.

**Decisions made, not asked (veto cheaply):**
1. `ReleaseClaims` skips a channel whose claim was already released as dead, only for a token in
   `_LostLimbOwners`. Without it the release returns `ClaimLost`, never clears the lost token, and leaks the
   token's live claims.
2. ClearFault row: the fault comes from destroying a bound segment, then the owning leg is destroyed so the binding
   is dead before `Request_ClearFault`. PHASE_1's arrange (segment only) cannot satisfy "next publish does not
   re-fault", because a live leg with a dead segment faults `InvalidBinding` on every update.
3. DetachedClaimedLeg and ClearFault rows bind Leg0 and Leg1. With only Leg0 rigged, the articulation has no
   chain left to publish once Leg0 dies (`CkProceduralRig_Processor.cpp:893`), so the "keeps publishing" assertion
   could never hold.
4. DetachedClaimedLeg uses a 0.25 s return timeout: with the dead limb's claim released, the return never
   captures that limb and ends at its deadline; a short deadline keeps "within 60 frames" meaningful.
5. PlayerEndPlay row: the scope is a separate transient entity, not a child of the player's entity, so the action
   outlives the player. EndPlay uses `Recovery::NotRequired` (the enum has no "not attempted").
6. ExplicitTerminal row fills the retained-stream budget with four actions bound to a missing limb (they finish
   `Rejected` as created) instead of playing four clips.
7. PhaseBoundary body diagnostics carry field `Phases[N].BodyKeys` and no slot, like every other body diagnostic.

**Follow-ups (not done):** an expired Explicit stream leaves its Finished action entity alive, since the service
only destroys actions whose terminal was acknowledged (leak, not a stall). A transient fault still interrupts actions
in the frame between Observe and the next Update (`ArticulationFaulted`).

**Logs** (copied to the executor's scratchpad, not the repo): baseline, red Lifecycle, red PhaseBoundary, A/B,
final Action/Articulation/CrawlerMelee.

**Next session:** maintainer rules on B2. Then re-run the `CrawlerMelee` gate (and `ProceduralAnimation.Action`
if the rule changes), commit Phase 1 with explicit pathspecs, and start Phase 2.

**Phase 1 commits (2026-10-13):** CkFoundation `b244c586f` (fix) + `20e61a914` (campaign docs), CkTests `721eb560`,
Orion `75f8fcf` (host rig fix + gitlinks).

## Session 2026-10-13 — Phase 2 (Opus executor)

Stakes: medium-blast, reversible (public API reshaped across runtime, editor, tests and host; nothing pushed).

**Baseline** (entry, CkFoundation `20e61a914`, toolbox v1.58): `--build --test --discover-fresh --test-pattern
ProceduralAnimation` 453 / 450 / 3 failed, all listed (`SurfaceMotion.SnapshotShowsThePendingContact`,
`PillarCrossingRenderedTraversal`, `PostsCentipedeReturns`); `Crawler` 44/44; `Jolt.Query` 6/6; `CrawlerMelee` 38/38
(Phase 1 exit).

**Rulings taken from a Fable adjudicator (the maintainer delegated design questions to it this session):**
- Q1: `ResolveClipSampleTime` / `FindReturnPrefix` stay public in `Core/`. Step 4 mislocated them ("Action_Utils"); they are
  header-only pure functions with their own unit rows. Step 4 line dropped.
- Q2: `Add_BodyKey` / `Add_LimbKey` / `Add_Marker` cut; rows that used them port to `Add_PoseKey` (CarriedStart mirrored
  track, Builder occupied-time/unknown-phase/nonfinite rows) rather than being deleted. `Add_PoseKey` stays (editor + host).
- Q3: `Gait::Add(Legs, Data)` takes every leg the body has (owner = the legs' common lifetime owner; a subset, a
  duplicate or two owners is rejected).
- Q4: RecoveryPie participation-state reads removed with the enum.
- Q5 (for Phase 3): the clip resave stays with the maintainer; the executor's gate is the in-memory PostLoad migration.
- Q6: the crawler stores the walker's articulation handle on `FMars_Fragment_Crawler` (state, not Params) with a
  `Get_Articulation` mixin; the three `DoCastChecked` sites use it.

**Changes (uncommitted at the time of writing; committed below).**
- `FCk_ProceduralWalker_Spec` (rig, gait, chains, optional body pose + spec, optional action player + spec, enum-mode
  enables); `Add_Walker(Body, Spec)` validates both optional specs before the first mutation and returns the walker with
  articulation, rigs, body pose and player handles. 27 AS and every C++ call site moved to the Spec.
- `Gait::Add(Legs, Data)`; 13 AS + C++ sites pass `Get_Legs(Body)`.
- The three control headers moved `Private/` → `Public/`; `PrivateIncludePaths` gone from the module and from
  `CkTests.Build.cs`; `Private/` deleted. `LimbControl_Control::Get_IsLocomotionValid`; `ActionPlayer::Add` ensures on
  invalid locomotion and on a failed Attach.
- Cuts (each re-grepped to zero consumers): `TryTransferClaims`, `GetControlState` + `FCk_ArticulationControlState`,
  `RequestInterrupt`, `ECk_ProceduralGaitParticipation_State`, `Outcome::PartLost`, `ReportStatus::ContentNotResident`,
  `_MaxTimeBacklog`, `ValidationReport._Advisory`, builder `Add_BodyKey`/`Add_LimbKey`/`Add_Marker`, the
  `ComputeCatmullRomTangents` export, receipt `_IntermediateSamples` + `ECk_ProceduralAction_SampleEvidence` +
  `MaxSamplesPerReceipt` (now `receipt_limits::MaxSamplingSteps`, the service's per-interval step bound).
  `Get_ReattachmentSettledRevision` kept: RecoveryPie reads it.
- `FailureContext`: `ECk_ProceduralAction_FailureMeasurement {None, Deadline, Budget}` + `Set_Deadline` /
  `Set_BudgetExcess`; value fields getter-only. `RecordedStore` budget check → `CK_ENSURE_IF_NOT` (records nothing).
- Docs: module `Claude.md` (Procedural actions section folded in from the untracked `PROCEDURAL_ACTIONS.md`, which was
  moved off the tree), `EDITOR_MODULES.md` row, `Source/CLAUDE.md` tier deps + T5 count, `CkEcs/Claude.md` production
  preview worlds.
- Host: `Mars_Crawler_Utils.as` composes surface motion → `Add_Walker(Spec)` (body pose + armed player) → monster;
  `Mars_Crawler_Feature.as` gains `Articulation`; `Mars_Crawler_ActionAdapter.as` reads `Get_Articulation()`.
- Compile fixes after the first build: `RecordedStore` budget member no longer `const`; the editor solver passes
  non-const rig/gait pointers into the Spec; three ReceiptStore rows and seven RecoveryPie diagnostics dropped the cut
  fields.

**Exit gates on the final binary:**
- `--build --test --discover-fresh --test-pattern ProceduralAnimation`: 453 / 449 / 4, exit 0, every failure listed.
  Against the baseline names: +`Gait.LandingProbeLiftsASwingOntoAStep` (listed `red`, documented load-sensitive 3 s
  WaitUntil). Run alone (`--test --no-live --test-pattern LandingProbeLiftsASwingOntoAStep`): 1/1 green, "Now passing".
  Flake, not this change.
- `--test --no-live`: `CrawlerMelee` 38/38, `Crawler` 44/44, `Jolt.Query` 6/6, all exit 0.
- Greps: `Private` in `CkProceduralAnimation.Build.cs` → 1 hit, `PrivateDependencyModuleNames.Add("AnimationCore")` (no
  include path); `Source/CkProceduralAnimation/Private` absent; every `Add_Walker(` passes a Spec (remaining hits are
  unrelated `DoAdd_Walker` test helpers); `_IntermediateSamples|SampleEvidence` → only an unrelated SceneNodeTween test
  member.
- AS: `Plugins/CkFoundation/Script/Generated/utils_procedural_animation.as` binds `Add_Walker(Body, const
  FCk_ProceduralWalker_Spec&)`; `utils_procedural_articulation.as` has `Request_ClearFault` (both gitignored, generated).
  Host crawler scripts compile (the CrawlerMelee/Crawler lanes boot them).
- `[EDITOR-VERIFY]`: Blueprint `Add Walker` node shows one Spec pin (VALIDATION.md).
- Comment audit done.

**Follow-ups:** `_PendingLastIntermediatePose` keeps its name although intermediate samples are gone (it is the last
accepted candidate before the published one); rename candidate for Phase 5. An untracked sibling package
`docs/campaigns/procedural-action-events/` (planning only, another session) plans against the editor seams Phases 3–4
change; left untouched.

**Phase 2 commits (2026-10-13):** CkFoundation `1b73417c7`, CkTests `23d46db7`, Orion `7dc732e`.

## Session 2026-10-13 — Phase 3 (Opus executor)

Stakes: medium-blast, reversible (asset layout + editor module + host editor script; the shipped clips migrate in memory
until the maintainer resaves them).

**Baseline** (entry, Phase 2 binary): `--test --no-live --test-pattern ProceduralAnimation.Action.Editor` 62 rows, 61 green;
`ManagedClip.BodyPulse` red in the lane on `LogAssetRegistry: Failed to open file for write
../../Intermediate/CachedAssetRegistry_0.bin.tmp` (lanes contending on the cache file) and green alone
(`--test-pattern ManagedClip.BodyPulse` 1/1). `CrawlerMelee.ShippedClipsLoad` 1/1.

**Rulings taken from the Fable adjudicator (design gaps PHASE_3 left against the current code):**
- P3-Q1: the legacy types (`FCk_ProceduralAction_EditorSubject`, `FCk_ProceduralAction_PoseGroup`, the provenance enum) stay
  in the runtime header as load-only types (plain `UPROPERTY()`, no Blueprint surface, no `AllowedClasses`) so the
  `_DEPRECATED` fields can load; deleted next compatibility version. The editor defines its own
  `FCk_ProceduralActionEditor_Subject` (scenario typed as `UCk_ProceduralActionPreview_Config`) and
  `FCk_ProceduralActionEditor_PoseLabel`. Migration: runtime `PostLoad` executes
  `UCk_ProceduralAction_Data::OnMigrateEditorOnlyData`, bound by the editor module to
  `UCk_Utils_ProceduralActionEditor_UE::MigrateLegacyEditorData`; unbound, the legacy data and version stay. Lazy migration
  in `Get_EditorData` was rejected: a deprecated property is load-only, so any resave before the toolkit opened the clip
  would drop the subject.
- P3-Q2: `_Recorded` dropped from the sidecar: `FCk_ProceduralActionRecordedStore` is a non-reflected solver cache owned by
  the solver world (20f07e8ee), never asset data. It stays in the runtime module; moving it (and the `RecordedGap` row) to
  the editor is a Phase 4 candidate.
- P3-Q3: pose provenance dropped (no field, assertions deleted); the session staging API and the GUID pose commands
  (`CapturePose/UpdatePose/DuplicatePose/MovePose/DeletePose/RenamePose`) deleted, `CaptureSolvedPose` records through
  `RecordPose`; the host seeding script outers its subject rig and gait to the editor data; every commit that touches the
  editor data `Modify()`s it.

**Step 1 (red first).** The three rows fail on the intended assertions on a binary built with three temporary sabotages
(migration delegate unbound, `Get_EditorData` not storing the object, `RecordPose` not adding the label):
`EditorDataLivesInSidecar` ("the editor data saved with the clip" null), `PostLoadMigratesLegacyEditorFields` (legacy
fields not empty, version 2), `Poses.RecordWritesKeysAndOneLabel` (0 labels). Fixture preconditions held. Sabotages
reverted (`rg RED-PROOF` → 0) and the binary rebuilt before the exit gates.

**Changes.**
- Runtime asset: implements `IInterface_AssetUserData` over the editor-only `_AssetUserDataEditorOnly` (Instanced);
  `_EditorSubject_DEPRECATED`, `_EditorPoseGroups_DEPRECATED`; `_CompatibilityVersion` `VisibleAnywhere` + getter only;
  version 3; the asset's editor accessors replaced by `Get_HasLegacyEditorData` / `Get_LegacyEditor*` /
  `Clear_LegacyEditorData`. No editor-module name left in the runtime module.
- Editor: `CkProceduralActionEditorData.{h,cpp}` (sidecar, labels, subject; utils `Get_EditorData` / `TryGet_EditorData` /
  `MigrateLegacyEditorData`; `Get_BodyChannelName`, `Get_ChannelsKeyedAt`). Commands keep subject + labels in
  `FEditorMetadata`; `CommitDraft` drops labels with no key left and writes the sidecar inside the transaction;
  `RetimeLabels` (a label follows a moved key only when no other channel keeps the old time and no label holds the new
  one); `RecordPose`, `RenameLabel`, `SetSubject`. PoseGroups layer deleted; session staging/pose-group API deleted; the
  model loses the pose GUID selection (Record selects the recorded key); the timeline's Poses row is gone and labels draw
  beside the first keyed channel at their time; Details' pose name renames the label. The legacy-preview pairing outers its
  copied rig/gait to the editor data too.
- Tests: 3 new rows; ported `UndoRedoRestoresTracksAndLabels` (renamed from `...TracksAndGroups`), `CreateFromRecipe`,
  `RenamePhaseUpdatesReferences` (now seeds a real key + label), `KeyTime`, `GuidedCreation`, `SolvedCapture` x3,
  `PoseWorkspace` x2, `PreviewSubject` (rig outered to the editor data); deleted the group-bookkeeping rows
  `CaptureUpdateDuplicateMoveDelete`, `SaveReopenKeepsIds` (its save/reopen coverage moved into `EditorDataLivesInSidecar`),
  `OldClipDerivesTemporaryGroups`, `StagingIsSessionOwned`. The builder version row sets the old version through reflection.
- Host: `Script/Editor/Mars_CrawlerAttackClips.as` seeds through `utils_procedural_action_editor::Get_EditorData`.
- Docs: `EDITOR_MODULES.md` row names the editor data and the migration.

**Exit gates on the final binary:**
- `--build --test --discover-fresh --test-pattern ProceduralAnimation.Action.Editor`: 61 / 61, exit 0
  (62 − 4 deleted + 3 new = 61).
- `--test --no-live`: `CrawlerMelee` 38/38 (**pre-resave**: the shipped clips migrate in memory); `ProceduralAnimation.Action`
  184/184 (Phase 1's 185 − 4 + 3).
- Greps: `CkProceduralAnimationEditor` in `Source/CkProceduralAnimation` → 0; the editor-only fields block holds only the
  user-data array and the two `_DEPRECATED` fields (two more `WITH_EDITORONLY_DATA` blocks hold the migration delegate and
  the legacy accessors); `CkProceduralActionPoseGroups.*` absent.
- AS: `Plugins/CkFoundation/Script/Generated/utils_procedural_action_editor.as` binds `Get_EditorData` / `TryGet_EditorData`;
  the host seeding script compiles.
- Comment audit done.

**Decisions made, not asked (veto cheaply):**
1. `WritePoseKeys` keeps the tangents of keys it rewrites (PHASE_3 listed `ReadTangents/WriteTangents` for deletion); they
   are now keyed on (phase, time) instead of a group. Deleting them would flatten a smooth key on every gizmo re-key.
2. A label at a time no channel keys any more is dropped on every commit (the label analogue of "a key a pose loses leaves
   that pose").
3. `RenameLabel` on an unlabelled time is refused (as `RenamePose` refused an unnamed key).
4. The version bump does not gate the migration: a property equal to its archetype default is not serialized, so a clip
   saved at version 2 loads as the new default. Migration keys on the legacy fields being non-empty.
5. The toolkit refreshes when its clip's editor data (a subobject of the clip) is edited in Details.

**`[EDITOR-VERIFY]` resave owed (maintainer):** open the four clips under `Content/Mars/Gameplay/Monsters/Crawler/Actions/`,
confirm the subject under the clip's editor data and labels beside the keys, save; then `--test-pattern
CrawlerMelee.ShippedClipsLoad`. `.uasset` changes are the maintainer's to commit. BusterBlock clips get the same treatment
when that project bumps its pin.

**Follow-ups:** clips migrated from older data keep their subject rig/gait subobjects outered to the clip itself
(re-outering during load was not attempted); `RecordedStore` to the editor module (Phase 4 candidate).

**Phase 3 commits (2026-10-13):** CkFoundation `be6ecd665`, CkTests `c14c83b1`, Orion `6c06a1f`.

## Session 2026-10-13 — Phase 4 (Opus executor)

Stakes: low-to-medium blast, reversible (editor module and its tests only; no runtime change).

**Baseline** (entry = Phase 3 final run): `ProceduralAnimation.Action.Editor` 61/61.

**Step 1 (red first).** Twelve rows in `Test_ProceduralAction_EditorConsolidation.cpp` (shared mounted fixture in
`CkTest_ProceduralActionToolkitFixture.h`), run against the unchanged editor (`--build --test --test-pattern
ProceduralAnimation.Action.Editor`, 73 rows): five red on the intended assertions —
`Refresh.OneSolverBuildPerEdit` (2 Solver worlds for one edit; its first counter hooked `OnPostWorldCreation`, which fires in
the `UWorld` constructor before the world is named, and read 0 — re-proven with an `OnPostWorldInitialization` counter on
marked preview worlds, and on a key that no phase boundary pins), `Time.OnePlayhead` (the session's own playhead stayed at 0
while the model was at 0.25), `Undo.RenameSectionKeepsSelection` (undo selected the first section), `Commands.DeleteKeyReportsAMissingKey`,
`Curves.RejectsDuplicateKeyTimes`. Green on entry, kept as behaviour coverage: `Gizmo.RotateWritesOrientation`,
`Gizmo.BendWritesBend`, `Details.ReachPolicyWritesTheTrack`, `Commands.RemoveEventRemovesMarkersAndWindows`,
`Commands.SetPhaseEasingAndUndo`, `Commands.MoveWindowKeepsItsLength`, `Undo.SubjectChangeRefreshesAppearance` (green on
entry: the old double refresh happened to rebuild the appearance; the row now pins it through the single path).

**Rulings taken from the Fable adjudicator:**
- P4-Q1: the `<= 9,500` line gate is informational. Recorded: 10,565 (from 11,011 at entry; the estimate was made against the
  pre-Phase-3 tree; every cut PHASE_4 names was executed).
- P4-Q2: `IsNormalized` / `IsLater` / `HasEventId` stay: `FCk_FloatRange_0to1` clamps command input before the validator
  sees it, and a validator refusal inside `Write_Into` is an ensure plus a cancelled transaction, not a user-facing refusal.

**Changes.**
- One refresh path: the 14 synchronous `NotifyContentChanged()` calls are gone (toolkit actions, viewport, timeline,
  channels, the curves callback, which the adapter no longer takes); 3 remain (the model's declaration and definition, and
  the toolkit's ticker funnel). The session's revision changes only when the compiled content hash, the subject or the
  preview config changes.
- One playhead: the session has no `_Time`/`_Playing`/`SetTime`/`Advance`/`SetPlaying`/`GetPose`; `EvaluateAt` is its only
  evaluation, the viewport evaluates at the model's time, and authored playback advances the model in the toolkit.
- One converter: `ToRawFraction` and `SnapToFrame` (+ `FramesPerSecond`) on the session replace the toolkit's, timeline's
  and transport's own conversions and frame rounding.
- Curve writes: `SetBodyKeys`/`SetPositionKeys` refuse a phase the compiler rejects with `KeyTime` (PHASE_4 said `KeyOrder`;
  the compiler's code is `KeyTime`).
- Commands: `AddLimbTrack`, `AddBodyKey`, `AddPositionKey`, `SetPositionKey`, `SetBendKey` deleted; `DeleteKey` refuses a
  missing key; `RemoveSlot` (Channels Delete key and slot context menu), `SetSlotSpace` (slot context menu) and
  `ReorderPhase` (timeline section context menu "Move section earlier/later") are wired.
- Undo: a refresh that finds a different subject than the preview's meshes were built for rebuilds the appearance (covers
  undo, redo and Details); a selected section that vanished (rename or its undo) re-selects what now sits at its index.
- Legacy preview pairing (`FindLegacyPreview`, `MakeSubjectFromLegacyPreview`, `Get_PreviewSiblingPackageName`) deleted with
  its toolkit hook; guided creation folded into the factory (the creation choice struct now lives in
  `CkProceduralAction_Factory.h`; the test-only `SaveAssets`/`DescribeSaveResults` deleted).
- Sweep: 17 same-line `if` bodies split; `FCk_ProceduralActionEditorModes`, `FCk_ProceduralAction_CurveAdapter`,
  `FCk_ProceduralActionViewportClient`; Session/Modes/Solver getters renamed `Get_X` (`Get_CanPlay`, `Get_IsCompiled`, ...);
  the proxy's seconds are `FCk_Time`; the SolverWorld header comment reworded; `DestroyWorld` guards `GEngine` and shuts the
  game instance down on the failed-create path.
- Tests: deleted `PreviewSubject.LegacyPreviewBecomesTheSubject` and `AuthoringCommands.PairedSaveFailureReportsWhichAsset`
  with their code; ported `AuthoringSession` (EvaluateAt), `AuthoringCommands` x3 (SetPoseKeys; GizmoKeys passes a unit bend,
  as the gizmo does), `EditorModes` (a real source change restarts the Solver; a no-op refresh keeps the revision),
  `MountedToolkit`, `CurveAdapter`.

**Exit gates on the final binary:**
- `--build --test --discover-fresh --test-pattern ProceduralAnimation.Action.Editor`: 71 rows (61 + 12 − 2), 70 green.
  The one red, `SolvedCapture.AcceptsSatisfiedSeek`, failed in two lane runs on `LogAssetRegistry: Error: Failed to open file
  for write ../../Intermediate/CachedAssetRegistry_0.bin.tmp` and nothing else, and is green alone
  (`--test-pattern SolvedCapture.AcceptsSatisfiedSeek` 1/1). Same environmental cause as the Phase 3 baseline's
  `ManagedClip.BodyPulse`: a row that happens to run when a lane writes the asset-registry cache fails.
- `--test --no-live`: `ProceduralAnimation.Action` 193/194 (the same row, same cause); `CrawlerMelee` 38/38.
- Greps: `NotifyContentChanged()` 3 (<= 4); no `_Playing`/`_Time;` member in the session header; 0 same-line `if` bodies;
  module 10,565 lines (gate informational, P4-Q1).
- Comment audit done. The two new test files were normalized to CRLF after the final build (line endings only).

**Decisions made, not asked (veto cheaply):**
1. `OneSolverBuildPerEdit` drives a Details key edit, not a gizmo release: the gizmo is disabled while a solved frame is shown.
2. "Window body drag" is pinned at the command the timeline's body drag calls (`MoveWindow`); the Slate drag is not simulated.
3. Section reorder is a context-menu action: a section-body drag already scrubs the playhead.
4. `SetScenario` commits the subject only; the ticker's subject check rebuilds the appearance once.
5. The factory creates the clip directly; the null-parent notification of the folded helper is gone (a factory always has a
   parent).

**Follow-ups:** the asset-registry cache write error that fails whichever row runs at that moment (not a test defect; an
`Intermediate/CachedAssetRegistry_*.bin.tmp` contention between lanes); `RecordedStore` to the editor module (P3-Q2).
`[EDITOR-VERIFY]` items for Phase 4 are VALIDATION.md 2-4.

**Phase 4 commits (2026-10-13):** CkFoundation `bb506462d`, CkTests `93fea5ec`, Orion `2ecdc8c`.

## Session 2026-10-14 — Phase 5 (Opus executor)

Stakes: medium blast, reversible (runtime module, behaviour-preserving; the existing suite is the spec).

**Baseline** (Phase-4 binary, `--test --no-live --test-pattern ProceduralAnimation`): 462 rows, 458 green, 4 red —
`SurfaceMotion.SnapshotShowsThePendingContact`, `PillarCrossingRenderedTraversal`, `PostsCentipedeReturns` (all listed) and
`WalkQualityOnHump` (not listed; red on the Phase-4 binary, before any Phase 5 change). `Gait.LandingProbeLiftsASwingOntoAStep`
(listed) passed; `PillarsEngageSteppedTops` and `SpiderInteractiveRosterDiagnostics` (listed flaky) passed.

**Step 1 — service split.**
- `FProcessor_ProceduralActionPlayer_Service::ForEachEntity` runs `FTechnique_ProceduralActionPlayer_Service` over
  `FContext_ProceduralActionPlayer_Service` (`ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h`). Steps, in the old
  body's order: `DrainRecoveries`, `SnapshotActions`, `LoadClips`, `SkipIdle`, `CloseForReattachment`, `ServiceActions`,
  `MeasureOtherLimits`, `StageReturns`, `StageClips`, `DispatchKeys`. Every early `return` of the old body is a step's `Abort`.
- Files: `…_Service.cpp` 235 lines, `…_Service_Admission.cpp` 357, `…_Service_Planning.cpp` 432, `…_Service_Staging.cpp` 445.
  `BindCompiledClip`, `CaptureClipInitial` and their two helpers moved to a new `CkProceduralActionPlayer_ClipBinding.cpp`
  beside the `ClipBinding.h` that already declared them (Action utils and a CkTests row call them).
- `FFragment_ProceduralAction::Get_Token(Params)` replaces the eight hand-built identity tokens (Service x4, Action processor
  x2, Observe, owner-loss reconcile). `rg "FCk_ArticulationControlToken\{[^}]"` → 2: the helper and the limb-control
  recovery token (`LimbControl_Control.cpp:305`, a root-owned token, not an action's).
- Stall reasons: each staging-pipeline `return` that publishes nothing records why on every Acquiring/Playing action
  (`_LastStall`: `ReturnBudgetUnavailable`, `CandidateOwnerMissing`, `ArticulationBusy`, `ReattachmentHoldRejected`,
  `ReturnPrefixInterrupted`, `ReattachmentRequested`, `ReturnPrefixRejected`, `PredictionMismatch`, `StageRejected`);
  a settled publication clears it (Observe). The publication watchdog copies it into
  `FCk_ProceduralAction_FailureContext::_StallReason`, and `Get_ResultText` appends `; Stalled on: <reason>`.

**Step 2 — pose fragment split + IsRigid.**
- `FFragment_ProceduralArticulation_Pose` (issued/settled/expected, `_PredictedSubjectPose`, `_SettledSubjectPose`),
  `FFragment_ProceduralArticulation_LegacyCandidate` (`_LegacyWalker`, the legacy handles/poses/presentation),
  `FFragment_ProceduralArticulation_ControlCandidate` (`_HasControlCandidate`, `_ControlHandles`, `_ControlPoses`); both
  `Add` sites add all three; the Update view takes all three, Observe takes the legacy one read-only, the rig and body pose
  write only the legacy one (plus the predicted pose).
- One `ck::procedural_animation::IsRigid(const FTransform&, double InRotationTolerance, double InScaleTolerance = 1e-6)` in
  `Core/CkProceduralRigidTransform.h` replaces eight definitions (Builder, ReceiptStore, Evaluation, Validation (unused),
  the Motion.h lambda, Service, Articulation Control/Utils). Each site keeps its old tolerances:
  `AuthoredRotationTolerance` (1e-4) for authored/receipt frames, `ComposedRotationTolerance` (`UE_THRESH_QUAT_NORMALIZED`,
  0.01 — what `IsNormalized()` tested) for runtime frames; Articulation Control keeps its `UE_KINDA_SMALL_NUMBER` scale tolerance.

**Decisions made, not asked (veto cheaply):**
1. `ServiceActions` is one step that runs six per-action helpers on each action in turn. PHASE_5's separate
   `Interrupt`/`PlanCandidates` steps would run interruption for every action before any admission, which reorders claim
   acquisition against return; the per-action order of the old body is kept.
2. `Get_Token` takes the action's Params: the identity lives there, not on the state fragment.
3. `_PredictedSubjectPose` stays on `_Pose`, not `_ControlCandidate`: the legacy rig and body pose write it too, and the
   articulation update reads it for both candidate kinds.
4. `IsRigid` takes a second (scale) tolerance with a default: Articulation Control tested scale at 1e-4, everything else at
   1e-6; one tolerance would have changed which frames that site accepts.
5. `ClipBinding.cpp` is a fifth file: the two exported functions had lived in the service TU only by history.

**Exit gates on the final binary:**
- `--build --test --discover-fresh --test-pattern ProceduralAnimation`: 462 rows, 457 green. Against the baseline names:
  the three listed reds are unchanged; `WalkQualityOnHump` (red in the baseline) passed; two rows went red that are green alone —
  `Action.Editor.PreviewWorld.ProductionSettlementAndJolt` failed only on `LogAssetRegistry: Error: Failed to open file for write
  ../../Intermediate/CachedAssetRegistry_0.bin.tmp` (the lane contention from Phases 3-4; 1/1 alone), and the listed-flaky
  `SpiderInteractiveRosterDiagnostics` ("0 traversals" in the lane; 1/1 alone, twice). Read as flake, not regression: both are
  green alone on the same binary, and the walker traversal rows that exercise the split legacy candidate (Crawler courses,
  `PillarsEngageSteppedTops`, `WalkQualityOnHump`) passed in the same run.
- `--test --no-live --test-pattern CrawlerMelee`: 38/38.
- Greps: `wc -l …_Service*.cpp` 235 / 357 / 432 / 445; `rg "FCk_ArticulationControlToken\{[^}]"` 2 (the helper and the
  limb-control recovery token; the PHASE_5 grep without `[^}]` also counts 14 empty-token comparisons in the articulation);
  one `IsRigid` definition (`Core/CkProceduralRigidTransform.h`).
- Comment audit done; new and edited files are CRLF.

**Follow-ups:** the asset-registry cache contention (now three rows across Phases 3-5); a unit row for `Get_ResultText`'s
`Stalled on:` suffix (left out to keep this phase's row names identical to its baseline); `WalkQualityOnHump` red on the
Phase-4 baseline and green after, unlisted either way: run it alone twice before listing it `flaky` in CkTests.

**Phase 5 commits (2026-10-14):** CkFoundation `7dc12adc5`, Orion `e307a24` (CkTests unchanged).

## Session 2026-10-14 — Validation (Opus executor)

**Final gate: not run, by maintainer ruling.** The full `--test --no-live --discover-fresh` gate was started and stopped
after a few minutes on the maintainer's instruction: "If everything passed then dont do a full suite." Validation stands on
the focused patterns run on the final binary (Phase 5 build):
- `ProceduralAnimation` 462 rows, 457 green: the three listed reds, and two rows green alone (asset-registry cache
  contention; the listed-flaky Spider roster). See the Phase 5 block.
- `CrawlerMelee` 38/38 (host, AngelScript `Script/ECS/Crawler/*.as` and `Script/Editor/Mars_CrawlerAttackClips.as` compile in
  that boot).
- Earlier phases' focused gates: see Phases 1-4 (`Action.Editor` 70/71 with one environmental red green alone).

**Three environments.**
- C++: the suites above.
- AngelScript: `Plugins/CkFoundation/Script/Generated/` carries `utils_procedural_action_editor::Get_EditorData` /
  `TryGet_EditorData`, `utils_procedural_articulation::Request_ClearFault` and
  `utils_procedural_animation::Add_Walker(Body, FCk_ProceduralWalker_Spec)`; the host script compiled in every test boot.
- Blueprint: `[EDITOR-VERIFY]` (VALIDATION.md, Three environments): place `Add Walker`, `Request Clear Fault`,
  `Create From Clip` in a graph; nodes compile.

**`[EDITOR-VERIFY]` owed by the maintainer:** VALIDATION.md items 1-5 (resave the four crawler clips after checking the
sidecar subject and labels; one Solver world per gizmo release; rotate/bend gizmo keys; undo of subject change and rename;
PIE Room 5 sever mid-strike).

**Not done, by design:** nothing pushed; PR hygiene items in VALIDATION.md (cherry-pick `907508cbe` first; name the CkEcs and
CkJolt changes in the PR body) are for the maintainer's PR.

**Campaign follow-ups (collected):** asset-registry cache contention between lanes (`Intermediate/CachedAssetRegistry_*.bin.tmp`,
three rows across Phases 3-5); `RecordedStore` to the editor module (P3-Q2); migrated rig/gait outers; rename
`_PendingLastIntermediatePose`; a `Get_ResultText` unit row for `Stalled on:`; `WalkQualityOnHump` (red on the Phase-4
baseline, green after; unlisted) to be classified in CkTests.
