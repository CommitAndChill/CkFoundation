# Texture, flipbook and material authoring workflow

Turn the requested look and gameplay role into a reproducible effect with explicit visual, data, and runtime evidence. Apply existing project conventions and authorization. This skill does not authorize installs, paid services, production replacement, framework changes, or publication beyond the user's request.

## Core tenets

1. **Start from the look in game.** Reference, silhouette, motion, timing, camera distance, lighting and gameplay readability determine the method. More noise, more particles and higher resolution do not establish quality.
2. **Use the simplest method that meets the target.** Reuse accepted recipes first when they fit. Procedural math, simulated motion, generated art and mesh debris solve different problems; combine them deliberately rather than forcing one backend to do everything.
3. **Approve appearance before multiplying work.** Establish a representative look and actual motion before expensive full output, variants or production integration. Preserve approval already given; do not ask again for routine steps inside its scope.
4. **Separate structure from appearance.** Keep controllable density/opacity, shape, timing and material data independent of optional baked lighting, color or generated detail. Retain layers when their timing, material or behavior differs.
5. **Keep animation coherent.** Evolve one field or simulation through time. Independently generated attractive frames are not a proven flipbook. A looping preview player does not establish a seamless loop or a working Niagara lifecycle.
6. **Acceptance has a scope.** Technical correctness, user visual approval, runtime behavior and measured performance are separate evidence states. Preserve known defects even when the user accepts a test as good enough.
7. **Optimize against the intended use.** Design for screen coverage, simultaneous effects, platform and scalability from the start; measure the integrated result before calling it optimized.
8. **Preserve the accepted result and its provenance.** New versions get new destinations. Keep editable sources, dependencies, parameters, execution evidence and exact selected frames. Do not silently rebake an approved sequence while packaging it.

## Ordered workflow and user involvement

### 1. Establish the brief

Read the user's references and the selected project's existing materials, Niagara systems, CkUsf authoring conventions and reusable assets. Do not repeat backend comparisons already settled by evidence.

Extract: gameplay purpose; reference and style; viewed scale/angle; silhouette and motion phases; duration; one-shot, repeat or seamless loop; lighting response; target platform and expected concurrency; delivery scope and location. For material work include UV/world space, tiling, surface response and editable parameters.

State a short working brief and your recommended method. Infer reversible details from context. Ask only for missing choices that materially affect the result, such as competing visual targets, required dynamic lighting, an unspecified destination needed for integration, or a real budget tradeoff. Do not turn the brief into a mandatory approval ceremony when the target is clear.

### 2. Choose the method and set a small proof

Read [techniques.md](references/techniques.md) for the relevant route. Selection order is a decision sequence, not a requirement to try every backend:

First choose the runtime representation: camera-facing card, layered/mesh effect, surface material or another project-supported representation. Decide whether baked lighting and one view are acceptable, or whether camera movement, parallax, intersections and dynamic lighting require a different representation. Choose the authoring backend to produce that representation; a convincing single-view render does not prove a suitable in-game asset.

1. Check accepted reusable assets/recipes and the current project's native implementation.
2. Choose shader math or native procedural generation when controlled structure and inexpensive motion can meet the target.
3. Choose fluid simulation directly when coherent rolling, advection or evolving volume is central; do not first spend rounds trying to imitate it with unrelated noise.
4. Choose image generation when authored visual character or source-art exploration is the unmet need. Use the available imagegen skill/tool; do not substitute it for calibrated data or assume temporal coherence.
5. Combine routes only where each adds a clear benefit. For example: simulated perimeter billows, procedural radial streaks and mesh soil fragments with independent timing.

Explain the tradeoff in a few sentences. Verify the actual selected backend/device with a small execution when its availability is uncertain. Keep GPU solver, GPU renderer and generated-image service evidence separate. A failure does not authorize a silent CPU fallback, tool substitution or installation when the task constrains those choices.

### 3. Produce and review a representative proof

Use reduced-cost output that still tests the difficult part. Sparse keyframes can settle an explosion's direction before a full bake, but animation needs continuous playback before motion acceptance. For a static material, show the relevant surface under useful lighting, scale and repeat conditions.

Show actual generated/rendered output inline when possible: reference beside result with matched scale/timing; individual layers beside the combination; alpha over contrasting backgrounds; relevant normal/material lighting views. Label source preview versus actual Unreal capture. Inspect continuous playback when available; otherwise disclose that only sampled frames were inspected.

