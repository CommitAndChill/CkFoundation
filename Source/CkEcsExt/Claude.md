# CkEcsExt

**Purpose:** Higher-level ECS utilities built on top of `CkEcs`. Adds entity-tree structures (SceneNode), transform management, EntityHolder (actor ↔ entity bridging helpers), EntityScript extensions, and the `UCk_Utils_Ecs_Base_UE` meta-fragment infrastructure.

**Depends on:** `CkActor`, `CkCore`, `CkEcs`, `CkLabel`, `CkLog`, `CkRecord`, `CkSettings`.
**Used by:** ~50 feature modules — nearly everything that touches entities goes through both `CkEcs` and `CkEcsExt`.

---

## Module layout

```
CkEcsExt/Public/CkEcsExt/
├── CkEcsExt_Utils.h   – UCk_Utils_Ecs_Base_UE (meta-fragment param storage)
├── EntityHolder/      – EntityHolder fragment (actor-holds-entity lifetime binding)
├── EntityScript/      – EntityScript extensions (additional script helpers)
├── SceneNode/         – parent/child scene tree for entities
├── Settings/          – EcsExt-specific settings
└── Transform/         – transform fragment utilities
```

---

## Core utilities

### `UCk_Utils_Ecs_Base_UE`

Provides two cross-cutting services:

1. **Meta-fragment parameter storage** — when a Meta Fragment (an entity-that-acts-like-a-component) is constructed on a client via replication, its construction params need to be pre-stored so the EntityScript can retrieve them. `UCk_Utils_Ecs_Base_UE` owns the param queue so clients don't have to manage it manually.
2. **Cross-record entity lookup** — `Get_EntityOrRecordEntry_WithFragmentAndLabel` queries both the main entity and its Record entries simultaneously, returning whichever has the specified fragment+label combo. Reduces O(n) searches at call sites.

A **Meta Fragment** is an entity whose handle is stored as a fragment on another entity — externally it looks like a regular fragment. The EntityScript for a Meta Fragment has its own construction lifecycle. `CkMeter`, `CkAttribute`, and similar systems use this pattern.

### EntityHolder

Binds an `AActor`'s lifetime to an entity. When the actor is destroyed, the entity is automatically destroyed. To give an Actor its own entity, use the Ck Entity Bridge (`CkEcs/EntityBridge/`, `CkEcs/Claude.md` § Actor entities) instead.

### SceneNode

Parent/child scene tree composed of ECS entities. Lets you parent entities to other entities (not actors) with relative transform support. Used by `CkIsmRenderer`, multi-part entities, and procedural hierarchies. `UCk_Utils_SceneNode_UE::Get_DriverWorldTransform` returns the world frame a node composes against (the anchor component/socket world for an anchor-driven node, otherwise the parent's current world), for features that derive motion from whatever drives a node (CkSway).

### Transform

Transform fragment utilities — read/write world/local transform of an entity, propagate to actor components when present.

---

## Common patterns

```cpp
// Checking if a handle is a valid Record or Record entry with a given fragment:
auto FoundHandle = UCk_Utils_Ecs_Base_UE::Get_EntityOrRecordEntry_WithFragmentAndLabel<
    TFragment_MyFeature_Utils,
    TRecord_MyFeature_Utils>(InHandle, MyFeatureLabel);
```

---

## Anti-patterns

1. Don't roll your own param-storage for Meta Fragments. Use `UCk_Utils_Ecs_Base_UE`'s queue — skipping it causes construction-order bugs on clients.
2. Don't build entity hierarchies with raw `FCk_Entity` parent pointers. Use `SceneNode/` — it handles destruction ordering and transform propagation.
3. Don't hold an actor ↔ entity relationship by keeping both raw pointers. Use `EntityHolder/` to bind lifetimes.

---

## Implementation notes

### SceneNode layer ordering

Each layer N has a `TProcessor_SceneNode_Update<N>` (parallel) and, for every layer but the last, a
`TProcessor_SceneNode_QueueChildren<N>` (serial) that runs after it and queues the
children of the nodes it moved. Layer 0's Update runs after `FProcessor_SceneNode_HandleRequests` and
`FProcessor_SceneNode_QueueRootChildren` (which itself runs after `FProcessor_SceneNode_FollowUnrealAnchor` and
`FProcessor_Transform_HandleRequests`); layer N's Update runs after `TProcessor_SceneNode_QueueChildren<N-1>` (the
`RunAfter` alias on the Update template). Both dependencies are load-bearing:

