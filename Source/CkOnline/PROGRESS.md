# Online services progress

Updated 2026-10-04. This is the current evidence ledger; gate files define the
contracts. Keep verified automation separate from unobserved editor acceptance.

| Gate | Current evidence | Human acceptance |
|---|---|---|
| 1: persistent actorless host | All 15 existing focused tests pass again in OnlineDirectGates-08 | User tried gyms; individual checklist results unspecified |
| 2: actual gameplay IP | Compiles; host/join/proof/disconnect/rejoin completed in OnlineDirectGates-06, but the fixture fails on engine Iris ensures | Pending; use the separate-process Online Direct IP gym |
| 3: persistent Direct lobby | Focused native TCP lobby, ready, expiry, cleanup, admission and malformed-wire tests pass in OnlineCommitGate-01 | User reports the five-step Lobby/Chat checklist passed on 2026-10-03; optional checks remain unconfirmed |
| 4: Direct text/shareable joins | Production TCP feature passes ordered text, sender identity, invalid-input and peer-isolation checks in OnlineCommitGate-01 | Same reported pass; logs do not independently prove roster/readiness/message delivery |
| 5: editor evidence checkpoints | Build and seven focused Direct/script tests pass in OnlineEvidenceGate-01; existing real loopback test covers checkpoint IDs and redaction | Pending; press V in both Lobby windows and again in both Chat windows |

## Evidence checkpoint phase, 2026-10-04

The Lobby and Chat gyms now expose **V** to record one immutable snapshot from
their owned active profile. A stale generation is rejected before the snapshot
read. Records include process/generation/epoch/local IDs, a sorted bounded ready
roster and ordered retained message sequence/sender IDs. Invite descriptors,
capabilities, endpoints, names, message text and error details are excluded.
This is test-gym instrumentation, with no change to the production wire protocol
or automatic pass verdict.

`Saved/Logs/OnlineEvidenceGate-01.log`: incremental build and **seven selected
tests passed**, zero failed/skipped/contaminated, process exit 0, no ensures or
script errors. Two fresh boots were used: inline discovery and one serial lane.
The existing real TCP loopback test now asserts identity/readiness/message
metadata and absence of names, text and invite capabilities in the formatted
records. It does not automate the rendered V action or prove gym travel.
The short human gate is `CkTests/Script/CkOnline/DIRECT_EVIDENCE_EDITOR_TESTING.md`.

The earlier implementation checkpoint and this subsequent evidence phase remain
separate local commit groups. The user supplied the commit author on 2026-10-04;
no commit was created by the earlier failed attempt. No push or history rewrite
occurred.

## Full focused online checkpoint before evidence instrumentation

`Saved/Logs/OnlineCommitGate-01.log`: incremental C++ build passed, AngelScript
loaded, Toolbox exited 0, and **22 selected tests passed**, with zero failed,
skipped or contaminated tests. Fresh output contains no ensures or script errors.
This includes the 15 host/Utils tests, atomic feature composition, and six Direct
service/wire tests. It excludes the separately failing gameplay IP fixture and
renderer-only tests. Native loopback coverage does not establish rendered UI,
GameInstance map-travel persistence or cross-machine behavior.

The final run used two fresh editor boots: inline discovery and one serial test
lane. OnlineDirectGates-08 previously needed three boots because the clock
regression was newly discovered. Serial limits concurrency, not the total
discovery/new-test boot count.
No extra rebuild or equivalent test rerun is required for documentation edits.

Use explicit native prefixes: the project has no AutomationGate.json, and the
default project prefix selected only the script AutoTest in OnlineDirectGates-05.
That run was not a native gate. The final arguments were:

```text
--build --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --project-prefix=Ck.Online.Host
--project-prefix=Ck.Online.Utils --project-prefix=Ck.Online.Feature
--parallel=1 --no-live --skip-renderer-tests
--project=D:\Repos\CkPlugins --output=Saved/Logs/OnlineCommitGate-01.log
```

