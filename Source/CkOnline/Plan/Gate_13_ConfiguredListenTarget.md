# Configure a consumer's listen target and gym gameplay port

Written 2026-10-04 before implementation. Evidence belongs in ../PROGRESS.md.
Entry: root 01b4fbd, Foundation d3a2b3b06, CkTests c55c07fc. Both plugin
checkouts are clean. OnlinePublicReadyLobbyGate-01 built incrementally and
passed eight focused tests. User-authorized AFK continuation and local commits
remain active; human editor acceptance remains deferred.

The existing ready-lobby consumer still builds an Unreal listen URL inline and
the lobby gym assumes gameplay port 7787 in both windows. This slice makes that
intent reusable and lets the same real gameplay flow use an explicit caller
port. It does not advertise gameplay ports over the control wire. Both windows
must select the same port; control TCP remains 7788. Steam still needs the game's
App ID and licensed test-account setup, which must not be invented.

1. Add getter-only BlueprintType FCk_OnlineDirect_ListenTarget with TravelUrl and
   GameplayEndpoint in the existing Foundation CkOnline module. Native TryBuild
   and reflected TryBuildListenTarget accept a soft UWorld map reference, a loaded
   GameModeBase subclass, literal canonical host IPv4 address and explicit port.
   TryBuildReadyLobbyListenTarget additionally requires an intrinsically valid
   hosting binding. Rejection resets the entire output. These pure helpers do
   not load assets/classes, travel, start drivers, mutate config or authenticate.
   Validate game-thread access, bounded canonical map package/object path with no
   subobject/options/redirect, valid non-abstract GameMode subclass, host address
   and port 1..65535 before building. Construct only map, port, game and listen;
   require exact FURL round-trip map/port/game/options and local-internal intent.
   An unloaded map reference proves path intent, not asset existence/type/cooking.
2. Replace the IP gym consumer's inline FURL builder with that API. Resolve the
   gym's already-loaded GameMode class in the consumer. Standalone IP hosting
   uses the generic helper; ready-lobby hosting uses the bound helper. Preserve
   ownership/live-binding/world/driver/deadline guards, recovery and state arming.
   The consumer continues to own map, mode, actual travel and driver readback.
3. Append GameplayPort to the native gym text-field enum and append F4 details
   row 43 after Y row 42, retaining all earlier indices/keys. Default remains 7787.
   Expose one validated text setter/getter and an edit-availability predicate;
   only recognized offline lobby/IP worlds with no pending driver/travel can edit.
   Accept 1..5 decimal digits and port 1..65535. Invalid/stale edits preserve the
   old port and every control value; edits never start/switch/stop a profile.
   Close the text editor before using the same setter, then recheck prerequisites.
   G passes the selected port to host/client paths; details show that same target.
4. Extend existing real TCP ready-binding tests with the builders and malformed
   inputs/output reset, including client/default binding rejection and unchanged
   live state/completions. Extend the real failed-entry recovery fixture to exercise
   the port setter before entry and rejection while entry is pending, restoring its
   original port. AS smoke calls both reflected builders on its actual ready host.
   Keep the FObjectKey world identity, bounded timings and eight-test population.
5. Add a short unchecked editor guide (under 250 words), indexed as deferred:
   exact two separate Standalone setup and Host J > Hosting > P / Client K joining;
   invalid port preserves draft, both windows use 7789, both R > Host G / Client G,
   fresh client proof P, Client X / Host X return with the same ready control IDs.
   F2 is a toggle: use only when details are hidden. This proves real driver port
   readback manually; automated URL/guard tests alone cannot establish it.

Bounded ownership: Foundation worker owns the new value/Utils/README; native test
worker owns IP consumer and native tests; gym worker owns native text input and
lobby AS gym/short guide; root owns contract, reflected AS smoke, review, evidence
and integration. Preserve unrelated work. No new module, wire policy, SDK, asset,
config, gameplay admission or profile policy. Normalize before one incremental
build/eight focused tests: two fresh boots (discovery and one serial lane), then
fresh error/ensure/script-log inspection. Local reviewable commits only; retain
both separate temporary research commits. No push, checkout duplication, artifact
deletion or history rewrite.