- Without the root publish (and through it `Transform_HandleRequests`) ahead of layer 0, layer 0 can run in parallel
  with request handling and miss the `FTag_Transform_Updated` that handling just added to the root (e.g. a tween
  writing world onto the root), so the gate check fails and nothing propagates.
- Without the layer-to-layer chain, descendants run before the tag their parent DEFERRED becomes visible.

Either gap stops motion from propagating past the first scene-node link, silently.

### SceneNode depth

`ck::MaxSceneNodeLayers` (`CkSceneNode_Fragment.h`, 16) is the one constant behind the layers: it sizes the
`TTag_SceneNode_Layer<N>` tags and the per-layer processors, which `CkSceneNode_Processor.cpp` registers from it in
ascending order (`Ck.SceneNode.Depth.EveryLayerIsRegisteredInOrder` checks the registry and the RunAfter chain).
A node's depth (`UCk_Utils_SceneNode_UE::Get_Depth`, unset when the handle holds no layer) runs from 0 to
`Get_MaxDepth() - 1`: 0 under a parent that is not itself a layered scene node, the parent's depth + 1 otherwise.
`Request_Detach` makes the node a plain root and re-layers its subtree from 0. Depth is per machine. Every layered node
also carries `FTag_SceneNode_Layered`, so `FProcessor_SceneNode_QueueRootChildren` excludes all of them with one type.

Two quiet queries (no log, no ensure) answer before an attach: `Get_CanCreateChild(InAttachTo)` for creating one node
(a plain transform parent answers true), and `Get_CanAttach(InNode, InAttachTo)` for an `Add` of a node with its whole
subtree. A re-parent is `Request_Detach` then `Add`, so a caller that must not lose the attachment asks
`Get_CanAttach` before the detach.

The overflow contract: an attach fits when the depth the node takes plus the height of its scene-node subtree is below
`Get_MaxDepth()`. `Add`, `Create` and `CreateAndAttachToUnreal*` decide it before any write; when it does not fit, the
attach is refused whole: it logs an error every time (`SceneNode depth overflow: ... Chain: [root > ... > leaf]`),
returns an invalid handle, and changes nothing. The error instead of an ensure is a maintainer-approved exception to
DEVELOPMENT.md non-negotiable 3: the attach is refused, not continued; callers that expect the cap ask the quiet
queries; and a Ck ensure reports a site only once per process outside automation. `Create*` creates no entity; `Add`
leaves its handle not a scene node and every descendant with its layer and parent, so a re-parent (`Request_Detach`
then `Add`) refused this way leaves the subtree detached and still composing under its root. `Add` on a handle that
already is a scene node is a caller error (an ensure; detach first). The late pass
(`FProcessor_SceneNode_LateFollowUnrealAnchor`) walks a subtree depth-first and needs no layers, so it has no cap of
its own.

Each layer costs two processors every frame whether or not any node sits on it (an empty-view check in the main pass,
a dirty check per local-settle replay). `Ck_AutoTest_SceneNode_LayerCostPerf` reads that cost as the scheduler's own
per-frame time over a fixed scene; compare two builds of the constant with it, same scene, same machine.

### SceneNode Unreal anchors

`FFragment_SceneNode_UnrealAnchor` is a scene-node's foreign Unreal anchor: a `USceneComponent`
(`Socket == None`) or a mesh socket. `FProcessor_SceneNode_FollowUnrealAnchor` composes
`entity.world = offset * anchor.world` every tick, read-only w.r.t. the anchor.

It is deliberately NOT the Transform module's `FFragment_Transform_RootComponent` / `_MeshSocket`: those
pair with `SyncFromActor`/`SyncToActor`, a bidirectional actor bridge that would drag a Movable anchor
around. Keeping a separate fragment makes anchor-follow a read-only SceneNode concern and leaves the
Transform feature untouched. The anchor fragment is not snapshotable (a live component ref can't be
remapped — parity with the Transform anchor fragments); the composed world pose is restored via
`FFragment_Transform`.

