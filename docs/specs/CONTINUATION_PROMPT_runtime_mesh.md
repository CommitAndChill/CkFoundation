# 1. One-line summary

Implement the framework-only CkRuntimeMesh slicing capability and CkJolt runtime convex-body admission described in `D:/Repositories/CkRepos/Orion/Plugins/CkFoundation/docs/specs/2026-10-08-CkRuntimeMesh-slicing-design.md`, starting with source verification and the native geometry/import gate.

This kickoff authorizes implementation within that specification; its earlier “design proposal / not permission to implement” line describes the previous documentation-only session. Read the entire spec. Do not merely summarize it and ask whether to begin. Present a concise execution plan, then proceed within scope. Raise a concrete blocker if source evidence invalidates a design decision; do not silently weaken acceptance criteria.

# 2. Repo state

Observed 2026-10-08; refresh before editing:

- Host: `D:/Repositories/CkRepos/Orion`, project `Mars.uproject`, branch `dev`, HEAD `50b3906f1d76b9e094543b97d4674441141588db`.
- Implementation belongs in `Plugins/CkFoundation`; framework tests follow the existing CkTests ownership convention. These are separate Git submodules.
- The previous session created only the design spec and this handoff. No implementation, builds, tests, commits, or pushes were performed. `Plugins/CkFoundation/Source/CkRuntimeMesh` did not exist at handoff.
- CkFoundation Git metadata access failed under the sandbox. Its branch, HEAD, and dirty inventory are unverified; do not infer a clean submodule or reset it. Obtain the current inventory before implementation.
- Preserve all existing host changes: cooking Pan material/export, AutoTests map, Fry/Tumbler kernels and tests, generated script files, Dicing/Fry/Searing/Tumbler station scripts, cooking audio, Blender cooking exporters, and the untracked Searing/Tumbler tests. In particular, deleted Tumbler test files belong to other work; do not restore them.
- The spec/handoff are saved inside CkFoundation but are not committed by this session. Do not commit, push, rebase, or bump submodule pointers unless separately requested.
- No sibling consumer repository needs changes for this task.

# 3. Active work and open questions

The user requested: “a spec for the ckfoundation work alone. Once I have this done, we can look to leverage it and add w/e game-specific detaisl on top of a wrapping /complementary feature”. This implementation must therefore remain independently usable and verifiable without cooking stations or other game features.

The spec fixes ownership, outcomes, coordinate frames, topology restrictions, and scope. Remaining implementation evidence includes cooked render-buffer seam reconstruction, native cutting on the supported topology corpus, reflected result bindings, module dependencies, finite workload ceilings, performance, and Jolt setup-result behavior. Convert these into explicit gates, not assumed successes.

# 4. Why the earlier investigation is not implementation proof

- Source inspection found Unreal geometry primitives and existing CK component/body infrastructure; it did not exercise the new framework capability.
- Mars already has an application-local visual procedural slicer, but its output has no Jolt collision or framework mesh-result contract. Do not promote that handwritten clipping/cap implementation into CK.
- The latest inspected editor log mounted GeometryScripting. A mounted plugin does not prove the required public API, runtime cook data, or physical behavior.
- CkJolt's existing DynamicMesh extraction serves static-world baking. It does not provide runtime convex body input or synchronized physical replacement.
- The design received a bounded review and was tightened for grazing outcomes, geometric tolerances, typed receiver cancellation, and callback-triggered teardown. These remain test obligations.

# 5. Diagnostics and first evidence

Read applicable AGENTS.md/CLAUDE.md and relevant .claude guidance. Load ck-methodology, ck-change-control, and the applicable build-test skill; use framework/interoperability/test skills as their work begins. Re-read current exemplars before copying patterns.

Capture current host/CkFoundation/CkTests revisions and dirty paths. Resolve the project engine through supported tooling and verify its version: the previous source/log inspection found UnrealEngine-Angelscript 5.7.4, while old project prose said 5.5. Engine-source path observed previously: `D:/Repositories/UnrealEngine-Angelscript`.

Read the current GeometryProcessing plane-cut APIs, runtime mesh conversion path, CK deferred completion primitives, component GC ownership, and Jolt setup/admission ordering. Establish the actual test names and existing failures for the affected surface. No fabricated test counts or copied historical green results.

# 6. Symptom-to-cause investigation map

These are hypotheses to test, not pre-diagnosed failures.

| Symptom | First hypothesis/check | Source area |
|---|---|---|
| Import works in editor, fails cooked | Missing CPU-readable render LOD; accidental editor-source dependency | Engine MeshConversionEngineTypes; RuntimeMesh import |
| Cut leaves holes or joins islands | Input seam topology, unsupported solid, or cap result not validated | Native plane-cut wrapper and output validation |
| Cut faces have wrong UVs/normals | Plane frame or per-corner overlay handling | RuntimeMesh cap generation/display |
| Objects disappear after GC | Weak/outer reference mistaken for ownership | CK component/object-pool lifecycle |
| Callback fires twice or crashes after teardown | Queue references survive callback or terminal result not latched | CkRequest completion; RuntimeMesh processor/EndPlay |
| Convex body stays pending | Setup failure not surfaced; legacy capacity retry copied into new source | CkJoltBody setup/result |
| Visible piece and collider disagree | Unit scale, local origin/COM, or convex approximation misunderstood | RuntimeMesh vertex export; Jolt shape setup |
| Both halves Ready but replacement is unsafe | Body readiness mistaken for an activation transaction | Later consumer composition; outside this spec |

# 7. Critical files

Paths below are relative to `D:/Repositories/CkRepos/Orion/Plugins/CkFoundation` unless stated otherwise.

