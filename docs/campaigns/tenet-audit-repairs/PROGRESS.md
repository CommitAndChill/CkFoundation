# Repair progress

## Current state — 2026-10-22

The initial focused baseline completed: Development build reported Succeeded; all six selected
TenetIntegrity tests failed at their intended assertions (0 passed, 6 failed, 0 contaminated).
Ownership, settings, test bridge, request-integrity, debugger profiles, and HangMonitor repairs
are implemented and source-reviewed. The third post-fix build succeeded; its focused gate passed
34/34 with Toolbox exit 0 and no contamination. The ProcDump opt-in case reported NOT EXERCISED.
The final GameSettings caller gate passed 35/35 after correcting one stale expected diagnostic;
the GOAP planner caller gate passed 34/34. Final rendered UI gates passed Combat HUD 1/1,
PauseMenu 2/2 (including gym selector), PowerWheel 1/1, and Journal 1/1. The initial PauseMenu
failure did not recur after a diagnostic-only test change; its cause remains unproven.
All 64 snapshot-05 source hashes matched after the final gates. No Unreal operation remains active;
the exclusive window was released to the networking chat. No further launch is planned.
Selected host and three plugins are on in-place bugfix/core-tenet-integrity branches.

Baseline revisions: Foundation 87eb9754d706735e51dc21bdcc1ece51ed8eba98;
Tests 6428ea4d247825ca0629574ee4a6cecf1bc2e74b;
Debugger 86327732a75fc0b2e9ebecacd4563de2979ba64a.

Existing unrelated host changes: .gitmodules and Script/Binds.Cache.Headers. Prior authorized Foundation
instruction changes remain local (14 tracked files and docs/campaigns/ensure-policy-clarification).
Shared instructions were published separately as 26d29a0 and 4c5db2ed2534c701cc60b8189987854b711ba422
to settings feature/shared-settings. The latter records operational failures versus developer ensures;
remote tip, tracking tip and HEAD matched with clean settings status after push.

## Done

- Read current ownership setup/transfer/traversal, fragment failure returns, settings provider and
  bridge contracts, root/module guidance, and canonical Toolbox instructions (actual path .claude).
- Source-confirmed same-registry-only reverse ownership; pending destruction is intentionally propagated.
- Astra analyzed the T& fallback tradeoff; maintainer subsequently confirmed that fallback is intentional.
  Sentinel finding withdrawn; no primitive API migration.
- Read-denial persistence regression compiled and reproduced original-byte loss and rejected-write leakage.
- Bridge constructor seams and regressions compiled and reproduced false success and partial admission.
- Ownership regressions reproduced descendant cycles, foreign reverse links, and callbacks on invalid input.

## Implemented

- Ownership admission validates complete chains/reverse links before mutation; foreign registry owners have
  no local reverse link. Queries reject invalid state before callbacks and repeated visits within a query.
  Pending-kill world resolution follows the explicitly included owner chain.
- Settings provider outcomes are explicit; batch admission stages provider/current/session state before
  callbacks. Failed I/O retains dirty/pending state; native installation preserves existing physical files.
  Revert closes its session before notifications. Local C++/AngelScript callers migrated.
- Test bridge rejects malformed whole requests, distinguishes infrastructure failure, retains captured
  results on publication failure and provides explicit recovery without rerunning tests.
- GOAP required goals reject unresolved keys; runtime tuning reaches actual planner params. Construction
  validates every class and resolves required CDOs before executing any construction script.
- EntityTag validates whole hydration payloads and restores both views; legacy names remain exact.
- Debugger profile replacement validates before teardown, pairs lifecycle callbacks, handles EndPlay,
  resets selection state, and rejects nested transitions. Confirmed duplicate ensure guards removed.
- HangMonitor services worker/helper cancellation for one two-second best-effort shutdown budget,
  retains observation after incomplete cleanup, never force-terminates children, and prevents rearm.
- Source review complete; Astra consultations supplied design analysis, not runtime verification.

## Baseline verification record

