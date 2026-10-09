# Runtime mesh progress

## Current state

2026-10-09: the framework capability is implemented and gated. Native import/slice kernel, public
RuntimeMesh feature (deferred queue, typed + generic completion), Display companion, CkJolt RuntimeConvex
source with observable setup results, C++/Blueprint/AngelScript parity, real-renderer capture, bounded
benchmark and the packaged cooked probe are all green on the final binaries (session 3 below). All work
is uncommitted and unpushed. Open: cast-shadow and separated-moving-body observations in the real
renderer, measured shipping ceilings (numbers recorded, decision pending), and the spec wording for G4
(cooked import needs a render-capable process).

Revisions (read directly from Git):

- Host dev: c06c8af028a474a385500b2a87ab4649412b6c72.
- CkFoundation dev: 9366c2a349dc8e9659853afb74dd439de5b1df01.
- CkTests dev: 17b956d509e06bbd74a885f4c2a826fd901bd12d.

Engine: Get-ProjectEnginePath.ps1 resolved D:/Repositories/UnrealEngine-Angelscript;
Engine/Build/Build.version reports 5.7.4. Toolbox v1.56 (74ddfe50).
No editor process or active toolbox build found at preflight.

Baseline: no RuntimeMesh directory or test surface existed. No pre-change suite was
run and no prior passing count is asserted. Existing known-red manifests retained:
host mtime 2026-10-03 19:20:23 UTC; CkFoundation 00:32:31 UTC;
CkTests 19:20:39 UTC. These are manifest evidence, not fresh executed results.

Preserved initial CkFoundation changes: deleted Content/CkAnimation/Utils_CkAnimation_FL.uasset;
modified CkUI_Layout_Subsystem.h/.cpp and CkUI_Input_Subsystem.h/.cpp;
untracked design spec and continuation prompt. Preserved CkTests changes:
Script/Generated/CkTestsAssets.as; Source/CkTests/CkTests.Build.cs;
untracked Test_UI_ExternalLayoutTeardown.cpp and Test_UI_InputTeardown.cpp.
Host tracked files were clean with submodules excluded from status.

Next: Gate 1B starts with a cooked CPU-readable generic seam/attribute fixture and
CPU-disabled counterpart through the internal native boundary. Inspect the CkTests
packaging harness and choose a focused Toolbox invocation before adding reflected
consumer APIs. Do not skip cooked input proof based on the synthetic render buffers.
Changes remain uncommitted/unpushed.

Resumed execution contract: Gate_02_Execution.md. New opt-in cook fixtures/probe
and four native cut test rows are under source audit. `native-07-cut-cook-author`
is the first integration build; its compiler reported a new test-local `LogPath`
name collision. No cooked or slice acceptance is inferred from compilation.

## Decisions and evidence

- Current doctrine requires feature code under Public/<Module>; the implementation
  places the non-consumer native contract in its Internal subdirectory. No editor dependency.
- Runtime converter source at MeshConversionEngineTypes/Private/StaticMeshLODResourcesToDynamicMesh.cpp
  discards duplicate/invalid triangles and splits nonmanifold ones. Strict preflight
  and postvalidation are required; true from Convert is not acceptance.
- FMergeCoincidentMeshEdges must use unique-pair matching and preserve overlays;
  unresolved/ambiguous seams cannot be repaired into a successful result.
- FDynamicMeshAABBTree3 default self-intersection query ignores adjacent triangles;
  adjacent overlapping faces require additional validation and negative fixtures.
- Initial native boundary is internal only. ECS, reflected API and cooked source
  admission remain Gate 1B; this avoids claiming a Ready feature before import proof.
- The local build-test wrapper references a missing .Codex path; canonical guidance
  was found and read at CkAuto/.claude/skills/build-test/SKILL.md.

## 2026-10-08 native-01

- Invocation: Saved/RuntimeMesh/Run-NativeGate.ps1 -RunName native-01 -Generate.
  This runs Toolbox --build --target=Editor --test --test-pattern=Ck.RuntimeMesh
  --no-live --discover-fresh --project=D:/Repositories/CkRepos/Orion/Mars.uproject
  --generate, with a unique output log. No config override.
