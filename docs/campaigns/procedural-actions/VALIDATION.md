# Executable acceptance rubric

This is the implementation acceptance specification. A row is unverified unless [PROGRESS.md](PROGRESS.md) records its required executed evidence. Focused build/test checkpoints have run; partial evidence does not certify the complete feature or replace the remaining editor, consumer and performance observations.

## 1. Release rubric

All mandatory rows must pass; a high total score cannot offset a failed invariant. Score each 0 = absent, 1 = source/unit evidence, 2 = required integration evidence including public surfaces; UI/feel rows additionally require explicit editor observation.

| ID | Mandatory observable contract | Required evidence |
|---|---|---|
| V01 | Malformed layout/clip/bindings reject atomically; no runnable partial subject/action | Native invalid-input tests plus AS wrapper cases |
| V02 | Standalone limb and body-only subject run with no gait | Native/ECS fixture and headless AS integration |
| V03 | Every bound part has one writer; conflicting SceneNode/physics/rig ownership rejects | Admission tests, actual settled transforms |
| V04 | Legacy walker, disable, detach and body action behavior retained with no new action | Existing ProceduralAnimation suite and sampled baseline comparisons |
| V05 | Disjoint limb actions succeed; body action excludes other actions; joint candidate failures cancel without a successful subset | Two-action and concurrent-admission fixtures |
| V06 | Borrowing removes gait swing/plant/reservation consistently; does not become structural leg damage | Source and runtime counts/poses throughout transition |
| V07 | Aggregate support gates admission AND every gait takeoff; emergency closure precedes later takeoff | Biped borrowed foot, multiple borrowers, catch/emergency lanes, contact loss |
| V08 | Prior reach stance AND feet plane cannot retain withdrawn support | Physics-to-Derived boundary test with generation mismatch |
| V09 | Gait returns from actual endpoint only after trusted touchdown | Mid-swing borrow, moved root, changed floor, exact contact observation; moved-root zero-time IK jump closes windows, settles a held-endpoint reattachment with no accepted-clock advance, then resumes bounded return under the original deadline |
| V10 | Failed return terminates ownership and exposes Unplanted; explicit retry can recover | No-ground fixture + later floor restoration |
| V11 | Detach/disable/death overrides actions without reclaiming released parts | Destroy/sever at every control state |
| V12 | Scope death, missed signal and request reentrancy cannot leak claims | Churn/lifetime tests with no listener and reentrant listener |
| V13 | Accepted receipt matches settled transforms; partial settlement fail-stops with observed pose and no successful sample | Deliberate late writer/part destruction between staging and receipt |
| V14 | Request completion alone cannot advance clock or emit a successful receipt | Deferred apply stall fixture |
| V15 | Phase endpoints, narrow windows and equal-time marker ordering survive hitches | 30/60/120 Hz and injected 100/250 ms steps, zero time |
| V16 | Identity-scoped consumption, reserved closures, terminal ordering, explicit overflow and bounded orphan retention | Two actions both sequence 1, missed signals, dead consumer, small inbox |
| V17 | Sampling budget failure cannot skip interval or invent continuity | Extreme path, singular solve, low budget |
| V18 | Geometry projection/crossing is observable; unsupported input never silently repaired | Unreachable targets, collinear poles, invalid quaternion/scale |
| V19 | Asset loading and cancellation are rooted/bounded; late completion cannot revive | Missing asset, delayed load, forced GC, scope destroyed |
| V20 | Active content remains immutable across edits/reloads; new revision admission is explicit | Active clip + editor/AS source edit |
| V21 | No game semantics in framework; noncombat reuse proven | Standalone inspection arm and carrier reach examples |
| V22 | C++, Blueprint and AngelScript expose same contract and diagnostics | Native tests, AS execution, recorded BP node smoke |
| V23 | Runtime works without Editor/Debugger modules and AS-disabled build remains valid | Host build matrix via supported tooling |
| V24 | Editor authored mode and solver mode are truthfully separated | Real editor walkthrough plus model tests |
| V25 | Solver preview runs production pipeline and invalidates state on seek/edit | Contact-obstructed fixture; replay from seed |
| V26 | Editor undo/redo, tab close, load cancel and world teardown leave no residual state | Mounted widget + real editor lifecycle |
| V27 | Transient execution does not accidentally serialize handles/leases | Snapshot posture/reconstruction fixture |
| V28 | Consumer migration still uses actual accepted geometry and preserves old jab behavior | CrawlerMelee group + visual A/B, game scope only |
| V29 | CPU/memory/query/receipt budgets measured with geometry and action counts | Recorded benchmark methodology, no target performance invented here |
| V30 | Direct/manual control uses same claims, publication and return | Input-timeout, partial batch, duplicate request and explicit end tests |

## 2. Planned test families

