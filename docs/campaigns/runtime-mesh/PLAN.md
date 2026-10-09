# Runtime mesh execution gates

Written 2026-10-08. Status exists only in PROGRESS.md. This index retires with PROMPT.md.

| Gate | Ownership and work | Required exit evidence |
|---|---|---|
| 1A Native import | New CkRuntimeMesh internal kernel; module descriptor/build; CkTests native fixtures | Runtime-only dependencies compile; explicit LOD/CPU checks; unique seam reconstruction; strict solid admission; immutable results; native tests and adversarial review |
| 1B Feature and cooked import | RuntimeMesh data/fragment/processor/utils quartet; rooted asset batch; reflected metrics/status; framework cooked fixture | Pending/Ready/Failed; source teardown; C++/BP/AS queries; packaged render-data import without editor source; seam/attribute parity |
| 2 Deferred slicing | RuntimeMesh request/result structs, processor, terminal delivery, lifecycle tests | Native plane/cap corpus; exactly two independent Ready outputs or none; ordered deferred drain; every cancellation/reentrant callback case in spec section 10 |
| 3 Display | RuntimeMeshDisplay companion and generic CkTests gym | Pooled rooted component/mesh, materials, no Chaos/static bake; forced GC/teardown; actual renderer normals/UV/material checks |
| 4 Convex admission | CkJolt body spec/setup/status and focused CkTests | Valid hulls and explicit mass/scale/COM; admission readiness; terminal capacity failure for RuntimeConvex; legacy-source retry compatibility |
| 5 Composition and limits | Generic framework fixture/gym and bounded benchmark | Full G1-G8 evidence; repeated cuts and independent bodies; C++/BP/AS/cooked parity; measured duration/memory/queue delay and justified shipping ceilings |

Gate 1 details: [Gate_01_NativeImport.md](Gate_01_NativeImport.md).
Before entering each later gate, write its concrete file/observation contract and
recheck the preceding gate against the then-current source. No later gate is
implicitly accepted by native tests or an editor build.

Reference patterns: CkTimer quartet for composition and registration;
CkFx Vfx setup for rooted asset batches; CkSnapshot_Posture.h for session markers;
CkPmg donut for component lifetime; CkJoltBody for physics admission.
Native geometry is new infrastructure and requires direct tests, not pattern inference.

Delegation: lead owns architecture, module registration, integration and final
adversarial review; geometry executor owns only Internal/CkRuntimeMesh_Geometry.*;
test executor owns only CkTests/Private/UnitTests/CkRuntimeMesh/. Build.cs edits
are lead-owned because CkTests already has unrelated local changes.