`FTag_Transform_ExternallyDriven` is the inverse switch on the Transform side: an anchor-bound child that
should instead follow its scene-node parent (Unreal `AttachToComponent` semantics) carries the tag, and
`SyncFromActor`/`SyncFromMeshSocket` back off while `SyncToActor` keeps pushing the parent-driven pose onto
the anchor.

`ECk_Transform_ReplicatedPosePolicy` (`UCk_Utils_Transform_UE::Request_SetReplicatedPosePolicy`, tag
`FTag_Transform_ReplicatedPoseYieldsToParent`) decides what a replicated pose does to a Transform that a machine
without authority parents locally. `AlwaysApplied` (the default) writes every arrival as a world request, so a
parent-driven node rejects it with the drain's ensure: an undeclared replicated entity parented on a client keeps
that signal. `YieldsToSceneNodeParent` makes the net-apply handlers only mark the arrival
(`FFragment_Transform_PendingReplicatedPose`); `FProcessor_Transform_ApplyReplicatedPose`, client-only and directly
before the request drain, discards the mark while the node is parent-driven and otherwise applies the latest
received location, rotation and scale from the replication driver through the normal request path. Attaching drops
any pending mark and interpolation goal; `Request_Detach` re-marks all three components, so the latest received
pose applies again on detach (it never queues a request, so a detach and re-attach in one call leaves nothing to
reject). The authority's send is unchanged and the policy is inert on a host. The one rule for callers: declare it
at composition, on every machine.

### Late pose: anchors that follow an animated socket

A skinned mesh publishes its pose (the read buffer `GetSocketTransform` uses) when its animation finishes, which with
parallel evaluation is as late as the end of `TG_PostPhysics`. Every TG_PrePhysics processor, including
`FProcessor_SceneNode_FollowUnrealAnchor`, therefore reads last frame's pose: a node on a moving bone trails it by one
frame of travel. That early sample stays as it is (simulation keeps its estimate); a second pass corrects the pose
for presentation:

- `CreateAndAttachToUnrealMesh` on a `USkinnedMeshComponent` adds `FTag_SceneNode_LateAnchor`. Component anchors and
  static-mesh sockets stay untagged and early-only.
- `FProcessor_SceneNode_LateFollowUnrealAnchor` (`FGroup_Transform_LatePose`, its own `TG_PostUpdateWork` partition)
  re-samples each late anchor, and where its pose changed writes it and composes its scene-node subtree with the
  early chain's rules: children with their own Unreal anchor are skipped, anchor-authoritative children (a
  RootComponent/MeshSocket without `FTag_Transform_ExternallyDriven`) are skipped with everything under them, an
  unchanged child ends its branch, and a restore rebase is left to the early chain. Each parent it composes is marked
  published (`FUtils_SceneNodePropagation::MarkPublished`), so the promoted tag next frame queues nothing; a child the
  walk skips because it is hydration-quarantined is queued for the early chain instead, or it would keep its old pose
  once the quarantine lifts if the anchor then holds still.
- **A late write carries `FTag_Transform_LateUpdated`, never `FTag_Transform_Updated`.** An `Updated` added in the late
  group would be cleared by the next frame's `FProcessor_Transform_Cleanup` before any TG_PrePhysics consumer ran, so
  they would never see the late write. `FProcessor_Transform_PromoteLateUpdated` (`FGroup_Transform_SyncFrom`, after
  `FProcessor_Transform_Cleanup`) turns the late tag into `Updated`, plus the one-frame `FTag_Transform_LatePromoted`,
  after that cleanup, so every TG_PrePhysics consumer (signals, replication, ISM, RaySense) sees each late write once,
  one frame after the pose, exactly as it saw the early sample before. It has no `MarkedDirtyBy`: the late tag is only
  added in the TG_PostUpdateWork partition, so the main pass always sees it and a pump pass would only re-run an empty
  view. When the mesh has not moved since the late pass, that frame's early sample reads the same pose and writes
  nothing.
