> **Historical checkpoint, retained 2026-10-12.** This September handoff preserves campaign evidence. Its branch revisions, pending steps and permissions describe that checkpoint; verify current source and PROGRESS.md before resuming. It does not authorize operations on the current multiplayer branch.

# One-line summary

Prepare BusterBlock compatibility with the GroundNav candidate and a safe, agent-scriptable Recast/GroundNav A/B choice between matched runs, while keeping production Recast, preserving all work, and **never advancing dev until the CTO explicitly approves readiness**.

2026-09-10 handoff. This is the authoritative next-work instruction. It supersedes older navigation-next wording that required finishing all editor acceptance before starting BusterBlock, or suggested publishing CK dev before BusterBlock validation. Older evidence remains valid only within its stated scope.

## Gate126 current focused-NPC checkpoint (2026-09-11)

**Focused NpcAI is 27/27.** Gate126a reused the Gate125 Development Editor binaries and ran all 27 rows in one fresh boot, 6m4s, Toolbox/editor exit 0, with zero failed/skipped/contaminated. The remaining `LivenessWatchdogAccrual` failure was a fixture identity mismatch: the synthetic subject lacked the production CrowdAgent alias. The corrected fixture composes the NPC as its own CrowdAgent and retains the original watchdog assertions and timings. A test-only stimulus driver keeps leaf/location inputs controlled between FrameCache and ObserverSafety; production code is unchanged. See `GATE126_NPCAI_LIVENESS_FIXTURE.md` and `GATE126_VALIDATION_EVIDENCE.json`.

The editor generated the expected 24-line test driver during startup and completed its script reload before automation. Source identity was captured after generation, before any test, and remained unchanged through completion; compiled artifacts and production code match entry. All eight working HEADs/local-dev refs/indexed gitlinks are unchanged, protected files remain 13/13, and the shared framework/test mirror remains 16/16. No AngelScript errors/warnings or ensures occurred; the two known missing external-actor paths each appeared twice during startup.

**Remaining scope:** newer BusterBlock tracked `origin/dev` tips still require a controlled preservation/rebase checkpoint; no rebase or additional commit occurred here. Production remains Recast with shadow Off. Gate124 A/B evidence remains historical after the EQS rebuild, and no fresh A/B, package, streaming, editor-acceptance or performance claim is made. All deferred and unrelated items below remain retained; the seven approved gyms remain closed. No local cook/package, push or dev advancement was performed.

## Gate125 historical focused-NPC checkpoint (2026-09-11)

**Current focused result:** Gate125d built the compatible Development Editor in 61.72 seconds and passed `CkEqs` 2/2. Gate125e then ran all 27 `NpcAI` rows in one boot: **26/27**, 0 skipped/contaminated. Only the pre-existing `LivenessWatchdogAccrual` live-line exemption remains (1.066707 s; entry baseline about 1.066671 s). The five new diagnostic failures, `BelowFloorRecovery`, and `SidewalkOwnedRoute` pass; there are no new failure names versus entry. `GATE125_VALIDATION_EVIDENCE.json` is authoritative: eight repository heads/local-dev refs unchanged, protected files 13/13, mirrored files 16/16, six ancestry checks, and final source/artifact identity unchanged before/after Gate125e. This focused result is not broader readiness.

**Current rebase debt:** BusterBlock tracked `origin/dev` advanced independently after Gate124 to root `c314b1ed7d81b9f7461afb7900e85dc8f425de0d`, Foundation `369d6766d6fd62537a61886f391cb5aa2b5660fc`, and Tests `b8087d6eff8fdaa3e93452630db16568e2268d98`; none is an ancestor of the current candidate HEAD. Host tracked refs are unchanged. The candidate’s local-dev refs, working HEADs, index gitlinks, protected 13 files, and final-run identity remain unchanged, so Gate125e measures the current local candidate only. The six prior ancestry checks cover Gate124’s fetched dev/entry pins, not these new BusterBlock tips. Before any further rebase, establish a controlled preservation checkpoint for the uncommitted implementation in the shared checkout. Gate125 did not attempt a rebase or make additional commits.

