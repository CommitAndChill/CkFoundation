# CkRuntimeMesh: bounded runtime slicing and convex body admission

Date: 2026-10-08  
Status: **Design proposal; not implemented or runtime-validated.**  
Scope: CkFoundation only. This is a capability specification, not an execution handoff or permission to implement.

## 1. Outcome and acceptance contract

A consumer can load a supported solid mesh, request a plane slice, inspect two independently owned geometric results, display them, and use their vertices to create new convex CkJolt bodies. The source remains unchanged. Neither slicing nor body creation assigns application meaning to a piece.

The minimum acceptance example is a generic 10 cm cube sliced through its centre: two closed meshes, each with volume 500 cm³, correct exterior/cap attributes, and independently movable convex bodies. A second slice operates on either result. No game assets, ingredient types, station logic, inventory, scoring, or cooking state may be necessary for this example.

Required acceptance categories:

| ID | Contract |
|---|---|
| G1 | Successful slices return exactly two valid, closed, connected solids; no source mutation. |
| G2 | Misses, grazing cuts, invalid input, unsupported topology, and resource limits have distinct observable outcomes. No partial result is published. |
| G3 | Exterior material IDs and supported vertex/corner attributes survive; cut faces have explicit material IDs, normals, and planar UVs. |
| G4 | Geometry works headless and in a cooked build without a renderer, an editor module, or editor source mesh data. |
| G5 | Generated mesh and component ownership survives forced GC and releases on entity teardown. |
| G6 | Runtime convex points can create a CkJolt body with an observable setup result; invalid input never silently becomes substitute collision. |
| G7 | The public capability is exercised from C++, Blueprint, and AngelScript. Native access alone is insufficient. |
| G8 | Existing primitive/static-mesh Jolt users and CkPmg users retain their existing public API contracts. |

All names introduced below are **proposed API names**, not existing callable APIs.

## 2. Decisions and scope limits

1. Add a focused runtime module, **CkRuntimeMesh**, rather than expanding CkPmg's analytical/debug-shape API.
2. Use Unreal's `FDynamicMesh3` geometry and native plane-cut/capping operations. Do not implement another triangle-clipping or polygon-cap algorithm in AngelScript.
3. Store immutable geometry on typed ECS entities. A slice derives two new geometry entities; it never edits the source in place or copies unrelated source-entity features.
4. Rendering is an optional companion feature in the same module, backed by `UDynamicMeshComponent`. Headless geometry has no component dependency at execution time.
5. Extend CkJolt with **setup-time runtime convex points** and explicit setup outcome reporting. CkJolt must not depend on CkRuntimeMesh or Unreal geometry-editing types.
6. Keep mesh derivation, display, and body creation separately composable. This version does not promise an atomic replacement of one active physical object with two active physical objects.

V1 exclusions: curved cuts, knife-volume subtraction, kerf/material loss, open sheets, skeletal meshes, soft bodies, general destruction/fracture, arbitrary booleans, convex decomposition, live body shape replacement, automatic physical-piece spawning, runtime asset writing, automatic replication, and save/load of derived geometry.

A plane may have any finite orientation in local space. A game can constrain that freedom. V1 accepts one closed, orientable, non-self-intersecting, connected solid with no cavities. Both slice results must individually remain connected solids. A cut yielding multiple islands on either side is explicitly rejected; it is not welded across gaps or converted into a variable-size piece array. Concave geometry is permitted only within those constraints. Exact concave physical collision is not promised by a single convex hull.

## 3. Verified substrate and gaps

Source inspected on the date above; host superproject HEAD was `50b3906f1d76b9e094543b97d4674441141588db`. The checkout has unrelated edits. The submodule revision was not captured because its Git metadata was inaccessible in the sandbox; source claims below describe inspected files, not an asserted clean commit.

