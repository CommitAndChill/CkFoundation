# Promote gameplay entry and return into Foundation

Prepared 2026-10-04 at root 2a1ec73, Foundation 57592becc and CkTests 8b468864.
Status: proposed contract; no implementation or new runtime evidence yet.
Gate13's eight passing tests were the automated baseline at proposal creation.
See PROGRESS.md for subsequent independent fixes and current evidence. Human
checks remain deferred in CkTests/Script/CkOnline/AFK_RETURN_EDITOR_TESTING.md.

## Smallest useful slice

Ready bindings and listen targets are reusable, but games still depend on
CkTests for Host/Join/Return orchestration. Promote that operation owner into
the existing CkOnline module. Keep the gym's map/mode selection, exact-two-player
policy, replicated marker/epoch, PlayerArray checks, RPC proof and UI in CkTests.
Engine connection is transport evidence; it does not authenticate a Direct
member or establish game-specific admission.

The current reusable boundary is CkOnlineDirectIpTest_Subsystem.cpp:255-397
(entry/return), 450-544 (deferred failure/recovery) and 621-655 (failure filtering).
Its continuous control check is at 452-460; readiness at 546-605 is gym policy.
Foundation already supplies a guarded feature host, curated local processors,
monotonic ticking and generation invalidation. Reuse those contracts without
changing CkEcs, global registration or core shutdown semantics.

## Proposed ownership and public contract

1. A typed connection feature owns canonical request/state/completion data in
   a persistent ECS registry; Utils queue bounded requests. A thin GameInstance
   facade/native adapter owns engine subscriptions and weak world/driver
   observations. Its host is independent of the Direct profile, so stopping
   control cannot destroy an in-progress return operation. No Actor scheduler,
   duplicate lobby cache or gameplay Actor belongs to this service.
   The new facade owns a separate FOnlineServiceHost; the existing online
   subsystem admits only one host and remains the Direct profile's owner.
2. Host/Join require the current owned Direct service, captured ready binding,
   explicit expected member count, configured map/mode/endpoint and return target.
   Revalidate generation, binding, context, driver compatibility and advancing
   deadline before publishing an operation or issuing travel. Reject atomically.
   Permit one active entry/return transaction; bound queued requests.
3. Correlate every operation with an ID and connection generation distinct from
   Direct service generation/lobby epoch. Report TransportEstablished only after
   actual listen-port/open-client-connection readback. A caller's correlated
   ConfirmGameReady request supplies the separate game readiness barrier; stale,
   duplicate or premature acknowledgements cannot complete another operation.
   Require both operation ID and connection generation on that acknowledgement.
   The original bounded entry deadline continues through AwaitingGameReady;
   transport/readiness observations are nonterminal. Entry succeeds exactly once
   after a valid acknowledgement; retained active-session identity then observes
   transport/control loss. Return and internal recovery use separate operation
   IDs/results and cannot complete the already successful entry again.
4. Claim terminal ownership before notifications. Failure callbacks copy bounded
   facts; owning ticks perform recovery after engine callback stacks unwind.
   Cancellation touches only the owned pending context. Return requires a new
   world identity, configured map/mode, Standalone, no driver/pending game/queued
   travel. Failed return terminates once, with no automatic retry loop. Teardown
   closes admission and does not start new travel.
   Engine events carry no operation ID: filter by tracked context handle,
   world/driver object identity and phase, with weak observations. Copy only
   accepted bounded facts into the owning queue. Prohibit replacement entry
   until cancellation/return has settled; reject old-world/old-driver teardown
   events, including events arriving after an entry completion. Remove native
   subscriptions during physical shutdown after guarded dispatch unwinds.
5. A9-A11 in .claude/reports/ADJUDICATIONS.md record the unsettled behavior
   policies. Recommendation: preserve the gym's continuous control authority and
   automatic failed-entry return as the first consumer contract. No production
   default is established before the maintainer's ruling.

