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
  entity's `FFragment_RuntimeMesh_Queue` (capacity `QueueCapacity`). `FProcessor_RuntimeMesh_Drain` runs the
  entries queued before the drain began, oldest first: it always executes the first, then continues while the
  wall-clock time spent (callbacks included) is under `UCk_RuntimeMesh_ProjectSettings_UE::_DrainTimeBudget`
  (default 5 ms) and at most `_MaximumSlicesPerDrain` (default 2, the original per-tick count; Mars's
  `Mars_AutoTest_FoodPiece_SourceDestroyedWhilePendingCommitsNothing` relies on queued slices waiting several
  frames) entries have run. The budget therefore only ever lowers the count, to one, when a single slice is slow. The order is FIFO and
  deterministic; how many run per tick is machine-dependent, so a test that needs an exact count sets the two
  settings for its own scope (`Ck.RuntimeMesh.Public.DistinctReceiversAndBudget`, `.DrainTimeBudget`). Each entry
  is detached before it executes, cut, every piece adopted as a new entity under the caller's result owner (or
  none), latched, then the typed receiver fires followed by the generic completion. Between entries the drain
  resolves the queue again through the transient entity, because a callback may have appended (that request waits
  for a later drain) or closed it.
- Slice result shape: `FCk_RuntimeMesh_SliceResult` keeps `_Positive`/`_Negative`, their metrics and cap counts,
  and adds `_PositivePieces`/`_NegativePieces` (`FCk_RuntimeMesh_SlicePiece`: handle, metrics, cap triangles),
  every island of each side, largest volume first; the scalar fields always describe piece 0 of each side.
  `FCk_Request_RuntimeMesh_Slice::_IslandPolicy` decides what a side that falls apart does:
  `RejectMultiple` (default; `RejectedTopology`, exactly one piece per side on success) or `SplitIslands` (every
  island becomes its own entity, at most `FCk_RuntimeMesh_CutLimits::_MaximumPiecesPerSide`, default 8, ceiling 64,
  else `RejectedLimit`). Every island is held to the cut limits a half is; one failing island (too small, too thin,
  over the vertex/triangle limits, failed admission) rejects the whole cut with the same outcome a half would get,
  and volume conservation sums every island. No subset of the source is ever published.
- Cap UVs: UV0 always carries the planar projection in the request plane's frame.
  `FCk_RuntimeMesh_Cap::_UVPolicy` decides the layers after it: `PlanarOnAllLayers` (default, the original
  behaviour), `PlanarOnLayer0_ZeroOthers` (zero), or `PlanarOnLayer0_KeepInterpolated` (each cap rim vertex takes
  the exterior's interpolated value at that vertex, from the lowest-ID exterior triangle touching it, so a per-vertex
  channel such as a baked morph offset stays continuous across the cap edge).
- `Display/CkRuntimeMeshDisplay_*`: optional presentation. `Add(FCk_Handle_Transform, Spec)` takes the owner's
  Transform handle (the handle type carries the requirement), ensures on `Spec.Get_IsValid()`, requires a Ready
  geometry in the same registry whose triangle material IDs fit the slots, and rejects atomically. Setup loads materials through a rooted batch (`RuntimeMeshDisplay.Setup`), copies
  the geometry, regenerates tangents natively, and creates a pooled `DestroyOnRelease`
  `UDynamicMeshComponent` with Chaos collision, simulation and navigation off. Runs in
  `FGroup_PostTransform` so it follows the Jolt writeback. The module's `OnWorldCleanup` hook releases
  displays before cancelling queued slices.
  The display has two runtime channels, both on the same queue and lifecycle.
  `Request_SetCustomPrimitiveData(Display, FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData)` carries one `FCk_CustomPrimitiveData` (CkGraphics) per request, validated by the request's `Get_IsValid()`
  against `FCustomPrimitiveData::NumCustomPrimitiveDataFloats`, queued on `FFragment_RuntimeMeshDisplay_Requests` and
  applied by `FProcessor_RuntimeMeshDisplay_HandleRequests` once setup is Ready (a request made while Pending waits; a
  display whose setup failed completes it `Failed`). Requests still queued at destruction complete `Failed_Cancelled`
  (`FProcessor_RuntimeMeshDisplay_CancelPendingRequests`, after EndPlay); one made once destruction began is refused
  `Failed_NotEnqueued` without an ensure. `Get_CustomPrimitiveDataFloat` reads a slot back as a test seam; it exposes a
  float, never the component. `Request_SetVisibility(Display, FCk_Request_RuntimeMeshDisplay_SetVisibility)` carries an
  `ECk_EnableDisable` applied as `SetVisibility` + `SetHiddenInGame` under the same queueing, Pending-wait and cancellation
  rules; `Get_Visibility` reads it back. A consumer that shows a stand-in mesh (Mars's battered shell) hides the display
  this way and keeps the geometry as the ledger and the body.

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
- The 2048 vertex / 4096 triangle ceilings and the queue capacity are provisional safety bounds. Raising the vertex
  ceiling is NOT a constant change: the import preflight packs triangle and edge keys with a 2048 radix
  (`Internal/CkRuntimeMesh_Geometry.cpp`, `TriangleKeys` / `EdgeOrientations`), and admission's self-intersection
  pass visits every triangle pair (AABB-pruned per pair, still quadratic), which already costs ~18 ms per admit at
  3072 triangles. A single slice is not preemptible by the drain's time budget.
- Settings: `UCk_RuntimeMesh_ProjectSettings_UE` (`Config = CkFoundation`, section "Runtime Mesh") owns the drain
  time budget and the per-drain maximum; `UCk_Utils_RuntimeMesh_Settings_UE` reads them from Blueprint/AngelScript.
  A value outside its range ensures and the drain runs one slice per tick until it is fixed.
