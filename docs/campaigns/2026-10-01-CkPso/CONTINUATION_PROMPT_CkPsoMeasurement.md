Start with: Fable · high · plan mode yes

## Paste this to resume

```text
Start with: Fable · high · plan mode yes
I'm continuing the CkPso campaign (PSO preparation for CkFoundation loading screens). Read this continuation prompt fully before doing anything: D:\Repos\CkPlugins2\Plugins\CkFoundation\docs\campaigns\2026-10-01-CkPso\CONTINUATION_PROMPT_CkPsoMeasurement.md
On 2026-10-08 feature/ck-pso was ported onto the CommitAndChill history (CkFoundation and CkTests) and gated in the CkPlugins2 editor. Remaining work, in order: confirm the two feature/ck-pso branches are pushed with PRs open to dev, build a packaged Development BusterBlock from a temp branch pinning the pushed CkFoundation tip (Buildkite, Neil runs it), Neil's three measurement runs, read the logs, then re-plan P2/P3 in plan mode.
Start by checking the Repo state table against git (both submodules, the superproject and the remote branches), then ask Neil which of the next steps have already happened.
```

# CkPso — ported to CommitAndChill; ship, then measure

Picking up: P1 (the drain gate) and the P3 miss-log parser are ported onto the CommitAndChill history and editor-gated
there. What is left is publishing the branches, the packaged measurement, and re-scoping P2 (warm-up) and the rest of P3
(miss capture) from its numbers.

This file was rewritten in place on 2026-10-08 and replaces the 2026-10-02 ("parked") version of itself. If anything
quotes "wait for the adoption" or "port feature/ck-pso" as the next step, that is the old text.

