# Design review and decision record

Design reviewed independently on 2026-10-11. This records document-contract review, not runtime certification. The user subsequently authorized implementation using tiered delegation and an independent final implementation audit covering performance, conventions, ergonomics, missing features and defects. All supported findings must be addressed before completion.

## Alternatives rejected

| Alternative | Rejection reason |
|---|---|
| Add SetFootTarget to the current gait | Leaves support, reservations, root pacing, ownership and replant ambiguous |
| Disable a walking leg during an action | Existing disable freezes/excludes the leg and changes structural support; not intentional borrowing |
| Give dedicated appendages dummy gait slots | Couples arms/tools to locomotion and imposes false support identity |
| Independent tween for every mesh | Multiple transform writers and clocks cannot guarantee coherent accepted geometry |
| Reuse CkChain as the limb solver | Its path-following/distance constraint contract differs from attachment-to-endpoint IK |
| General ability/attack graph in CkFoundation | Embeds game policy and adds unnecessary graph machinery to a reusable motion library |
| Full animation graph/automatic retargeting in first release | Adds implicit blending/ownership decisions before basic control/return is proven |
| Normal EditorPreview world for Solver mode | Production ECS excludes it; Jolt runtime processors would not run |
| Live DataAsset mutation as tuning | Invalidates active content and bypasses request authority/revision semantics |

## Adversarial findings and disposition

| Finding | Resolved contract |
|---|---|
| Per-action sequences acknowledged through player-wide cursor | Identity-scoped streams; read/consume include ActionIdentity |
| Borrowing checks support but next gait swing removes last foot | Check prospective support before every takeoff lane; emergency deferred until closure |
| Independent part transforms can partially settle | No render atomicity claim; actual pose captured, no receipt, articulation Faulted and recomposition required |
| Manual Ready state omitted; no normal End | Ready is observable; End returns resources and completes |
| Failed return can be reborrowed | Unplanted ineligible until explicit recovery reaches trusted touchdown |
| RootAtStart captured before acquisition delay | Capture at committed acquisition |
| Body-only clip can have no presentation | Body-channel admission requires a valid presentation binding |
| Cancellation can leave a consumer's active window open | Reserved sequenced WindowsClosed and Terminal records without fake pose evidence |
| Action destruction discards recovery settings/deadline | Articulation owns captured policy, remaining attempts and original service deadline |
| InitialSource required in prose but absent in schema | Slot and BodyChannel explicitly carry initial-source policy |
| Body action changes another action's limb attachments | Body action subject-exclusive; body+limb choreography in one instance |
| Orphan stream waits forever for dead consumer acknowledgement | Finite terminal retention, diagnosed expiration, old identity returns StreamExpired |
| Closure invalidates earlier accepted intervals retroactively | Process stream in sequence; closure only affects subsequent boundaries |
| MaxAdvance cap on whole tick slows ordinary 60Hz playback | Per-substep cap; bounded multiple evaluations and explicit backlog |
| Paused world watchdog claims service when no tick occurs | Deadlines enforced on serviced pumps and explicit world teardown; no unserviced liveness claim |

Independent reviewer reread the final three repairs and reported no remaining blocker in those contracts. Main independently inspected the cited gait takeoff and individual rig transform publication code before accepting findings.

## Implementation review still required

Final audit must inspect actual code, tests and measurements: single storage home, request completion/terminal distinction, all public environments, asset roots, reentrancy, foreign writers, support caches, bounded recovery/retention, malformed input, sampler allocations/query counts, editor ownership/undo/seek, game collision coverage and missing acceptance rows. A design review cannot substitute for this audit.

No review acceptance authorizes commit/push. Unrelated working-tree changes remain owned by their original session.

## Independent implementation audit at R36 (not final acceptance)

Owners were crossed: runtime Action/ActionPlayer/Articulation/store reviewed by the locomotion implementer; Gait/LimbControl/Jolt and legacy publication reviewed by the API/game implementer; crawler receipt/collision/damage integration reviewed by the editor implementer. Main inspected the reported code and independently reviewed editor source invalidation, world readiness, scrub coalescing and teardown. No audit edits were made while the build was active.

