# Ready lobby to gameplay round trip

Written 2026-10-04. Current evidence lives in ../PROGRESS.md. This is a test-consumer contract, not a production connection/admission API. Retire it when the permanent connection recipe replaces this fixture.

## Scope

Reuse the CkTests Direct IP fixture from the existing Lobby/Chat gyms. Two separate processes on one machine keep their actorless TCP lobby alive while entering stock IP gameplay at `127.0.0.1:7787`, sending the existing owned client RPC proof, and returning to Online Lobby. No CkOnline wire change, provider SDK, global driver override or engine change belongs to this gate.

The neighboring implementation is `UCkOnlineDirectIpTest_Subsystem` and its replicated GameState/proof component. Native TCP snapshots remain the source of lobby identity/readiness. The replicated gameplay session marker remains independent from the captured Direct lobby epoch.

## Steps and verification

1. Add a bounded value binding for a valid, owned two-member ready snapshot. Capture generation, epoch, local member ID, sorted distinct roster IDs and Hosting/Joined role atomically. Reject unready/default/stale values before issuing travel or opening a listener.
   → Verify with the existing real TCP loopback test at its ready and text checkpoints, including malformed copies and no successful partial binding.
2. Add explicit ready-lobby Host/Join entry points to the IP test helper. Preserve the standalone IP actions. Use fixed gameplay and return modes; no invite capability enters a travel URL. Revalidate the frozen lobby values while active, and return on control loss/change or wrong replicated epoch.
   → Verify an incremental build and compatible focused native guard/socket/script coverage. This does not establish two-process networking or rendered travel.
3. Add G in Lobby/Chat and bridge identity/readback in the IP panel. X returns a bridged session to Online Lobby; V records bounded control/transport metadata.
   → [EDITOR-VERIFY] Unready G is disabled without travel (Gate 7 UI); native entry rejection remains independently tested. Ready Host G starts listening; Client G joins. Shared marker/epoch and the client's P proof match. Client X then Host X return; V shows preserved per-process identities, ready roster and transcript. A new chat message reaches both peers.

## Evidence boundary

The original same-process IP automation remains red on Iris ensures. Do not retry another speculative same-process workaround, suppress ensures or disable Iris. The combined rendered gate is a separate-process human observation until proper two-role orchestration exists.

This fixture correlates a gameplay session with a lobby epoch. It does not authenticate a gameplay PlayerState as a particular Direct member: there is no private per-member admission ticket. No client-supplied member GUID is accepted as authenticated identity. Production admission, cross-machine endpoints, automatic launch coordination, Tailscale and host migration remain later contracts.