- Evidence: Saved/RuntimeMesh/native-01.log and native-01.exit.json, exit 1.
  Project generation reported "Visual Studio 2022 x64 must be installed in order
  to build this target." No C++ compilation or automation verdict was produced.
- Confirmed vswhere lists Visual Studio Community 2026 18.10.12217.157 with MSVC
  14.44.35207 and 14.51.36231 present. Engine source supports VisualStudio2026.
  GenerateProjectFiles.bat pauses after its error; the owned paused cmd child was
  stopped after checking its PID, parent and command line, allowing Toolbox to exit.
  Retry the supported Toolbox build without IDE generation; do not change global
  compiler settings or install tools for this feature.
- Adversarial review drove explicit bowtie and orphan-vertex checks, complete
  per-corner attribute validation, bounded tolerances, converter triangle/vertex
  preservation checks, post-weld displacement checks, and incremental native
  triangle intersection testing (no all-intersection result materialization).
- A proposed streaming race was rejected after source verification:
  UStaticMesh::StreamOut is game-thread-only (StaticMesh.cpp:8983-8990) and advances
  CurrentFirstLODIdx synchronously before async discard (StaticMeshUpdate.cpp:284-300).
  Import is synchronous game-thread-only, rejects excluded LODs and holds an LOD
  reference while copying. Actual packaged/streaming validation remains Gate 1B.

## 2026-10-08 native-02 through native-04

- native-02 omitted --generate. Toolbox selected Visual Studio 2026 and MSVC
  14.44.35229, compiled UHT output, then failed with new-code compiler errors:
  export/friend declaration linkage and a signed material-index comparison.
  Exit 1; no tests executed. Evidence: Saved/RuntimeMesh/native-02.log/.exit.json.
- Moving exported declarations before friends and validating signed material IDs
  allowed CkRuntimeMesh itself to compile/link in native-03. Its CkTests consumer
  still reported C4273 inconsistent DLL linkage on the unannotated friends.
  Exit 1; no tests executed. Evidence: native-03.log/.exit.json.
- Matched existing CkGroundNav/CkVoxelNav headers by explicitly marking all four
  friend declarations CKRUNTIMEMESH_API. native-04 compiled and linked both modules;
  fresh discovery selected 10 Ck.RuntimeMesh tests (0 renderer-only). Exit 1:
  9 passed / 1 failed / 0 skipped / 0 contaminated. Failing name:
  Ck.RuntimeMesh.Validation.AdjacentCoplanarFold. It incorrectly admitted the
  folded closed mesh and published a payload. Evidence: native-04.log:20662-20688
  and native-04.exit.json. No source changes during that run.
- Lead verified a test-fixture defect against the actual UE 5.7.4
  StaticMeshVertexBuffer.cpp:66-69: the TArray overload ignores InNumTexCoords.
  Fixtures now allocate explicit vertex/UV counts and populate tangents/UVs;
  assertions distinguish the intended 0-, 2- and 5-layer cases.
- Final bounded independent review found no additional confirmed serious native
  source defect. Lead inspected the cited paths. The nonmanifold fixture exercises
  raw-edge preflight, not the defensive post-converter count check. Direct Admit
  separately exercises the native vertex ceiling; ambiguous seam pairing has an
  explicit negative fixture. Review is not a runtime verdict.
- Lead found the broad-phase defect in engine BoxTypes.h:526-529:
  FAxisAlignedBox3d::Intersects uses strict interior overlap, so coplanar triangle
  bounds with zero thickness are skipped. Use native DistanceSquared for inclusive
  contact and explicitly pass the intersection tolerance into the native query.
  Strengthen the fixture with a direct native-query observation, a rotated fold,
  and a valid rotated cube control. Verification is recorded below.

## 2026-10-08 verified Gate 1A checkpoint

- native-05-fold: Toolbox --build --target=Editor --test
  --test-pattern=Ck.RuntimeMesh.Validation.AdjacentCoplanarFold --no-live
  --discover-fresh --project=D:/Repositories/CkRepos/Orion/Mars.uproject.
  Build succeeded, isolated test 1 passed / 0 failed / 0 contaminated, exit 0.
  Evidence: Saved/RuntimeMesh/native-05-fold.log and native-05-fold.exit.json.
