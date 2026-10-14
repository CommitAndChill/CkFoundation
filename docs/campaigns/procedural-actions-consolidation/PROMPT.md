# Procedural actions consolidation — executor package

Planned 2026-10-13 by a Fable session from a staff-level audit of `feature/procedural-anim-ext`
(CkFoundation, 7 commits over `origin/dev`, +29k lines in `CkProceduralAnimation` and the new
`CkProceduralAnimationEditor`). Executors are Opus sessions with no memory of the audit. Everything
an executor needs to decide is decided here; a phase that seems to need a design choice is a
planning bug — stop and record it in PROGRESS.md.

## Problem statement

The branch's architecture is right (clip → compile → player → limb control → articulation → rig;
every mutation a request; every wait bounded) and its runtime tests are strong (CkTests +24.6k
lines). It is not mergeable because:

1. Three lifecycle holes brick or leak: a claim outlives its limb and faults the whole rig; `Faulted`
   is permanent; creation fails silently at capacity, and the default delivery mode makes that
   reachable.
2. Two processors write one pose field with no ordering edge between them.
3. The feature module grew a `Private/` folder the module rules forbid, and tests reach into it.
4. Roughly a third of the public surface has no consumer; the editor keeps three playheads and
   refreshes twice per edit; the runtime asset carries editor-only data.
5. The composition API hides its order: `Gait::Add` needs legs it doesn't take, `Articulation::Add`
   rejects a walker root while `Add_Walker` adds an articulation silently and doesn't return it.

Maintainer rulings taken before planning (do not reopen): move editor data out of the runtime
asset into an editor-owned sidecar **(yes)**; collapse the PoseGroups layer to labelled keys
**(yes)**; split the player service and the pose fragment **(yes, last runtime phase)**.

## Chosen approach

Five executor phases, each one session, each gated by focused toolbox patterns, one full gate at
the end. Runtime fixes come first because the editor phases rebuild on the runtime API.

| Phase | Scope | Gate pattern |
|---|---|---|
| 0 (maintainer) | Commit the working tree | — |
| 1 | Runtime lifecycle blockers + ordering + result semantics + ensure shapes | `ProceduralAnimation.Action`, `ProceduralAnimation.Articulation`, `CrawlerMelee` |
| 2 | API shape: walker spec/struct, `Gait::Add` over legs, `Private/` → public, surface cuts, receipts, docs; host adaptation | `ProceduralAnimation`, `CrawlerMelee`, `Crawler` |
| 3 | Asset split (editor sidecar) + poses as labelled keys; host seeding + clip resave | `ProceduralAnimation.Action.Editor`, `CrawlerMelee.ShippedClipsLoad` |
| 4 | Editor consolidation: one refresh, one playhead, one converter, curve validation, command cuts, undo gaps, tests | `ProceduralAnimation.Action.Editor` |
| 5 | Service split + pose fragment split (behaviour-preserving) | `ProceduralAnimation` full pattern |

## Rejected approaches (kill reasons)

- **Patch `HoldClaimedChannels` to skip dead legs** (the working-tree fix): treats one symptom; the
  dead claim still freezes every other action on the subject until a 1 s return timeout. Replaced
  by reconciliation in Phase 1.
- **Keep editor data on the runtime asset behind `WITH_EDITORONLY_DATA`**: a T4 runtime header
  already names the editor module in an `AllowedClasses` string, and the host's editor script
  sets a field that vanishes cooked. Replaced by `IInterface_AssetUserData` + editor-only array
  (engine precedent: `UAnimationAsset`, `USkeleton::AssetUserDataEditorOnly`).
- **A sibling `_EditorData` asset**: the maintainer removed sibling `_Preview_DA` assets this week
  for being invisible and confusing. No sibling files.
- **Splitting the stray commits out of history**: `907508cbe` (runtimemesh) and the CkEcs/CkJolt
  changes in `b23668049` are already on `origin/feature/procedural-anim-ext`. Rewriting pushed
  history is out. At PR time, cherry-pick `907508cbe` onto `dev` first and name the CkEcs/CkJolt
  changes in the PR body (see VALIDATION.md).
- **`Gait::Add` keeps taking a `Transform`**: the 2..64-attached-legs precondition is only
  discoverable by the ensure firing. Replaced by `Add(legs, data)`; 14 call sites, all mechanical.
- **Per-key `_Label` fields on `TransformKey`/`VectorKey`**: duplicates a label per channel and
  puts an editor concern in runtime keys. Labels live in the sidecar as `(phase, time, label)`.