- **Previous stays contiguous for TG_PrePhysics consumers.** A late write is the normal direct write:
  `FFragment_Transform_Previous` becomes the value this frame's TG_PrePhysics consumers already read. If the mesh moved
  between the late pass and next frame's early sample (a character that moved before the ECS tick, a script moving the
  actor), that early write would start the pair at the late value and drop the early -> late step from every pair (a
  probe's linear cast, a ray-sense sweep, an ISM's previous transform). So an early write that follows a promoted late
  write (`FTag_Transform_LatePromoted`; both early sites, `FProcessor_SceneNode_FollowUnrealAnchor`, which runs after the
  promotion, and the `TProcessor_SceneNode_Update` layers) writes Current only: the pair runs from last frame's early
  value to this frame's and covers the late step. Without an early write the pair is (early -> late). Pinned by
  `CkAutoTest_SceneNode_AnimatedSocketAnchor_PreviousStaysContiguousForAnEarlyConsumer`.
- The late presentation consumers, `FProcessor_Transform_SyncToActor_Late` and
  `FProcessor_UnrealComponent_PushTransform_Late`, push this frame's late writes to actors and hosted components. They
  are idempotent, so the promoted tag repeating the push next frame is a comparison. The render-state flush follows
  every tick group, so what they push is what renders. `SyncToActor_Late` skips an entity whose root component is gone,
  and drops its late tag: its actor can be destroyed between the passes, a destruction that has only begun is not
  excluded by the view, and the promoted tag would hand the same dead actor to next frame's early `SyncToActor` (both
  hit its root-component ensure before the fix; pinned by
  `CkAutoTest_SceneNode_AnimatedSocketAnchor_AnActorUnderALateAnchorFollowsAndCanDie`).
- The late processors are `ck_exp` processors, so the scheduler's empty-view skip makes the pass free in a world with no
  late anchor. That verdict is re-derived only when one of a view's include types is added or removed somewhere, and
  destroying an entity is not such a mutation, so after the last late anchor dies the pass can keep running over an
  empty view until the next such mutation. Only that direction can be stale: a late anchor appears by adding its tag,
  which re-derives the verdict, so a non-empty view is never skipped.
- The TG_PostUpdateWork world actor ticks at high priority, so another TG_PostUpdateWork tick that reads a late node
  reads this frame's pose.

Known limits: a Jolt kinematic body on a late node is pushed in TG_PrePhysics and still trails the rendered pose by
one frame; ISM and ISKM proxies of a late node read its transform in TG_PrePhysics and render one frame behind it too;
a skinned mesh that finalizes its own pose in TG_PostUpdateWork (its tick or its animation completion lands there)
can finalize after the high-priority late pass, which then reads last frame's pose for it; the camera manager updates before TG_PostUpdateWork, so a camera driven by a late node sees the early pose;
a component attached under an animated bone and anchored through `CreateAndAttachToUnrealComponent` is not late;
one level of late anchoring is the supported scope: the late pass samples every late anchor before the late actor push
(`FProcessor_SceneNode_LateFollowUnrealAnchor` runs before `FProcessor_Transform_SyncToActor_Late`) and its subtree
walk skips children with their own Unreal anchor, so a node that follows a socket on actor B's skinned mesh, where B
is itself moved by a late anchor, samples B at its earlier transform and trails it by one frame, as it did before the
late pass existed;
`FProcessor_Transform_SyncFromMeshSocket` (the Transform module's own socket bridge) has the same one-frame lag and is
not covered by this pass. Replication sends the same values at the same cadence as before.

### Physics ownership

An entity is EITHER Chaos-simulated (CkOverlapBody's `UShapeComponent` path, CkRaySense's engine-trace
path) OR Jolt-simulated (CkSpatialQuery's Probe, CkJolt's bodies/characters) — never both; the two engines
maintain independent, non-interacting world representations. Composing both onto one entity is a design
error surfaced at COMPOSITION time by `ck::physics_ownership::TryClaim_Chaos` / `TryClaim_Jolt`
(`CK_ENSURE` at the composing call site + `false` return, so the caller returns an invalid handle), never a
runtime log.

The two tags are COUNTED because multiple SAME-world features may legitimately stack on one entity (a Probe
and a JoltBody both claim Jolt; two Sensors both claim Chaos) — only the cross-world claim is a conflict.
`Release_*` drops one claim; EndPlay teardown does not need to release (the entity is dying).

### Bridged actors after a snapshot restore — respawn, not rebind

A restored bridged entity comes back with no live actor (the EntityBridge + Transform-root links are not
persistable). The v3 loader's answer is **actor-first respawn**, not a rebind of the restored entity: it
spawns a FRESH actor of `FFragment_ActorSpawnIntent`'s class at the saved transform (deferred, so the
saved `UPROPERTY(SaveGame)` fields land before `BeginPlay`), and that actor's own `BeginPlay` calls
`Request_SpawnLinkedEntity` again, which links the bridge and runs `WithActor::Construct` exactly as in a fresh world. The
saved payloads then hydrate onto it. See `CkSnapshot/CLAUDE.md` § "The v3 load machine".

