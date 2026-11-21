# Design: replicated lifetime ownership

Status: implemented as candidate B (Neil's call, 2026-11-21) and through one independent diff review;
NOT yet compiled or run as of this writing. Adam reviews the result. A is the fallback.
Base: CkFoundation `d6cfdf2b3`. Engine: UE 5.8.4 fork, Iris on (`net.Iris.UseIrisReplication=1` in BusterBlock).
Reviewer's decision to confirm or reverse: the ordering mechanism (section "Ordering: two candidates").

## Problem

`UCk_Utils_EntityLifetime_UE::Request_TransferLifetimeOwner` (`EntityLifetime/CkEntityLifetime_Utils.cpp:751`)
changes an entity's lifetime owner on the machine that calls it and nowhere else. A client keeps the
owner the entity was BUILT under (`Net/EntityReplicationDriver/CkEntityReplicationDriver_Fragment.cpp:175`
and `:261`). When that old owner is destroyed, the client applies the destroy in the net receive and
tags the old owner's whole lifetime subtree for destruction at once
(`EntityLifetime/CkEntityLifetime_Utils.cpp:202-210`), so the transferred entity dies on the client
while it lives on the server.

Concrete case: a picked-up world item. The server transfers the item to the player's inventory
(`CkInventory_RequestHandlers.cpp:171`) and destroys the world item. The client loses the item, and
because `ck::inventory_net::ApplyNamedSnapshots` refuses a whole owner snapshot that names an
invalid item (`CkInventory_NetSnapshot.h:75-79`), that player's inventories stop updating on the
client until the server removes the item.

The same gap exists for every caller of the transfer on a replicated entity (15 call sites: inventory
add/split/remove, save-load restore, `CkSnapshot_Subsystem.cpp:1642`, procedural leg detach).

Two neighbours of the same class - "a client child whose replicated owner is not there":
- A late joiner receives a child whose build owner no longer exists. `_OwningEntityDriver` is null,
  `OnRep_ReplicationData` ensures and returns (`CkEntityReplicationDriver_Fragment.cpp:138`), and
  nothing retries. The EntityScript and Ability OnReps dereference it unchecked (`:248`, `:349`).
- `_PendingChildEntityConstructions` is appended to (`:151`, `:257`) and never drained.

## What the engine guarantees (read from the 5.8 Iris source)

Paths under `Engine/Source/Runtime/Net/Iris/Private/Iris/ReplicationSystem/`.

1. A root object and its dirty subobjects are written as one atomic batch (`ReplicationWriter.cpp:3098-3104`).
2. On receive, per root batch: an existing object's reliable RPCs run BEFORE its state is applied
   (`ReplicationReader.cpp:79`, `:2047-2054`; not for an object created in that packet, `:2048`);
   when the batch is complete, subobject destroys run first, THEN RepNotifies (`:1961-1981`).
3. Reliable RPCs on one object go through one reliable queue. Nothing orders RPCs across objects, or
   an RPC against state.
4. A subobject removed with reliable RPCs pending is held until they are acknowledged; only then is
   its destroy sent (`ReplicationWriter.cpp:715-719`, `:965-983`). A root destroy waits on its
   subobjects' reliables too (`:721-776`).
5. A multicast is enqueued per connection and silently dropped when the target is not in that
   connection's scope or its reliable queue is full (`ReplicationWriter.cpp:315-320`, `:336-339`).

Consequence: rewriting the child's replicated owner is necessary but NOT sufficient. If the rewrite
and the old owner's destroy share a batch, the destroy runs before the child's RepNotify (point 2).
Across packets nothing orders them (point 3). Something else must stop the old owner's destroy from
killing the child.

## Shared core (needed whichever ordering mechanism is chosen)

### Published owner

The wire can only name an entity that has a replication driver. An inventory entity has none: it is
a plain child of its owner (`CkInventory_Utils.cpp:162`), built locally on every machine. So the
published owner is **the nearest driver-bearing entity on the child's current lifetime chain**, or
"world" when there is none. For a picked-up item that is the player's entity, not the inventory. A
client therefore parents the item to the player; the server parents it to the inventory. Both are
correct for lifetime: the server still sends the item's own destroy when the inventory dies.

One function computes it, and every path that writes `_OwningEntityDriver` uses it: the three
establishment paths (`Request_TryBuildAndReplicate`, `Request_TryReplicateExisting`,
`Request_Replicate`) and the transfer. Today the EntityScript path captures the spawn-time owner in a
request and writes it later (`EntityScript/CkEntityScript_Processor.cpp:397-405`, `:489-507`), so a
transfer in between would publish a stale owner.

"World" is published the way the EntityScript data already does it, as self-reference (`:213`,
`:265`); the other two variants adopt the same convention. On a client, a NULL owner is never world:
it means unresolved, and the child keeps (or is held under) what it has.

### Server: the transfer publishes itself

An explicit call at the END of `Request_TransferLifetimeOwner`, after the reverse link and the
inherited destroy tags are written (`:801-815`), with the old and new owner; host only; skipped when
the entity or the new owner is already in destruction. Not an EnTT `on_update` listener: that fires
inside `Replace` (`:799`) with the links half-written, and would also fire for the client-side
transfers this design adds.

For the moved entity, or, when it has no driver, for each first driver-bearing descendant (their
nearest replicated ancestor changed too):
- rewrite `_OwningEntityDriver` to the published owner and mark it dirty;
- if `_IsOwningEntityDriverDependentOnThis` was true, clear it, and lower the old owner's
  `_ExpectedNumberOfDependentReplicationDrivers` only when the child was actually counted there. A
  child is counted by the old owner's establishment walk (which stops at driverless entities) or by
  the EntityScript bump while that owner was just created. So an EntityScript child lowers iff its
  flag was set; a definition-built child lowers iff its flag was set AND the old owner has already
  published. The new owner's count is not touched.

### Client: idempotent OnRep

`OnRep_ReplicationData`, `OnRep_ReplicationData_EntityScript` and `OnRep_ReplicationData_Ability` get
an explicit "already constructed" guard (the Ability one has a TODO for this, `:290-292`). When
constructed and the published owner resolves to an entity other than the current lifetime owner:
`Request_TransferLifetimeOwner` (Setup refuses to change an owner, `:642-655`), and nothing else.
Construction, net params, replicated objects and dependent counts run once.

Dependent counts on a client that is mid-construction: the child's driver remembers which owner
driver it incremented and reconciles that record first, on every notify, whether or not a re-parent
follows (an A -> B -> A change coalesced on the wire shows an unchanged owner but a cleared flag).
An old owner's OnReplicationComplete can still fire one packet early in three interleavings, all
"expected dropped before the child's own rewrite was applied"; none is a hang. The PR must say so.

A late joiner needs no new path: its first OnRep builds under the published owner. If that owner
has not arrived yet (it can now be on another root actor), the first notify is a quiet wait, relying
on Iris notifying again when the reference resolves. That wait is logged once and has NO watchdog
yet: neither existing stall mechanism in this module can carry it without a new processor.

## Ordering: two candidates

Both stop the old owner's replicated destroy from killing a child the server moved away.

### A. Release RPC on the old chain

At transfer time the server calls a reliable multicast `Request_ReleaseLifetimeDependents` on the
driver of every replicated entity on the OLD chain that is not on the new one. By points 3 and 4 it
runs on a client before that entity's own destroy. The client handler moves each released child to
its published owner if that already resolved, else to the transient entity (a hold).

Needs, beyond the core:
- an owner epoch in the replication data, bumped per publish and carried by the release. Without it
  A -> B -> A parks the child forever: the client never saw B, the release on A parks the child, and
  no OnRep fires because the value is unchanged (`REPNOTIFY_OnChanged`, `:78`);
- per-frame batching (a Loot All must not queue one reliable RPC per item);
- the ancestor walk on the server.

Holes that remain: point 5 (queue full, or the old owner leaving scope for one client while the
child's host stays) drops the release silently, and the child dies. An ancestor destroyed by a path
that is not its own driver's destroy is not ordered at all.

### B. A client's cascade spares a dependent that has its own live driver

On a client, `Request_DestroyEntity`'s cascade (`CkEntityLifetime_Utils.cpp:206-210`) does not
destroy a dependent whose own replication driver is alive. That dependent is destroyed when ITS
destroy arrives, which the server already sends per entity
(`EntityLifetime/CkEntityLifetime_Processor.cpp:121-141`).

"Alive" means all of: the entity carries the client-only replica marker; its driver object is valid
and still names it; and the driver's host Actor (`GetTypedOuter<AActor>()`) is valid, not being
destroyed and has begun play. The host-actor terms matter: a client-local `AActor::EndPlay` (level
streaming out, tear-off) detaches only the root and leaves child drivers untouched, and each replica
strong-roots its own driver, so without them a spared child would leak for good. In the window
before a host actor has begun play nothing is spared, which is today's behaviour.

The spared dependent is re-homed in the dying owner's Teardown phase, BEFORE the Teardown tag is
added: to its published owner if that resolves to an entity not in destruction, else to the
transient entity. A spared dependent that has itself reached EndPlay by then is left alone; it
finalizes with its owner. Re-homing at Finalize instead (the first draft) left a live child under a
past-Teardown owner for a tick, and every ordinary ownership-chain walk ensures on that.

Needs, beyond the core: the cascade check and the Teardown re-home. No RPC, no epoch, no ancestor
walk, no batching, and the transfer function itself is unchanged apart from the publish call.

Why it is order-independent: nothing has to arrive before the old owner's destroy. Covers the cases
A cannot: point 5, ancestor cascades, the host actor closing, relevancy loss.

Cost: client teardown for EVERY replicated subtree now follows each entity's own destroy. For a
subtree on one root these arrive in the same batch and the same frame, so nothing observable
changes. Across roots a child can outlive its owner's destroy by a frame or more; that is why the
re-home waits for Finalize rather than orphaning at once. Host behaviour is unchanged.

Leak check: a spared dependent whose destroy never comes. Channel close and relevancy loss end
replication for every subobject, and each driver's `PreDestroyFromReplication` / `BeginDestroy`
destroys its own entity (`CkReplicatedObjects_Fragment_Params.cpp:82-133`), so no path leaves one
behind while the connection lives.

### Recommendation: B

Less machinery, no wire addition, and it closes holes A leaves open. The price is a semantic change
in core lifetime code on clients, which is the maintainer's call and needs the full net lane plus a
soak, not just the new rows. A is the fallback if the cascade must not change.

## Order cases (with B)

| Arrival on the client | Result |
|---|---|
| Owner rewrite, then old owner's destroy | OnRep re-parents; the destroy no longer reaches the child |
| Rewrite and destroy in one batch | Destroy spares the child (live driver); the RepNotify that follows re-parents it |
| Rewrite lost or late, destroy first | Child spared; re-homed to transient in the old owner's Teardown phase; OnRep finishes the move when the rewrite arrives |
| New owner never relevant to this client | Child stays under transient until its own destroy |
| A -> B -> A, B never seen | Value unchanged, nothing to do; A's destroy would spare the child anyway |
| Late join after the transfer | Child built under the published owner; old owner never seen |
| Server destroys owner AND child (ordinary teardown) | Child's own destroy arrives with the owner's; same frame on one root |

## Not solved by this design (say so in the PR)

- The driver's Outer actor is fixed when the driver is added. A transferred child stays a subobject
  of its build owner's host actor; if that actor is destroyed or goes irrelevant, the child's driver
  is destroyed on clients whatever its lifetime owner. BusterBlock world items sit on a pooled
  ActorRelay channel that is never released, so the pickup is unaffected. Proposal: a server ensure
  when a host actor is destroyed while it still hosts a driver whose entity lives under another root.
- The client parent is the nearest REPLICATED ancestor, so it can differ from the server's exact
  parent (item under player vs item under inventory). Code that needs the exact parent on a client
  must use the feature's own data (the inventory record), not the lifetime owner.
- Two narrow paths still use the build-time owner. The EntityScript spawn cancel tests the owner
  captured when the spawn request was made (`EntityScript/CkEntityScript_Utils.cpp:461-466`,
  `CkEntityScript_Processor.cpp:94-122`), so a replica whose spawn is still queued dies with its old
  owner even after a re-parent. The PendingReplication match reads the CURRENT owner
  (`CkEntityScript_Processor.cpp:564-579`), so a client's pending entity made under the old owner is
  never consumed and only the 10 s retry warning reports it.
- Cost on a host: every transfer now walks the ownership chain, and for a driverless entity its
  subtree, including for non-replicated callers (procedural leg, snapshot restore). Not measured.
- A quiet wait for an unresolved first owner has no watchdog (see "Client: idempotent OnRep").

## Adjacent findings (not part of this change; for Adam)

- `FProcessor_EntityLifetime_DestroyEntity` calls `UCk_Ecs_ReplicatedObject_UE::Destroy`
  (`RemoveReplicatedSubObject`) and THEN the reliable multicast `Request_TriggerDestroyAssociatedEntity`
  (`CkEntityLifetime_Processor.cpp:132-139`). Under Iris an RPC called after the removal is routed
  through the root with a target the client cannot be relied on to resolve
  (`NetBlob/NetBlobManager.cpp:281-304`), so it is at best unordered and possibly never runs.
  Inferred from the engine source; not observed. Worth a row before anything depends on that RPC.
- `_PendingChildEntityConstructions` / `_PendingChildAbilityEntityConstructions`: dead queues. Either
  drain them when the owner's entity becomes valid or delete them and keep the warning truthful.
- `CkEcs/CLAUDE.md` names `ECk_Processor_NetModePolicy`, a type that does not exist.

## Tests (red first, CkTests)

Written, uncompiled, red on today's code by reading:
`Ck.Inventory.Net.AS_DataOnly_TransferredItemOutlives{SourceOwner,RelayHostedSource,SameFrameSourceDestroy}`
and `AS_DataOnly_LaterAddReplicatesAfterTransferredItemSourceDestroyed` (the stalled snapshot).

To write with the change:
- CkEcs net row: transfer between two replicated owners; the client's lifetime owner follows.
- CkEcs net row: transfer to a DRIVERLESS child of a replicated owner (the inventory shape); assert
  the client's parent is the replicated ancestor. The two-replicated-owner row cannot see this.
- CkEcs net row: transfer, then destroy the old owner; child survives.
- CkEcs net row: transfer out of W, then destroy W's OWNER; child survives.
- CkEcs net row: A -> B -> A, then destroy A's sibling B; child stays under A.
- CkEcs net row: transfer during EntityScript construction publishes the current owner.
- CkEcs net row: ordinary teardown of a replicated subtree still destroys every member on the client
  (guards B against a leak).
- Old owner's OnReplicationComplete fires exactly once on a client that was mid-construction.
- Server-only: after a transfer the child's dependent flag is false and the old owner's expected
  count dropped. The child must be built inside the old owner's construction, or the flag is false
  already and the row passes for the wrong reason.
- Late join: only if the multi-PIE harness can add a client mid-test; otherwise stated as untested.

All green with NO lifetime code in CkInventory's client sync.

## Why CkFoundation and not the game or the engine

- Not the game: the game cannot change a replicated owner or a client's destroy cascade, and the
  inventory-side mirror (rejected 2026-11-21) is an unreconciled copy of server state (tenet 6).
- Not the engine: changing Iris so RepNotifies precede subobject destroys would touch every project
  on the fork and still leave cross-packet order undefined.