## Historical Gate124 verified isolated-A/B checkpoint (2026-09-11)

**Historical isolated-A/B state:** GateL built the prior Development Editor in 14.65 seconds. The isolated scriptable A/B scenario passed all three fresh `-c` manifest runs: Recast, GroundNav, and Recast rollback each have exit 0, unchanged/matching identity, 24/24 policy checks, Crowd arrival, cleanup, and markup retirement. The reports measure effective Recast 35/144/default cell height 1, with the open/tall fixture retaining the distinct GroundNav/Crowd 42/192 profile. This establishes only the isolated native filter/markup/Crowd contract; it is not a production-readiness, packaged, performance, or broader gameplay-equivalence result.

The controlled fixture uses markup half-Y 300. The prior half-Y 325 failures are preserved: their painted regions extended 25 units beyond wall centers and did not isolate policy lanes. The correction does not establish production overlap parity. The happy-path cleanup does not close the direct async GroundNav markup-rejection rollback test gap, and installed-filter-policy hot mutation remains unsupported. The startup FText self-test error remains an open startup diagnostic.

Host root/Foundation/Tests remain `a518a99d5` / `e9a833ddc` / `f3e7501a`; BusterBlock root is `a16b2bce5` on `feature/navigation-ab-readiness`. `GATE124_FINAL_CHECKPOINT.json` remains historical isolated-A/B identity evidence: its three prior A/B trials and their FText self-test errors do not describe the rebuilt Gate125 artifact. Host binaries were not rebuilt after the earlier 14/14 mirroring, an explicit historical limit. `GATE125_VALIDATION_EVIDENCE.json` records the current 16/16 mirror and Gate125e before/after identity. New compatibility implementation is uncommitted; the approved earlier candidate checkpoints were committed; local dev remains unchanged, and no cook, push, or publication occurred. The sections below are historical entry provenance and retained obligations, not steps to repeat.

**Historical entry evidence:** the initial audit and first focused baseline ran before the verified isolated A/B checkpoint. Read `GATE124_COMPATIBILITY_AND_REBASE_PLAN.md`, `GATE124_COMPATIBILITY_SOURCE_AUDIT.md`, `GATE124_ENTRY_EVIDENCE.json` and `GATE124_RECAST_BASELINE_EVIDENCE.json` before repeating work. At that historical entry checkpoint the original root/plugin revisions below still applied; only origin/dev tracking refs and campaign docs had been updated. All original protected hashes, presentation/source/binary/log hashes and fixture MD5 matched. Offline checks passed 21/21 and accepted four historical records.

**Historical entry ancestry:** host Foundation **35 behind / 66 ahead**, Tests **14 behind / 77 ahead** refreshed dev; neither candidate contains the game's current plugin pin. Both game pins are contained in refreshed dev. No candidate bump, rebase, commit or stash has happened. The plan's **four local candidate commits plus path-scoped preservation of three tracked unrelated files** was explicitly approved by the user on 2026-09-10 after this checkpoint; that scoped plan is now complete; inspect PROGRESS and do not repeat its commits, stashes or rebases. No push/dev advancement is authorized.

**Historical unchanged-game `NpcAI` baseline:** **22/27**, exit 1, one boot, unchanged Development Editor binaries. Failed rows: BelowFloorRecovery, DownedOnDamage, LivenessWatchdogAccrual, SidewalkOwnedRoute, TransientStallKeepsCheckoutQueueSlot. All carry a missing source-control remote-ref diagnostic; LivenessWatchdogAccrual also fails the live-queue exemption assertion (1.066671s stalled). Retain both failure classes separately; no unrelated fix or timeout increase was made. Raw logs contain no AS compile errors/warnings, one expected duplicate-provider ensure and two missing external-actor startup errors. Editor exit 255 is the requested test-failure exit after all rows, not separate crash evidence.