| Existing source | Confirmed contract and implication |
|---|---|
| `Source/CkPmg/CkPmg.Build.cs`; `Source/CkPmg/Public/CkPmg/CkPmg_Processor_Donut.cpp` | Procedural rendering/component-lifetime precedent. Not an arbitrary solid-slicing API. |
| `Source/CkUnrealComponent/Public/CkUnrealComponent/CkUnrealComponent_Fragment_Data.h:34` | Component hosting and static-world baking already exist. Static baking is not a movable runtime body bridge. |
| `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Fragment_Data.h:24` | Body sources are `ExplicitShape` and `StaticMeshAsset`; no runtime point-cloud source. |
| `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Processor.cpp:249` | Dynamic triangle-mesh body shapes are rejected. Render topology cannot be supplied as dynamic triangle collision. |
| `Source/CkJolt/Public/CkJolt/StaticWorld/CkJoltBakeExtraction.cpp:373` | Authored convex elements already build Jolt hulls; reuse the construction/validation pattern, not its asset-only input path. |
| `Source/CkJolt/Public/CkJolt/StaticWorld/CkJoltBakeExtraction.cpp:1597` | DynamicMesh extraction reads cooked component BodySetup for the static world; it is not live dynamic-shape synchronization. |
| `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Utils.h:83`; processor `:440` | `Get_IsBodyAdded` reports broadphase admission. There is no public paired activation transaction or terminal setup-failure result. |
| `Source/CkEcs/Public/CkEcs/Request/CkRequest_Completion.h:13` | Existing request outcomes distinguish synchronous rejection, execution failure, and teardown cancellation. Reuse that lifecycle. |
| `Source/CkEcs/Public/CkEcs/Snapshot/CkSnapshot_Posture.h:17` | Runtime state must declare a persistence posture; omission is not an acceptable opt-out. |
| `Source/CkThirdParty/Public/CkThirdParty/JoltPhysics/Jolt/Physics/Collision/Shape/ConvexHullShape.h:25` | Native Jolt accepts point clouds; its final hull has a 256-point limit in this checkout. Do not confuse input points with final hull vertices. |

Engine evidence: local UnrealEngine-Angelscript `Engine/Build/Build.version` reports 5.7.4. `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Private/MeshBooleanFunctions.cpp:203` uses native `FMeshPlaneCut` and constrained Delaunay capping. Its Blueprint wrapper returns the target mesh and does not expose all geometric validity guarantees required here. The CK feature must validate results itself. `Engine/Source/Runtime/MeshConversionEngineTypes/Private/StaticMeshLODResourcesToDynamicMesh.cpp:20` requires CPU-readable mesh buffers. Editor-only source-model conversion is not a packaged input strategy.

## 4. Module and ownership boundaries

### CkRuntimeMesh

- Owns mesh data, import validation, local geometry queries, plane slicing, result lifetime, and the optional display adapter.
- Public API uses reflected CK structs, typed handles, soft asset references, requests, results, and signals. No public `FDynamicMesh3*`, mutable `UDynamicMesh*`, Jolt types, or raw component mutation escape hatch.
- Internal native geometry storage is value/RAII-owned by its ECS feature. UObject-backed display storage follows the rooted component/object-pool pattern in `Source/CLAUDE.md`; an outer or weak pointer alone is not a GC root.
- Expected direct dependencies: Core/CoreUObject/Engine, CkCore/CkEcs/CkEcsExt/CkLog/CkResourceLoader, required transform facilities, and the engine's runtime GeometryCore/DynamicMesh/GeometryFramework/mesh-conversion modules. Use only dependencies actually required by implementation; confirm their tiers and names before adding Build.cs entries.
- Prefer native runtime geometry operations. If GeometryScriptingCore wrappers are used internally, explicitly declare that plugin/runtime dependency. Never depend on Monolith, Modeling editor mode, GeneratedDynamicMeshActor, or a host editor plugin enabling geometry support.

### CkJolt

- Owns point-cloud validation for physics, convex shape construction, mass/inertia/COM interpretation, body admission, and setup status.
- Receives an owned copy of local-space points. It does not retain a RuntimeMesh handle or fetch live vertices from another feature.
- Reuses the existing world, body registry, collision layer, motion, force, and teardown infrastructure.

### Consumer boundary