| Finding | Decision and evidence still needed |
|---|---|
| Valid capacity can reserve every slot and overflow its first pose | Confirmed in `ValidateCapacity`, `ReserveUnbound`, `PublishAccepted`. At R36 the repair was pending. The minimum is now three records per concurrent instance (`CkProceduralActionReceiptStore.cpp:244-247`); the minimum-minus-one rejection and all-first-pose case are in `Test_ProceduralActionReceiptStore.cpp:179-205`. Their `PlayerBudgetValidation` row passed in `Saved/Logs/ProceduralActions-Gate3-R40.log`. This is a checkpoint disposition, not a final memory-budget measurement. |
| Destroyed crawler child action scope strands surviving crawler | At R36 `AttackResolve` returned before identity-stream drain when the action handle died, and `Release_TerminalAttackAction` required that handle. The later copied-identity drain/cleanup repair has direct live coverage: both `Mars_AutoTest_CrawlerMelee_ActionScopeLoss` and `Mars_AutoTest_CrawlerMelee_LongLimbLiveScopeLoss` passed in `Saved/Logs/ProceduralActions-Crawler-R39.log`. |
| Store does not independently validate cross-record accepted clock monotonicity | Actual manual and clip producers were inspected: clocks advance monotonically and only after publication succeeds; zero-time entry blend is intentional. No live defect demonstrated. Optional future hardening, not a blocker or a runtime fix. |
| Active protection adds per-planted-leg ground queries; action service may repeat default/candidate solve work | Bounded work confirmed; costs unmeasured. Use the 162-case trace, including WalkerBorrowed, before an optimization decision. Do not introduce cross-frame pose caches without root/gait/revision invalidation proof. |
| Long-limb proxy consumer can issue 2,048 queries per receipt by default, up to configured ceiling 8,192 | Finite guard is not a performance result. Measure game adapter trace separately from the framework workload. No throughput claim. |
| Existing Jolt ray normal helper substitutes UpVector after a failed body lock | Main verified the helper is unchanged in the current Git diff. Conditional concurrent-removal trigger, no runtime reproduction: recorded adjacent follow-up, left untouched. No claim that this was repaired. |
| Private gait control declarations use condensed multi-parameter signatures | Formatting-only conformity repair required after source unfreezes. |

The reviewed request queue uses a moved drain and live-tag checks; the claimed same-array reentrancy fault was not supported. Legacy Rig/BodyPose route through Articulation when installed. These source observations do not replace final focused/full gates, the native AS-disabled packaged runtime, performance measurements, or continuous editor/game observation.

## Implementation findings during Gate 1/2

These are checkpoint findings, not the final independent audit. Runtime acceptance is tracked in PROGRESS.