Launched 2026-10-22, detached Toolbox PID 22388:
`--build --config=Auto --target=Editor --test --test-pattern=TenetIntegrity --project-prefix=Ck --no-live`
with explicit project D:/Repos/CkPlugins_2 and output Saved/Logs/TenetIntegrity-Red.log.
Resolved configuration: Development. Build scheduled 716 incremental actions.
The invocation completed; source freeze lifted. Test duration 29s. Four test lanes each exited 255
after completing their tests; Toolbox preserved all results. No AngelScript compile diagnostics found.
The invocation used two discovery boots (headless and renderer) plus four test lanes: six editor boots,
exceeding the earlier two-boot estimate.
That estimate conflated test invocations with editor boots; use explicit serial execution for the next
focused gate and account for discovery/renderer launches separately. Toolbox process exit code was not
retained by the detached launch; build/result log verdicts and editor exit codes are the recorded evidence.

## Decisions and blockers

- User: unresolved required GOAP key is an ensure invariant violation.
- User: construction batch must validate all entries before running any.
- User: invalid-fragment return after an ensure is intentional; the developer may choose to continue.
  The sentinel finding is withdrawn, not deferred. Keep the reference APIs.
- User: explicit settings read/write/flush outcomes and local caller migration approved.
- User approved bridge retained terminal result, stopped admission, and explicit recovery. Do not release
  ownership/reclaim on fault. Recovery republishes captured outcome or retries outstanding request deletion.
  External driver timeout/re-execution and process-exit loss are accepted limits of this local repair.
- User approved native replacement for ordinary physical settings files on Win64/Mac/Linux. Retain UE
  physical-path mapping; bypass of move-specific virtual/quota wrapper behavior is explicitly accepted.
  Use sibling-temp MoveFileExW replace/no-copy or POSIX rename; do not claim universal crash durability.
  IFileManager::Move's destination deletion before install was source-confirmed in selected engine.
- Settings ResetAll/Revert must honor the existing atomic-configuration tenet. Bounded Astra review is
  complete: a single atomic batch-write provider operation is the smallest supported change.
  User approved atomic provider/current admission followed by non-failing void apply commands, with
  CK_ENSURE_IF_NOT rejecting nested mutations during callbacks. No external rollback protocol.
- User approved recording both EntityTag views in new saves; legacy saves retain exact name counts
  and restore an empty gameplay-tag view, since explicit gameplay identities cannot be reconstructed.
- User: operational I/O errors are player-facing and use ordinary Shipping-retained result branches,
  never ensures. Programmer invariants retain the ensure-only failure path.
- User: ownership queries reject repeated visits before another callback; valid reparenting remains allowed.
- User: debugger profile lifecycle callbacks cannot request nested transitions; use ensure rejection.
- User approved bounded graceful HangMonitor shutdown: two-second best effort, incomplete cleanup
  reported, no force-kill, engine exit continues. Runtime unload requires successful Stop first.
  ProcDump is not configured/installed locally; real-process tests must report NOT EXERCISED if absent.
- User approved rejecting repeated settings sections instead of combining them. Independent review
  found that previous reads/writes/removals only used the first repeated section. Rejection is now
  implemented before cache publication, with physical-file preservation/retry regressions. Unknown raw
  handwritten lines remain preserved under the original provider contract; their rejection is not approved.

## First post-fix gate — build failed

Toolbox PID 30980, supervised command session 38051, launched 2026-10-22 12:29 local:
`--build --target=Editor --test --test-pattern=TenetIntegrity --project-prefix=Ck --no-live --parallel=1`
with explicit project D:/Repos/CkPlugins_2 and output Saved/Logs/TenetIntegrity-Green-01.log.
Configuration resolves from the current receipt; 693 incremental actions. The initial three-boot
estimate was corrected to up to four: headless discovery/inline tests, renderer discovery, remaining
normal tests, and renderer-required tests. No editor boot occurred: the build failed before tests.
The supervisor retained the Toolbox process object; TenetIntegrity-Green-01.exit records exit 1.
Saved/Automation/TenetIntegrity-FrozenSources.json records SHA256 for 61 changed/new source files.
All 61 hashes matched after completion. The freeze was released only after process completion.
Compiler/linker failures: a const settings-fixture getter bound to a non-const dynamic delegate;
missing direct CkAStar linkage for test EnTT fragment access (the constructor exists in CkAStar.lib);
and two bridge-fixture SetReadOnly calls using IFileManager instead of IPlatformFile.
All three were corrected in the parser/conformance batch. No production constructor or delegate API
was changed to accommodate the fixtures. Tests have not yet run on these repairs.