The verified isolated A/B scope is Crowd route solving while direct projections and sidewalk extraction retain Recast. AccessZone/Entryway compatibility markup, all eight native definitions, and the per-run startup selector are implemented in the local candidate. The scenario is verified only on its authored map and retains the deferred obligations below.

# Historical entry repo state

Use only the existing checkouts D:\Repos\CkPlugins_3 and D:\Repos\BusterBlock. Initial conversation workspace should be D:\Repos\CkPlugins_3. Refresh these snapshots before any mutation; they are not permission to reset a changed checkout.

| Repository | Branch | Verified HEAD / gitlink | State at handoff |
|---|---|---|---|
| CkPlugins_3 root | feature/ck-navigation | fd9f6abf7d0c9d4de95d1f98b714d11c48ca7101 | Dirty, preserved below |
| Its CkFoundation | feature/ck-navigation | ea6f8bf0b714bd4a398022c87f72a8e3f2d06ccb | Dirty candidate |
| Its CkTests | feature/ck-navigation | cfd3ea1704ba45f5ec1feca48b4ac46ac541dc35 | Dirty candidate |
| Its CkAuto | current checked-out commit | 5532971dbbb821bf8e5420e350a61afb79d0eb3a | Root gitlink agrees |
| BusterBlock root | feature/npc-voice-transform-follow | eab2de2ab8ab83bee512e605cc7e4dca518a86c1 | Clean status snapshot |
| Its CkFoundation | bugfix/late-game-jolt-contact-churn | b46cc762bc6d5a0452ad2470eba760b85955848d | Clean; root gitlink agrees |
| Its CkTests | bugfix/late-game-jolt-contact-churn-tests | c3b7c8ffe3ec2628f27ccecc1b1ea04909070ab2 | Clean; root gitlink agrees |
| Its CkAuto | root-recorded gitlink | 676b70c457500d6f9f139042743c6eea3aadd068 | Verify checked-out revision before use |

CkPlugins_3's other root gitlinks were unchanged on refresh. No fetch occurred during planning: cached origin/dev...HEAD is Foundation 3 behind / 66 ahead and Tests 0 behind / 77 ahead. These are **not current remote guarantees**. BusterBlock's two plugin tips were not available as objects in CkPlugins_3's corresponding local stores during the planning check. Fetch and reconcile actual ancestry later; do not assume the navigation branch includes the game's newer CPU/lifetime fixes or replace them blindly.

Committed navigation work already exists on feature/ck-navigation (including streaming and editor snapping). Showcase, benchmark, diagnostics and current documentation remain uncommitted; no commit, push, merge, rebase, or BusterBlock source change was performed in the closing/planning turns.

**Explicitly preserved unrelated dirt in CkPlugins_3:**

- Config/DefaultGameplayTags.ini and Script/Generated/CkPlugins_EntitySpawnParams.as.
- Root CONTINUATION_PROMPT_CkNavigationReplacementCampaign.md.
- Foundation Source/CkNavigation/Public/CkNavigation/Settings/CkNav_ProjectSettings.h: pre-existing line-ending-only dirt. Preserve exact bytes; do not normalize it incidentally.
- Tests Script/CkGroundNav/CONTINUATION_PROMPT_GroundNavTuningRange.md.
- Eight existing files under Content/CkJoltData/CkTests/GroundNavAcceptance/Maps/GroundNavStreamingAcceptance: .uasset/.ckexport pairs for JoltCell_-1_0, JoltCell_0_-1, JoltCell_0_0 and JoltIndex. These are user data, not cleanup targets.

The first thirteen hash rows in GATE119_SHOWCASE_BASELINE_AND_CONTRACT.md identify that unrelated dirt. GATE119_PRESENTATION_ARTIFACTS.md identifies 21 approved presentation files (AS adapters/shared HUD/controller, generated wrapper and external map actor). All 34 matched after Gate123. Its old DLL hashes are historical; use GATE123_LOCAL_BENCHMARK_EVIDENCE.json for current benchmark source/binary identities.