- native-06-final: same command with --test-pattern=Ck.RuntimeMesh. Fresh discovery
  selected all 10 registered tests (0 renderer-only). Final build succeeded;
  10 passed / 0 failed / 0 skipped / 0 contaminated, exit 0 at
  2026-10-08T21:48:24.9814013Z. Every test lane logged its final exit code 0.
  Evidence: Saved/RuntimeMesh/native-06-final.log and native-06-final.exit.json.
- Executed delta: native-04 9 pass / 1 fail {AdjacentCoplanarFold} -> isolated
  corrected row 1/1 -> complete group 10/10. This was a defect in the new validator,
  not an inherited failure or a flake. Source review missed the strict AABB
  semantics; the runtime fixture exposed them. The corrected broadphase and
  centimetre tolerance were independently re-reviewed against engine source.
- Source stayed frozen after 21:44:10 UTC. Native-05 compiled the final source:
  Binaries/Win64/MarsEditor-CkRuntimeMesh.dll mtime 21:44:39 UTC and
  MarsEditor-CkTests.dll mtime 21:44:40 UTC. Native-06 verified the same binaries.
  Subsequent edits are campaign documentation only.
- Verified: 36 split render vertices reconstruct 8 geometric vertices; the
  10 cm cube retains 1000 cm3 volume, local centroid/bounds, all corner normals,
  two UV layers, linear colors and two material IDs. Source buffers remain
  independent of immutable output. Missing UVs/colors use the specified defaults.
- Verified negative corpus: CPU/LOD availability, finite bounds/options, render
  indices/sections/materials/attributes, open/duplicate/nonmanifold inputs,
  ambiguous seams, disconnected/cavity/inverted solids, bowties, orphan vertices,
  and axis-aligned/rotated coplanar folds. A valid rotated cube remains accepted.
  This is a bounded synthetic corpus, not arbitrary asset or performance proof.
- Tracked diff whitespace checks passed in both submodules. New module/test files
  have no trailing whitespace. Host status with submodules excluded remains clean;
  the original unrelated submodule changes remain. No staging, commit, push,
  submodule-pointer update, engine-source edit or global toolchain change occurred.
- Source/CkRuntimeMesh/Claude.md exists but follows the repository's existing
  *.md ignore rule; future requested commits must force-add this module guide.
  No ignore-policy change or staging was performed in this session.

### Changed surfaces

- CkRuntimeMesh.Build.cs / CkRuntimeMesh_Module.cpp: runtime-only module dependencies.
- Internal/CkRuntimeMesh_Geometry.h/.cpp: synchronous render-LOD import, strict
  bounded native admission and immutable geometry/metrics; no consumer engine API.
- CkFoundation.uplugin / Source/CLAUDE.md / module Claude.md: registration and
  ownership documentation, with public ECS/display/physics work explicitly pending.
- CkTests Test_RuntimeMesh_Import.cpp: ten focused native automation rows.
- CkTests.Build.cs: only the additive CkRuntimeMesh private dependency is ours;
  the pre-existing CommonInput change is preserved.
- docs/campaigns/runtime-mesh/: execution gates, scope, ownership and this evidence.
- Saved/RuntimeMesh/Run-NativeGate.ps1: ignored local detached-run helper; accepts
  RunName, optional TestPattern and Generate, and records the real Toolbox exit.

## Open evidence

## 2026-10-08 resumed native cut and cooked infrastructure

- `native-07-cut-cook-author`: new-code C4459 test-local `LogPath` collided
  with Unreal's log category. Exit 1, no tests. Renamed to ProbeLogPath and
  explicitly resolved the child log path to absolute.
- `native-08-cut-cook-author`: build passed; opt-in AuthorFixtures 0 passed /
  1 failed / 0 skipped / 0 contaminated. Fixture contract failed before saving
  the first asset; no binary assets were created. Added field diagnostics and
  `FStaticMeshCompilingManager::FinishCompilation` after Build/PostEditChange.
  Async compilation remains a hypothesis until the retry produces evidence.
