# CkUnrealComponent

**Purpose:** Hosts an Unreal `UActorComponent` on a child ECS entity. `Add` composes a component entity under
an owner entity; the component itself is created later by the Setup processor, registered with the world,
kept at the owner's transform (scene components), optionally ticked by a processor, optionally baked into the
Jolt static world, and destroyed at the entity's EndPlay. Each hosted component is its own entity with its own
`FCk_Handle_UnrealComponent`; the owner keeps a TRANSIENT record of them.

**Depends on:** `CkCore`, `CkEcs`, `CkEcsExt`, `CkGraphics`, `CkJolt`, `CkLabel`, `CkLog`, `CkRecord`,
`CkSettings`, `CkUsf` (+ `CkProfile` private).

---

## Lifetime — the component is null until Setup has run

`Add(Owner, FCk_UnrealComponent_Spec)` only creates the component entity: it adds the Spec (aliased as
`ck::FFragment_UnrealComponent_Params`), `ck::FFragment_UnrealComponent` (owner handle + a WEAK component
pointer) and `FTag_UnrealComponent_NeedsSetup`, then connects the entity to the owner's record. The hosted
`UActorComponent` does not exist yet.

`FProcessor_UnrealComponent_Setup` creates it through `UCk_Utils_Object_UE::Request_CreateNewObject` with the
`DestroyOnRelease` pool policy (the ObjectPooling pin is the GC root, so the fragment holds a
`TWeakObjectPtr`), outered to the world (non-scene) or to the host actor (scene components: the per-world
`UCk_ComponentHost_Subsystem_UE` actor, or the editor selection-proxy host for editor previews), registers it
with `RegisterComponentWithWorld`, pushes the owner's transform, applies the tick policy and the static-world
bake policy, registers the component↔handle bridge, and broadcasts `OnAdded`. A scene component requires the
owner entity to have a Transform.

Until then `Get_Component` returns null. Bind `OnAdded` (default policy replays an in-flight payload this
frame) or use a deferred request rather than polling for the component, and never cache it across frames.

`FProcessor_UnrealComponent_EndPlay` broadcasts `OnRemoved`, removes baked static-world bodies (before the
component, because the subsystem keys them by component pointer), unregisters the bridge, releases the pool
pin and destroys the component.

## Key API (`UCk_Utils_UnrealComponent_UE`)

| Group | Functions |
|---|---|
| Spec builders | `Make_Params(Class, TickPolicy, DebugName)`, `Make_Params_FromArchetype(Archetype, TickPolicy, DebugName)` |
| Composition | `Add(Owner, Spec)` → component handle; `Request_Remove` (immediate: destroys the component entity, completes synchronously) |
| Static-world bake | `Request_BakeIntoJoltStaticWorld`, `Request_RemoveFromJoltStaticWorld` (immediate; see the Spec's `ECk_UnrealComponent_StaticWorldBakePolicy` for the automatic path) |
| Transform push | `Request_DisableTransformPush`, `Request_EnableTransformPush`, `Get_CanEnableTransformPush` (immediate) |
| Render data | `Request_SetCustomPrimitiveData` (deferred), `Get_CustomPrimitiveDataFloat` |
| Queries | `Has`, `Get_Component`, `Get_OwningEntity`, `TryGet_OwningHandle_FromComponent`, `Get_AllHandles`, `Get_AllComponents`, `TryGet_HandleByType`, `Get_HandlesByType`, `TryGet_ComponentByType`, `Get_ComponentsByType` |
| Signals | `BindTo_OnAdded` / `UnbindFrom_OnAdded`, `BindTo_OnRemoved` / `UnbindFrom_OnRemoved` |

Processors: `Setup` (marker `FTag_UnrealComponent_NeedsSetup`), `PushTransform` and `Tick` and
`HandleRequests` (`FGroup_PostTransform`), `EndPlay` and `CancelPendingRequests` (`FGroup_EndPlay`), and the
outline trio in `CkUnrealComponent_OutlineProcessor.h` that applies resolved `CkUsf` outline claims to a
hosted primitive component (cosmetic net modes only).

## `Request_SetCustomPrimitiveData` — the deferred request

Writes one `FCk_CustomPrimitiveData` (`CkGraphics`: an index plus a Float / Vector2 / Vector3 / Vector4 /
LinearColor value) onto the hosted `UPrimitiveComponent` with the engine setter matching the value type
(LinearColor is written as a Vector4). The request is queued on `ck::FFragment_UnrealComponent_Requests` and
drained by `FProcessor_UnrealComponent_HandleRequests`, whose view excludes `FTag_UnrealComponent_NeedsSetup`,
`ck::FTag_DestroyEntity_Initiate` and pending-kill. A request made in the same frame as `Add` therefore waits on
the entity — the queue is only removed once drained — and is applied the first time the processor runs after
Setup.

| Situation | Result |
|---|---|
| Invalid handle | ensure; `Failed_NotEnqueued`; handle returned unchanged |
| Index < 0, or index + the value's float count > `FCustomPrimitiveData::NumCustomPrimitiveDataFloats` (36) | ensure naming index and count; `Failed_NotEnqueued`; nothing queued |
| Valid, component not set up yet | queued; applied after Setup; `Succeeded` |
| Valid, hosted component is a live `UPrimitiveComponent` | written; `Succeeded` |
| Hosted component is not a `UPrimitiveComponent`, or is gone | ensure naming the component class; `Failed`; no write |
| Component entity destroyed with requests pending | each completes `Failed_Cancelled` (`CancelPendingRequests`, ordered after `EndPlay`) |
| Request made once the component entity carries `ck::FTag_DestroyEntity_Initiate` (e.g. the same step as `Request_DestroyEntity` on its owner) | no ensure (legitimate during teardown); `Failed_NotEnqueued` synchronously; nothing queued |
| Several requests in one frame | applied in enqueue order; a later write to the same slot wins |

The drain copies and clears the queue before handling, so a `Request_SetCustomPrimitiveData` issued from a
completion delegate is kept for the next pass rather than lost.

The request parameter is taken by value (the CkTimer shape): the completion delegate is set on that copy, so a
caller reusing one request struct across calls never replays an earlier call's delegate.

`Get_CustomPrimitiveDataFloat(Handle, Index)` reads `UPrimitiveComponent::GetCustomPrimitiveData().Data`. It
returns 0 when the component does not exist yet, is not a primitive, or nothing has been written at that
index; an invalid handle, or an index outside `[0, FCustomPrimitiveData::NumCustomPrimitiveDataFloats)`,
ensures and returns 0.

The value-type switch is `UCk_Utils_Graphics_UE::Apply_CustomPrimitiveData` (`CkGraphics`), shared with
`CkIsmRenderer`'s proxy.

## Anti-patterns

1. **Writing the hosted component's render state from game code when a request exists.** Use
   `Request_SetCustomPrimitiveData` rather than resolving the component and calling
   `SetCustomPrimitiveData*` yourself — the request already handles the component not existing yet and
   completes against the entity.
2. **Assuming the component exists right after `Add`.** It is null until Setup; bind `OnAdded` or go through a
   deferred request.
3. **Holding the component strongly.** The ObjectPooling pin is the root; a strong ref elsewhere outlives the
   entity's EndPlay destroy.
4. **Moving a transform-pushed scene component directly.** The push overwrites it every frame the owner's
   transform changes; use `Request_DisableTransformPush` first (Unreal physics takeover) and
   `Request_EnableTransformPush` to hand ownership back.
5. **Baking a continuously moving blocker into the Jolt static world.** Every real move re-bakes and churns
   the broadphase; that content belongs on a kinematic `CkJoltBody`.