| Finding | Evidence and disposition |
|---|---|
| Request queue disappears after drain | R3 manual test emits missing Action Requests fragment ensure; six Utils request paths changed to AddOrGet. EndPlay no longer requires that ephemeral fragment. Manual acceptance, callbacks and owner loss passed R6. |
| Invalid receipt fixtures obscured storage tests | R3 subsequent Continuous body-only records omitted previous body pose; corrected fixtures retain strict validation. All three repaired rows passed R4 and R6. |
| Editor creates abstract UObject | R3 three CurveAdapter tests all emit the same abstract-class ensure; private nontransactional proxy now concrete UCurveFloat. All three passed R4 and R6. |
| Two body-action producers can overwrite presentation provenance | Player Add rejects nonneutral/pending legacy state; installed player rejects legacy enqueue/drain and stops legacy sample publication. Existing no-player baseline extended to verify boundary; passed R6. |
| Missing geometry is measured as zero motion | Motion helper previously skipped missing IDs and truncated unequal chains; now mismatches/nonrigid inputs yield unbounded motion. Focused validation passed R4 and R6. |
| Return timeout misses stalled ordinary publication | Owned-token timeout now faults any genuine pending publication, allowing claim cleanup. Direct production default-issue bridge fixture passed R6. Elapsed LimbControl deadline fixture remains required. |
| Independent storage caps multiply excessively | Need global exact retained-record and pose-transform budgets, checked accounting before copying, reserved closure/terminal records. Latest receipt retention must have one storage home and count against the budget. Design agreed, implementation pending. |
| Scheduler metadata omits indirect writes | Service/Observe mutate child Action and control fragments beyond the root query. Existing template traits only infer query access; review explicit dependencies and supported side-effect metadata before final audit. Do not add mandatory child fragments to root queries. Unresolved source-review concern, no runtime failure attributed. |
| Active actions read mutable player tuning | UpdateTuning replaces Player tuning and existing Service/Observe read it. Pin tuning and revision per instance at creation; future default updates must not alter active action clocks, budgets, return or receipt stamps. Confirmed source gap, scheduled for the next runtime checkpoint. |
| Legacy walker visual chains are optional | Broader Gate 1 run exposed unconditional Rig Params read for simulation-only legs. Adapter now exposes only bound chains in topology order. Repair gate completed: 306/309 passed, two listed reds and one listed flaky which passed alone; all 21 new adapter failures resolved. |
| Runtime Params retain construction Specs | Action Params retain PlaySpec/ControlSpec and Player runtime state contains defaults. Next checkpoint dissolves Specs into runtime values and moves authoritative next-instance defaults/revision to Tunables. |
| Compiler input hierarchy is unbounded | Add hard documented content counts before copying compiled arrays, bounded diagnostics, and admission checks against same-time event and overlapping-window budgets. Managed-clip checkpoint requirement. |

The interim independent callback review initially proposed orphaned stage tickets after normal next-frame cancellation. Main checked actual scheduler/handle lifetime source and requested a reachable trigger. Reviewer withdrew that blocker: Derived requests precede Service, Camera observation drains before the next request pass, and Initiate-marked actions remain valid until Teardown. Cross-frame Pending interruption remains a targeted test case, not a reproduced defect. `FragmentList` must not be overloaded with indirect access metadata: it also drives empty-view/consumable checks in CkEcs, so doing so would change query semantics.

## Next checkpoint source review

- Aggregate record/pose budgets, pinned per-action tuning and dissolved Specs are authored but await a shared build/test. Main found that partial consumption still retained an obsolete consumed pose beside a newer queued pose; the owner repaired it and added an exact-accounting case before acceptance.
- Independent review supplied coherent near-wall contact normals against a floor support frame. The previous pairwise-only cone accepted this geometry. The support cone now applies both to the frame up and pairwise contacts; a focused case covers CountOnly, projected mode and the inclusive angular boundary. This is authored source evidence until the next test result.
- Main found the new private editor preview copied its receipt pages without acknowledgement, eventually exhausting the runtime inbox, and rejected zero-limb body-only subjects. Repairs are required before accepting the editor host checkpoint. Long recording history also needs an explicit finite limit and an empty-page fast path.
- RootOwnedDeadlineCannotBeExtended exercises the registered Physics reconciler without an action player, destroys the scope and repeatedly requests cancellation, then checks the original deadline, claim release and explicit terminal acknowledgement. ActionManualPublicSurface exercises actual body presentation plus read/consume/end through AngelScript. Both are new unexecuted fixtures at this point.

## Release review checkpoint after R40 (not final acceptance)

The R36 table above preserves what was known at that review; later dispositions are noted there or here rather than rewriting the historical diagnosis. `Saved/Logs/ProceduralActions-Gate3-R40.log` records 121 Action tests, 119 passed and two editor rows failed (`PieCoexist.IndependentWorlds`, `SaveReopen.FocusedAssetIdentity`). A subsequent isolated SaveReopen run passed 1/1; the combined editor result remains unresolved by that isolation alone. The `NoFloorFailureAndExplicitRecovery` row passed in R40 after the fixture used a scoped 60 Hz world timestep; R39 failed the same row with only 13 observed publication opportunities before its original 0.8 s service deadline. This establishes a focused controlled-cadence pass, not broad runtime reliability.