## Second post-fix gate — 28 passed / 6 failed

Toolbox PID 34016, supervised command session 5146:
`--build --target=Editor --test --test-pattern=TenetIntegrity --project-prefix=Ck --no-live --parallel=1 --own-game-settings`
with explicit project D:/Repos/CkPlugins_2 and output Saved/Logs/TenetIntegrity-Green-02.log.
Expected selection is 34 tests, including repeated-section rejection and unknown-section preservation.
The prior launch budget remains up to four editor boots, accounting for discovery and renderer lanes.
Saved/Automation/TenetIntegrity-FrozenSources-02.json records 63 changed/new source and Build.cs hashes;
all were verified UTF8/CRLF before launch and all hashes matched after completion. Toolbox exit was 1,
recorded in TenetIntegrity-Green-02.exit; C++ build reported Succeeded. The build log/exit and red baseline are preserved under
Saved/Automation/TenetIntegrityEvidence/ before Toolbox log rotation.

Failures and evidence-led corrections:
- InfrastructureFailureIsNotSuccess, DirtyPackagesRequireExplicitOptIn, and PublicationFaultRetainsCapturedOutcome:
  literal [RunController] expected diagnostics were incorrectly registered as regex. Use AddExpectedErrorPlain,
  retaining exact text/count/severity. Assertions did not expose terminal-result behavior failures.
- InstallFailurePreservesOriginal: a zero-access handle did not prevent native rename. A scratch native
  experiment proved locking the ORIGINAL destination with read access and no delete sharing preserves
  original/temp files on failed replacement and succeeds after unlock (NativeInstallDestinationLock.json).
  Fixture corrected; production write-sharing remains unchanged.
- RepeatedSettingsSectionsRejected: existing reads are case-sensitive; StartsWith and operator== are not.
  Recognition, duplicate identities, and new staged writes now explicitly match the existing case-sensitive
  contract. Tests reject canonical repeats and preserve lowercase opaque sections through canonical writes.
- BridgeEndPlay: the fixture dispatched BeginPlay without actor initialization, so Unreal RouteEndPlay skipped
  its callback. The fixture now registers a world context and initializes actors before spawning; real
  DispatchBeginPlay/DestroyActor remain the exercised path. Test-helper conformance is complete.

All seven ownership tests, construction admission, GOAP native admission/tuning, both EntityTag tests,
and the other three profile tests passed in this run. Real ProcDump test reported NOT EXERCISED; its
success row is not live-process coverage. Deterministic bounded-shutdown test passed.

Inline discovery saw the old void Request_FlushStorage wrapper while declining regeneration ownership.
The owning lane regenerated the truthful bool wrapper; later fresh startup compiled all AngelScript stages.
Evidence: CkPlugins-backup-2026.10.22-20.09.04.log:714-727, current generated utils_game_settings.as:151-154,
and successful stage logs in CkPlugins_2.log / CkPlugins.log. No caller rollback or fake success is indicated.
The next fresh gate must confirm startup cleanliness rather than counting the failed discovery as green.

Coordination hold: Toolbox34016/session5146 completed and no local Unreal operation remains active.
The networking chat requested/confirmed an exclusive CkPlugins_3 window; do not launch a build, editor,
or test until that chat explicitly releases the window. Human coordination authorization was subsequently
verified directly via read_thread: userMessage 01a1653c-c5ad-7a33-a6af-94cade2cdd06 in chat
01a1565a-d71b-7633-89e9-90bec0342bd6 answered “Yes, coordinate with that chat” to the exclusive-window request.
A narrowly scoped coordination reply requested handoff after its active Avoidance operation; no interruption
was requested. This verified human authorization covers window coordination/release, not unrelated messaging.
Source-only work in this checkout continues; shared engine source/binaries remain untouched.

## Third post-fix gate — 34 passed / 0 failed