Launch through the documented detached UnrealToolbox workflow. DirectIp has a
different prefix and must retain its own verdict when selected.

## Failures and limits

OnlineDirectGates-06 selected 22 tests: 18 passed and four failed. Three Direct
failures exposed an idle-socket defect. Engine source confirms a nonblocking
Recv may succeed with zero bytes on would-block. The transport now peeks only
when the socket reports read readiness and treats a failed peek as disconnect;
an idle zero-byte successful read leaves the peer connected. All affected Direct
tests pass in the final run.

A final read-only review found that adding a short positive lifetime/timeout to
an extreme finite clock can round back to the same timestamp. Direct now rejects
those deadlines before opening a socket or queuing remote ready/text. The generic
host likewise rejects a non-advancing deadline before invoking its adapter.
The new ExtremeClockRejectsBeforeTransport test checks real feature/host rejection,
same-port availability, and no remote ready/text mutation after a real loopback
join; the existing generic deadline test also covers this rounding case.

Gate 2 first failed because late Listen created Iris before its PIE ID was set.
Host now uses normal local listen travel and waits for the replacement world's
bound IP driver before publishing Hosting. OnlineDirectGates-06 then completed
host/join, a replicated client RPC proof, disconnect, rejoin and a second proof.
Its verdict still fails: Iris reports both a DataStream first-packet ensure and
a PIE_1 PlayerState referenced by the PIE_0 NetDriver. Functional observations
cannot override those errors. No ensure suppression, Iris disablement, engine
patch or global networking configuration change is used.

After two failed approaches, do not repeat speculative same-process attempts.
The separate-process editor gym is ready for human verification. A future clean
automated gameplay gate needs two-role process orchestration with a combined
verdict and cleanup; the existing Toolbox Gauntlet bridge runs one role only.
That orchestration has not been implemented or run.

Work stays in D:/Repos/CkPlugins. Root feature/online-services-research and both
[DROP BEFORE MERGE] docs commits remain unchanged. Foundation/CkTests actual
starting baselines were 6cf825dd/66d8cbed; unrelated root/submodule dirt remains preserved.
Implementation and permanent documentation use local checkpoint commits
independently reviewable from the removable research commits. No push,
cleanup or published-history rewrite is authorized by this checkpoint.

## Reported editor acceptance, 2026-10-03

The user reports all checklist steps passed. The latest presented checklist was
the five-step Lobby/Chat guide; gameplay IP and the optional checks are not marked
accepted without a separate report. Archived logs are
`Saved/Logs/CkPlugins-backup-2026.10.04-06.20.47.log` and
`Saved/Logs/CkPlugins_2-backup-2026.10.04-06.20.29.log`. Both processes used this
checkout and entered Direct Lobby; the editor also entered Direct Chat. No
join/roster/ready/message outcome records exist in these logs, so the visual
report remains the evidence for those outcomes. Stable IDs, retained transcript
and generation across travel will gain explicit gym checkpoints in the next
bounded phase.

The rendered travel logs exposed a legitimate unset loading-screen widget class
being passed to CreateWidget. A small independent guard now selects the existing
built-in throbber without that invalid call. OnlineLoadingScreenGuard-01 passed
the existing LoadingScreenHolderSemantics test; its headless run does not verify
the rendered fallback. The final build and 22-test online gate passed after this
guard and the test plugin's missing OnlineSubsystemUtils dependency metadata
were added. Existing startup logger/FText automation errors are unrelated and
are not described as clean logs.

User authorized proceeding through the next few playable gates while AFK.
The bounded sequence is gameplay IP, persistent Direct lobby/ready, then text
and shareable join details. Lobby and chat use a real actorless TCP control
channel, not the fake Gate 1 adapter or UE gameplay replication. Entering gameplay
still depends on truthful Gate 2 evidence.
Steam/account/App ID setup, two-machine/Tailscale evidence, packaging, Blueprint
graph execution and rendered human acceptance remain separate observations.