Native names begin Ck.ProceduralAnimation.Action.* to remain inside the focused feature filter:
- Admission.AtomicInvalidTopology, DuplicatePart, MissingSlot, InvalidKeys, UnsupportedSpace, InvalidScale.
- Articulation.NoGait, BodyOnly, LegacyPoseEquivalence, SinglePublisher.
- Ownership.DisjointActions, BodyExclusive, ConflictRejectsAll, AggregateSupport, OwnerDestroyed, StaleGeneration, ReentrantCancel.
- Support.WithdrawInvalidatesStance, WithdrawInvalidatesFeetPlane, PreservesLastContact, EmergencyClosesBeforeTakeoff, NoDamageSag, LostContact, NonPlanarRejects.
- Return.TrustedTouchdown, FromActualEndpoint, NoGroundTimeout, RetryAfterFailure, UnplantedRejectsBorrow, DeadlineSurvivesScopeDeath, DetachDuringReturn.
- Publication.SettledOnly, MissingRequiredPart, ClockWaitsForReceipt, ForeignWriterFaultsSubject, CoupledCandidateRejected, ZeroTime.
- Timeline.NarrowWindowAtHitch, PhaseEndpoint, MarkerOrder, BudgetExceeded, ContentRevisionPinned.
- Receipts.ConsumerGap, OverflowFailsLoudly, IndependentActionCursors, TerminalClosesWindows, PriorAcceptedIntervalSurvivesClosure, OrphanExpires, NoDuplicateConsumption.
- Assets.Missing, CancelDuringLoad, GCDependencyLifetime, LateCompletion.
- Manual.ReadyObservable, PartialBatchRejected, PendingBatchBusy, ExplicitEndReturns, InputTimeoutReturns.
- Editor.TransactionIdentity, SeekRebuildsFixture, SessionClose, StalePreview.
These names are specifications, not discovered tests or expected run counts.

Headless AS names follow existing CkAutoTest_ProceduralAnimation_* registration, with new Action_* subjects. One representative test must call both namespace and handle-mixin surfaces. At least one test composes the complete consumer request -> solve -> settle -> receipt -> cancel/return cycle; testing only utility output is insufficient.

## 3. Focused toolbox commands

Run from the HOST project root after reading the real toolbox skill and checking editor/DLL locks. Do not run raw UBT or UnrealEditor automation. These command forms are verified from the toolbox skill, not executed here.

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Feature.log --project="D:/Repositories/CkRepos/Orion"
./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-CrawlerMelee.log --project="D:/Repositories/CkRepos/Orion"
```

Each pattern is a separate run; the toolbox treats a supplied pattern according to its current filtering contract. Read population, real process exit, structured report and fresh editor log verdict. Zero discovered tests is failure. Generated wrappers need fresh discovery/restart when structural AS reload requires it.

Baseline policy: inspect AutomationGate.json and the current toolbox known-red rules. Record current baseline source/time, pass/fail names and scope actually run. For extraction equivalence, capture old/new fixture samples on the same inputs/build conditions; a known-red list alone cannot establish pose equivalence. Do not run a redundant full baseline if current toolbox rules use knownReds. Never add a known-red entry to conceal a newly introduced failure.

Final integrated release gate, once all focused gates and editor observations are complete:
```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Final.log --project="D:/Repositories/CkRepos/Orion"
```

Use the toolbox's supported config/build/cook route for additional non-editor/AS-off environments after inspecting current capability; do not invent flags. An unavailable matrix leg is explicitly pending and blocks that capability's certification.

## 4. Runtime geometry and consumer evidence

The generic release does not promise collision-complete melee. A consumer that uses limbs for hits must add:
- player intersecting the midpoint while tip misses;
- fast arc between frames, including full extension and IK branch changes;
- wall between limb and player;
- target moving during the interval;
- duplicate damage suppression per window/target;
- cancel/sever on the terminal active sample;
- accepted versus requested pose disagreement.

Report the collision method and its measured/derived error bound. More samples alone are not proof of continuous collision correctness. If the method cannot cover a declared move, reject that move in the consumer authoring validator.

## 5. Editor protocol [EDITOR-VERIFY]

1. Open a new action asset. Bind a dedicated-arm topology and distinct preview appearance. Confirm no gait required.
2. Author three phases, a body offset, endpoint/bend track, one marker and a narrow window. Validate, save, reopen: IDs and timings identical.
3. Play in Authored mode. Confirm no accepted-contact badge.
4. Switch to Solver, place an obstacle beyond current reach. Observe requested dashed versus accepted solid pose and explicit failure/projection result.
5. Seek backwards: fixture reset/resimulation, no stale plant carried across time.
6. Change mesh scale only: solver length and contact unchanged. Change topology length: preview rebuild required.
7. Borrow a locomotor limb on sloped coherent ground; remove its landing surface during return. Observe finite RecoveryFailed/Unplanted state, then explicit successful recovery after restoring ground.
8. Detach the acting limb. Observe no later transform requests to its released parts.
9. Undo/redo phase reorder, slot rename, marker move and mirrored track. Confirm references by ID, not row index.
10. Close tab during loading/seek, stop PIE, reopen two tabs. No stale world, delegates or asset roots; inspect fresh logs.
11. Record continuous motion at normal runtime input without per-frame screenshot stalls. Assess readable anticipation, motion continuity and return foot sliding. This is separate from automation.
12. Assert that Solver preview does not mutate global GFrameCounter and never ticks the same private world twice in an editor frame. Verify seek progress/cancel across frames, two independent preview tabs, and PIE/editor tick continuity during preview.

## 6. Performance and scalability

Benchmark idle legacy walkers, dedicated actions, one borrowed limb and two disjoint actions at 1/10/100 subjects on fixed fixtures, with 2/4/8 links and explicit sampling settings. Record CPU time by phase, allocations, Jolt query count, max backlog, retained result memory and editor refresh cost. Use existing stats/Insights patterns. Compare identical input, build and content revision. No FPS promises before measurement.

Bounds must be testable: 64 limbs, 8 links, configured active/retained instances, MaxSamples, receipt capacity and retry budgets. Capacity failure is observable. Scratch storage is subject-owned/reused where safe, never unsafe shared mutable process-global state.

## 7. Stop rules

Any new crash, ensure on valid input, old-pose mismatch, missing terminal outcome, unbounded wait, duplicate writer or partial admission stops the phase. Restore the previous known-good implementation step before diagnosing; do not stack unverified fixes. A mismatched proposed API/engine capability triggers an amended design and repeat review, not a silent improvisation. Two failed attempts require a recorded pivot.