1. `docs/specs/2026-10-08-CkRuntimeMesh-slicing-design.md` — authoritative capability contract and acceptance rubric.
2. `CLAUDE.md` and `Source/CLAUDE.md` — framework conventions, module boundaries, UObject rooting, persistence posture.
3. `Source/CkEcs/Public/CkEcs/Request/CkRequest_Completion.h` — synchronous rejection, exactly-once completion, cancellation.
4. `Source/CkEcs/Public/CkEcs/Request/CkRequest_Data.h` — request ownership and delegate lifecycle.
5. `Source/CkPmg/Public/CkPmg/CkPmg_Processor_Donut.cpp` — procedural component setup/lifetime precedent, not slicing code.
6. `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Fragment_Data.h` — shape-source/spec extension.
7. `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Processor.cpp` — shape construction, COM/mass, body admission and failures.
8. `Source/CkJolt/Public/CkJolt/Body/CkJoltBody_Utils.h` — existing readiness and public API conventions.
9. `Source/CkJolt/Public/CkJolt/StaticWorld/CkJoltBakeExtraction.cpp` — authored convex construction and static DynamicMesh extraction.
10. `Source/CkEcs/Public/CkEcs/Snapshot/CkSnapshot_Posture.h` — explicit session-state declaration.

# 8. Things ruled out

Do not add cooking/ingredient rules, station integrations, inventory, scoring, mesh replication, cut save/load, soft bodies, arbitrary booleans, automatic convex decomposition, live SetShape, or atomic live-body replacement. Do not change CkPmg behavior, rely on Monolith dependencies, or expose engine/Jolt mutable internals publicly. Do not silently replace failed geometry with a primitive collider.

# 9. Architecture notes and gotchas

- CkRuntimeMesh owns immutable geometry; slicing publishes two independent Ready results while preserving the source. ResultOwner cannot be the source or its lifetime descendant.
- Display is optional and rendering-only; geometry must work headless. RuntimeMeshDisplay owns/root-references its component and mesh. Chaos collision/simulation and automatic static-world baking stay disabled.
- CkJolt receives copied local-space convex points, never RuntimeMesh handles. No reverse dependency on CkRuntimeMesh.
- RuntimeConvex requires unit entity scale and positive explicit kg mass. Geometry is local centimetres; do not recenter vertices or double-apply scale. Hulls fill concavities by definition.
- Source import uses explicit CPU-readable cooked render LOD data. Preserve the exact attribute contract in the spec, including seam reconstruction; no editor-only fallback.
- NoIntersection and TouchingOnly are successful no-cut outcomes. Sliver/topology/limit rejection and cancellation are distinct. Only a successful slice publishes two outputs.
- The typed result is latched before external callbacks. A callback that destroys source/result owner cannot retroactively change success to cancellation. No queue/fragment references survive a callback.
- RuntimeConvex setup needs explicit terminal failures; existing source retry behavior must remain compatible. Body Ready means admitted to the world, not staged without collision.
- Hard ceilings/default budgets remain to be selected and recorded from a bounded benchmark. Resolve them before claiming production readiness; do not invent performance numbers.

# 10. Concrete execution and verification flow

1. Read the spec and applicable guidance fully; refresh source/Git/engine facts and preserve dirty work. If another session has already implemented part of this feature, inspect and resume it rather than overwrite it.
2. Create a compact execution package under `Plugins/CkFoundation/docs/campaigns/runtime-mesh/`: stable mission/spec link, concrete phase/file ownership and gates, and a single PROGRESS.md with evidence and blockers. The spec is a capability design, not already a complete execution package.
3. Begin with the native geometry/import slice from spec section 11. Resolve runtime module dependencies, immutable ownership, source validation and typed contracts. Prove cooked LOD/seam import before broadening the API. Use generic framework fixtures.
4. Implement and validate deferred slicing/lifetime, then optional display, runtime convex body admission/setup results, and the framework composition proof in that dependency order. Independent read-only research/review may be delegated; keep tightly coupled edits and final verification coordinated.
5. Build and run automation only through `D:/Repositories/CkRepos/Orion/CkAuto/UnrealToolbox.exe`, with an explicit absolute `--project=D:/Repositories/CkRepos/Orion/Mars.uproject`. Read the current build-test instructions for the actual argument spelling and launch procedure. Never call Build.bat, UBT, or raw Automation ExecCmds directly.
6. Launch long toolbox jobs detached through PowerShell with the helper process hidden and the toolbox's own progress facility available; never pass `--no-progress-window`. Record the actual output log, invocation, process exit and editor-log verdict; an async “completed” notification is not proof. Respect editor/build-lock rules.
7. Run narrow affected gates, record failing names and final-binary results, and preserve any existing known failures. Do not default to the full suite, a broad Mars filter, or a wide pre-change baseline. Fresh discovery is required when newly registered tests would otherwise be absent.
8. Exercise public APIs in C++, BP and AS. Separate headless, real-renderer, cooked-runtime and benchmark evidence. Do not label unobserved runtime paths passing. Update PROGRESS.md at each completed gate and proceed through authorized work without asking to continue after every routine step.
9. Review each completed gate for topology, lifetime, callback ordering, coordinate/mass, and legacy-contract regressions. Any failed assumption that requires changing the spec must be documented and resolved explicitly. Do not stack patches over a regression caused by this work.
10. Report implemented surfaces, changed files, exact evidence and remaining gaps. Leave changes uncommitted/unpushed. If the work exceeds a session, finish at a coherent gate and leave enough current evidence to resume without reconstructing this conversation.

# 11. Suggested first message

“I’ll verify the spec against the current CkFoundation and engine source, establish the implementation gates, and start the native geometry/import slice. Game integration stays outside this work.”
