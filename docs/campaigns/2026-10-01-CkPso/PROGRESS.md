# CkPso — PROGRESS

PSO preparation for CkFoundation loading screens. Plan of record:
`C:\Users\neilj\.claude\plans\d-users-neilj-downloads-pso-smart-utils-compiled-orbit.md` (approved 2026-10-01).
Module doc: `Source/CkPso/Claude.md`. Engine research for P2: `P2_WARMUP_RESEARCH.md` (this folder).

## State as of 2026-10-08

| Phase | State | Evidence |
|---|---|---|
| P1 drain gate, progress, hitch telemetry | Implemented; editor-side gate run | see "Gate" below |
| P3 miss-log parser (pure text → record) | Implemented and unit-tested; nothing consumes it yet | `Ck.Pso.MissLogParser.*` 14/14 |
| Port onto the CommitAndChill history | **Done 2026-10-08**, editor-gated; push + PRs are the next step | see "Ported to CommitAndChill (2026-10-08)" |
| Measurement stop | Next after the push: BusterBlock temp branch + Buildkite, then three runs; runbook written, not run | `CONTINUATION_PROMPT_CkPsoMeasurement.md` |
| P2 manifest warm-up | Research only (`P2_WARMUP_RESEARCH.md`); scope waits on the measurement | — |
| P3 capture device, miss report, debugger tab | Not started | — |
| BusterBlock adoption | Separate follow-up | — |

## Ported to CommitAndChill (2026-10-08)

Plan: `C:\Users\neilj\.claude\plans\pasted-content-id-2a7d-start-with-expressive-storm.md` (Slices A-D).

`feature/ck-pso` in both repos is now a cherry-pick onto CommitAndChill `dev`; the old branches are kept as
`backup/ck-pso-chainkemists` (also on the old remote as `chainkemists/feature/ck-pso`: `87ecc54ec` / `052dc0eb5`).

| Repo | Base (CommitAndChill `dev`) | Port commits |
|---|---|---|
| CkFoundation | `cf25641a3` | `8fb9d6983` module + gate · `960363393` miss-log parser · `20f3eac61` docs · `dac3c5388` parked docs · this docs commit |
| CkTests | `17b956d50` | `29edb582` two AutoTests · `feb2c9f0` generated wrapper + placed actors |

