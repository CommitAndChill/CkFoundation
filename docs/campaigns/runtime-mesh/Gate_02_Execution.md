# Remaining execution contract

Read with PLAN.md and the capability specification. PROGRESS.md owns status.

## Gate 1B: cooked source before public import

- CkTestsEditor owns opt-in `RuntimeMeshCooked.AuthorFixtures` and
  `RuntimeMeshCooked.Import` tests. Fixture assets live only under
  `/CkTests/CkRuntimeMesh/Cooked/`; unknown existing packages must not be overwritten.
- CkTests owns the explicit packaged probe startup hook and immutable geometry
  checks. The probe must run with editor-only data absent, import the CPU-readable
  seamed cube, preserve every authored channel, and reject its CPU-disabled peer.
- Toolbox builds Editor/Game and drives the editor automation wrapper. Toolbox
  has no cook/stage command: UAT may cook/stage with compilation disabled after
  the Toolbox Game build. The wrapper checks the actual packaged process exit and
  a unique fresh log verdict. No raw Automation command is a substitute.
- Both cook rows use an opt-in root outside the normal Ck test population. Missing
  packaged evidence fails an explicit run; it is never converted to a skip/pass.
- After cooked proof, add the RuntimeMesh reflected data/fragment/processor/utils
  quartet, rooted asset-loading batch, session posture and processor registration.
  Setup-only loading state is released at terminal resolution. Ready state retains
  only immutable geometry plus queryable status/reason, not the construction spec.

## Gate 2: native cut and deferred publication

- Internal Slice.h/.cpp use native FMeshPlaneCut and constrained Delaunay filling.
  Both copies are validated before returning either. Native tests cover numerical
  volumes, cap channels, repeated/translated/oblique cuts and all rejection classes.
- RuntimeMesh requests use nested reflected plane/cap/limit groups and required
  per-operation typed receivers. A bounded globally snapshotted drain prevents
  callback submissions to later entities from executing in the same drain.
- Revalidate source/owner/receiver before execution; keep terminal result and
  request delegates in owned local storage. No fragment references survive a
  callback. Publish two Ready children under an independent valid owner or none.
- CkTests owns native/lifecycle tests, reflected Blueprint invocation evidence and
  AngelScript consumer tests. Generated wrappers use the supported generator only.

## Gates 3-5

- Display owns separate setup/material loading and pooled rooted component/mesh
  lifetime. Geometry stays independent. Chaos and automatic static baking are off.
- Jolt owns its copied RuntimeConvex points, validation and hull setup. It has no
  RuntimeMesh dependency. Setup readiness is latched only after world admission.
  Capacity failure is terminal only for the new source; legacy retries remain.
- Jolt setup waiters cancel through all-world teardown, including entities that
  never obtained a runtime body or requests fragment. Terminal callbacks may
  destroy their entity; subsequent processing must not read stale fragments.
- Before modifying existing Jolt behavior, capture focused affected legacy test
  counts. Run the same group on the final binary and add new adversarial cases.
- A generic CkTests composition fixture proves C++/BP/AS, cooked import/slice,
  independently movable bodies, real rendering and GC/teardown. No Mars gameplay.
- Benchmark named bounded workloads, operation/frame duration, queue latency and
  temporary memory before accepting final ceilings. Headless green is not renderer
  evidence and editor green is not packaged evidence.

## Ownership and review

The lead owns dependencies, registration, public architecture, evidence integration
and final acceptance. Delegates own explicitly assigned disjoint files. No source
edits during a build/test snapshot. Independent review challenges geometry,
lifetime, reentrancy, cooked evidence and legacy compatibility; the lead verifies
each finding against current source before accepting it. No commit or push.