Judge silhouette, hierarchy, energy, weight, rolling/deformation, density, birth/decay, clipping, flicker and usefulness at the gameplay camera. If a round fails, identify the cause and change one main visual axis before rerendering. Do not use blur, exposure changes or more turbulence as unexplained cures.

**User checkpoint:** for a new or materially changed visual direction, present the proof, your recommendation and its concrete limitations before expensive final generation, packing a new animation, or promoting it into production. Ask for a decision on the visible result, not a tool plan. Existing explicit authorization to continue through these stages takes precedence. A new seed within an approved recipe may be generated without another planning approval; present its result without claiming all parameter combinations are approved.

### 4. Finalize and validate the assets

After the required visual decision, generate the agreed final output. Package exact approved frames when packaging alone was authorized. If a changed crop, lighting, timing, resolution or compression materially alters the accepted appearance, show that change before calling it the accepted result.

Read [asset-contracts.md](references/asset-contracts.md) for channel, alpha, atlas, normal and reproducibility checks. Choose dimensions/frame count from the brief; historical 64-frame 8x8 atlases are examples, not defaults. Record source and game-export precision separately. Create only channels the consumer needs.

Technical export fixes inside the agreed look do not need repeated permission. If they visibly change the look, surface the change. Keep the first approved effect intact while generating variants.

### 5. Integrate in the authorized project

A request to create a Niagara effect or CkUsf material in a named project can authorize implementation there. Approval of an isolated texture experiment alone does not. Do not import into a guessed checkout or change a shared template to make a preview work.

Inspect the project's real material/renderer and lifetime path. Use owned copies or instances where appropriate, expose a small useful parameter set, and preserve accepted sources. For CkUsf inspect the current source/generation/reload APIs rather than inventing them. Keep routine loaded-material reload separate from explicit global/shading-model recompilation. Follow the project's build-test workflow if native or script changes require a gate; asset-only iteration should reuse the authorized live editor where possible.

Validate the actual imported settings, compressed/mipped appearance, normal interpretation and lighting. In Niagara verify SubUV order and interpolation, particle age/lifetime, emitter/system cycle, spawn/stop/retrigger, bounds/culling and required gameplay repetition. Seeked screenshots establish frame correspondence; ordinary playback establishes cadence and lifecycle. Save/reload and recheck the owned asset rather than relying on transient editor values.

Show actual in-engine playback under the intended camera and lighting before runtime visual acceptance. If the user skips Unreal validation, honor that choice and label runtime evidence unverified. Do not restart a previously stopped experiment just to fill a gate.

### 6. Measure, tune and deliver

Measure the integrated effect on the target platform when available, or state the actual proxy device and limitations. Check a representative worst-use scene: intended distance/screen coverage and simultaneous effects, relevant lighting and scalability levels. Record texture format/resolution/mips and memory, draw/material complexity, translucent overdraw, emitter/particle counts, and CPU/GPU cost as applicable. Use comparable before/after conditions for optimization claims; repeated comparable captures are needed for a reliable performance improvement claim.

Address the measured bottleneck while preserving approved readability: trim transparent coverage, reduce overlapping layers, reduce redundant sampling/work, choose suitable blend/lighting, reduce channel/atlas memory, set practical bounds and distance/scalability behavior. Do not blindly disable mips, force uncompressed textures or replace volume motion with many overlapping cards.

**User checkpoint:** bring back an actual visual/performance tradeoff when meeting the budget changes the approved look. Routine tuning within the accepted brief is autonomous. A final presentation should distinguish assets technically validated, visuals accepted, runtime exercised and performance measured; missing evidence stays explicitly pending.

Deliver selected assets plus editable source/recipe, dependencies and reproduction command, parameters/seeds, channel/atlas manifest, validation evidence, previews, approval scope and known limitations. State what changed in the project and what remains unverified. A preview host, exported video or map-check pass is not gameplay integration or performance proof.

## Reference routing

- [techniques.md](references/techniques.md): read the selected method's strengths, limitations and recipe guidance.
- [asset-contracts.md](references/asset-contracts.md): read before final export, import or diagnosing alpha/data/atlas issues.
- [investigation-evidence.md](references/investigation-evidence.md): consult when reusing prior accepted recipes or resolving contradictory historical completion notes. It records evidence boundaries, not installed runtime paths or a universal tool ranking.