Also preserve the dirty benchmark map, native runtime actor/navigation config/editor tests, module dependencies, timing diagnostics/tests, Python validator and all campaign docs. GATE123_LOCAL_BENCHMARK_EVIDENCE.json lists 17 source/tool files, four DLLs and five logs. The new handoff and pointers in PROGRESS/previous prompt are additional documentation dirt. A clean BusterBlock status does not imply its ignored local notes or artifacts are disposable.

# Current limits and retained questions

## Current CTO decisions

- "Do NOT merge to dev. Keep rebasing to dev until we're ready." Colleagues routinely pin dev; merely landing there exposes the candidate. Do not merge, fast-forward, push or otherwise advance any dev branch. Rebase feature branches onto refreshed dev at safe checkpoints, then revalidate.
- Active phase: **BusterBlock compatibility and A/B readiness**, not full production GroundNav migration.
- Prepare for coding agents to select providers for matched A/B runs. Recommended first scope is scriptable **per-run/world startup selection**, not mixed-provider NPC cohorts or mid-route hot-switching. If "Agents" means individual gameplay agents switching live, clarify before expanding architecture.
- Keep BusterBlock's production default explicitly Recast and shadow mode off. Retain its Recast nav data, direct sidewalk path and rollback.
- Seven gyms were run and approved by the user; "No need to check them further." Do not reopen them or demand screenshots.
- Level-editing checks are tabled, **not cancelled**. Preserve the backlog below.
- Build-machine performance was deferred to BusterBlock integration. Preparing A/B controls now does not claim or automatically authorize completing the full performance campaign immediately.
- Recommended main model: gpt-6-astra / high; routine bounded work can use gpt-5.6-terra / medium. Lead retains compatibility semantics, architecture, integration and evidence decisions. Confirm actual model settings through recommend-model-effort at session start; do not infer them from role names.

## Confirmed compatibility hazards

1. Incoming CkNav_ProjectSettings.h defaults to GroundNav. BusterBlock's Config/DefaultCkFoundation.ini navigation section (122-126) has no explicit Ck provider pin. Engine Recast settings alone are not a Ck provider selection.
2. Exactly eight mappings still name legacy UNavigationQueryFilter classes: Customer, CustomerShopping, Employee, Goon and each .Strict variant. Incoming _QueryFilters expects soft references to UCk_NavFilterDefinition_DataAsset. A provider pin alone does not preserve filters.
3. CustomerShopping has its own dynamic Entryway boundary; strict filters additionally exclude stationary-crowd markup. Preserve exact restrictions/costs and required nav-area registration, not just eight tag names.
4. Bb_SidewalkPathNetworkDetector.cpp directly casts default nav data to ARecastNavMesh at multiple sites; retain the real Recast backing rather than pretending the Ck facade switch migrates this code.
5. Existing local crowd evidence does not establish convergence or game equivalence: Gate123 GroundNav reached 1/240 (239 PathPending); Recast reached 0/240 (233 Walking, 7 PathPending) at the sample boundary. Both passed the fixed-window harness conditions only.

## Deferred and unrelated work — do not lose these rows

