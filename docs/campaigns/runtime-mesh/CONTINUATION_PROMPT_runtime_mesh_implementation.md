# 1. One-line summary

The CkRuntimeMesh capability (native import/slice kernel, public deferred-slice feature, Display
companion, CkJolt RuntimeConvex admission with setup results, C++/BP/AS parity, real-renderer capture,
bounded benchmark, packaged cooked probe) is implemented and green on the final binaries as of
2026-10-09 01:01 UTC; it is uncommitted and unpushed. Remaining work is acceptance polish, not
implementation. Read `PROGRESS.md` (session 3) before doing anything.

# 2. Repo state

All on `dev`; HEADs unchanged from the previous handoff (host c06c8af0, CkFoundation 9366c2a3, CkTests
17b956d5). Nothing staged, committed, pushed or pointer-bumped. Preserve exactly:

- Unrelated dirt: CkFoundation deleted `Content/CkAnimation/Utils_CkAnimation_FL.uasset`; CkUI/CkUICore
  subsystem edits; CkTests `CommonInput` Build.cs line, `Script/Generated/CkTestsAssets.as`,
  `Test_UI_ExternalLayoutTeardown.cpp`, `Test_UI_InputTeardown.cpp`; host `Plugins/Monolith` dirt and
  `Script/Binds.Cache.Headers`.
- Campaign work (CkFoundation): `CkFoundation.uplugin`, `Source/CLAUDE.md` tier row, `Source/CkRuntimeMesh/`
  (module, Internal kernel, public quartet, Display/, Claude.md), `Source/CkJolt/Public/CkJolt/Body/*` and
  `Subsystem/CkJolt_Subsystem.*`, `docs/campaigns/runtime-mesh/`, `docs/specs/*RuntimeMesh*`.
- Campaign work (CkTests): `CkTests.Build.cs` (+CkRuntimeMesh), `CkTests.cpp` (probe hook),
  `CkTestsEditor.Build.cs` (+BlueprintGraph, CkEcsExt, CkJolt, CkRuntimeMesh, GeometryFramework,
  MaterialEditor, MeshDescription, RHI, RenderCore, StaticMeshDescription),
  `Private/UnitTests/CkRuntimeMesh/*` (import, slice, public, display, benchmark, PendingImportGC, cook
  probe + listener), `Private/UnitTests/CkJolt/Test_JoltBody_RuntimeConvex*.spec.cpp` + listener,
  `CkTestsEditor/Private/CkRuntimeMesh/*` (cook authoring, cooked wrapper, BP parity + harness, render
  authoring + capture + listener), `Script/CkRuntimeMesh/CkAutoTest_RuntimeMesh_SliceParity.as`,
  `Content/CkRuntimeMesh/Cooked/*` (2 assets) and `Content/CkRuntimeMesh/Render/*` (4 assets).
- Generated churn that belongs with the AS test: `Script/Generated/CkTests_AutoTestActors.as` and the
  external actor under `Content/__ExternalActors__/AutoTests/AutoTests_CkTests_Level/B/2B/` (the map
  populator placed the parity test in the CkTests auto-test level). Review before committing.
- `Source/CkRuntimeMesh/Claude.md` is ignored by the repo's `*.md` rule; force-add it when committing.

Helpers (ignored, under `Saved/RuntimeMesh/`): `Run-NativeGate.ps1` (`-TestOnly`, `-BuildOnly`,
`-Target Game`, `-ProjectPrefix`, `-EditorArgs`), `Run-CookGate.ps1` (cooks to `<run>-Cooked/Windows`,
stages loose files, no `-pak`). Every run leaves `<run>.log` + `<run>.exit.json`.

# 3. Open items (in priority order)

1. Real-renderer observations still missing: cast shadows (add a receiver plane and a sky light to
   `Test_RuntimeMesh_Render.cpp`, assert a darkened region) and two separated RuntimeConvex bodies moving
   apart with their displays following (compose Transform + Display + JoltBody on both halves, tick the
   editor Jolt world in LiveExtract mode, capture twice). Lead inspects every PNG under
   `Saved/Automation/RuntimeMesh/Render/`.
2. Shipping ceilings: `native-39` numbers are in PROGRESS. Either keep 2048/4096 with a per-tick time
   budget on the drain (`DrainBudgetPerTick` becomes a time budget), or pick lower counts; document the
   decision in the module `Claude.md` and the spec.
3. Spec amendment: G4 wording (cooked import needs `FApp::CanEverRender()`; `-nullrhi` cooked processes
   drop static-mesh render data, StaticMesh.cpp:7365).
4. Optional: build UnrealPak through the user's IDE and re-stage with `-pak` to prove the IoStore container
   path; the loose-file project-store path is what was proven.
5. Optional cleanup: the cooked fixture marker `Gate1B-v1` in `Test_RuntimeMesh_CookAuthoring.cpp` is a
   campaign name; renaming it requires re-authoring the two cooked assets.

# 4. Gates to rerun after any source change

Toolbox only (`CkAuto/UnrealToolbox.exe`, absolute `--project`); never Build.bat/UBT/raw Automation.

- `--build --target=Editor --test --test-pattern Ck.RuntimeMesh --discover-fresh` (35 rows incl. the
  renderer-only capture and the benchmark; expect 35/35).
- `--test --test-pattern Ck.Jolt.Body.RuntimeConvex` (3/3) and the legacy baselines
  `Ck.Jolt.Body.Lifecycle` (3/3), `Ck.Jolt.Body.OwnershipExclusivity` (3/3), `Ck.Jolt.EditorWorld` (2/2).
- `--test --project-prefix RuntimeMeshCooked --test-pattern RuntimeMeshCooked.PendingImportGC` alone.
- Packaged: `--build --target=Game`, then `Run-CookGate.ps1 -RunName <new>`, then
  `Run-NativeGate.ps1 -RunName <new> -TestOnly -TestPattern RuntimeMeshCooked.Import -ProjectPrefix
  RuntimeMeshCooked -SkipDiscovery -EditorArgs "-CkRuntimeMeshCookedExecutable=<staged Mars.exe>"`.
  The cook must run after the editor build (generated AngelScript must match the binaries) and the game
  must be rebuilt whenever a reflected surface changes.
- Ck ensures log each message twice; expect ensure text with occurrence 0 (at least once), prove
  exactly-once with listener counters.

# 5. Decisions made this session (veto cheaply)

- Loose-file staging instead of pak (no UnrealPak.exe; toolbox cannot build programs).
- Probe runs with `-RenderOffScreen` (engine contract above) and forces `/Engine/Maps/Entry` with
  `GameModeBase` via `FCommandLine::Append` (the project's default map is the Camp map).
- Getters ensure on invalid handles and on non-Ready geometry (`Get_Metrics`, `Copy_LocalVerticesCm`).
- `FProcessor_RuntimeMesh_WorldEndPlay` kept although `CancelWorld` + `SourceEndPlay` carry teardown.
- Display `Add` rejects atomically (invalid handle, nothing added); setup-time failures stay observable.
- Display `Add` takes `FCk_Handle_Transform&`; every reflected spec/request carries `Get_IsValid()` and `Add`
  ensures on it first. CkJolt's legacy `Add(FCk_Handle&)` was deliberately left unchanged (G8).