- Independent source review found and corrected slice output admission's inherited
  volume floor, delayed input count check, and inconsistent plane coordinate bound.
  Added explicit small-scale, interpolated exterior attributes, oblique cap frame,
  far-from-origin and concave U-prism multi-island fixtures.
- `native-09-cut-corpus`: final current native sources compiled. Fresh discovery
  selected 16 tests; 16 passed / 0 failed / 0 skipped / 0 contaminated; Toolbox
  exit 0 at 2026-10-08T22:12:30.4859372Z. Evidence:
  Saved/RuntimeMesh/native-09-cut-corpus.log and .exit.json.
- Executed native delta: previous import group 10/10 -> import plus six slice
  rows 16/16. The small admitted 0.012 cm cube produces two 8.64e-7 cm3 halves;
  translated cube at x=100000 remains sliceable; concave U-prism split rejects
  disconnected output with no payload. No ECS/public/cooked slice claim yet.
- Cook authoring and packaged probe remain opt-in under RuntimeMeshCooked, outside
  the normal Ck root. The native run selected 16 rows and did not execute the two
  artifact-dependent cook rows. The probe rejects editor-only-data builds and
  requires actual packaged exit plus fresh unique PASS log.
- `native-10-author-retry`: build passed, AuthorFixtures 0/1, exit 1 at
  22:14:28.5864107Z. Diagnostics showed CPU access 1, materials 2, LOD 1,
  triangles 12, UV layers 8, colors 36. This disproved the async timing hypothesis.
- Source confirmed the fast-build path reaches the TArray vertex-buffer overload
  that ignores the requested UV count. Standard StaticMesh MeshBuilder uses the
  mesh-description UV count. The authoring test now uses the standard asset build,
  not patched buffers or engine changes.
- `native-11-author-standard`: build passed, AuthorFixtures 1/1, exit 0 at
  22:17:43.4603642Z. Both owned packages saved at 22:17:02 UTC:
  CkTests/Content/CkRuntimeMesh/Cooked/SM_Import_CPU.uasset (12047 bytes) and
  SM_Import_NoCPU.uasset (12006 bytes). This is editor asset proof, not cook proof.
- Focused pre-change Jolt baselines: `native-12-jolt-lifecycle-baseline` 3/3,
  exit 0 at 22:20:21.5771484Z; `native-13-jolt-ownership-baseline` 3/3,
  exit 0 at 22:22:39.5940894Z; `native-14-jolt-editor-baseline` 2/2,
  exit 0 at 22:24:43.6677126Z. All had zero failures, skips and contamination.
  ChurnThousandReturnsToBaseline is known-flaky but passed this captured baseline.
  Evidence is Saved/RuntimeMesh/<run-name>.log and .exit.json for every run.
- `native-15-game-build`: first Toolbox Game build completed with exit 0 at
  2026-10-08T22:59:42.9339103Z (1496 actions; UBT Result: Succeeded).
  Mars.exe is 494261760 bytes, written at 22:59:38 UTC; Mars.target was written
  at 22:59:42 UTC. Existing monolithic Jolt LNK4217 warnings were non-fatal.
- `native-16-cook-import`: cook command exited 0 (699.16 seconds; 0 errors /
  5 warnings), but staging failed and helper recorded actual exit 1 at
  2026-10-08T23:12:15.4041408Z. IoStore requires a package-store manifest/project
  store in the expected <CookRoot>/Windows directory; actual CookRoot contains
  Engine/ and Mars/ directly. Verify platform output-path semantics before retry.
  All native compilation was disabled after the successful Toolbox Game build.
  Live Source/Script remains frozen until the actual packaged import verdict;
  ECS, display, Jolt and parity changes remain unapplied staged patches.

## Session handoff requested by user

- User requested to continue implementation in another session. All three
  delegates are frozen; no live Source/Script changes were applied after the
  native Game build. No new benchmark or Display coverage extension was started.
- Exact continuation and failed-stage diagnosis instructions are in
  CONTINUATION_PROMPT_runtime_mesh_implementation.md beside this file. The
  detached native-16 cook/stage has finished with exit 1. No build/cook job or
  delegate remains active; the next session can take over without overlap.
