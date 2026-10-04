# CkOnline Direct consumer guide

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

Actions are Host, Join (Text contains a copied descriptor), Leave, SetReady and
SendText. Host assigns member identity, ready state, text sender and sequence.
Public operation GUIDs are distinct from private transport correlation tickets.
A completion owns its terminal result before callbacks; stale responses cannot
complete a reused public ID. Requests and pending work are bounded to 64.

The host advertises an explicitly selected literal IPv4 address and TCP port
(default loopback 127.0.0.1:7788). Capacity includes the host and is bounded to
eight members. A copied descriptor contains the endpoint, fresh lobby epoch and
an expiring capability. Treat it as a join secret. Invite expiry prevents new
admission while existing peers remain connected. Rehost produces a new epoch;
a previous descriptor cannot silently admit into that new lobby.

`Snapshot.Get_HostAddress()` exposes the validated literal IPv4 host address
after successful Host or authoritative Joined admission. It stays empty while
Joining and clears on leave, stop or connection failure. A client reads its invite
target here, independently of its own advertised-address draft. This address is
public routing information; it does not expose the invite capability or prove a
gameplay member's identity. Consumers choose their separate gameplay port.

The ready-lobby CkTests gym freezes this address and uses gameplay port 7787;
control continues on TCP 7788. Its default remains loopback. An explicit reachable
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
