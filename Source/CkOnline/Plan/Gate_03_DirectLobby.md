# Persistent Direct lobby gate

Written 2026-10-03. Current evidence and moving status live in ../PROGRESS.md.
This contract is superseded when the permanent consumer guide replaces it.

After this gate, two initially independent play instances can explicitly create
and join one Direct lobby, see the same authoritative roster and ready states,
leave/rejoin, and travel between gym maps without losing their lobby connection.

The online domain stays in the existing GameInstance-owned service registry.
Add a native typed feature composition seam to that host: attach fragments before
publishing, chain local processors before lifecycle, dispatch signals after the
scheduler returns, and close provider resources during guarded shutdown. Generic
operation adapters become optional for a typed-only host; unsupported generic
requests reject at admission rather than going through a dummy adapter.

The new CkOnline Direct feature follows the existing OnlineService and CkTimer
quartet conventions. Utils enqueue bounded typed requests, processors drain a
copy/reset the queue, and native socket code owns only values and sockets. The
provider has no world, GameMode, player-controller, or gameplay Actor dependency.

New infrastructure: bounded, versioned, length-framed nonblocking TCP on the
private reachable network selected by the user. All reads, writes, peers, pending
operations and retained messages are bounded. Host membership is authoritative;
member identity comes from the admitted socket, and invitations carry a fresh
lobby epoch and expiring capability. This is a private-network Direct profile,
not a public directory, persistent friend service, account authentication, or TLS
channel. No provider SDK or external service is configured in this gate.

Acceptance:

- Actual loopback sockets connect two separate service hosts; no fake provider or
  in-memory delivery shortcut. Generic requests on the typed-only host reject.
- Create/Join complete once with a valid epoch and two distinct member IDs.
- Ready changes appear only after the host accepts them; both peers converge.
- Full, expired/wrong-capability, malformed and oversize requests reject without
  partial membership or unauthorized mutation. A failed join does not become Ready.
- Leave removes the peer; rejoin obtains a newly admitted identity. Host loss
  produces explicit failure and bounded cleanup, with no automatic rehost.
- Shutdown from a result callback and final-owner release close sockets safely.
- Two-window Online Lobby gym displays TCP state, service generation, lobby epoch,
  roster, ready status and last error. It explains that gameplay may still be
  Standalone because this gate tests the persistent control channel.
- Host travels to Online Chat while client stays in Online Lobby: same generation,
  epoch and roster; the service socket outlives the map. Human acceptance remains
  separate from automated socket evidence.

Implementation boundaries: CkOnline native host/composition, Direct quartet,
native TCP provider and lazy GI facade; CkTests real-socket automation, thin gym
views and focused text entry. Do not change shared scheduler or engine sources.
If composition cannot remain local/atomic, record the failure and simplify before
continuing. Preserve all unrelated work and the two removable root docs commits.