The networking chat explicitly released its window after Toolbox27020 completed at 20:24:30 UTC,
and confirmed it will hold further launches until this chat releases the window. Coordination acknowledged.
Toolbox PID 30544, supervisor session 2739; output Saved/Logs/TenetIntegrity-Green-03.log:
`--build --target=Editor --test --test-pattern=TenetIntegrity --project-prefix=Ck --no-live --parallel=1 --no-nullrhi --own-game-settings`
with explicit project D:/Repos/CkPlugins_2. Toolbox exit 0; build Succeeded; 34 passed, 0 failed,
0 contaminated. No AngelScript compile failures, stale-script diagnostics, or unexpected ensures.
FrozenSources-03.json contains 63 source/build-rule hashes, all unchanged after completion.
Actual launches were five: stale cached normal selection (6), headless discovery, newly discovered
normal selection (27), renderer discovery, and renderer-only selection (1). The prior three-launch
estimate was wrong: --no-nullrhi did not combine these scheduled lanes in this invocation.
Logs/exit and final fresh startup are preserved in Saved/Automation/TenetIntegrityEvidence/.
Four source files differ from Green-02: INI provider casing, settings file fixtures, debugger lifecycle
fixture/conformance, and exact bridge diagnostics. Shared engine source remains unchanged.

## Caller verification and evidence summary

| Gate | Build | Toolbox exit | Results | Editor launches | Preserved evidence |
| --- | --- | --- | --- | --- | --- |
| Green-03 / TenetIntegrity | Succeeded | 0 | 34 passed, 0 failed, 0 contaminated | 5, detailed above | `Saved/Automation/TenetIntegrityEvidence/Green-03.log`, `.exit`, `Green-03-FreshStartup.log` |
| GameSettings-01 | Test-only | 1 | 34 passed, 1 failed, 0 contaminated | 1 cached editor | `Saved/Automation/TenetIntegrityEvidence/GameSettings-01.log`, `.exit` |
| GameSettings-02 | Succeeded | 0 | 35 passed, 0 failed, 0 contaminated | 3: normal tests, headless discovery, renderer discovery | `Saved/Automation/TenetIntegrityEvidence/GameSettings-02.log`, `.exit`, `GameSettings-02-FreshStartup.log` |
| Goap-01 / planner callers | Test-only | 0 | 34 passed, 0 failed | 1 cached editor | `Saved/Automation/TenetIntegrityEvidence/Goap-01.log`, `.exit` |
| UI-CombatHud-01 | Test-only | 0 | 1 passed, 0 failed, 0 contaminated | 1 cached rendered editor | `Saved/Automation/TenetIntegrityEvidence/UI-CombatHud-01.log`, `.exit` |
| UI-PauseMenu-01 | Test-only | 1 | 1 passed, 1 failed, 0 contaminated | 1 cached rendered editor | `Saved/Automation/TenetIntegrityEvidence/UI-PauseMenu-01.log`, `.exit`, `UI-PauseMenu-01-FreshStartup.log` |
| UI-PauseMenu-02 | Succeeded | 0 | 2 passed, 0 failed, 0 contaminated | 3: headless discovery, renderer discovery, rendered tests | `Saved/Automation/TenetIntegrityEvidence/UI-PauseMenu-02.log`, `.exit` |
| UI-PowerWheel-01 | Test-only | 0 | 1 passed, 0 failed, 0 contaminated | 1 cached rendered editor | `Saved/Automation/TenetIntegrityEvidence/UI-PowerWheel-01.log`, `.exit` |
| UI-Journal-01 | Test-only | 0 | 1 passed, 0 failed, 0 contaminated | 1 cached rendered editor | `Saved/Automation/TenetIntegrityEvidence/UI-Journal-01.log`, `.exit`, `UI-Journal-01-FreshStartup.log` |

GameSettings-01 failed only because AtomicBatchRejectsAll expected the old "registration rejected"
message; the five intended ensure diagnostics now say "Invalid GameSettings definition, nothing was
registered". Rejection/no-partial-state assertions passed. The one-line correction in
`Source/CkGameSettings/Public/CkGameSettings/CkGameSettings_Registry.spec.cpp` uses
AddExpectedErrorPlain with that stable message, retaining occurrence count 0 and all assertions.
This is the only source difference from snapshot 03 to 04. Snapshot 04 records 63 source/build-rule
hashes; all remained unchanged through GameSettings-02. Its fresh startup had no script compile errors,
unexpected test errors, or contamination. The final result supersedes the stale-expectation failure,
whose original log and exit remain preserved.