- Saved/RuntimeMesh/Staged holds unapplied ECS, Display, Jolt, BP/AS parity,
  integration and renderer files. These are NOT compiled/runtime-verified.
- Independent review found finite nonunit RuntimeConvex rotations could reach
  Jolt, and a generic failure ensure duplicated legacy-specific diagnostics.
  Both corrections and a rotation test are saved in JoltRuntimeConvex.patch
  only. Review found no confirmed queue/display production defect in the
  combined staged set; this is static evidence, not a runtime pass.
- Display missing-material failure, actual world-cleanup release and repeated
  churn tests remain planned only. Real renderer still needs physical separated
  pieces, repeated cuts, shadow and origin-alignment observations. Benchmark and
  measured shipping ceilings remain open. Packaged probe currently checks import,
  not slicing/public composition.

## 2026-10-09 session 3: staged work applied, gated, packaged probe real

All runs under Saved/RuntimeMesh/<run>.log + .exit.json (actual toolbox/UAT exit codes).
Source stayed uncommitted; no push, no gitlink bump, no engine edit.

### Cook/stage gate (native-16 diagnosis)

- Cause 1 (confirmed, UAT source): staging appends `Windows` to `-CookOutputDir` unless the leaf already
  is the cook platform (CopyBuildToStagingDirectory.Automation.cs ~1272), while the cooker writes
  straight into the override when it lacks `[Platform]` (CookOnTheFlyServer.cpp GetOutputDirectoryOverride).
  Run-CookGate.ps1 now passes `<run>-Cooked/Windows`. native-17: stage reached the pak step.
- Cause 2 (confirmed): this engine checkout has no UnrealPak.exe and the toolbox builds only
  Editor/Game/Server/Client, so the helper drops `-pak` and stages loose cooked files plus the IoStore
  project store. native-28/34/37: COOK, STAGE, PACKAGE all completed, exit 0.
- Cause 3 (confirmed): a cook editor boot with the new AngelScript parity test present but old binaries
  refuses to run (`Cannot run when angelscript has failed to compile`). Cooks now run after the editor build.

### Applied staged work and review corrections

- Feature, Integration, Parity, Render patches applied mechanically (scratch Codex-patch applier, dry-run
  first); two malformed hunks (Jolt processor header insertion anchor; a merged hunk in the parity patch)
  were split by hand. Display files copied to Public/CkRuntimeMesh/Display/.
- Three Opus executors (feature, display, Jolt) applied doctrine corrections under lead audit: single
  `CK_ENSURE_IF_NOT` shape everywhere (retired double guard removed), atomic Display `Add` that adds nothing
  on rejection, named constants (`QueueCapacity` 16, `DrainBudgetPerTick` 2, `MaxRuntimeConvexPoints` 2048,
  `MaxSetupWaiters` 128, `MaxMaterialSlots` 256), ensure-on-invalid getters, `CastChecked` where the
  fragment is proven, per-site Jolt admission ensures with the shared `JoltBody setup failed for Entity`
  prefix, Jolt `Add` copying the spec once instead of a hand-built minimal spec, Display material load
  started in Setup, `CK_REGISTER_PROCESSOR` include added, three real compile errors fixed by reading.
- Lead fixes after the first build: the drain processor is a plain `ForEachEntity` over the transient
  entity's queue (the base template requires one even with a custom DoTick); `CastResult.h` include;
  C4459 rename; `MarkedDirtyBy = FTag_Transform_Updated` removed from the Display transform processor
  (the scheduler warns on unordered consumers of a shared dirty tag); BP harness callbacks made
  `BlueprintCallable` so `Create Event` binds them; the parity test accepts `BS_UpToDateWithWarnings`
  (by-ref event parameters warn); the Public-test listener's UFUNCTION bodies moved inline into its
  header so the game target links.
- Ck ensures write their message in two log entries (`CkEnsure:` and `CkEnsures:`), so the new tests
  expect ensure text at least once; exactly-once remains proven by the listener counters.
