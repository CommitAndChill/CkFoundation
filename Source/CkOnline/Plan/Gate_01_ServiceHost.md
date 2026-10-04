# Persistent service host

Updated: 2026-10-02. Status: C++ compilation and all 15 focused Online automation
tests pass. Manual editor acceptance is pending.

Baseline: Foundation `6cf825dd`, CkTests `66d8cbed`, CkPlugins research branch
`434f81df`, associated AngelScript engine `0b36f262` (5.7.4). Work is in place.
The two temporary research commits remain independent of this implementation.

## Scope and decisions

Prove one GameInstance-owned, actorless service registry with a bounded asynchronous
operation lifecycle. Introduce only CkOnline and focused CkTests fixtures/gyms.
An injected native adapter supplies fake operations in the test module. This is a
hosting seam, not a login/lobby API or a working Steam/Direct implementation.

Reference patterns: CkTimer's complete feature quartet for handles, requests and
signals; CkGameSession for FEcsWorld ownership; CkLoadingScreen for non-Actor GI
ticking; Test_Scheduler_PumpGating.cpp for explicit local descriptors.

1. Add reflected operation requests/results and a feature quartet under CkOnline.
   Verify: invalid/default/stale inputs reject without provider work; accepted
   requests have one terminal result and consistent generic completion.
2. Add the native host and GameInstance subsystem. Build a local descriptor chain
   from existing lifecycle processors; remove only the local copies' gameplay
   edges. Stop for design review if shared scheduler changes prove necessary.
   Verify: no Actor host, no global processor registration, isolated GI contexts,
   correct destruction ordering and no partially published failed host.
3. Add deterministic adapter tests and interactive gyms in CkTests.
   Verify: immediate/delayed/duplicate/late completion, deadline/cancellation,
   bounded admission, listener-triggered stop, invalidation, and map travel.
4. Incrementally build and run a focused fresh gate through UnrealToolbox.
   Planned fresh editor boots: up to four for Toolbox discovery and the native
   and AngelScript Online selection. The tests exercise the service's actual
   scheduler, requests and signal lifetime; the Toolbox pattern cannot union
   unrelated sibling selections in one invocation. Additional boots require a
   concrete failure or changed verification requirement first. No baseline/full
   suite, packaging, Shipping configuration, or artifact cleanup is planned.

## Invariants

Only owning-thread processors apply accepted operations. Provider callbacks carry
owned values into a bounded thread-safe inbox. Pending terminal ownership is
removed before notification. Signals dispatch after scheduler.Tick returns;
logical shutdown closes admission immediately and physical teardown waits for
active dispatch to unwind. Deadlines use monotonic FCk_Time. The service graph
uses Full scope, never snapshot LoadKernel. Service entities are not replicated
or attached to gameplay-world lifetime chains.

Admission is bounded to 64 outstanding execute operations, 64 queued requests
(including cancellation commands), and 256 callback values. Inbox overflow
fails pending work explicitly. The public operation GUID
is distinct from a private callback ticket, so a reused public ID cannot receive
an old callback. Invalid requests fail admission; a deadline that overflows at
dispatch fails before calling the adapter. Accepted execute operations produce
one rich signal and one generic terminal completion. Cancel commands have their
own generic acknowledgement; they do not create another execute-result signal.

Native host ownership, ticking, adapter calls, and final release belong on the
game thread. Only the adapter's completion function may run on another thread.
Adapters must release native work in Shutdown and reconcile any provider-side
effects arriving after cancellation. This fake-adapter gate does not prove that
contract for any real provider. Shutdown closes admission before callbacks and
retains the registry until dispatch returns; its local lifecycle drain uses
three ordered passes of the existing destruction processors.

Validation computes a total side-effect-safe value once, diagnoses with
CK_ENSURE_IF_NOT, then uses an ordinary failure branch. This follows the current
maintainer instruction and supersedes contradictory older ensure-body guidance.

## Human acceptance

Every major gate supplies a CkTests gym with visible state and exact editor steps.
The first gate's instructions live in CkTests/Script/CkOnline/EDITOR_TESTING.md.
Human observations are recorded separately from automated tests. Do not mark the
manual gate accepted until the maintainer confirms it.

## Evidence

- Source feasibility inspected; no online code or tests existed at gate entry.
- `Saved/Logs/OnlineHostGate-05.log`: incremental Development editor build passed;
  15 selected tests passed, zero failed/skipped/contaminated. The selection is
  fourteen native tests under `Ck.Online` plus the AngelScript functional test
  `Ck_AutoTest_OnlineService_UtilsExecuteCompletes`. The fresh test process exited
  zero. The full editor log contains only the deliberately exercised invalid-ID
  ensure (reported through two CK diagnostic channels), with no script errors or
  unexpected ensures. This is focused headless evidence, not a full-suite or
  renderer verdict.
- Invocation: `CkAuto/UnrealToolbox.exe --build --target=Editor --test
  --test-pattern=Online --project-prefix=Ck --no-live --parallel=1
  --skip-renderer-tests --output=Saved/Logs/OnlineHostGate-05.log
  --project=D:\Repos\CkPlugins`. Toolbox reused the cached selection and reconciled
  it in a concurrent discovery editor; all 15 rows produced actual results.
- Final script-only gate after remapping the gym controls:
  `Saved/Logs/OnlineHostGate-06.log`, same test flags without `--build` or
  `--target`, one fresh editor boot. All 15 tests passed again, exit zero,
  zero failed/skipped/contaminated. The fresh full editor log again has no script
  errors or unexpected ensures. No source edits followed this gate.
- Earlier attempts found and fixed processor access declarations, log include
  placement, and destructor shutdown incorrectly calling Unreal's `AsWeak()`
  after final owner release. Shutdown now checks `DoesSharedInstanceExist()`
  before retaining an owner. Final-owner release, release during a listener,
  generic-completion shutdown, replay, external teardown, queue/inbox bounds and
  worker callbacks all pass their focused native tests. The initial four-boot
  estimate was exceeded by discovery/bootstrap work and automatic crash retries;
  those retries were stopped before correcting the destructor path.
- The existing CkTests ignored spawn-params canonical was absent. Four complete
  source-derived recovery declarations bootstrapped compilation; the current
  generator then wrote its canonical and removed that temporary recovery file.
  No existing CkTests callers were changed. The root Placeable tests needed the
  current `FCk_IsmProxy_Spec` name in two calls to match this Foundation baseline.
- The AS smoke generated its normal actor wrapper and one external actor asset.
  Editor startup also appended four unrelated baseline gameplay tags and
  normalized comments in the root generated spawn-params file; these remain
  unstaged and separate from Online implementation ownership. Existing unrelated
  submodule revisions were preserved. No commits, pushes or history rewrites were
  performed; both temporary research commits remain unchanged.
- Two editor gyms and their exact checklist are present in CkTests. Controls use
  keys allowed by the shared control panel; movement keys remain reserved.
  Actual gym interaction, map travel and a Blueprint graph execution still need
  the maintainer's observations. The native reflected-call test exercises
  UFunction marshalling, not a Blueprint graph.
- Whitespace checks pass across the implementation and test diff. The automated
  tests do not establish manual acceptance of either gym.
- Real provider/transport behavior, EIK migration, AS-disabled builds and release
  packaging are outside this hosting gate and remain unverified.
