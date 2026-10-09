# CkRuntimeMesh

Immutable runtime solid geometry: import from cooked render data, deferred plane slicing into two
independently owned results, and an optional `UDynamicMeshComponent` display companion. The capability
contract is [the slicing spec](../../docs/specs/2026-10-08-CkRuntimeMesh-slicing-design.md); acceptance
evidence lives in `docs/campaigns/runtime-mesh/PROGRESS.md`.

## Layers

- `Internal/CkRuntimeMesh_Geometry.*` and `Internal/CkRuntimeMesh_Slice.*`: native kernel. Reads an
  explicit CPU-accessible render LOD, reconstructs geometric seams while keeping per-corner attributes,
  admits only a single closed connected positive-volume solid, cuts with `FMeshPlaneCut` plus constrained
  Delaunay caps, and validates both outputs before publishing. Coordinates stay in the source's local
  centimetre frame. Internal headers are implementation and test contracts, not consumer API.
- `CkRuntimeMesh_*` quartet: the public feature. `UCk_Utils_RuntimeMesh_UE::Add(Handle, Spec)` composes on
  the given entity; a soft source mesh loads through a rooted ResourceLoader batch (consumer id
  `RuntimeMesh.Setup`) that lives only in `FFragment_RuntimeMesh_PendingImport` and is released at
  terminal resolution. `FFragment_RuntimeMesh` keeps only `SetupState`, `SetupFailure` and the immutable
  `FValidatedGeometryPtr`. `Request_Slice` validates structurally, then queues on the world transient
  entity's `FFragment_RuntimeMesh_Queue` (capacity `QueueCapacity`, `DrainBudgetPerTick` per drain);
  `FProcessor_RuntimeMesh_Drain` detaches each entry, cuts, adopts both halves as new entities under the
  caller's result owner (or neither), latches, then fires the typed receiver followed by the generic
  completion. Nothing reads a queue fragment after callbacks start.
- `Display/CkRuntimeMeshDisplay_*`: optional presentation. `Add(FCk_Handle_Transform, Spec)` takes the owner's
  Transform handle (the handle type carries the requirement), ensures on `Spec.Get_IsValid()`, requires a Ready
  geometry in the same registry whose triangle material IDs fit the slots, and rejects atomically. Setup loads materials through a rooted batch (`RuntimeMeshDisplay.Setup`), copies
  the geometry, regenerates tangents natively, and creates a pooled `DestroyOnRelease`
  `UDynamicMeshComponent` with Chaos collision, simulation and navigation off. Runs in
  `FGroup_PostTransform` so it follows the Jolt writeback. The module's `OnWorldCleanup` hook releases
  displays before cancelling queued slices.
  `Request_SetCustomPrimitiveData(Display, FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData)` is the display's only
  runtime channel: one `FCk_CustomPrimitiveData` (CkGraphics) per request, validated by the request's `Get_IsValid()`
  against `FCustomPrimitiveData::NumCustomPrimitiveDataFloats`, queued on `FFragment_RuntimeMeshDisplay_Requests` and
  applied by `FProcessor_RuntimeMeshDisplay_HandleRequests` once setup is Ready (a request made while Pending waits; a
  display whose setup failed completes it `Failed`). Requests still queued at destruction complete `Failed_Cancelled`
  (`FProcessor_RuntimeMeshDisplay_CancelPendingRequests`, after EndPlay); one made once destruction began is refused
  `Failed_NotEnqueued` without an ensure. `Get_CustomPrimitiveDataFloat` reads a slot back as a test seam; it exposes a
  float, never the component.

## Rules

- Every reflected Spec and request carries `Get_IsValid()` for its field-level contract (CkChain precedent);
  `Add`/`Request_*` ensure on it first and keep only handle, registry and lifetime context checks for themselves.

- Geometry is immutable and independent of rendering and physics; a derived result keeps no reference to
  its source, so destroying the source never invalidates results.
- Never expose mutable `FDynamicMesh3`, the component, or Jolt types; never import editor source models,
  fall back to another LOD or a primitive, repair holes, recenter results, or bake transforms.
- Convex physics is CkJolt's `RuntimeConvex` shape source fed with `Copy_LocalVerticesCm`; this module has
  no CkJolt dependency and CkJolt has none on it.
- CkGraphics is a public dependency (same tier) for the display's `FCk_CustomPrimitiveData` request payload, the shape
  CkUnrealComponent and CkIsmRenderer already take.
- All state is `FCk_Snapshot_Session`; derived cuts are not persisted.
- The 2048 vertex / 4096 triangle ceilings and the queue limits are provisional safety bounds until the
  campaign benchmark selects shipping values.
