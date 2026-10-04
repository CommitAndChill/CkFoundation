# Gameplay entry failure and host-loss recovery

Written 2026-10-04. Evidence/status lives in ../PROGRESS.md. Retire when a permanent
connection recipe replaces the gym fixture. The user requested committing the
completed gates and continuing while AFK; all editor checks remain deferred.

The next bounded gate exercises actual failed gameplay entry while the actorless
TCP lobby stays valid. Use one Standalone PIE client world and a native TCP lobby
host, never a second gameplay world. The existing two-world Iris fixture remains
red and excluded. No member authentication, wire, engine, driver implementation, global config,
firewall or two-role orchestration change belongs to this gate.

Research read the IP helper's ClientTravel, deadlines, failure delegates and exact
Lobby return, the existing PIE fixture startup/cleanup, real TCP feature tests,
owned gym invite admission and AS recovery hints. Proven neighbors are the owned
clipboard invite's atomic validation/rollback and the production observed-binding
return path. A bound silent UDP endpoint gives a deterministic unavailable gameplay
server; retaining an unrelated TCP port would not protect a UDP endpoint.

1. Add CkTests Request_JoinInvite as a clipboard-free entry into the existing owned
   validation/queue/rollback path. Route clipboard and saved-draft joins through it;
   invalid input must not start a profile, change a draft or mutate another owner.
   -> Test malformed/default invite against the actual fresh GI and real TCP join
   through the common API. Preserve existing clipboard/manual keys and accepted
   invite behavior. The production Direct facade/wire is unchanged.
2. Add explicit bridge-context and standalone-rejoin availability getters. Align
   panel/error hints with the native refusal of R after bridged play. Existing
   standalone Host/Join/Rejoin entry points retain their behavior.
   -> Native assertions cover actual post-failure context/rejoin state; AS compiles
   the getters. Do not derive behavior from presentation strings or fake a PASS.
3. A new EditorContext native test starts one Online Lobby PIE world, joins/readies
   the real native TCP host, and rejects a mismatched gameplay address before travel.
   It then calls real ClientTravel to a held silent UDP endpoint, waits for the
   helper's bounded failure/return, and verifies the new exact Standalone Lobby,
   absent driver/pending game/proof, preserved GI/control binding and actual error.
   -> Use a PIE-only map override so the open editor map stays untouched. Restore
   only owned PIE/settings/generation resources on every exit. Allow 35 seconds
   for the helper's existing 15-second join and 15-second return deadlines; no
   clipboard mutation, SaveConfig, engine modification, ensure suppression or
   direct test return.
   -> The first runtime gate exposed a surviving PendingNetDriver after local
   return. Cancel the pending connection through the engine's validated owned
   context before OpenLevel, and require no pending game before publishing Offline.
   Keep the failed-entry assertion intact and rerun the same focused siblings.
4. Add a short two-check guide for early Client G and established Host X first.
   -> [EDITOR-VERIFY, DEFERRED] Same map, two separate Standalone windows. Join Host
   J, wait Hosting, P / Client K; both R. Client G before Host must return with an
   error and retained control. Recover Host G, wait IP Hosting, Client G/P. Then
   Host X first returns both; compare control continuity and repeat G/P for fresh proof.

Entry checkpoint: root b5c7b75, Foundation 09d46f0b5, CkTests d29174bb, CkAuto
f1983a7e. Baseline is OnlineAdvertisedEndpointGate-01: seven focused Direct/script
tests pass and the incremental build/fresh logs are clean. Prior completed gates
are locally committed in independent slices; unrelated dirt and both removable
research commits remain preserved. No push was requested.