The R39 crawler gate passed the two scope-loss rows but still failed its loaded-branch and projected-geometry rows (32/34). Those three loaded long-limb rows passed together in `Saved/Logs/ProceduralActions-LongLimbLoaded-R40.log` (3/3). The R40 framework noncombat gate remained 1/2: `NoncombatCarrierReach` could withdraw the leg but did not publish the small reach. Its R40 production diagnostic showed the default manual Up pole forcing roughly 180 degrees of chain motion against a 5-degree limit. The R41 source checkpoint makes manual bend override opt-in, preserving the authored solver pole by default; the native/AngelScript regressions and its game-facing result await the full R41 process verdict. No R40 pass is attributed to that later source change.

The independent R40 release read found no additional confirmed Crawler receipt/collision/damage/cleanup defect. It did identify a public feedback gap: the noncombat `SubmitCoordinatedGesture` helper discarded its deferred request result; the R41 source checkpoint now returns the request handle and accepts a completion delegate, pending verification. The long-limb adapter's 2,048-default/8,192-maximum world shape-cast budget is finite but unmeasured. The opt-in 162-region matrix and Insights allocation analyzer are also unrun; their regions include test-driver work, and the allocation tally covers the whole GameThread, so processor CPU and game query costs need separate trace attribution. Native AS-disabled/runtime, live editor usability, final public-environment gates and independent final audit remain open.

Three further independent R40 findings have R41/R42 source repairs awaiting executed verification: dedicated finite bend normalization can overflow and fall back to an unintended pole; external clip edits do not invalidate an open Solver; preview Action Tuning is missing. Admission and runtime now reject a degenerate normalized bend; `Rig.DedicatedPoleMagnitudeAndFullReach` adds the extreme finite vector regression. Toolkit source notifications coalesce clip and preview dependency changes on the ticker, with the mounted SharedPreviewRefresh regression. Preview config now carries separate Action Tuning, validated and applied to its private production player, with a direct tuning comparison/invalid-value fixture. Earlier R40 generic admission, Modes and PreviewSettings passes do not certify these later changes. R41 stopped at compilation before any automation: missing `ValidateTuning` declaration and protected `SButton::ExecuteOnClick` fixture call (`Saved/Logs/ProceduralActions-Gate3-R41.log:107-155`). R42 includes the actual declaration header and routed Slate input; both mouse-up events are dispatched even when mouse-down is unhandled.

The separate authoring review reported function-formatting and `FCk_Time` authoring-session convention misses. The R42 source checkpoint reformats the new runtime Action, receipt-store and LimbControl declarations; the authoring-session correction remains separately owned. R41 stopped at compilation, and the R42 source changes have not yet been built or tested.

R42 also migrates AuthoringSession and EditorModes to FCk_Time, retains raw seconds at engine/widget boundaries, and rejects a nonrepresentable seek-step count with a diagnostic before integer rounding. Main and independent reviewer inspected the strict 2^63 bound, finite negative-to-zero behavior, preservation of an existing target after rejection and recovery on a valid seek. The checkpoint is running with sources frozen; no R42 test verdict is claimed yet.

## R42 executed disposition

R42 actual exits and logs now supersede the pending checkpoint statements above: `ProceduralActions-Gate3-R42.log` records build success, 123 selected/121 passed/two editor failures/zero contaminated, exit 1; `ProceduralActions-Noncombat-R42.log` records 2/2, exit 0; `ProceduralActions-Crawler-R42.log` records 34/34, exit 0. The native bend-default/overflow, preview tuning and bounded-seek regressions pass; the AS noncombat reach now passes without relaxing its 2 cm/5 degree motion budget. All crawler scope-loss and loaded collision rows pass together. SaveReopen passes, while SharedPreviewRefresh and PieCoexist remain red.

Main and editor reviewers verified the SharedPreview fixture tries to click the Body tab after opening Limb in front; R43 activates Body and waits a frame. The claim that SharedPreview mouse-up used an invalid button key was rejected after direct source inspection: only mouse-move used `Invalid`. The PIE source-world chain was verified against engine 5.7.4: GlobalMapOverride loads the original Entry world, editor asset loading initializes that Inactive world, and PIE duplicates it without making the source an editor context. R42 cleanup exposes its still-initialized Landscape subsystem. R43 must establish explicit fixture ownership before cleaning that source; absence from world contexts alone is insufficient. Slate mouse-down returns true independently of target delivery, so routed-input success must be established through hit-path diagnostics and changed transport state. These two fixture repairs still require executed verification.