- Render: the all-white capture had alpha 0 and the default material. Editor material loads skip shader
  compilation (`r.ShaderCompiler.JobCacheDDC`); the test now calls
  `UMaterialInterface::SubmitRemainingJobsForWorld` + `FinishAllCompilation` before capturing and asserts
  `IsComplete()`. Fixture marker renamed to `RenderFixture-v1` (no fixtures existed yet).

### Executed evidence (final editor binaries unless noted)

| Run | Pattern | Result |
|---|---|---|
| native-20-build-author | RuntimeMeshCooked.Author | build ok; AuthorFixtures 1/1, AuthorRenderFixtures 1/1 (4 render assets under CkTests/Content/CkRuntimeMesh/Render, 20:02 local) |
| native-23/24/25 | Ck.Jolt.Body.Lifecycle / OwnershipExclusivity / Ck.Jolt.EditorWorld | 3/3, 3/3, 2/2: identical to the pre-change baselines (ChurnThousandReturnsToBaseline listed flaky, passed) |
| native-26 | RuntimeMeshCooked.PendingImportGC (alone) | 1/1 |
| native-27 | RuntimeMesh_SliceParity (AngelScript, functional-test root) | 1/1 |
| native-31 | Ck.Jolt.Body.RuntimeConvex | 3/3 (Setup, EditorWorld, BodyCapacity) |
| native-32-build-runtimemesh | Ck.RuntimeMesh | 34/34, 0 contaminated, exit 0: 16 native, 10 Public, 5 Display, Blueprint.CompiledSliceEvent, AS parity, Render.CheckerSliceDisplay (real-renderer editor) |
| native-39-benchmark | Ck.RuntimeMesh.Benchmark | 1/1; JSON Saved/Automation/RuntimeMesh/Benchmark/03213846-4BAC-EB7C-4B9B-1EB2788A7C46.json |

Deltas: native-21 had 4 red (BP, two Public rejection rows, Render) and native-22 had 3 red Jolt rows;
all explained above and green on the final binaries. No pre-existing test changed state.

Render PNG inspected by the lead (Saved/Automation/RuntimeMesh/Render/E9C6CCA0..._BaseColor.png): the
negative half-cube shows red and blue exterior checker faces by material ID and the green cap checker
with 4x4 cells (10 cm / CmPerUVUnit 10 => correct planar UV scale), clean seams. FinalColor shows only the
lit top (directional light, no sky), so cast-shadow appearance is still unobserved.

Benchmark (AMD Ryzen 7 7800X3D, Development editor, 20 reps, median): Box12 Admit 0.014 ms / Cut 0.10 ms;
Box192 0.27 / 0.92 ms; Box768 1.76 / 3.34 ms; Box3072 (1538 v / 3072 t) Admit 17.97 ms / Cut 17.58 ms
(halves 925 v / 1846 t); queue of 16 drains in 8 ticks, worst single drain 0.41 ms on the 12-triangle
fixture. Working-set deltas sampled 0 except one +68 KB (process-wide sample, not a peak). Ceilings stay
at 2048 / 4096 and 16 / 2: a 3072-triangle admit+cut is ~36 ms on this CPU, so the ceiling is a
frame-budget decision for the consumer, not a safety limit; a per-tick time budget would be the next step.

### Packaged probe (G4)

- native-33/36: toolbox Game builds succeeded (Mars.exe 495938560 bytes, 20:51 local).
- native-35: the packaged game compiled the generated AngelScript cleanly (0 errors) and the probe ran;
  NativeImport failed. native-38 with per-fixture diagnostics: `CPU[no render data] NoCPU[no render data]`.
- Confirmed in engine source (StaticMesh.cpp:7365): a cooked game discards static-mesh render data on
  load unless `FApp::CanEverRender()`, so a `-nullrhi` packaged process can never read cooked render
  LODs. This is an engine contract: cooked import needs a render-capable process (off-screen is fine);
  already-imported geometry, slicing, queue, Display-less composition and Jolt bodies remain headless.
  Spec G4 wording "without a renderer" should be amended to "without a window/renderer output".
