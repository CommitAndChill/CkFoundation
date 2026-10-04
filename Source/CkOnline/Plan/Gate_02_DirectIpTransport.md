# Direct IP transport gate

Updated: 2026-10-03. Status: compiles and completes the native host/join/proof/
disconnect/rejoin sequence, but the same-process PIE fixture fails on Iris
ensures. Separate-process human acceptance is pending. Current evidence lives
in ../PROGRESS.md; this is not a clean automated gameplay transport gate.

## Purpose and boundary

Gate 1 established the actorless, GameInstance-owned service host using a fake
adapter. This gate establishes an actual gameplay transport before designing the
production Direct provider and typed connection/lobby requests. Its smallest
implementation is a CkTests fixture using the existing Unreal IP driver. It does
not extend the generic operation API with an ad hoc payload, add another service
registry, or claim that a working lobby, discovery, login, invitation, or Steam
backend exists.

One initially independent Standalone instance explicitly hosts; another explicitly
joins an IPv4:port endpoint. A native GameInstance helper owns only fixture state,
deadlines and failure subscriptions. Unreal owns socket/driver and world lifetime.
A replicated test GameState carries a host-generated session marker, roster
through PlayerState, join/leave counters, and proof sequence. The marker is a
correlation value for this transport test, not a provider lobby ID.

Connected requires an open IpNetDriver server connection, an admitted local
PlayerState and the replicated host marker. A client-owned component sends a
reliable server RPC, and the host publishes the resulting sequence back through
replication. Local host button presses cannot satisfy this proof. Disconnect uses
ordinary world travel and driver teardown; the client retains its endpoint in its
GameInstance and reconnects only after an explicit Rejoin action.

## Acceptance contract

1. Both instances start Offline with no driver. Automatic PIE Listen Server or
   Client setup cannot establish that the fixture's Host/Join caused admission.
2. Host opens the requested port and creates a marker only after listen succeeds.
   Join accepts a bounded literal unicast IPv4:port, never arbitrary travel options.
   Invalid requests reject without opening a driver or altering replicated proof.
3. Both display the same valid marker and two distinct peer IDs. A client proof
   traverses the owned RPC and returns as the same replicated sequence/player ID.
4. Client disconnect restores its Offline gym and removes its peer from the host.
   The host stays listening and retains its marker. Explicit rejoin restores two
   peers and another proof advances the host's existing sequence.
5. Failure has visible diagnostics and no false connected state. Failed joins and
   host loss return to the fixture; recovery requires a user action. Two
   GameInstances must never consume each other's global failure events.

## Verification and delivery

The existing native fixture creates two initially independent Standalone PIE
contexts inside one process. It uses real IP sockets and replication, but its
Iris errors prevent a passing verdict. Client travel replaces its world, so the
fixture resolves the same PIE context again after each travel. It restores the
user's Play settings and temporary editor-world overrides on cleanup.

After two failed approaches, a further speculative shared-process retry is not
the next gate. A future automated replacement needs two-role process
orchestration, a combined verdict and bounded cleanup. The checked-in Toolbox
Gauntlet bridge supports only one role; no two-role harness has been added.

The major human gate is the separate-process, two-window Online Direct IP gym.
Exact settings, controls, expected transitions and failure recovery are in
CkTests/Script/CkOnline/DIRECT_IP_EDITOR_TESTING.md. Automated and human acceptance
are recorded separately. The overall gym verdict cannot auto-pass a two-window
comparison it cannot observe.

Keep this implementation in the existing authorized checkout. Preserve unrelated
dirty paths, existing submodule revisions and both root [DROP BEFORE MERGE]
documentation commits. Gate 2 fixture and Gate 1 production host remain reviewable
as separate future commit slices. No merge, history rewrite, cleanup or push is
part of this gate.

## Evidence

- Incremental C++/AngelScript compilation passed. Normal local listen travel
  fixed the late-Listen Iris startup-order error from OnlineDirectIpGate-02.
- OnlineDirectGates-06 completed host/join, first proof, disconnect/rejoin and
  second proof, but fails on a DataStream first-packet ensure and cross-PIE
  PlayerState reference in Iris. These are engine errors, not accepted failures.
  The gameplay fixture is excluded from the 22 passing OnlineDirectGates-08
  tests; its verdict remains failed.
- Separate-process rendered editor acceptance and cross-machine evidence remain
  pending. The Direct TCP lobby/chat tests establish their own control channel,
  not gameplay IP admission or replication.
- Source review corrected stale proof readback after rejoin and failure/travel
  recovery. Invalid port/endpoint, duplicate hosting and host-local proof are
  included in the native fixture; PIE teardown cannot end a pre-existing session.
- Submodule revisions and both root temporary documentation commits were
  rechecked and remain unchanged. Whitespace checks pass. No commit, push,
  history rewrite or build-artifact cleanup occurred.