## R43 executed disposition and R44 review

R43 passed 122/123 Action rows with zero contaminated and actual exit 1. SharedPreviewRefresh and Entry source-world cleanup passed. PieCoexist still failed: `targetInPath=0`, `capture=0`, with the PIE window as both active and hit window. R44 routes the engine's window-activation event before the normal pointer path and refuses to count an occluded click as input. This is still pending executed verification.

The visual-protocol audit identified three missing capabilities in the authoring host: an atomic slot rename command, current settled geometry through return/recovery, and a bounded replayable environment schedule that can observe floor loss and explicit recovery after action termination. R44 adds these without changing the runtime clip format or adding gameplay semantics. Live settled diagnostics must reject detached limbs even while their transferred parts remain alive; the configured floor overlay must not survive an observed live floor removal. These repairs and their regressions remain source-only until the next gate.

The new detach regression targets a real action in Returning with issued-but-unsettled publication, then calls the public detach API and checks later transform queues and poses. The existing borrowed-detach test exercised gait withdrawal directly and did not establish that action-state boundary. V27's existing posture/reconstruction fixture meets the specified transient serialization boundary; a newly suggested full save/load round-trip requirement was withdrawn as outside that row's actual contract.

R44's independent command review found that slot rename omitted the selected preview's slot-keyed initial endpoint/bend maps, and that phase-index ordering accepted the zero absolute interval `[A@1, B@0)` across adjacent phases. Main verified both. The frozen checkpoint renames both maps in the same transaction, rejects destination collisions atomically, and compares resolved `FCk_Time` window endpoints. Tests include map preservation/undo/redo, adjacent rejection, nonadjacent valid windows, edge moves and phase reorder.

Scenario integration review additionally rejected the known-impossible Detach-then-Recover sequence during preflight, made Finished a real direct-tick stop with explicit future seek as a separate operation, preserved same-step seek before a remaining observation horizon, and connected the Solver slider to that horizon. The source-only crawler target-loss report was rejected: `Close` sets `Receipt.Closed`, and the target-history branch already checks it. Only target-loss integration coverage was added; no consumer behavior was changed for that report. All these R44 dispositions still await the frozen build/test result.

## R44 executed disposition and R45 bounded correction

R44 completed with build success (258.26 seconds), Action 128 selected/127 passed/one failed, zero contaminated, actual exit 1. All 123 headless rows passed, including slot rename/maps/undo, exact window edges, preview environment replay/recovery, settled-pose observations and detach during pending return. PIE coexistence also passed. The remaining rendered `MountedToolkit.TwoTabsModesAndCapture` failure reproduced alone (0/1, zero contaminated, 39 seconds, actual exit 1). Main and the independent reviewer inspected both saved captures: Appearance was below the virtualized Details viewport, and the slot button was below the timeline scroll viewport. The fixture tried off-screen clicks and advanced without confirming their result.

R45 keeps the routed input and gizmo assertions, scrolls targets into view, observes Root Frame before scrolling it out of the virtualized tree, and waits for actual property/selection state. The production timeline now places frequent slot/phase selection before creation/retarget tools. Both files are frozen for the rebuilt gate; no R45 test verdict is recorded yet. The saved LimbNod preview has no mesh asset reference and consequently displays no limb component; main must populate its preview appearance and inspect the live output before visual acceptance.

Performance-pipeline review found that matching case names did not prove metrics, allocations and timers came from the same trace. Saved capture/analysis/export scripts now use trace, source-snapshot and artifact SHA256 manifests, reject duplicate allocation rows, and put the owned Insights export behind a finite process timeout. Synthetic 162-case positive and negative provenance checks passed; these are tooling checks, not measured runtime performance. The native AS-off runner now creates its log directory and bounds packaged execution at 180 seconds; its actual Game build is running. No native-runtime, performance or final visual certification is claimed.