- The wrapper now launches the staged exe with `-RenderOffScreen` instead of `-nullrhi`.
- native-40-cooked-probe (editor rebuilt, staged exe from native-37 unchanged): RuntimeMeshCooked.Import
  1/1, exit 0. Packaged probe log Saved/RuntimeMeshCooked/420051934C3FEEAE682E468BCD79F1B4.log, every step
  PASS: NativeImport (CPU fixture indexCpu/posCpu/attrCpu/colorCpu all true, 12 tris, 2 sections, 2 UV
  layers; NoCPU fixture indexBytes 0, indexCpu false, failed CpuDataUnavailable), NativeSlice 500/500 cm3,
  BootMap Entry, World (Entry began play with an ECS world), PublicAdd Ready through the real processors
  (1000 cm3), PublicSlice Succeeded through the real drain with 500/500 cm3 halves under an independent
  owner, Display Ready, JoltBody RuntimeConvex promise Ready x1 with empty diagnostic. Final token PASS,
  process exit 0, 0 AngelScript errors in the cooked game.

### Maintainer rulings applied after the first green (2026-10-09 01:32 UTC)

- Add functions take the typesafe handle for a feature they require: `UCk_Utils_RuntimeMeshDisplay_UE::Add`
  now takes `FCk_Handle_Transform&` (the Transform rejection branch is gone); RuntimeMesh `Add` stays on
  `FCk_Handle` because it composes on any entity. CkJolt's legacy `Add(FCk_Handle&)` was left as is under G8;
  converting it is a separate, caller-breaking change.
- Reflected specs validate themselves: `Get_IsValid() const -> bool` on `FCk_RuntimeMesh_Spec`,
  `FCk_Request_RuntimeMesh_Slice`, `FCk_RuntimeMeshDisplay_Spec` and `FCk_JoltBody_RuntimeConvexSpec`
  (out-of-line in each `_Fragment_Data.cpp`, CkChain precedent). `Add`/`Request_Slice` ensure on the spec
  first and keep only handle, registry and lifetime context. Jolt `Add` now rejects a malformed RuntimeConvex
  spec synchronously as an observable `InvalidInput`.
- Re-gated on the rebuilt binaries: native-41 `Ck.RuntimeMesh` 35/35 (benchmark row included), native-42
  `Ck.Jolt.Body.RuntimeConvex` 3/3, native-43 Game build, native-44 cook/stage exit 0, native-45 packaged
  probe PASS on every step (log Saved/RuntimeMeshCooked/728AB9F74399828F3B0718AD0AEC9705.log).

- Typesafe-handle utils never re-check `Has`; they guard with plain `ck::IsValid` (default validity already
  spans pending kill up to Teardown) and never with `IsValid_Policy_IncludePendingKill`, so a garbage handle
  ensures. Applied to the Display getters and swept across CkProceduralAnimation utils (27 redundant checks
  removed; cross-feature admission checks in `Add`/`Create` kept). The Display pending-teardown test reads the
  cancelled fragment through the pending-kill policy itself instead of the public getters. Gates: native-46
  `Ck.RuntimeMesh` 34/35 (the test's own read), native-48 `Ck.RuntimeMesh.Display` 6/6 after the test fix,
  native-47 `AutoTest_Procedural` 58/60 with the two failures being CkTests' listed known reds
  (PillarCrossingRenderedTraversal, PostsCentipedeReturns; added 2026-10-01, toolbox exit 0).

## Remaining acceptance evidence

- Real renderer: cast-shadow appearance (needs a receiver plane and a sky/ambient term) and two separated
  Jolt bodies moving apart with their displays following; the existing capture proves materials, cap
  UVs and seams only.
- Shipping ceilings: numbers are recorded (native-39); choose and document final values, or add a
  per-tick time budget to the drain so the triangle ceiling is a memory bound rather than a frame bound.
- Spec amendment for G4: "without a renderer" means no window; a `-nullrhi` cooked process cannot read
  cooked render LODs by engine design (StaticMesh.cpp:7365).
- Other platforms, pak/IoStore container staging (needs UnrealPak built), replication/persistence stay out
  of scope. No full-suite claim is made: the full `AutomationGate.json` gate was not run this session.