Other pins in the CkPlugins2 host: CkGameplayDebugger `11d89540a`, CkAuto `e0e756ed8` (toolbox v1.56); superproject
branch `chore/commitandchill-adoption` @`9b28853`. Every toolbox run needed `--engine-path=D:\Repos\UnrealEngine-Angelscript`
(the project's engine GUID is not registered on this machine).

The only cherry-pick conflict was `Source/CLAUDE.md` (module count and list). `git diff --stat origin/dev`: CkFoundation
touches only `Source/CkPso/**`, `CkFoundation.uplugin`, `Source/CLAUDE.md`, `Source/CkLoadingScreen/Claude.md` and this
folder; CkTests only `Script/CkPso/*.as`, `Script/Generated/CkTests_AutoTestActors.as` and the two placed-actor uassets.

### Host gate file split (Slice A, superproject commits `c96abb3`, `9b28853`)

Toolbox v1.52+ reads each plugin's own `AutomationGate.json`, and a test listed twice exits 80. Of the host's 44 entries
from 2026-10-01: **28 dropped** (now carried by the plugin lists: 26 CkTests, 1 CkFoundation, 1 CkGameplayDebugger),
**15 pruned** (passed in both Slice A gates), 1 kept (`Ck.Snapshot.Meta.FragmentPostureCoverage`), and **11 added**
(1 red that OSCILLATES, `Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal`; 10 flaky) -> 12 host entries.
Plugin lists at these pins: CkFoundation 1, CkGameplayDebugger 6, CkTests 37.

### Baseline (Slice A: adoption pins, no CkPso)

Two fresh-boot full gates, toolbox v1.56: **3993 run / 3955 passed / 38 failed / 0 contaminated** both times
(`Saved/Logs/BuildTest-SliceA-Gate.log`, `Saved/Logs/Test-Prune.log`). Another project's gate shared the machine.

### CkPso gates (Slice B: same pins + the two `feature/ck-pso` tips)

- Build: Succeeded; `Module.CkPso` compiled and linked with no source change (`Saved/Logs/BuildTest-SliceB-Pso.log`).
- Scoped `Pso`: **37/37**, exit 0: 28 `Ck.Pso.*` (DrainTracker + MissLogParser), both
  `Ck_AutoTest_Pso_HoldReleasesAfterQuietPeriod` and `Ck_AutoTest_Pso_HoldFailsOpenOnStall`, and 7 unrelated tests the
  substring also matches. `LoadingScreen`: **6/6**, exit 0 (`Saved/Logs/Test-SliceB-LoadingScreen.log`; this history
  has 6 such tests, the old one ran 8).
- The editor boots rewrote nothing in either submodule (`git status --porcelain` empty in both, wrapper unchanged).
- Full gate (`--test --no-live --discover-fresh`, 2 auto-sized editors, BusterBlock_alt's test holding the engine shared
  throughout; `Saved/Logs/Test-SliceB-FullGate.keep.log`): **4023 run / 3984 passed / 39 failed / 0 contaminated**,
  44m 51s, **exit 1**. The 30 extra tests over the baseline are the 30 CkPso tests; all of them passed.
  - 14 reported NEW. **11 are listed `flaky`** (4 host, 6 CkTests, 1 CkGameplayDebugger); they count as new only
    because 11 listed flakies failed, over the toolbox's re-run cap of 10. **3 were unlisted**:
    `Ck_AutoTest_Crowd_Grounding_StationaryAgentReGrounds`, `Ck_AutoTest_Crowd_NarrowGap_TraverseCalm`,
    `Ck_AutoTest_GroundNav_Rebuild_AdjacentPaintDoesNotReplan`.
  - **All 14 pass alone** (`--test --no-live --known-reds off --test-pattern <path>`, `Saved/Logs/Test-SliceB-Alone-{1..14}.log`).
  - None is attributed to CkPso. The two Crowd AutoTests failed on `LogAssetRegistry OpenFile failed` for a CkUsf
    `GeneratedLooksTest/P34748/` scratch package written by another editor; the GroundNav one on another test's
    `ck.astest.nonexistent.cvar` console warning; `IskmRenderer_BatchedVisual` (4.5 s timeout) and
    `PillarsEngageSteppedTops` (65 s step bound) on timing under load. No failure text mentions PSO or the loading
    screen. In their lanes only the pure `Ck.Pso.*` unit tests ran before them; the two Pso AutoTests (the only Pso tests
    that set `ck.Pso.*` cvars) ran in lane 3 and later in lane 4.
  - The 3 unlisted went into the host file as `flaky`, `[owner: CkTests]` (uncommitted in the superproject).
  - Still red (listed): 25. Now passing: `SpiderCourseTraversal` (host, OSCILLATES, not pruned) and 4 plugin reds
    (`Ck.ProceduralAnimation.Gait.LandingProbeLiftsASwingOntoAStep`,
    `Ck.UiAuthoring.EcsDebugger.StateMachineInspector.AuthoredVariants`, `Ck.UiAuthoring.StyleLab.Window`,
    `Ck_AutoTest_Crowd_BunchUp_SettlesAtSharedGoal`), the plugins' to prune by PR. Listed flaky, passed: 15.

## Parked 2026-10-02

No code changed, nothing built, nothing pushed on 2026-10-02. The session was planning and read-only recon.

- `feature/ck-pso` is based on chainkemists dev `faa395509`. BusterBlock is moving its CkFoundation and CkTests
  submodules to CommitAndChill, whose history is rewritten and shares no merge-base with `faa395509`. That adoption
  (BusterBlock branch `feature/ckf-rename-adoption-cnc`) is unsubmitted work in another conversation.
- Neil's call: wait for the adoption to be submitted and the machine to go quiet, then port CkPso onto the new
  history, gate it in the editor there, and measure the game from the post-adoption line.
- Push and PRs stay on hold; the PR target now depends on where the adoption lands.
- The runbook (three runs, flags, what each outcome decides) and the recon facts are in the continuation prompt
  beside this file and in the parked plan `C:\Users\neilj\.claude\plans\pasted-content-id-26ee-start-with-glimmering-leaf.md`.
- Read against engine 5.7.4, not yet compiled: every symbol the `PSO_PRECACHING_VALIDATE` branch uses exists with
  the signature CkPso expects (`PSOPrecacheValidation.h:81,162,225-228,244,327`).

## What P1 is

- `FCk_Pso_DrainTracker` (`Source/CkPso/Public/CkPso/Drain/`): pure state machine, `Idle → Draining → Settling → Complete | TimedOut`.
- `UCk_Pso_Subsystem_UE` (`Subsystem/`): GameInstance subsystem; holds the loading screen through `ICk_LoadingProcess`
  + `Register_LoadingProcessor`; feeds the tracker `FShaderPipelineCache::NumPrecompilesRemaining()`.
- `UCk_Pso_ProjectSettings_UE` / `UCk_Utils_Pso_Settings_UE` (`Settings/`), cvars `ck.Pso.*`, console command `ck.Pso.DumpReport`.
- Tests: `Ck.Pso.DrainTracker.*` (C++), `Ck_AutoTest_Pso_HoldReleasesAfterQuietPeriod` and
  `Ck_AutoTest_Pso_HoldFailsOpenOnStall` (AngelScript, in CkTests `Script/CkPso/`).

## Gate (2026-10-01, CkPlugins2 host, toolbox v1.51)

Pre-change baseline (serial, fresh boot, untouched tree + the host `PlaceableTests` script fix): 3819 run, 3775 passed,
**44 failed**, 0 contaminated. The 44 are recorded as `knownReds` in the host's `AutomationGate.json`. The four roots
that file adds to the gate (108 tests) all passed on the unmodified tree.

Post-change:
- Build: Succeeded; `Module.CkPso` compiled and linked on the first attempt, no source fixes needed.
- Scoped `Pso`: 37/37 (final run after cleanup, `Saved/Logs/Final-Pso.log`). `LoadingScreen`: 8/8.
- Full suite (two lanes, machine shared with three other sessions' test runs): 3959 run, 52 failed = the 44 known reds
  + **8 new**. None of the 8 is attributed to CkPso:

| New failure | Attribution | Evidence |
|---|---|---|
| `Ck.DebugScene.Target.CapacityAndInvalidInputAreAtomic`, `Ck.Jolt.BakeExtraction.Text3DGlyphExclusion`, `Ck.PathNetwork.Vectorize.Loop`, `Ck.UnrealComponent.TransformPropagation.DirtyOwnersOnly` | Victims of a headless-editor crash, not test failures | Same engine fatal each time: `GenericWindow.cpp:113 GetRestoredDimensions is not expected to be called on this platform`, from `FTabManager::SavePersistentLayout` on a ticker. The same fatal is in the baseline log (it killed `Ck.CrowdDebugger.ProductionLifecycle.Release`, a known red). No CkPso frame in any callstack. All four pass alone. |
| `Angelscript.CppTests.AngelscriptCodeCoverage.IntegrationTest` | Passes alone | Failed on missing coverage reports for host scripts |
| `Ck_AutoTest_GroundNav_Link_DisabledMidCrossingHoldsTheBodyAndResumesOnEnable`, `Ck_AutoTest_GroundNav_Link_TraversalHandshakeFiresExactlyOnce` | Pass alone | Their errors in the gate were another test's log lines (a GameSettings deferred-cvar timeout and its console warning) |
| `Ck_AutoTest_Crowd_AvoidanceVolume_InitialPathAvoidsExpandedObb` | **Pre-existing order/load dependence** | Passed in the serial baseline. Fails alone 2/2 with CkPso, fails after its baseline predecessor, and **fails alone with the CkPso module unloaded and its tests removed** (`Saved/Logs/AB-NoCkPso.log`). Same assertion every time: `Avoid If Possible did not fall back through the sealed corridor: ECk_Nav_PathStatus::Partial`. |

Not covered by any of this: the real PSO drain. Precaching is hard-disabled under `WITH_EDITOR`
(`PipelineStateCache.cpp:4408`), so only the simulated path (`ck.Pso.Debug.SimulatedRemaining`) ran.

## Never compiled

`CkPso_Subsystem.cpp`'s `#if PSO_PRECACHING_VALIDATE && UE_WITH_PSO_PRECACHING` branch (validation hit/miss counters)
only exists in non-editor builds. It was checked against `PSOPrecacheValidation.h` by reading. The first packaged
build is its first compile.

## Decisions on record

- New module, not an extension of CkLoadingScreen; subsystem-shaped, API on the subsystem.
- Gate is default-on, bounded by `_DrainMaxWait` (60s) and `_DrainStallTimeout` (10s).
- A stall is a count that has not CHANGED for the stall timeout. A rising count is work still arriving.
- A Complete window re-opens only while the loading screen is still showing — otherwise gameplay PSO requests would
  put the screen back up mid-game.
- Tracker time is game time clamped to 0.1s per tick (Neil, 2026-10-01): avoids a new row in the wall-time fence
  `Ck.Snapshot.Meta.WallTimeReadsAreAllowListed`. Consequence: pause or a snapshot load's time freeze pauses the budgets.
- A drain timeout logs a Warning, not an ensure.
- Miss capture will be a dev-only log parser (Neil, 2026-10-01), not an engine-fork delegate.
- `ck.Pso.Debug.StallTimeoutOverride` exists so the fail-open AutoTest takes about half a second; a 10s test fails on
  any unrelated Warning the editor logs in its window.

## Open items

1. **Measurement (Neil):** package BusterBlock (Development) against this branch; run cold (`-clearPSODriverCache`)
   and warm; collect `ck.Pso.DumpReport` and the log. For miss counters add
   `-ini:Engine:[SystemSettings]:r.PSOPrecache.Validation=2`.
2. P2 scope. The research found four Ck render paths with no PSO precache coverage at all, which no manifest can fix:
   CkIskmRenderer batched clusters (`CkIskmVF4/8` lack `SupportsPSOPrecaching` and a collector), CkPmg procedural
   meshes, the CkPixelArtRenderer upscaler, and the CkUsf outline compute shaders. Details and citations in
   `P2_WARMUP_RESEARCH.md` §6. Confirmed by reading code, not observed in a packaged run.
   **Correction (2026-10-02):** §6 calls the CkUsf outline compute shaders a miss because
   `r.PSOPrecache.GlobalShaders` is 0. It is 1 on Windows (`Engine/Config/Windows/BaseWindowsEngine.ini:26`), so
   that row is unproven either way; the measurement's miss list decides it. §6 was corrected on 2026-10-08.
3. P3 remainder: capture `FOutputDevice`, miss report, debugger surface. The parser reports blocks it recognises but
   cannot decode as unset; the capture layer must count and surface those (engine format drift would otherwise read
   as "0 misses").
4. Progress ratio stays at 100% if a window re-opens. Knob: `DoUpdate_ProgressRatio` in `CkPso_DrainTracker.cpp`.
5. Host-side, not CkPso: the `GetRestoredDimensions` headless crash. The order-dependent Crowd AutoTest above is now
   listed `red` in CommitAndChill CkTests' own list.
6. Port onto the CommitAndChill history: done 2026-10-08; it compiled with no source change. Next: push both
   `feature/ck-pso` branches and open PRs to `dev` (CkTests depends on CkFoundation), then the BusterBlock temp
   branch for the measurement build.