- **Leg/Limb handle collapse and the campaign-docs-name-the-host convention**: out of scope;
  recorded as open rulings in PROGRESS.md.

## Executable spec

Each phase leads with tests the executor writes **red first** (exact names in the phase files),
plus greps with expected hit counts as exit criteria. Phase 5 is behaviour-preserving, so its
spec is the existing suite (374 C++ rows + 77 AS tests under `ProceduralAnimation`) staying at its
recorded baseline.

## File inventory (why each matters)

Runtime, `Plugins/CkFoundation/Source/CkProceduralAnimation/Public/CkProceduralAnimation/`:
- `Articulation/CkProceduralArticulation_Processor.cpp` — `HoldClaimedChannels` (fault site :121-130,
  :287), Faulted early return (:91), ten `Faulted` writes.
- `Articulation/CkProceduralArticulation_Fragment.h` — `_Claims` (`_LimbOwners`, `_BodyOwner`),
  `_Health` (`_State`, `_Fault`, offenders), `_Pose` (22 members).
- `Articulation/CkProceduralArticulation_Control.cpp` + `Private/.../CkProceduralArticulation_Control.h`
  — claim table ops (`ReleaseClaims`, `TryEvaluateCandidateBatch` → `ClaimLost` :530,
  `FailPendingPublication` :766 sets Faulted).
- `Articulation/CkProceduralArticulation_Utils.{h,cpp}` — `Add` (standalone only, rejects a gait root :8),
  `Request_RecoverUnplantedLimbs` (:388-414; the request-shape template).
- `LimbControl/CkProceduralLimbControl_Control.cpp` + private header — `Get_InterruptionReason`,
  `BeginReturn`, `LimbUnavailable` reason (:925).
- `ActionPlayer/CkProceduralActionPlayer_Service.cpp` — 1,416 lines; `ForEachEntity` :391-1416
  (passes: recoveries :402, snapshot :437, interruption :542-638, return prefix :600, planning :605,
  staging :883, key dispatch :1391).
- `ActionPlayer/CkProceduralActionPlayer_Processor.h` — Service `RunAfter` (:84-87), `EndPlay` (:120).
- `Action/CkProceduralAction_Utils.{h,cpp}` — `Create_FromClip`/`DoCreate` (:36-100), silent capacity
  return (:59), forbidden ensure shapes (:324-333, :350-359).
- `Action/CkProceduralAction_Processor.cpp` — cancel/release result semantics (:53-61, :91-96).
- `Action/CkProceduralAction_Fragment_Data.h` — `PlaySpec._Delivery` default (:317), `Outcome` enum
  (`SubjectLost` :40, `PartLost` :41 unused), `FailureContext` (:447-500).
- `Action/CkProceduralAction_Data.h` — the asset (:424), `_CompatibilityVersion` (:444), editor-only
  block (:455-471), `PoseGroup` (:388), `EditorSubject` (:353-369), key structs (:98, :124).
- `Core/CkProceduralActionValidation.cpp` — compiler; string diagnostic codes via `AddError` (:377).
- `Core/CkProceduralActionReceiptStore.{h,cpp}` — `MaxRetainedInstances`, `OrphanRetention` (:25),
  expiry (:718), `ReserveUnbound`.
- `CkProceduralAnimation_Utils.{h,cpp}` — `FCk_ProceduralWalker` (:50-66), `Add_Walker` (:84-183,
  adds the walker articulation :175-183).
- `Gait/CkProceduralGait_Utils.cpp` — `Add` precondition on legs (:24-39).
- `Rig/CkProceduralRig_Processor.cpp:1046` — writes `_PredictedSubjectPose` (ordering gap).
- `Gait/CkProceduralGaitParticipation_Control.cpp:291-297` — `LimbUnavailable` for locomotor limbs.
- `Source/CkProceduralAnimation/CkProceduralAnimation.Build.cs:8` — `PrivateIncludePaths` to remove.
- `Source/CkProceduralAnimation/Claude.md` — links the gitignored `PROCEDURAL_ACTIONS.md` (:3).

Editor, `Plugins/CkFoundation/Source/CkProceduralAnimationEditor/`:
- `Private/Authoring/CkProceduralActionAuthoringCommands.cpp` (1,966 lines) — `CommitDraft` :124-161
  (calls `PostEditChange` :159), `RecordPose` :1904, `UpsertGroup` :428, uncalled commands.
- `Private/Toolkit/CkProceduralAction_EditorToolkit.cpp` — `OnObjectPropertyChanged` :540-556,
  `OnTicker` :518-536, `Refresh` :373-416, `PostUndo` :562.