A9 and A10 are independent: A9's continuous-authority choice means configured
return on control loss even after successful entry, regardless of A10's entry
failure choice. Post-readiness transport loss also needs an explicit contract;
recommend the same bounded configured return as today's gym. Any caller-selected
variant must use explicit policy values, reject unspecified/invalid values, and
have the gym explicitly select its existing behavior. Zero-initialized data must
not silently choose a policy. This is an alternative contract, not an accepted
default or maintainer ruling.

Source review recommends a private stock-IP adapter with OnlineSubsystemUtils
as a private CkOnline dependency and an explicit Foundation plugin reference,
matching CkTests' existing dependency arrangement. Those changes are proposed,
not applied. Preflight the configured GameNetDriver's already-loaded valid class
and require exact UIpNetDriver for this adapter; no LoadClass or CDO/resource
operation in preflight. After travel require a valid exact-stock non-default,
non-archetype driver owning the observed world. For hosts require no
ServerConnection. For clients first require a valid exact UIpConnection,
non-default/non-archetype, owned by this driver and USOCK_Open. Only then call
IsNetResourceValid and read address/port and role. Actual readback is authoritative
because the engine can select a configured fallback.
Do not rewrite driver configuration or imply compatibility with arbitrary IP
subclasses. Keep the concrete driver type out of public signatures.

Selected-engine evidence: NetDriver.h:1584 implements GetLocalAddr; 1841/1862
make GetSocketSubsystem/IsNetResourceValid PURE_VIRTUAL. IpNetDriver.h:104
implements resource validity with GetSocket; IpNetDriver.cpp:1306-1314 accesses
connection resolver/socket state. IpConnection.cpp:96-101 constructs the private
resolver; cleanup at 297-311 clears sockets without resetting that resolver.
That invariant supports exact constructed stock objects, not arbitrary subclasses
or corrupted internals. UnrealEngine.cpp:14646-14665 resolves configured/fallback classes
and calls IsAvailable on their CDO. Do not call arbitrary driver resource methods
to discover whether their prerequisites hold.
Return's no-queued-travel invariant is proposed, not existing runtime evidence:
the current fixture does not assert Context.TravelURL or World.NextURL on return.
The selected engine declares them in Engine.h:346 and World.h:1971;
UnrealEngine.cpp:15237-15243 queues SetClientTravel through Context.TravelURL.
Extend the real recovery readback accordingly; do not clear another owner's
queued travel merely to manufacture a passing observation.

## Evidence and editor acceptance

After implementation, plan two fresh boots: discovery plus one serial focused
lane, using an incremental UnrealToolbox build. Extend existing real failed
ClientTravel/replacement-world recovery coverage; add focused correlated
completion, rejection, cancellation/re-entry and return-failure coverage through
the actual feature. Inspect fresh complete logs. Do not fake ready engine state.
Include compatible unchanged service-host coverage in the same final lane.
Native/AS evidence and human Blueprint execution remain distinct observations.

The human card must have at most three checks and repeat joining: open the
CkTests TestGyms level, select two Standalone players, Separate Server Off and
Run Under One Process Off; launch New Editor Window (PIE), then Tab > CkOnline >
Online Lobby in both windows. Host J, wait Hosting, P / Client K. Both R, Host G
and wait IP Hosting / Client G, then client P for fresh RPC proof. Client X /
Host X return. A separate short fault check covers the chosen control-loss and
failed-entry and established transport-loss policies. Each card must name what
the observation proves.

The inspected Toolbox/Gauntlet surface has no confirmed supported two-role
orchestration route. No automated two-process gameplay result is claimed.
Runner supervision/rendezvous is separate infrastructure work; do not bypass
Toolbox with raw editor launches or repeat the known-red two-world Iris fixture.

Preserve unrelated work and both separate [DROP BEFORE MERGE] ancestors. Keep
implementation, consumer/tests, evidence and root pointer checkpoints reviewable.
Steam credentials/App ID, remote/Tailscale, packaging and broad EIK removal remain
separate gates; no new checkout, artifact deletion, push or history rewrite.
