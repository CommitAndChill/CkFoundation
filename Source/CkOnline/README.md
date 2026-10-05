# CkOnline Direct consumer guide

## Current continuation checkpoint, 2026-10-04

Implementation is underway on the selected existing CkPlugins checkout and
feature/online-services-research branch. The 2026-10-02 research transfer under
docs/research/online-services remains historical: its research-only authorization
and zero-runtime-evidence statements do not describe the current branch. Later
user instructions authorized implementation, gyms and local commits while AFK.
Use this permanent guide, PROGRESS.md and Plan/ gate contracts for current work;
retained implementation must not depend on the two removable research commits.

The latest scoped gate is queued-travel admission. OnlineQueuedTravelGate-01.log
records an incremental build and eight passing Direct/script tests after a real
red reproduction. That result covers the selected tests, not the whole delivery
roadmap. Human checks of new panels, F8/F9 and recovery remain deferred.

| Original delivery phase | Current evidence / remaining requirement |
|---|---|
| A: scope/provider compatibility | Selected source, engine and guides reconciled; first production consumer and licensed Steam setup remain unresolved |
| B: persistent service host | Guarded actorless ECS host has focused native/script evidence; human graph and optional-AS evidence remain separate |
| C: actual transports | Earlier two-window Direct round trip reported working; newer changes are unchecked; Steam, two-machine and installed Direct startup remain unverified |
| D: Direct vertical slice | Persistent TCP lobby/ready/text/invites and gym gameplay bridge exist; production gameplay owner is proposed, Direct-only staging remains unverified |
| E: Steam/social | No live account/App ID/invite or transport gate completed |
| F: turnkey integration | Reusable binding/listen targets and guided gyms exist; production recipe, second game, graph execution and optional-AS build remain incomplete |
| G/H: migration and future providers | Consumer migration/removal, release staging, EOS/EGS, voice and expanded topology remain separate scope |

The next production slice is Plan/Gate_14_GameplayOperationOwner.md. A9-A11 in
.claude/reports/ADJUDICATIONS.md await control-lobby authority, failed-entry return
and established transport-loss rulings. Keep existing gym behavior; do not create
production defaults or substitute more preparation helpers for that owner.

Editor cards live in CkTests/Script/CkOnline/DEFERRED_EDITOR_CHECKS.md. Start with
AFK_RETURN_EDITOR_TESTING.md for the combined normal path; F8 and F9 have separate
short cards. Each uses two separate Standalone windows, Separate Server Off and
RunUnderOneProcess Off, then Host J > Hosting > P / Client K; both R. Gate status
must stay unchecked until the user reports the named observations.

## Direct consumer recipe

The Direct profile provides an actorless lobby and text control channel over real
TCP. The GameInstance-owned online host retains one private ECS registry and
native socket resources across gym/map travel. Gameplay connections are a
separate Unreal IP transport gate. No account service or provider SDK is needed.

Start the profile with `UCk_OnlineDirect_Subsystem_UE::Request_Start(Spec)`.
Creation validates the complete spec and local processor graph before publishing
the service. A running profile occupies the host; stop it explicitly before
starting a different profile. Stop only affects the facade's own generation.

Use `Request(Action, Text, Ready)` for the reflected GI facade. For native
consumers, cast the root service with `UCk_Utils_OnlineDirect_UE::Cast`, construct
a fresh `FCk_Request_OnlineDirect` with a valid operation GUID and call
`TryRequest`. A true admission result means **queued**, not completed. Listen
to OnCompleted for the authoritative result and OnChanged for immutable snapshot
values, or poll Get_Snapshot/Get_LastCompletion/Get_CompletedCount in a view.

Actions are Host, Join (Text contains a copied descriptor), Leave, SetReady,
SendText and RenewInvite. Host assigns member identity, ready state, text sender
and sequence.
Public operation GUIDs are distinct from private transport correlation tickets.
A completion owns its terminal result before callbacks; stale responses cannot
complete a reused public ID. Requests and pending work are bounded to 64.

The host advertises an explicitly selected literal IPv4 address and TCP port
(default loopback 127.0.0.1:7788). Capacity includes the host and is bounded to
eight members. A copied descriptor contains the endpoint, fresh lobby epoch and
an expiring capability. Treat it as a join secret. Invite expiry prevents new
admission while existing peers remain connected. Rehost produces a new epoch;
a previous descriptor cannot silently admit into that new lobby.

Host-only RenewInvite rotates the capability and restarts the immutable spec's
configured invite lifetime without restarting the lobby. Use
`Request(ECk_OnlineDirect_Action::RenewInvite, "", false)` on the GI facade or enqueue that
action through native `TryRequest`. Wait for Succeeded before copying the new
`Snapshot.Get_JoinDescriptor()`. Renewal publishes one changed snapshot while
preserving generation, lobby epoch, endpoint, identities, readiness and transcript.
Old invites processed after rotation reject as Unauthorized; existing admitted
members stay connected. Joined, Joining, Offline and Failed services reject renewal
as InvalidState. Invalid deadlines reject as InvalidInput without changing the
invite or the renewal Execute clock cache. Host ticks and transport polling still
advance their monotonic clocks independently.