- `Private/Toolkit/CkProceduralAction_EditorToolkit_Actions.cpp` — nine `NotifyContentChanged` calls.
- `Private/Session/CkProceduralActionAuthoringSession.cpp` — `Refresh` bumps `_Revision` (:144),
  `SetTime`/`_Playing`, pose-group staging (:297-372).
- `Private/Session/CkProceduralActionEditorModes.{h,cpp}` — solved-frame cache; `SourceChanged` restarts
  on any revision (:102-104).
- `Public/.../CkProceduralActionEditorModel.h` — the model (`_Time` :218, `_Playing` :224).
- `Private/Widgets/SCkProceduralAction_Timeline.cpp` — Poses row, raw-fraction converter (:734-750),
  `Snap` (:730).
- `Private/Widgets/SCkProceduralAction_Transport.cpp` — second converter (:144), step math (:33).
- `Private/Curve/CkProceduralAction_CurveAdapter.cpp` — unvalidated key writes (:349-355).
- `Private/Authoring/CkProceduralActionPoseGroups.{h,cpp}` — the layer to delete.
- `Private/Asset/CkProceduralActionGuidedCreation.cpp`, `Private/CkProceduralActionPreview_Config.cpp`
  (legacy pairing :370-426).

Tests, `Plugins/CkTests/`:
- `Source/CkTests/CkTests.Build.cs:13-19` — private include path to remove.
- `Source/CkTests/Private/UnitTests/CkProceduralAnimation/{Gait,Articulation,Action}/*` — eight files use the
  private control classes directly.
- `Source/CkTestsEditor/Private/CkProceduralAnimation/*` — seven files use `PoseGroup`.
- `Script/CkProceduralAnimation/*` — gym fixture + rows calling `Add_Walker`/`Gait::Add`.
- `AutomationGate.json` — known reds (read, never edit from here).

Host, `D:/Repositories/CkRepos/Orion/`:
- `Script/ECS/Crawler/Mars_Crawler_Utils.as:150-200` — composition (SurfaceMotion → Add_Walker →
  BodyPose → cast → ActionPlayer).
- `Script/ECS/Crawler/Mars_Crawler_ActionAdapter.as:152` — sets `Delivery::Explicit` explicitly.
- `Script/Editor/Mars_CrawlerAttackClips.as:269,287,318-340` — seeds `EditorSubject`.
- `Content/Mars/Gameplay/Monsters/Crawler/Actions/*` — four clips to resave after Phase 3.

## Glossary

- **Claim** — a controller token's ownership of a limb channel (`_LimbOwners[index]`) or the body.
- **Token** — `FCk_ArticulationControlToken{InstanceId, Generation}`; identifies one action instance.
- **Carried start** — a track may omit its time-0 key; the phase starts from the value carried in.
- **Receipt / stream** — per-action observation records the game reads; `Explicit` delivery means
  the consumer acknowledges; `Auto` means the player does.
- **Legacy walker** — a gait+legs subject whose articulation `Add_Walker` creates; vs **standalone**
  (layout-based) subjects created by `Articulation::Add`.
- **Sidecar** — the editor-owned `UAssetUserData` object stored in the clip's editor-only user-data
  array (one file on disk, stripped from cooks).

## Skills to load, and when

- Before any phase: `ck-change-control`, `ck-tests-authoring-and-running` (CkTests), the project
  `/build-test` skill (toolbox only; never Build.bat/UBT).
- Phase 1 and 5: `ck-macros-and-codegen` (request structs, `CK_DEFINE_CONSTRUCTORS`, tags),
  `ckecs-architecture-contract` (processor ordering, RunAfter semantics).
- Phase 2 and 3: `ck-angelscript-interop` (every public change is C++/BP/AS; regenerated
  `Script/Generated/*` must not be hand-edited).
- Phase 3 and 4: `ck-slate-tools` (CkGameplayDebugger) for widget edits.

## House rules the executor must not relax

Root `CLAUDE.md` of CkFoundation governs: `CK_ENSURE_IF_NOT` with recovery in the body (never the
`{}` + `if (NOT)` split), `ck::IsValid`/`Is_NOT_Valid`, `NOT`, trailing returns, Allman, no
anonymous namespaces or file statics, UPROPERTY on its own line, no project names in framework
code, no campaign breadcrumbs in source comments, commits only with explicit pathspecs, never
stage `Config/DefaultGameplayTags.ini` or `Content/__ExternalActors__`, never edit
`AutomationGate.json` to hide a red, never hand-edit `Script/Generated/*`, never touch
`Plugins/Monolith`. Build and test only through `./CkAuto/UnrealToolbox.exe`.