Because Construct DOES re-run, actor-side wiring is rebuilt by the normal construction path and no
post-restore reattach hook is needed. An earlier design re-bridged the restored entity in place instead
(`UCk_Utils_ActorRebind_UE::Request_RebindActor` + a `ck::FTag_ActorJustRebound` marker game code keyed a
reattach processor on); it lost its last caller when `FProcessor_ActorRespawn` was deleted (`a8ad20458`,
2026-07-12) and the whole path was removed in 2026-08. Do not reintroduce it — under v3 there is nothing
for it to do.

`WithActor::Construct` only stamps the intent for a RUNTIME-spawned actor. A level-placed one gets
`FFragment_SaveKey` (from `ck::save_key::Get_LevelPlacedIdentity`) instead: the level re-creates it on every boot,
so respawning it from the save would duplicate it, and capture's classification lets the intent outrank the key —
they are alternatives, never both. `_SnapshotRespawnable` therefore only governs runtime-spawned bridges.

`FFragment_ActorSpawnIntent` stores the class as a plain `FString` (`UClass::GetPathName`), NOT a
`TSoftClassPtr`: a soft-object path does not survive the snapshot's SaveGame memory-archive
`SerializeItem` round-trip (it comes back empty), whereas a plain FString round-trips reliably — the same
value path `FFragment_SaveKey`'s FGuid uses. Resolved at respawn via `FSoftClassPath::TryLoadClass`. The
raw `AActor` pointer is never serialized; the spawn transform comes from the restored Transform fragment,
not from this fragment.

---

## See also
- `CkEcs/Claude.md` — the handle, processor, signal, and EntityScript primitives.
- `CkRecord/Claude.md` — the Record system that Meta Fragments are stored in.
- `CkLabel/Claude.md` — label matching used in cross-record lookup.

## Transform-local settle after derived producers

### Tail-pump writes after the normal propagation pass

Tail-pump transform requests can apply after the normal SceneNode pass. `FTag_Transform_Updated`
is cleared near the start of the next frame, so it cannot be the sole wake-up for those descendants.
`FProcessor_SceneNode_QueueChildren_BeforeCleanup` runs after Physics and before both `FProcessor_Transform_Cleanup`
and `FGroup_Transform_SyncFrom`, preserving cleanup-before-anchor-write ordering while
publishing only updated parents with child records into the durable, consumed SceneNode propagation queue.
The existing world-transform watermark avoids re-queuing already-published poses. This includes intermediate
parents, not only scene roots; the normal depth-ordered pass then resolves their descendants.
It does not extend the public transform marker's lifetime or use that persistent marker to trigger a pump.

### Derived-producer barrier

`FProcessor_Transform_HandleRequests` and the `TProcessor_SceneNode_Update` layer chain are registered once
in `FGroup_Transform`. Those canonical processors declare `LocalSettleAfter = FGroup_Transform_Derived`, so
the scheduler can replay the same ordered transform-resolution slice before `FGroup_Transform_Finalize`.
The request drain additionally declares `LocalSettleTrigger = true`, so the barrier activates only when its
consumed transform request fragment is dirty; the marker-less SceneNode layers
then replay in their normal depth order. A camera view-anchor request therefore drains and all descendants
compose in the same frame without a camera exception, duplicate processor identity, or a second transform
group.

Do not activate this barrier from `FTag_Transform_Updated`: that tag remains present until cleanup and cannot
converge a settle loop. The consumed transform request queue is the activation source. The SceneNode chain
is replay-only and stays unaware of which derived system produced the request.