A later complementary feature chooses which entity to cut, when to request it, which results become bodies, parent/result lifetimes, mass assignment, physical replacement, and application state. Mesh volume is geometric cm³; it is not automatically mass, quantity, quality, or any item identity.

## 5. Geometry source and immutable model

Proposed typed feature: `FCk_Handle_RuntimeMesh`; utils: `UCk_Utils_RuntimeMesh_UE` / generated `utils_runtime_mesh`.

`FCk_RuntimeMesh_Spec` contains a soft static-mesh source, explicit render LOD index, import tolerances, and finite operation limits. `Add(Handle, Spec)` follows normal CK composition/deferred asset loading. Setup status is observable as `Pending`, `Ready`, or terminal `Failed`, with a typed reason. Requests submitted before Ready are rejected as `NotReady`; they do not silently wait behind an unbounded asset load.

V1 source admission:

- Read cooked render data at the explicitly selected LOD. The asset must have CPU access enabled before cooking. Never fall back from an unavailable render LOD to editor source data, another LOD, or a primitive.
- Reconstruct shared geometric vertices across render-buffer seams while retaining per-corner normal/UV/color attributes. Use the declared positional tolerance; ambiguous or invalid reconstruction is rejected. Test UV seams and hard-normal seams explicitly.
- Require finite positions, valid indices, valid material IDs, nondegenerate triangles, consistently oriented closed manifold topology, one connected solid, no self-intersections, and positive volume. No silent hole filling or topology repair during import.
- V1 preserves 0-4 UV layers, normals, vertex colors, and triangle material IDs. A source with more than four UV layers is rejected. Missing colors mean opaque white; missing UVs are permitted and caps introduce UV0. Rendering tangents are regenerated from the retained normals/UV0; authored tangent vectors are not a preserved channel. Skin weights, morph data, and custom attributes are outside the source contract.
- Treat static-mesh materials as optional display defaults, separate from geometric material IDs. Geometry setup must not wait on rendering materials and must work without a renderer.

A Ready RuntimeMesh never changes its geometry. A derived result has the same feature with an internally adopted, already-validated payload. No additional asset import is required to slice a result again. No source revision race exists because geometry is immutable.

The entity transform is not baked into geometry. Local units are centimetres. Slicing, bounds, centroids, and exported vertices all refer to the original mesh-local origin; results are not silently recentered. Destroying the source after successful derivation must not invalidate its results.

## 6. Plane-slice request and result

Proposed operation: `Request_Slice(Source, FCk_Request_RuntimeMesh_Slice, Completion)`.

The reflected request payload contains:

| Field/group | Meaning |
|---|---|
| Operation ID | Caller-supplied correlation ID, unique among its in-flight requests. |
| Plane | Local position, normal, and tangent. Finite, nonzero, orthogonalized/normalized once; reject a degenerate frame. Tangent fixes cap UV orientation. |
| Cap | Nonnegative material slot ID, uniform vertex color, UV scale in cm per UV unit, and UV offset; positive finite scale. The display must supply that material slot. |
| Limits | Minimum output volume in cm³, minimum output normal extent in cm, geometric epsilon in cm, absolute/relative volume-conservation tolerance, maximum input/output triangles and vertices. Finite bounds are required; no unbounded mode in v1. |
| Result owner | Valid entity in the same registry/world, distinct from and not lifetime-descended from the source. Owns both result entities independently of source lifetime. |
| Result receiver | Required typed `FCk_Delegate_RuntimeMesh_OnSliceResolved` delegate taking one `FCk_RuntimeMesh_SliceResult` payload. Copied into the queued request. A receiver that becomes unbound before execution cancels the operation before any output entities are created. |

Use nested reflected value structs for groups; do not expose an expanding positional argument list. All limits are validated at submission and rechecked where source/output data becomes available. The module also has hard ceilings and a finite per-frame operation budget; caller limits cannot exceed them. Initial ceiling values are tuning inputs to the implementation benchmark, not performance guarantees.