- [ ] **Editor snap, outside PIE:** with a baked field, place/move PathNetwork nodes on flat ground, ramps and the upper of overlapping floors. Off-surface placement must be explicitly unsnapped. Check responsiveness and wrong-floor failures.
- [ ] **Editor undo/redo:** Ctrl+Z/Ctrl+Y must restore node positions, route data and connections without stale state.
- [ ] **LiveExtract:** edit representative large static geometry inside a GroundNav volume; record extraction/update timing, field identity/epoch and localized preview changes. Undo/redo must re-sweep correctly. This uses collision from the current editor scene, not proof from a runtime gym.
- [ ] **Real World Partition/data layers:** authored cell/layer load, deactivate/reactivate, replacement, unload/reload with idle, installed and in-flight routes. Native in-memory fixtures do not prove these events or persistent authored manifests.
- [ ] **Real cook/manifests:** commandlet DryRun followed by build-machine write/cook; inspect all-profile manifests/index/tiles, selector, identity/fingerprint and loaded-from-cook behavior. Sequential save can orphan assets on I/O failure; fix I/O and recook, never claim transactional disk rollback.
- [ ] **Package acceptance:** Development/Test execution and Shipping compile/package/provider-selection/load checks, including appropriate production-map behavior. No local cook/package. Functional BusterBlock package compatibility is separate from deferred performance and remains necessary before a strong no-break release claim.
- [ ] **Matched performance:** at least three eligible alternating provider pairs on one Development-game artifact, same scenario/settings/seed, full run/build/machine/map provenance, raw samples and provider Insights traces. Behaviorally different pairs are inconclusive, not wins.
- [ ] **Production adoption:** only after isolated-map and required package/behavior gates, migrate one real BusterBlock map and exercise rollback. This remains later than compatibility landing; no global GroundNav opt-in now.
- [ ] **Named Recast debt:** Gate102 Queue front member did not settle its claimed slot within 240 polls / 6.08 seconds. A third timeout increase was rejected. Diagnose if reproduced/relevant to BusterBlock; do not silently increase timeouts or hide it.
- [ ] **Inherited non-navigation suite failures:** historical Gate112 was 1867/1877; eight SceneNodeTween rows, ScriptProcessor_PumpStopsAfterMarkerDrain and VisualLod_RenderBandProfileLifecycle. Compare names against fresh evidence, do not call them current failures without rerunning or absorb their fixes into this task.
- [ ] **Other BusterBlock work:** preserve the committed NPC voice/transform-follow and CPU/lifetime/contact-churn work represented by its current branch/pins. Its separate CONTINUATION_PROMPT_LateGamePerfNewReports.md is a 2026-09-04 historical CPU handoff, not current navigation authority or a verified current performance result. Do not resume it automatically.

# Why prior fixes or investigations were insufficient

CkPlugins host passes are not BusterBlock acceptance. A direct pointer bump changes both default provider and filter representation; merely choosing Recast cannot repair legacy filter loading. The recent A/B harness proves native service dispatch, timing availability, fixed-window accounting and cleanup—not representative game convergence, cooked data or performance equivalence.

Gate121 failed compilation; Gate121b exposed brush creation/bounds; Gate121c exposed a continuous Walking-only check that rejected normal replans; Gate121d passed counts but left agents running during deferred validation, producing post-sample timeouts. Gate123 snapshots outcomes, requests public Crowd Stop, waits for Idle and then validates captured positions. Do not resurrect these defects or treat prior red/provisional gates as final evidence.

The prior proposed release sequence was rejected by the user because colleagues consume dev. The candidate must stay off dev throughout compatibility/A/B preparation.

# Available diagnostics and the first evidence to collect

**First action:** read local guidance, refresh both roots/submodules/dirty paths and hash manifests, then produce the BusterBlock compatibility baseline and exact candidate ancestry/ownership plan. No first-action rebase, pointer bump, build or broad staging.

- Read applicable AGENTS.md or CLAUDE.md in each checkout. Before game edits read BusterBlock RESILIENCE_TENETS.md; designer ambiguity belongs in docs/superpowers/notes/NPC-Customer-Employee-designer-questions.md, not an invented gameplay rule.
- Read current filters' constructors/callers and actual area registrations, provider boot selection, crowd routes and sidewalk detector before designing the bridge.
- Record the exact before/after plugin revisions and build configuration, focused test names, real failed diagnostics and baseline behavior. A dirty local candidate's commit SHA alone is not its full identity.
- Gate123: Saved/Logs/S14-Gate123-MatchedBenchmarkFinal.log, **5/5**, exit 0; SHA256 4649f895c26e07c940c6298b2a6b83769515ad52823a7322747f142d52f79157.
- Gate122: Saved/Logs/S14-Gate122-PathDiagnostics.log, **7/7**, exit 0; SHA256 4252ba9676b4278caa5b9a4c32238870cdf87678d95fac6366116f14a111411a.
- Validator **21/21**, plus four real eligible reports. Recheck offline if useful; do not boot unchanged gyms merely to resume.
- Historical presentation gates: Gate119c GroundNav 523/523 (historical Iris network ensure retained), Gate120 PathNetwork 108/108. Gym approval is user attestation, not an archived screenshot/log claim.
- Next navigation campaign gate number: **124**, only for a warranted new slice. Runtime log paths rotate; compare recorded hashes before attributing evidence.

