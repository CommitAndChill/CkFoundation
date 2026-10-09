# Gate 1: native geometry and import

Written 2026-10-08. Depends on verified repository/engine preflight.
Status and all numerical results live only in PROGRESS.md.

## Entry

- Capture host, CkFoundation and CkTests revisions/dirty inventory.
- Resolve engine with the repository helper; inspect its actual runtime geometry APIs.
- Inspect the existing known-red manifests and their mtimes. RuntimeMesh has no
  existing implementation or tests; do not invent a previous passing count.
- Preserve unrelated changes, including deletions and mixed CkTests.Build.cs edits.

## 1A native kernel

Files: new Source/CkRuntimeMesh build/module/internal geometry files and Claude.md;
CkFoundation.uplugin; Source/CLAUDE.md; CkTests native RuntimeMesh tests and one
additive dependency in its existing build file.

Use native render-LOD conversion only after validating CPU buffers, index/section
ranges and material IDs. The engine converter can discard duplicate triangles or
split nonmanifold edges, so conversion success alone is insufficient. Reject any
loss, ambiguous seam pairing, residual boundary, invalid orientation, bowtie,
disconnected shell/cavity, self-intersection, nonfinite data or nonpositive volume.
Native seam merging preserves per-corner overlays. Never apply build scale twice.
Area, distance and volume tolerances have distinct units. Limits are finite and
provisional until the benchmark; no production performance claim.

Return an immutable RAII payload only after admission. Internal native access is
for feature implementation and CkTests, not a consumer mutation API. Do not expose
an ECS Ready contract until the import proof is established.

| Observation | Expected | Failure response |
|---|---|---|
| Outward 10 cm cube, split render seams | One closed solid, volume 1000 cm3, error <= 0.01 cm3; original local origin and attributes retained | Inspect conversion, winding and seam maps; no repair/fallback |
| Missing CPU data/LOD, invalid indices/sections, >4 UVs, malformed limits | Typed failure, no geometry | Fix admission before expanding API |
| Open, duplicate, nonmanifold, disconnected/cavity, crossing or overlapping adjacent triangles | Typed rejection, no geometry | Add discriminating fixture and correct validator |
| Final native binary | Focused Ck.RuntimeMesh tests discovered and run, real exit/log verdict | Diagnose exact failed names; no broad baseline |

## 1B feature and packaged input

First prove a generic CPU-readable render LOD with seams after cooking, with editor
source data absent; also verify CPU-disabled failure. Use the internal native
boundary before broadening the consumer API. Native synthetic-buffer tests are
not a substitute. Inspect the existing CkTests packaging/test harness before
choosing the smallest fixture and Toolbox gate.

Add reflected nested source/import policy, typed handle/status/reasons/metrics,
immutable session geometry fragment, transient pending-load state and setup/endplay
processors following CkTimer/CkFx. Retain only necessary steady-state policy;
no rendering/material fields in geometric state. Root source assets only while needed.

Then verify public queries in C++, Blueprint and AngelScript.

## Exit

1A and 1B are recorded separately. Gate 1 is accepted only after both have evidence.
Read final diffs and logs, record exact filters/counts and outstanding checks, and
update module docs and PROGRESS.md. No commits or pushes are authorized.