For each output, normal extent is `max(dot(Vertex - PlanePosition, Normal)) - min(dot(Vertex - PlanePosition, Normal))` over its vertices. This is extent along the cut normal, not a global minimum thickness or an oriented-bounds heuristic. Both outputs must meet MinimumNormalExtentCm and MinimumVolumeCm3. Conservation requires `abs(Vpositive + Vnegative - Vsource) <= max(AbsoluteVolumeToleranceCm3, RelativeVolumeTolerance * Vsource)`; absolute tolerance is positive, relative tolerance is finite in [0, 1), and module policy caps both so a caller cannot disable validation. Geometric epsilon is a distance, never reused as an area or volume threshold.

Semantics:

1. Utils validates structural inputs and copies the request into a deferred queue. Source/owner validity is checked again at execution.
2. Processor snapshots/drains in submission order. Requests issued by callbacks execute on a later drain, never recursively inside the current slice.
3. Work uses temporary copies. Classify all source vertices by signed distance to the normalized plane. With epsilon E and min/max distances Dmin/Dmax: Dmin > E or Dmax < -E means NoIntersection; otherwise Dmin >= -E or Dmax <= E means TouchingOnly; otherwise attempt a slice. This distinguishes a miss from contact with a face/edge/vertex and never fabricates a zero-volume half.
4. Produce positive and negative half-space meshes using native geometry operations, with **zero kerf**. Cap all cut loops using topology-aware triangulation. Preserve exterior attributes by interpolation and split cap normal/UV overlays at the seam. Cap UVs use the request plane position as origin, its orthonormal tangent/bitangent as axes, and the same planar coordinates in every UV layer. Each side has its outward normal; use the supplied uniform cap color. Do not infer cap UV orientation from triangle traversal order.
5. Validate both outputs, volume conservation by the formula above, side classification, minimum volume/normal extent, and resource limits. Reject unsupported multi-island outputs rather than join them.
6. Create two private result entities under ResultOwner. Roll back both if either payload cannot be admitted. Publish handles only after both features are Ready. No renderer or physics body is created by this operation.
7. Commit and latch the terminal result before invoking external callbacks, then emit the typed result followed by generic CK completion. A successful slice never consumes, hides, moves, or destroys Source.

Result payload: operation ID, typed outcome/reason, positive/negative handles (valid only on success), each mesh's volume/centroid/bounds/triangle count, and cap triangle count. Positive/negative are defined by the supplied normal, not component enumeration order. Newly created result entities carry only geometry and lifetime plumbing; the consumer composes other features.

Outcome mapping:

| Domain outcome | CK completion | Result entities |
|---|---|---|
| `Succeeded` | `Succeeded` | Two Ready geometry entities |
| `NoIntersection` | `Succeeded` | None; completed geometric query with no cut |
| `TouchingOnly` | `Succeeded` | None; contact within epsilon, but no solid split |
| `RejectedTooSmall`, `RejectedLimit`, `RejectedTopology` | `Failed` | None |
| Malformed request / `NotReady` / invalid owner at submission | `Failed_NotEnqueued` synchronously | None |
| Source/result owner dies or typed result receiver becomes unbound after enqueue | `Failed_Cancelled` to a surviving generic completion receiver | None; never invoke the dead typed receiver |
| Internal geometry/admission failure | `Failed` | None |

The typed result must be delivered to the submitted per-operation result receiver, not a single shared “last result” slot. The generic completion callback alone cannot encode NoIntersection/TouchingOnly or return handles. Follow the existing CK exactly-once completion lifecycle and prove the proposed typed delegate in all three environments. Every accepted operation has one terminal result and one generic completion while the respective bound receivers exist; destroyed receivers are never invoked. EndPlay cancels pending work before releasing data. For synchronous rejection the typed reason is available on the same call path when its receiver is bound. Ordinary misses/sliver rejection are domain outcomes, not ensures; malformed contracts and unexpected internal failures follow CK diagnostic requirements.