ProcDump's opt-in row in Green-03 reported NOT EXERCISED; its success is not live-process shutdown
coverage. Deterministic bounded-shutdown verification remains separate.

## UI verification and unresolved observation

The networking chat handed back after QueueCross, then this chat ran two cached UI invocations.
CombatHud passed 1/1 in one editor, exit 0. PauseMenu selected its GymSelector sibling as well;
GymSelector passed, PauseMenuProof.PIE failed, exit 1 (one editor, no contamination). Logs and exits
are preserved as UI-CombatHud-01 and UI-PauseMenu-01 in the evidence folder. No script compile errors.
The supervisor stopped before PowerWheel/Journal. Snapshot 04's 63 hashes remained unchanged.
This chat verified all local Unreal processes exited and released the window back to networking.

Read-only diagnosis: this child uses local FCk_UiAuthoringProof_Settings, not GameSettings provider APIs.
TryPrepareSettingsChild is unchanged; the removed operational ensures were already skipped for children.
The first failure is action count 0. Unconditional Escape then closes the parent, explaining later
pause/context assertion cascades. There is not yet evidence identifying the actual preparation failure.
Test_PauseMenuProof.cpp now includes the existing Get_SourceError in that first assertion and returns
on rejection. This diagnostic-only fixture change adds one file to snapshot 05 (64 frozen sources).
After explicit handback, Toolbox 25700 rebuilt successfully and reran the same two-test order:
GymSelector then PauseMenuProof.PIE. Both passed, exit 0. Production code was unchanged; this is
focused-gate success, not evidence that the earlier intermittent failure was repaired or pre-existing.
PowerWheel and Journal then each passed 1/1 in a fresh rendered editor. Final logs have no unexpected
automation errors, AngelScript compile errors, stale-script diagnostics, or contamination.

A bounded Astra read-only consultation identified a candidate, not a confirmed cause: Click snapshots
mouse capture after down, then pumps messages/Slate while capturing the pressed screenshot before up.
It does not recheck capture, pressed state, or the hit path immediately before release. A handled release
does not prove successful action delivery. The new SourceError getter is pure, but an empty error alone
would not exclude script/session rejection because some admission paths do not populate it.
If this failure recurs, the next discriminating experiment is test-only: observe the parent session's
OnAction with the existing passive listener and capture pressed/capture/hit-path/session state immediately
before mouse-up, preserving the input sequence. No production workaround, delay, or retry was added.
This observation remains open; no fixture/root-cause attribution or broad regression-free claim is made.

## Closeout and compatibility

- All six original failing regression cases passed by exact name in Green-03. The final compatible
  settings and GOAP caller gates passed. The latest UI gates passed with the intermittent observation above.
- Custom settings providers and downstream BlueprintNativeEvent overrides must migrate to explicit
  read results plus output values, bool writes/flush, and atomic StoreValues. Count APIs report -1 on
  failure. Local providers and five AngelScript consumers are migrated; downstream Blueprint assets
  were not inventoried or exercised here.
- Native physical file replacement was exercised on Win64. Mac/Linux paths are source-reviewed only;
  virtual/quota move-wrapper bypass and lack of a power-loss durability guarantee are approved limits.
- Bridge recovery retains the captured outcome only within the process; external driver reruns and
  process-exit loss remain outside this local repair. Console recovery wiring is source-reviewed;
  processor publication/recovery behavior is exercised by regressions.
- Ensure macro expansion and build flags are unchanged. Shipping currently retains ensure checks;
  that observed configuration is not approval of a release exception. No Shipping build was run.
- All 64 source hashes matched snapshot 05 at final completion. UTF8/no-BOM/CRLF and plugin diff
  whitespace checks passed. Project changes remain uncommitted on bugfix/core-tenet-integrity;
  unrelated host .gitmodules and Script/Binds.Cache.Headers remain untouched.
- Verified no local project editor/Toolbox remained before explicitly releasing the exclusive window
  to the networking chat. No further build/test operation is planned for this repair batch.
No repository-wide regression claim is planned. ProcDump runtime behavior remains unexercised locally
because the external executable is absent; deterministic bounded-shutdown coverage is separate evidence.