~~~powershell
# Read-only/offline checks from D:\Repos\CkPlugins_3
python -B -m unittest discover -s Plugins/CkTests/Tools -p test_nav_benchmark_report.py -q
python -B Plugins/CkTests/Tools/nav_benchmark_report.py --require-eligible Saved/Logs/S14-Gate123-MatchedBenchmarkFinal.log
~~~

# Likely symptoms, causes and files

| Symptom | Likely mechanism to verify | Files |
|---|---|---|
| Game has no usable Ck navigation after update | Incoming GroundNav default with no published game field | CkNav_ProjectSettings.h; BusterBlock DefaultCkFoundation.ini |
| NPCs ignore customer/shopping restrictions or emit filter ensures | Legacy class config cannot load as neutral definition; default-filter fallback weakens policy | BB_NavQueryFilters.cpp/.h; CkNavFilterDefinition_Registry.cpp; Recast adapter |
| Sidewalk discovery returns empty masks | Direct Recast backing missing or default nav data no longer Recast | Bb_SidewalkPathNetworkDetector.cpp |
| Candidate loses recent game fixes | Navigation feature history does not contain the game's current plugin pins | Root gitlinks; refreshed plugin ancestry |
| Green sample but stalled/replanning crowd | Fixed-window eligibility confused with actual convergence | MatchedBenchmarkActor; Crowd PathRefresh/watchdog; game scenario |
| Wrong A/B label or falsely successful fallback | Requested provider differs from actual routing/readiness | NavSurface provider selection; game A/B control/reporting |
| Editor nodes snap to wrong layer/stall | Deferred editor authoring/LiveExtract acceptance still unverified | VALIDATION section 4.3; GroundNav/PathNetwork editor modules |
| Editor succeeds but package fails | Missing cooked assets/config/filter registration or lifetime difference | Game packaging config; GroundNav cook/manifests; runtime loader |

# Critical files

Paths below are rooted at D:\Repos\CkPlugins_3 unless marked BusterBlock.

1. Plugins/CkFoundation/docs/campaigns/navigation-next/PROGRESS.md — gate ledger, rejected approaches, named debt.
2. Plugins/CkFoundation/docs/campaigns/navigation-next/VALIDATION.md — complete deferred checklist; its old priority ordering is superseded here, not its acceptance obligations.
3. Plugins/CkFoundation/docs/campaigns/navigation-next/GATE123_LOCAL_BENCHMARK_EVIDENCE.json — exact local source/artifact/log identities.
4. Plugins/CkFoundation/docs/campaigns/navigation-next/GATE121_MATCHED_BENCHMARK_CONTRACT.md — shared fixture, native query dispatch, timing and fixed-window contract.
5. D:\Repos\BusterBlock\Config\DefaultCkFoundation.ini — missing explicit provider pin and eight legacy mappings.
6. D:\Repos\BusterBlock\Source\BusterBlock\AI\Navigation\BB_NavQueryFilters.cpp — filter semantics; read its adjacent header too.
7. D:\Repos\BusterBlock\Source\BusterBlock\AI\Navigation\Bb_SidewalkPathNetworkDetector.cpp — direct Recast dependency.
8. Plugins/CkFoundation/Source/CkNavigation/Public/CkNavigation/Settings/CkNav_ProjectSettings.h — incoming provider default and config type change; preserve unrelated line endings.
9. Plugins/CkFoundation/Source/CkNavigation/Public/CkNavigation/NavSurface/CkNavFilterDefinition_Registry.cpp — registration/config precedence, failed-load behavior and validation.
10. Plugins/CkTests/Source/CkTests/Private/CkGroundNav_MatchedBenchmarkActor.cpp — working provider-labeled native harness, sample boundary and cleanup exemplar.