Callback reentrancy is explicit: detach the executing request and copy its result/delegates into processor-local owned storage before callbacks. Teardown cancellation applies to uncommitted requests, never to an already-latched terminal outcome. If the typed callback destroys Source, generic completion still reports the latched outcome to a surviving receiver and may carry a now-invalid source handle. If it destroys ResultOwner, the published result handles expire through normal lifetime rules; later completion does not resurrect them or change success to cancellation. Success means the geometry publication committed, not that the consumer kept its outputs alive. Do not read fragments or retain references into the queue after invoking callbacks. Consumers own result cleanup and must validate handles at later use sites.

## 7. Queries and optional display

Required read surfaces: setup status/reason, geometry metrics, and a copied local vertex-position array for convex input. Do not expose mutable triangle storage. Returned arrays are caller-owned snapshots, subject to the source limits.

Proposed companion: `FCk_Handle_RuntimeMeshDisplay`, with its own `Add(Handle, Spec)`, setup status, and ordinary entity teardown. Spec names a Ready geometry handle, material slots as soft references, and standard visibility/shadow configuration. One display consumes one immutable geometry result. Its owner supplies the transform; vertices remain mesh-local.

- Use `UDynamicMeshComponent` as a rendering adapter with Chaos collision and simulation disabled. Do not automatically bake it into the Jolt static world.
- Explicitly own/root the component and its dynamic mesh, register with the correct world, and release through the existing CK component lifetime pattern. Do not create raw sibling components outside ECS ownership.
- Display takes its own immutable geometry reference/copy so source geometry destruction cannot leave a dangling mesh read. Material loading/failure has an observable terminal result; do not report display Ready until required materials are resolved.
- Headless consumers omit the display feature. Missing rendering must never turn geometry/body tests into vacuous successes. Actual appearance requires a real-renderer test/gym.
- No instancing, Nanite, LOD generation, or lighting-parity promise with static meshes is part of this contract. Validate the actual render path in the target engine build.

## 8. CkJolt runtime convex source

Append `RuntimeConvex` to `ECk_JoltBody_ShapeSource` without renumbering existing entries. Add a nested reflected runtime-convex specification to `FCk_JoltBody_Spec`: copied local point array, hull tolerance, and maximum convex radius. Existing source defaults remain unchanged.

Admission contract:

- At least four finite distinct points spanning a nonzero 3D volume; reject empty, collinear, coplanar, NaN/Inf, and over-budget input. Enforce native hull limits from the vendored Jolt version and return its failure reason.
- Points are in the body entity's local frame, in UE centimetres. V1 requires unit entity scale for this source; consumers bake desired scale into vertices first. `_ShapeScale` remains a StaticMeshAsset setting. Never double-apply scale.
- Build one convex hull. Selecting RuntimeConvex explicitly means taking the convex hull of the points: concave notches/holes are filled. It is not exact arbitrary-mesh collision. Never silently substitute a box/sphere or run decomposition when hull creation fails.
- Do not recenter points. Let Jolt's native shape COM and the existing explicit COM-offset policy operate; preserve the body-origin/render-origin relationship. Test off-origin asymmetric clouds.
- For RuntimeConvex v1, require `MassSource = Explicit` with finite positive `MassKg`. Jolt calculates inertia for that mass. This avoids inventing a density policy from mesh volume or inheriting an inappropriate unit convention. `FromStaticMesh` and `FromShape` are rejected for this new source, without changing legacy-source defaults.
- Motion type, collision profile, damping, gravity, friction, and restitution reuse existing body settings. RuntimeConvex supplies geometry only. New validation must not “ensure then fall back” into a live body with a different mass/shape.
- Once created, the shape is immutable. Recuts create new body entities. No SetShape request is included.

### Setup result is part of the extension

Add an observable `Pending / Ready / Failed` setup state and typed reason, with a bind/promise surface that also resolves for an already-terminal body. Ready means `AddBodiesFinalize` has completed and `Get_IsBodyAdded()` is true; successful hull construction alone is not body readiness.

RuntimeConvex setup must terminate as Ready, explicit failure, or owner teardown; unsupported input, invalid profile, native hull failure, and capacity failure cannot leave a caller waiting indefinitely. Capacity failure for the new source is terminal; retry is an explicit new caller attempt, not an unbounded invisible loop.

