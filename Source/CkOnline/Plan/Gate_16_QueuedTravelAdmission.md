# Preserve an already queued travel destination

Prepared 2026-10-04 at root ff05791, Foundation f6b18ceaa and CkTests b49c4c01.
Scope: localized CkTests entry admission; production Gate14 remains pending A9-A11.
Baseline: Gate15 incremental build and eight focused tests passed with clean logs.

The current Host/Join checks allow Offline Standalone with no active driver even
when its actual owned context has PendingNetGame, TravelURL or World.NextURL.
Selected engine SetClientTravel overwrites Context.TravelURL/TravelType; engine
Browse can replace an existing pending game. Entry must not displace that work.
The existing gameplay port editor already rejects all three busy conditions.

Require a valid exact owned current GI/world context with no pending game and
empty context/world travel URLs before publishing gameplay entry. Recheck after
preparation and immediately before state/binding/deadline mutation or travel.
Use the same idle-context definition for Offline and successful return readback.
Rejection uses the existing diagnostic but preserves travel, operational state,
binding and Direct revision/completion. Never clear another owner's travel.
This prerequisite does not decide control-loss or recovery policy.

Extend the existing real one-PIE recovery fixture: queue a valid same-Lobby reload
through engine SetClientTravel, attempt an otherwise-valid ready-lobby Join in
the same synchronous step, and assert rejection plus unchanged actual queued
URL/type, world, control identity and no new bridge. Await the real reload into
a different idle Lobby world before recording the next gameplay departure;
then retain genuine pending Join and timeout/return coverage. No manual engine
field mutation or fake ready world. Runtime coverage of World.NextURL and the
Offline-plus-pending branch is not inferred from the queued-client-travel case.

Expose a client-ready Lobby gym diagnostic on F9. It queues the real local reload
and attempts the competing join synchronously; report numbered observations
only after rejection preserves the queued destination, then the new Lobby is
idle with the same ready control identity. Keep diagnostic state separate from
operational state. No rollback that manufactures success; no overall PASS.
Retain existing controls and colors. A short unchecked card repeats two separate
Standalone windows, Host J > Hosting > P / Client K; both R, Client F9, then
Host G > IP Hosting / Client G and Client P. Include the normal return check.

Write the failing regression before the admission fix. Two incremental build/test
invocations are planned: red exact recovery test, green compatible eight tests.
The selected Toolbox adds one inline discovery editor to each serial lane, so
this is four fresh boots total. Freeze source during each invocation and inspect
complete fresh logs. No broad suite, artifact cleanup or alternate checkout.

Root owns IP header/cpp and evidence/integration. Native fixture and AS panel/card
have disjoint bounded owners. Preserve unrelated work and the two separate
[DROP BEFORE MERGE] documentation ancestors. Keep fix/coverage, diagnostic,
permanent evidence and root pointer commits independently reviewable.

Evidence, 2026-10-04: OnlineQueuedTravelRed-01.log reproduced false Offline,
overwritten destination, admitted Join, changed state and new bridge (1 failed,
exit 1). OnlineQueuedTravelGate-01.log passed incremental build and all eight
compatible tests (0 failed/skipped/contaminated, exit 0, tests 57s). Four fresh
boots covered red and green discovery/serial lanes. Fresh final logs have no
errors, ensures, script failures or reload contamination. Queued-server travel,
Offline-plus-pending and Host rejection branches are source-reviewed, not new
runtime claims. F9 execution/rendering and two-window proof remain unchecked.