The operator guide GROUNDNAV_ACCEPTANCE_GUIDE.html remains useful for deferred human tasks. Follow guide links for further sources instead of bulk-reading every campaign file.

# Things ruled out

- No merge, fast-forward or push to dev, and no premature "ship" workflow. Eventual readiness does not itself authorize publication; return to the CTO for approval.
- No worktree, clone, mirror, repository copy or alternate checkout, including temporary verification. Existing other checkouts are user data; do not repurpose them.
- No destructive clean build, deleting Binaries/Intermediate/DDC, reset/checkout discard, blanket staging or automatic stash workaround. If safe in-place rebase requires a commit/preservation decision, get approval rather than bypassing dirty ownership.
- This handoff creates no blanket commit, force-push or PR permission. Prepare logical owned changes; resolve publication authority explicitly.
- No local cook/package, no raw Build.bat/UnrealBuildTool/editor automation commands.
- No removal of Recast or unrequested rewrite of the sidewalk vectorizer.
- No repeated gym sign-off/capture requirement; no marking tabled editor checks complete.
- No arbitrary timeout increases, silent unrestricted-filter fallbacks or provider mislabeling.
- No promise of live per-NPC provider migration, mixed-provider cohorts, game-wide convergence, FPS gain or package safety from current local evidence.

# Architecture notes and gotchas

- Provider-neutral surface selection is world-scoped. The low-level UCk_Utils_Nav_UE::Request_FindPath path remains Recast-specific; selecting GroundNav does not magically reroute it. The benchmark uses native GroundNavPath requests for that leg. Game callers must be inventoried before promising A/B.
- Neutral filters express required/excluded area tags and costs; exact Recast semantics and native area registrations must survive the migration. If a legacy behavior is not expressible, design an explicit compatibility bridge and document the limit rather than approximating silently.
- Validate requested GroundNav readiness and data before an A/B run. Report requested and effective provider, settings, scenario/seed and result. Missing data means explicit ineligibility; ordinary production remains Recast. Switch back between runs and verify restoration.
- The harness capsule is radius 42/full height 192; BusterBlock's current Recast config is radius 45/height 144, cell height 10, dynamic generation. Do not copy fixture dimensions into the game. Resolve the actual game agent profiles and match both providers.
- LiveExtract is an editor geometry-source mode. It is not a runtime performance benchmark and is not covered by the user's gym attestation.
- Guard failure paths independently of ensure macros. The registry currently puts some early-outs inside CK_ENSURE_IF_NOT; disabling ensure checks could erase those guards. Current macro comments say that configuration is unreachable under the pinned build override, so do not call this a reproduced Shipping crash. Audit and obey the user's total-validation requirement in touched compatibility code.
- All acceptance classes remain distinct: host automation, game Editor behavior, human authoring, actual cooked/streamed maps, packaged behavior and matched performance.
- Rebase checkpoints must retain current game fixes and exact ownership. Any conflict resolution or integration change invalidates affected prior gates. Freeze source during Toolbox runs; no concurrent builds/rebases against the same project or shared binaries.

# A concrete numbered diagnostic and verification flow