Preserve existing primitive/static-mesh behavior, including existing retry policy. Expose their true pending/ready/failure state without silently changing their retry semantics. Map existing terminal setup exits carefully and test them; do not broaden this task into repairing unrelated Jolt request-completion behavior. Teardown completes outstanding setup waiters as cancelled via the established lifetime conventions, not by calling a destroyed receiver.

This is **not** an activation barrier: Ready bodies have entered the world. Sleeping bodies can still collide. `NoCollision` is not a supported staging shortcut in the current layer derivation. A later consumer that replaces live bodies needs a separately verified staging/commit protocol; observing two Ready flags does not prove atomic physical replacement.

## 9. Persistence, networking, scheduling, and compatibility

- V1 derived meshes, display components, native buffers, and operation queues are session state; declare the appropriate snapshot posture explicitly. Saved source assets are ordinary cooked assets, not a saved cut history. Consumers requiring reconstruction must retain their own durable recipe; a snapshot must not claim to preserve derived cuts.
- No network transport, authority routing, replicated mesh buffers, automatic late-join reconstruction, or bit-identical replay guarantee. Operation IDs are correlation, not network identity. Geometry handles are registry-local. A future wrapper owns authoritative cut events and authoritative physical/quantity results.
- V1 geometry work executes synchronously inside the scheduled request processor, with bounded mesh sizes and a bounded number of operations per frame. Enqueueing is deferred; the native cut itself is not claimed to be asynchronous. Worker-thread geometry and cancellation mid-operation are deferred.
- Queues also have finite capacity. Rejection at saturation is explicit. Record operation duration, input/output sizes, temporary memory, and queue delay in the focused benchmark; no frame-time claim until measured on target hardware.
- Existing CkPmg and hosted components remain unchanged. Existing static-mesh asset collision and primitive body call sites retain their defaults. Update generated CK wrappers through the supported generator; never hand-edit generated bindings.
- Dynamic geometry display is not automatically wired to body transform ownership. Use the ordinary entity-transform/Jolt-writeback path; never have Chaos and Jolt drive the same object.

## 10. Validation rubric

Use only the host's `CkAuto/UnrealToolbox.exe` for builds and automation. Follow the applicable build-test skill for exact arguments and fresh discovery when implementation starts. The groups below are **proposed test inventories**, not claimed existing filters or passing counts. Capture the affected baseline/known failures before changing existing behavior; run focused gates on the final binary. Do not default to the whole Mars suite.

| Group | Required cases and observations |
|---|---|
| Geometry unit | Centred 10 cm cube -> two 500 cm³ solids; offset cut -> expected unequal volumes; rotated plane; second-generation cut; translated local geometry; exterior attributes and cap IDs; cap normals point outward; volume conservation. |
| Rejection unit | Plane misses, touches a face/edge/vertex, zero normal/tangent, NaN/Inf, slivers, open/nonmanifold/self-intersecting source, cavity source, cut producing multiple islands, all size/queue limits. No input mutation or partial outputs. |
| Import/cook | Explicit render LOD, CPU access present/absent, UV/hard-normal seam reconstruction, multiple materials, vertex colors, missing asset/LOD. Repeat source import and slice in a packaged build with editor data absent. |
| Lifecycle AutoTest | Deferred ordering; typed callback destroys source, result owner, or completion receiver; two queued operations with distinct receivers/IDs; result receiver dies before processing; source/result-owner teardown before processing; forced GC while queued and while displayed; repeated create/slice/destroy returning resource counts to baseline. Assert exactly-once latched outcomes and no post-callback fragment reads. |
| API parity | Equivalent C++, BP, AS calls produce matching outcome categories, two readable result handles, and readable metrics. Compile optional-AS configuration if binding-specific native code is introduced. |
| Jolt AutoTest | Valid/off-origin clouds, gravity/contact/query/impulse, explicit mass, COM offset, shape/body readiness ordering; malformed and degenerate clouds, hull overflow, invalid mass/profile/scale, body-capacity failure. No live substitute body after failure. |
| Compatibility | Existing affected primitive/static-mesh Jolt tests and component lifetime tests; legacy source retry behavior; no CkPmg rendering change. |
| Real-renderer gym | Generic checker-textured solid with at least two exterior material IDs and a distinct cap material; inspect seams, normals, cap UV orientation, shadows, repeated cuts, separated bodies, visual/body-origin alignment and teardown. |
| Benchmark | Report worst observed operation/frame time, queue latency, and peak memory across a named bounded workload. Select and document shipping ceilings from measurements; no unmeasured performance acceptance claim. |