`Snapshot.Get_HostAddress()` exposes the validated literal IPv4 host address
after successful Host or authoritative Joined admission. It stays empty while
Joining and clears on leave, stop or connection failure. A client reads its invite
target here, independently of its own advertised-address draft. This address is
public routing information; it does not expose the invite capability or prove a
gameplay member's identity. Consumers choose their separate gameplay port.

`FCk_OnlineDirect_ReadyLobbyBinding` captures a ready roster for gameplay consumers.
Use `utils_online_direct::TryCaptureReadyLobby(Snapshot, Hosting, ExpectedMembers,
Binding)` in AngelScript or the corresponding `UCk_Utils_OnlineDirect_UE` function
in native code and Blueprint. Choose an explicit expected count from 1 to 8;
the two-player gym chooses 2. Capture requires the exact role, valid generation,
epoch and local identity, a canonical host IPv4 address, and unique ready member
IDs including the local member. Rejection resets the entire output binding.

`MatchesReadyLobby(Binding, Snapshot)` compares that captured identity and sorted
roster with a fresh ready snapshot. Invite renewal, chat, names and revision alone
do not break matching. A changed member, role, readiness, address, generation or
epoch does. This value provides correlation; consumers still check ownership,
current service state and their separate gameplay admission policy.

`TryParseGameplayEndpoint(Endpoint, Canonical)` accepts literal unicast IPv4:port
and normalizes decimal octets and port. DNS, map paths, options and invalid ports
reject. `TryParseReadyLobbyEndpoint(Endpoint, Binding, Canonical)` additionally
requires an intrinsically valid binding and its captured host address; gameplay
ports remain caller-selected. Both parsers clear rejected output and support
using the same string as input and output. These helpers do not start travel or
authorize a connection.

`TryBuildListenTarget(Map, GameMode, HostAddress, Port, Target)` prepares a
`FCk_OnlineDirect_ListenTarget` with getter-only travel URL and gameplay endpoint.
The map is a soft UWorld reference with a canonical full object path such as
`/Game/Maps/Lobby.Lobby`, at most 512 characters, with no subobject or URL options.
The asset name must match the package basename. The GameMode must be an already
loaded, valid GameModeBase subclass without abstract, deprecated or replaced-class
flags. The host address is canonical
literal unicast IPv4 and the explicit gameplay port is 1..65535. Validation runs
on the game thread and resets the entire target on rejection.

`TryBuildReadyLobbyListenTarget(Binding, Map, GameMode, Port, Target)` additionally
requires an intrinsically valid hosting binding and uses its captured address.
Both builders round-trip exactly the local map, port, game mode and listen options.
They do not load maps or classes, start a listener, travel or change configuration.
A valid unloaded map path does not prove that the asset exists or is cooked.
Consumers still verify live ownership, binding, world and driver prerequisites
before issuing travel and report success only after their actual listener is ready.

The ready-lobby CkTests gym freezes this address and defaults gameplay to port 7787;
F2 details expose F4 to select a gameplay port while the consumer is offline.
Both windows must select the same port; the control invite does not advertise it.
Control continues on TCP 7788. Its default remains loopback. An explicit reachable
LAN/Tailscale address is supported by the handoff recipe, with cross-machine
acceptance tracked separately in CkTests/Script/CkOnline/DEFERRED_EDITOR_CHECKS.md.

This private-network profile provides neither TLS nor account authentication.
Reachable LAN/Tailscale addresses can be selected explicitly; public discovery,
NAT traversal, friends, Steam, voice and durable messages are outside these gates.
Host loss produces failure; recovery is explicit. No automatic rehost occurs.

Text is nonempty, at most 240 TCHAR code units and 1024 UTF-8 bytes. Transcript retention
is bounded to the last 32 host-acknowledged messages. Typed data exposes values
and handles, never native socket pointers. Network receive/send queues and frames
are bounded; malformed peers must not mutate an accepted member's state.

Native feature composition attaches fragments to the same service entity before
publication. Two local Direct processors follow generic service processors and
precede entity lifetime. The host serializes the scheduler, dispatches afterwards
and closes feature resources inside its guarded shutdown boundary. No feature
processor is globally registered into gameplay's scheduler.

See PROGRESS.md for actual build/automation evidence. The editor acceptance
checklists live in CkTests/Script/CkOnline/DIRECT_IP_EDITOR_TESTING.md and
DIRECT_LOBBY_EDITOR_TESTING.md. Human two-window, clipboard/focus, cross-machine,
Blueprint graph, packaging and AngelScript-disabled acceptance are separate
observations; none follows merely from a successful native loopback test.
