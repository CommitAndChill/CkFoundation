# Runtime mesh mission

Written 2026-10-08. Stable contract; live evidence and status belong only in
[PROGRESS.md](PROGRESS.md). Retire this package when the capability is accepted
and its permanent module documentation covers the delivered contract.

Implement [the framework specification](../../specs/2026-10-08-CkRuntimeMesh-slicing-design.md).
The 2026-10-08 implementation request supersedes the spec's earlier documentation-only restriction.
All eight acceptance categories G1-G8 remain required for complete delivery.

Geometry is immutable, local centimetres, independent of rendering and physics.
Construction inputs are unpacked; retain only configuration actually read later.
Operation policy, geometry state, and display configuration have separate owners.
Display is an optional companion. Jolt accepts copied convex points and has no
dependency on RuntimeMesh. No game integration, live-body replacement, mesh
replication, cut persistence, commits, pushes, or submodule pointer changes.

Read the spec and current source before resuming. Follow [PLAN.md](PLAN.md),
check its entry conditions, and update PROGRESS.md after each evidence gate.
Use UnrealToolbox with the absolute Mars.uproject path for all builds/tests.
Do not weaken topology, attribute, lifetime, or cooked-data acceptance to get green.