Geometry tests must state explicit numerical tolerances and scale ranges. For the analytic 10 cm cube fixture, initially require absolute volume error <= 0.01 cm³ per half and conservation error <= 0.02 cm³; these are proposed acceptance thresholds, not measured results. Boundary and small-scale tests use declared epsilon/minimum-thickness settings rather than reusing that cube tolerance blindly.

All eight acceptance categories in section 1 need evidence. Compile/headless green does not establish rendering, packaged input availability, physical input interaction, or multiplayer. The last two are outside this framework-only acceptance scope.

## 11. Intended delivery slices and file ownership

These are dependency boundaries for a later execution plan, not started phases.

1. **Native geometry + import:** `Source/CkRuntimeMesh/` module, typed data feature, immutable storage, validation/queries, source loading, native tests. Prove cooked input and seam reconstruction before committing to the public import contract.
2. **Deferred slicing + lifetime:** same module's requests/processors/results and lifecycle tests. Prove two-result publication and every terminal path before adding presentation.
3. **Display:** RuntimeMeshDisplay companion, rooted component/material lifetime, real-renderer gym. Headless operations must stay independent.
4. **Runtime convex bodies:** `Source/CkJolt/` shape-source data/factory/setup/result surfaces, targeted Jolt tests. No reverse dependency on RuntimeMesh.
5. **Framework composition proof:** generic sliced-solid gym and C++/BP/AS/cooked acceptance evidence. No host-game station changes.

Registration/documentation will touch `CkFoundation.uplugin`, `Source/CLAUDE.md`, new module guidance, and normal processor/binding registration points identified during implementation preflight. Follow the existing test ownership convention in CkTests; do not add framework verification to Mars-only tests. No source files, assets, descriptors, or generated bindings are changed by this specification.

Before implementation, convert these slices to a concrete inventory and gates under the framework methodology. Load framework source/AngelScript guidance, ck-macros-and-codegen, ck-angelscript-interop, ck-tests-authoring-and-running, and build-test as applicable. Implementation is additive API plus behavior-sensitive Jolt setup reporting; any core lifecycle/persistence/replication change is a scope escalation, not implicit authorization.

## 12. Alternatives rejected and remaining evidence

| Alternative | Reason not selected |
|---|---|
| Generalize the current application-side triangle slicer | Couples reusable geometry to application assembly and lacks the required topology/attribute contract. |
| Expand CkPmg into arbitrary editable solids | Its current analytical/debug shape surface does not own solid topology or source import; keep existing consumers stable. |
| Stock `SliceProceduralMesh` as the public contract | Useful narrow engine API, but component-centric mutation/ownership and component collision do not establish CK geometry results or Jolt bodies. |
| Make Physics consume `UDynamicMeshComponent` directly | Couples shape ownership to rendering/Chaos cooking and excludes a clean headless path. |
| Add live SetShape or a universal destruction transaction now | Not required for independent derived geometry/new-body admission; expands ordering and contact-lifetime obligations. |
| Automatic convex decomposition, mesh replication, and cut persistence | Separate capabilities with independent acceptance burdens; do not smuggle them into plane slicing. |

Still to prove during implementation: native cut behavior on the accepted topology corpus, render-buffer seam reconstruction after cooking, public reflected result bindings, actual bounded performance, and runtime convex setup/result behavior. None is marked solved by this document. In particular, **safe replacement of a live physical object remains a later composition contract**, even after all framework gates above pass.