1. Read this entire handoff and current local guidance; confirm Astra/high and bounded delegation. Refresh roots, submodules, dirty paths, original/protected hashes and actual game pins. Stop on unexpected ownership changes; preserve them.
2. Establish the current BusterBlock Recast baseline and the eight filters' semantic contract: customer restrictions, shopping Entryway boundary, employee/goon rules and strict crowd avoidance; include direct Recast sidewalk and queue behavior. Determine exact compatible test filters from current discovery/source, not invented names.
3. Reconcile feature history with refreshed dev and BusterBlock's current plugin pins. Before rebase, obtain a safe, authorized in-place plan for uncommitted owned work and unrelated dirt; do not auto-stash or discard. Work on feature/bugfix branches in the same directories. Rebase at controlled checkpoints; **never advance dev**.
4. Design the smallest compatibility slice: explicit game Recast/shadow-off configuration plus lossless eight-filter migration/bridge and required area registration. Add focused real-production-path tests for wrong/missing mappings, no silent policy weakening, and existing Recast routes. Preserve recent CPU/lifetime fixes and root ownership.
5. Apply only the candidate revisions and paired game compatibility changes in the existing BusterBlock checkout under that plan. Incrementally compile and run the selected Editor regression plus startup/log checks. Compare named outcomes with the baseline; any new ensure, filter mismatch, stale callback, routing regression or unresolved relevant queue failure blocks release readiness.
6. Implement bounded A/B readiness: scriptable provider selection between runs, one representative isolated authored game scenario, matched endpoints/workload/settings and effective-provider/readiness reporting. Keep production defaults Recast. Missing GroundNav field/filter/cooked data must reject the trial clearly. Prove Recast -> GroundNav -> Recast across runs without claiming mid-route hot switching.
7. Classify remaining work honestly: controls/harness ready versus GroundNav scenario behavior validated versus cooked/package ready versus performance measured. Escalate any unexpectedly large game integration or unrepresentable filter contract before broadening scope.
8. Schedule functional build-machine cook/package/gameplay checks needed for BusterBlock no-break readiness; local tools may prepare instructions/assets but must not cook/package. Collect formal matched performance only in the agreed BusterBlock integration phase. Package-only failures and behaviorally unequal pairs remain blockers to their respective claims.
9. Keep every deferred backlog row above visible in PROGRESS/VALIDATION. Table editor snap/undo/LiveExtract until the CTO resumes them; do not use them to silently block all compatibility source work or erase them from final campaign acceptance.
10. At each stable checkpoint record exact source/gitlinks/logs and rollback identity. Rebase onto newly changed dev only with preserved ownership and re-gate affected work. Present readiness plus remaining gaps to the CTO; **stop before dev publication**. Later production map adoption and eventual delivery remain explicit separate decisions.

Before each UnrealToolbox invocation, state planned editor-boot count and why. Use the vendored tool from the project being tested; incremental Editor builds only when required. Localized work normally gets one red and one green boot, with extra boots justified by new evidence. Tests reuse current binaries; --config belongs only with an explicitly selected build configuration.

~~~powershell
# Template: replace the project, exact test filter and unique output path after the baseline audit.
# For BusterBlock, run from D:\Repos\BusterBlock and set --project=D:\Repos\BusterBlock.
# For host gates, run from D:\Repos\CkPlugins_3 and set --project=D:\Repos\CkPlugins_3.
$compatArguments = @(
    '--test', '--test-pattern=<verified-filter>', '--no-live',
    '--parallel=1', '--serial-lane=*',
    '--project=D:\Repos\BusterBlock',
    '--output=D:\Repos\BusterBlock\Saved\Logs\<unique-compatibility-gate>.log'
)
# Add --build --target=Editor only when the change requires compilation.
$compatGate = Start-Process -FilePath '.\CkAuto\UnrealToolbox.exe' -ArgumentList $compatArguments -PassThru -WindowStyle Hidden
$compatGate.Id
# Poll this same detached process and its --output log in bounded intervals; do not launch duplicates.
~~~

Never pass --no-progress-window. Hidden applies to the parent console, not the Toolbox progress window. Inspect real exit status, named results and fresh runtime diagnostics, not just the summary count. Respect the session's approval policy for launches; no raw-engine fallback.

# A suggested first message

I'm resuming BusterBlock compatibility and per-run Recast/GroundNav A/B readiness on feature branches only. I'll first verify both existing checkouts, protected dirt and plugin ancestry, then establish the current Recast/filter/sidewalk baseline before any candidate update. I will not advance dev, remove Recast, rerun approved gyms or cook/package locally; editor snap/undo/LiveExtract, real WP/cooked-manifest/package acceptance, formal performance and production rollout remain explicitly tracked.
