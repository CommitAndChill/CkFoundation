# Make the ready-lobby binding a Foundation consumer API

Written 2026-10-04. Evidence belongs in ../PROGRESS.md. The user authorized
continuing while AFK; editor acceptance stays deferred. Entry: root cdb038f,
Foundation 663a939f9, CkTests 0efd1ea1. Both plugin checkouts are clean. Baseline:
OnlineInviteRenewalGate-02 incremental build/eight focused tests pass.

Source research read the current Direct public data/Utils/facade/transport, the
test-only FLobbyBinding and IP endpoint/travel/recovery paths, real TCP binding
tests, fresh failed-entry fixture and reflected AS smoke. The existing proven
neighbor is the two-ready-member gym handoff: capture a value binding, check it
again before travel, and retain correlation across recovery. Actual travel still
belongs to the test consumer, with gym map/mode/port assumptions.

The original replacement also needs Steam. Selected engine source has Steam
identity and lobby/session interfaces and its SDK payload, but this project has
no supplied App ID, account licenses or two-account test setup. Do not invent
those prerequisites or add a mandatory provider dependency to Direct. The next
independent slice moves existing ready-lobby validation into a reusable public
Foundation surface. It makes no authentication, gameplay admission or travel claim.

1. Foundation owns new Direct/CkOnlineDirect_ReadyLobbyBinding.h/.cpp in the
   existing CkOnline module. FCk_OnlineDirect_ReadyLobbyBinding is BlueprintType,
   getter-only CK_PROPERTY_GET/BlueprintReadOnly, default invalid. Native TryCapture
   takes snapshot, host/client role, an explicit expected-member count, and output.
   Expected count is 1..8; the caller chooses policy. Validate state, generation,
   epoch, local ID, exact bounded roster, address, every unique ready ID and local
   inclusion before publishing sorted IDs. Failure resets output completely.
   Matches recaptures using stored role/count and compares generation, epoch,
   local ID, canonical address and IDs; revision, invite, names and chat are excluded.
   -> Existing real TCP readiness/chat/renewal/rejoin coverage uses this same API.
2. Existing Direct Utils exposes reflected TryCaptureReadyLobby, MatchesReadyLobby,
   TryParseGameplayEndpoint and TryParseReadyLobbyEndpoint. Output parameters are
   ordinary output refs, not UPARAM(ref). Parsers accept literal IPv4:port only,
   preserve current decimal normalization, reject options/DNS/non-unicast addresses
   and reset outputs. Handle input/output aliasing safely. Binding endpoint parsing
   validates intrinsic binding invariants before matching its admitted host address.
   A binding proves correlation, never current liveness or identity credentials.
   -> Real host/client native tests cover default, malformed, stale and count/role/
   readiness rejection without changing live state or completions. AS uses reflected
   utils_online_direct wrappers on its actual ready host and verifies renewal continuity.
3. CkTests Bridge header becomes a type alias with no copied validation. IP consumer
   and existing TCP/recovery fixtures migrate to getters and explicit expected count
   2. IP parser methods may remain forwarding shims; parsing belongs to Foundation.
   Preserve gym ownership/generation guards, travel behavior, FObjectKey fixture
   identity, all timing bounds, existing keys and destinations. Do not add mutable
   production setters just to keep the old invalid-binding test assignment.
   -> Same eight focused tests, including real failed ClientTravel return; no new
   same-process gameplay world or known-red Iris fixture.
4. Add a short unchecked editor guide explaining the actual purpose: the gym now
   consumes Foundation validation. Repeat exact map/two separate Standalone setup
   and Host J > Hosting > P / Client K joining. Verify unready gameplay is unavailable,
   then both R, Host G / Client G, fresh gameplay proof and Client X / Host X return
   with the same ready control identity. Keep it under 250 words and index as deferred.
   -> Rendered/BP/clipboard acceptance stays unobserved until the user tests it.

No new module, Actor, SDK dependency, wire message, config mutation, auth policy,
travel owner or implicit profile switch. Root owns architecture/AS/integration;
bounded workers own production value/Utils, native consumer/tests, and editor docs.
Normalize before final gate: incremental build plus existing eight-test population,
two fresh boots (discovery and one serial lane), fresh errors/ensures/script review.
Preserve unrelated work and both separate removable research commits; no push,
new checkout, artifact deletion, dependency removal or history rewrite.