As of: 2026-10-08 · CkFoundation `feature/ck-pso` (CommitAndChill; this file's commit sits on `dac3c5388`) · CkTests
`feature/ck-pso` @`feb2c9f0` · CkGameplayDebugger @`11d89540a` · CkAuto @`e0e756ed8` (toolbox v1.56) · CkPlugins2
superproject `chore/commitandchill-adoption` @`9b28853`

If HEAD has moved past this, re-check Repo state before trusting the rest.

## Repo state

Checkout: `D:\Repos\CkPlugins2`. The submodules' `origin` is now `github.com/CommitAndChill/<Repo>`; the old history is
the `chainkemists` remote.

| Repo | Branch | Base | Commits | Pushed |
|---|---|---|---|---|
| `Plugins/CkFoundation` | `feature/ck-pso` | CommitAndChill `dev` `cf25641a3` | `8fb9d6983` module + gate · `960363393` miss-log parser · `20f3eac61` campaign docs · `dac3c5388` parked docs · `docs(pso): record the CommitAndChill port and its gates` | see the Slice C report (2026-10-08 plan) |
| `Plugins/CkTests` | `feature/ck-pso` | CommitAndChill `dev` `17b956d50` | `29edb582` two AutoTests · `feb2c9f0` generated wrapper + two placed actors | see the Slice C report |
| `Plugins/CkFoundation`, `Plugins/CkTests` | `backup/ck-pso-chainkemists` | old chainkemists history | the pre-port branch (`87ecc54ec` / `052dc0eb5`) | yes, as `chainkemists/feature/ck-pso` |
| superproject | `chore/commitandchill-adoption` (off `dev-ckplugins2`) | `origin/dev` + `815a001`, `6cabe81` | `33458aa` submodules → CommitAndChill · `c96abb3` drop 28 host reds · `9b28853` prune 15 / list 11 | see the Slice C report; PR to `dev` planned |

The superproject's working tree has the CkFoundation and CkTests gitlinks at the `feature/ck-pso` tips, **uncommitted**
on purpose: a committed gitlink must name a pushed SHA, and the pointer bump belongs after the PRs merge.

Plans:
- Port + ship plan (2026-10-08, Slices A–D): `C:\Users\neilj\.claude\plans\pasted-content-id-2a7d-start-with-expressive-storm.md`.
- Parked plan (runbook, outcome table, recon facts, the original Build route): `C:\Users\neilj\.claude\plans\pasted-content-id-26ee-start-with-glimmering-leaf.md`.
- Original campaign plan: `C:\Users\neilj\.claude\plans\d-users-neilj-downloads-pso-smart-utils-compiled-orbit.md`.
- Living state, gate numbers and attributions: `PROGRESS.md` beside this file ("Ported to CommitAndChill (2026-10-08)").

Not this campaign's, never stage: superproject `Config/DefaultGameplayTags.ini`,
`Script/Generated/CkPlugins_EntitySpawnParams.as`, untracked `.claude/skills/*.md`, and the `Content/**` uassets that
`Test_EntityScript_EditorServices` leaks (see Gotchas).

## Confirmed vs inferred

Confirmed on 2026-10-08:
- The port: `git diff --stat origin/dev` lists only `Source/CkPso/**`, `CkFoundation.uplugin`, `Source/CLAUDE.md`,
  `Source/CkLoadingScreen/Claude.md` and `docs/campaigns/2026-10-01-CkPso/**` in CkFoundation; only `Script/CkPso/*.as`,
  `Script/Generated/CkTests_AutoTestActors.as` and the two placed-actor uassets in CkTests.
- CkPso compiles and links in the editor on the new history with no source change (`Saved/Logs/BuildTest-SliceB-Pso.log`).
- Gates (details and attribution in `PROGRESS.md`): scoped `Pso` 37/37 incl. both Pso AutoTests, `LoadingScreen` 6/6;
  full gate 4023 run / 3984 passed / 39 failed (baseline without CkPso: 3993 / 3955 / 38), exit 1 on 14 NEW = 11
  listed flakies over the re-run cap + 3 unlisted AutoTests; all 14 pass alone, none attributed to CkPso. The 3 went
  into the host `AutomationGate.json` as `flaky` (superproject, uncommitted at the time of writing).
- `r.PSOPrecache.GlobalShaders=1` on Windows (`Engine/Config/Windows/BaseWindowsEngine.ini:26`; 1 = global compute
  shaders, `PSOPrecache.cpp:19`). `P2_WARMUP_RESEARCH.md` §6 is now corrected.

Inferred (what would confirm):
- CkPso compiles and links in a non-editor target, including the never-compiled
  `#if PSO_PRECACHING_VALIDATE && UE_WITH_PSO_PRECACHING` regions (`CkPso_Subsystem.cpp`) — the first packaged build.
- The pending count drains to zero on a cold run instead of stalling — measurement run 1. **This is the claim most
  likely to be wrong.**
- Plugin cvars set with `-ini:Engine:[SystemSettings]:ck.Pso.…` before the module loads are applied at registration —
  the gate line in the run's log.
- Everything listed as inferred on 2026-10-01/02 and not re-checked here (Fab plugin precompile is a no-op, three Ck
  render paths lack precache coverage, the CkUsf outline row is open either way).

## Next steps, in order

1. **Push + PRs, if the Slice C report says they are not done:** `git push -u origin feature/ck-pso` in
   `Plugins/CkFoundation` and `Plugins/CkTests`, then PRs to CommitAndChill `dev` (`/ck-ship-pr`). The CkTests PR depends
   on the CkFoundation one (the tests call `UCk_Pso_Subsystem_UE`); never bump the CkTests pin ahead of CkFoundation's.
   The superproject `chore/commitandchill-adoption` goes to CkPlugins as a PR to `dev`. Pointer bumps come after the merges.
2. **BusterBlock temp branch** (plan Slice D): in a throwaway shallow clone in the scratchpad, never in
   `D:\Repos\BusterBlock` or `BusterBlock_alt` (others' work; not even a fetch): BusterBlock `origin/dev` tip with the
   CkFoundation gitlink set to the **pushed** `feature/ck-pso` tip, pushed as `temp/ck-pso-measure`.
3. **NEIL: Buildkite** on that branch: Development, pristine cook, AngelScript vm, AutoTests off, Steam deploy to a spare
   branch. CkTests is not part of the measurement build.
4. **NEIL: the three runs**, on a machine with the other sessions' builds and tests paused for about 20 minutes:

   | Run | Flags on top of the common set | Answers |
   |---|---|---|
   | 1 cold, gate on | `-clearPSODriverCache` | Does the drain reach zero; peak and duration; what still hitches |
   | 2 warm, gate on | — | What the driver cache carries; cost of the gate on a normal launch |
   | 3 cold, gate off | `-clearPSODriverCache -ini:Engine:[SystemSettings]:ck.Pso.WaitForPrecache=0` | What the gate buys |

   Common set: `-ini:Engine:[SystemSettings]:r.PSOPrecache.Validation=2 -ini:Engine:[SystemSettings]:ck.Pso.LogDrainEveryFrame=1
   -LogCmds="CkPso Verbose, LogPSOHitching Verbose" -abslog=<folder>\pso_<n>_<name>.log`.
   Same route each time: boot, ~10 s at the main menu, start or load the same game, ~3 minutes through a crowd, a
   VFX-heavy moment and the menus, back to the main menu, quit from the menu. Back up `Saved\SaveGames` and
   `Saved\Config` first; do not delete `Saved`. No console typing is needed.
5. Read the three logs against the outcome table in the parked plan; write the numbers into `PROGRESS.md`; settle the
   CkUsf outline row in `P2_WARMUP_RESEARCH.md` §6 from run 1's miss list.
6. Re-plan P2/P3 in plan mode. Then delete `temp/ck-pso-measure`.

## Decisions made rather than asking (each has an opt-out)

From 2026-10-08 (the port plan):
- The port is a cherry-pick onto new `feature/ck-pso` branches off CommitAndChill `dev`; the old branches were renamed
  `backup/ck-pso-chainkemists`. Opt-out: name the new ones `feature/ck-pso-cnc`.
- Slice A's gate (the adoption pins, no CkPso) is the baseline for the CkPso gate; no separate pre-change run.
- Both full gates used `--discover-fresh` (new toolbox, new history).
- New flaky entries go to the host `AutomationGate.json` with `[owner: <repo>]`; they belong in the owning plugin's
  list once they are red in a gate there.

From 2026-10-01/02, still standing:
- Gate is default-on with fail-open budgets. Opt-out: `_WaitForPsoPrecache` default Disable.
- Stall = count unchanged for the timeout, not "not decreased". One line in `CkPso_DrainTracker.cpp`, one test block.
- A Complete window only re-opens while the loading screen is showing.
- Subsystem ticks without a game viewport (the headless tests need it).
- `ck.Pso.Debug.StallTimeoutOverride` (non-Shipping) exists so the fail-open test takes about 0.5 s.
- AutoTests are named `HoldReleasesAfterQuietPeriod` / `HoldFailsOpenOnStall` (the originals were a prefix of one another).
- Commit messages carry no Co-Authored-By footer (the repo's commit hook says so).
- CI is the primary build route, three runs rather than two, validation level 2 on all three.

## Critical files

- `C:\Users\neilj\.claude\plans\pasted-content-id-2a7d-start-with-expressive-storm.md` — port + ship plan (Slices A–D).
- `C:\Users\neilj\.claude\plans\pasted-content-id-26ee-start-with-glimmering-leaf.md` — runbook, outcome table, recon facts.
- `Plugins/CkFoundation/Source/CkPso/Claude.md` — module doc: API, cvars, the inert rule, telemetry, ini recipe.
- `.../CkPso/Public/CkPso/Subsystem/CkPso_Subsystem.{h,cpp}` — arming, inert rule, hitch report, every log line the
  measurement reads; the never-compiled validation regions.
- `.../CkPso/Public/CkPso/Drain/CkPso_DrainTracker.{h,cpp}` — the state machine; all timing semantics.
- `.../CkPso/Public/CkPso/Settings/CkPso_Settings.{h,cpp}` — project settings and the `ck.Pso.*` cvars.
- `.../CkPso/Public/CkPso/Diagnostics/CkPso_MissLogParser.{h,cpp}` — P3 parser, not wired to anything.
- `Plugins/CkTests/Script/CkPso/*.as` — the two AutoTests.
- `D:\Repos\CkPlugins2\AutomationGate.json` — host roots + host-only known reds; plugin lists beside each `.uplugin`.

## Ruled out / already proven

- Measuring on the old chainkemists history before the port: declined by Neil on 2026-10-02.
- Checking BusterBlock's CkFoundation out at an old-history SHA: no shared history with CommitAndChill.
- Building or fetching in `D:\Repos\BusterBlock` / `BusterBlock_alt`: other conversations' in-flight work.
- A capture device for the measurement: not needed, miss blocks can be aggregated offline from the log.
- Shader precompile, runtime thread-pool sizing, runtime validation toggles, live "repair" sessions, an engine-fork
  miss delegate, `PrecachePSOsBoostToHighestPriority`: dropped, reasons in the original plan.
- CkPso causing a gate failure: ruled out on the old history (2026-10-01 table) and on the new one (PROGRESS.md,
  2026-10-08 section).
- `Ck_AutoTest_Crowd_AvoidanceVolume_InitialPathAvoidsExpandedObb`: pre-existing, now listed `red` in CommitAndChill
  CkTests' own list.

## Gotchas

New on 2026-10-08:
- **CkPlugins2's `EngineAssociation` GUID is not registered on this machine.** Every toolbox run needs
  `--engine-path=D:\Repos\UnrealEngine-Angelscript` (per run; do not edit the registry or the `.uproject`).
- **CkAuto LFS smudge fails after the remote rename** (`origin` now CommitAndChill): fetch the objects explicitly with
  `git lfs fetch origin <sha>` in `CkAuto`, then check out again.
- **`Test_EntityScript_EditorServices` leaks assets into the host `Content/`** (`Content/__CkEntityScriptEditorServices_*`,
  `Content/EntitySpawnParams/*`, `Content/CkPlugins/Cube_EntityScript.uasset`), and the editor's SCC provider may stage
  them. Check the index before every superproject commit; never `git add` a directory.
- **`Ck_AutoTest_ProceduralAnimation_SpiderCourseTraversal` oscillates** (red, red alone twice, then green in the next
  gate). It is listed `red` with an `OSCILLATES` reason, so it detects nothing while listed; do not prune it on one pass.
- **Concurrent gates on the machine produce load flakes**: renderer-pass physical-click / clipboard assertions, and
  AutoTests failing on `LogAssetRegistry OpenFile failed` for a CkUsf `GeneratedLooksTest/P<pid>/` scratch package written
  by another editor. Run each new failure alone before calling it anything.
- `--test-pattern Pso` is a substring match: it also selects unrelated tests whose names contain "pso" across a word
  boundary (`...Loops On...`, `...Drops Only...`). Read the per-test list, not just the count.

From 2026-10-02:
- **The toolbox has no package verb.** Packaging is BuildGraph (`Package Clients`) or CI, and it needs the engine lock
  exclusively, which the other sessions' test runs starve.
- **`ClientTargetName` must be set to `BusterBlock`** for a local BuildGraph run (agent-reported, `Build.xml:304`).
- **`Get-ProjectEnginePath.ps1` fails for BusterBlock** (unregistered GUID), and PowerShell script execution is blocked
  by policy on this machine. Hardcode `D:/Repos/UnrealEngine-Angelscript`.
- **Miss blocks are multi-line and unprefixed.** Use `grep -A14`. There is no `-logPSOPrecacheMiss` switch.
- **Packaged builds print raw enum names** (`TimedOut`, `MaxWait`); the editor prints spaced display names.
- **`-clearPSODriverCache` wipes the GPU vendor's whole DX cache folder** (D3D12 only). Counts survive a busy machine;
  durations and the 20 ms hitch counter do not.
- **`-ini:` overrides are ignored in Shipping**; the measurement build must be Development.
- **A zero peak everywhere means the build or flags are wrong**, not that there is nothing to precache.
- BusterBlock_alt and BusterBlock share one `.git` object store; a fetch in one moves remote-tracking refs for both.

From 2026-10-01, still true:
- `--generate` starves on the shared engine lock; a plain `--build` picks up a new module.
- `Ck.Snapshot.Meta.WallTimeReadsAreAllowListed` counts wall-time reads per file.
- CkFoundation ignores `*.md` outside `.claude/` and `docs/`; a module's `Claude.md` needs `git add -f`.
- Automation matches test names by substring: never name one AutoTest as a prefix of another.
- Removing an AutoTest leaves its placed actor behind; the stale actor runs as a fake pass.
- The editor's source-control integration stages placed-actor uassets during test boots; check the index before committing.
- Every Warning logged while an AutoTest runs fails it; declare expected ones via `Get_ExpectedLogErrors`.
- Headless editor crash `GetRestoredDimensions is not expected to be called on this platform` kills whichever test is
  in flight; worse with more lanes.
