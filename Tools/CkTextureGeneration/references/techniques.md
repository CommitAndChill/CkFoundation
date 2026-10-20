# Technique selection and recipes

Choose for the visual problem, required controls and runtime budget. These recipes consolidate investigated methods; they do not require trying every tool or claim that one backend wins every effect.

## Shader math and CkUsf

Use the selected project's native shader/material path for controllable UV transforms, erosion, distortion, gradients, simple rings and parameterized surface response when it can meet the reference efficiently. Prefer reusable texture inputs for expensive detail when profiling supports that choice; a texture-free shader is not automatically cheaper.

Inspect current CkUsf examples, generated parameter bindings, material domain/blend mode, rendering path and supported shading models. Define what each parameter controls and how it is driven by particle age or gameplay. Verify on the actual generated material, including shader readiness before capture. Reuse automatic loaded-material reload and the project's manual command; global or shading-model reload remains explicit because it affects shared shader state. Do not broaden a material request into a new shading-model implementation without a consequential design decision.

Keep detail frequency useful at the camera distance. Test dynamic lighting and relevant renderer paths rather than inferring them from a lit source image or a successful compile. This is integration guidance, not evidence that every CkUsf platform/path has been tested.

## Native Blender procedural textures and geometry

Good fit: periodic erosion masks, authored directional wisps, coherent noise evolution, flow/distortion fields, correlated PBR channels, radial streaks, debris and parameterized variants. Native nodes/geometry provide editable shape and shared signals across channels.

Start with structural controls: silhouette/envelope, broad forms, directional motion, density and detail scale. Add fine detail only after the large forms read. Use periodic construction for tiles and inspect repeated previews. Evolve a continuous field for animation rather than randomizing each frame. Separate coarse irregular debris from fine dust.

Limits observed: overlapping smooth lobes can read as blobs; uniformly spaced radial puffs and many fine rays look mechanical; changing noise is not a gas simulation. Dense cores may hide deformation. A static volume under lights can be broad/noisy without delivering useful rolling motion. Use a simulator directly when that motion is the target.

For PBR, derive albedo, roughness, metallic and normal/height from common material structure. Do not fabricate missing AO or calibrated displacement: identify neutral placeholder channels and relative artistic height. Independent attractive images are not an aligned material set.

## Blender fluid simulation

Good fit: evolving smoke/explosion forms, connected billows, rolling curls and advection whose motion must remain coherent. The investigated Continuum Flow route demonstrated actual GPU gas simulation. Preserve a proven scene's emission shape and trajectories when changing execution backend: a spherical source is not equivalent to an accepted directional lobe recipe.

1. Check the actual add-on, device and cache path with a small native bake. Record solver and renderer separately. Do not hard-code a historical GPU name/index into a portable recipe.
2. Establish source shape/velocity, emission interval, domain scale/cell size, buoyancy, turbulence/advection and dissipation from the motion brief. Keep the camera and framing stable across time.
3. Review useful early/middle/late frames, then continuous motion. Solve broad shape, rolling and density together; stronger turbulence may turn dense billows into wisps instead of improving the target.
4. Render separate lit beauty and unlit projected density/opacity when the consumer benefits. Keep their actual relationship explicit; changing volume illumination/extinction can change exported alpha.
5. Retain caches and all referenced scene dependencies; reopen the saved source and replay representative frames before delivery.

Specific lessons:

- **Explosion:** preserve the accepted opening impulse and directional trajectories. Separate ignition, flame/cooling and smoke phases; inspect hot-to-dark transitions and orange rims. Declare any procedural ignition core or density microdetail added to real simulation.
- **Smoke:** dense connected rolling and slender wisps are different targets. Evaluate folds over time while retaining mass/readability. A selected window can start already developed and end nonzero; describe it as such rather than a full birth/death cycle or seamless loop.
- **Dust:** show a fast outward impulse, momentum loss, thinning and a low settling veil when requested. Density fading alone does not prove descent. Density centroids can support diagnosis but do not establish physical grain settling. The investigated dust was a gas/concentration approximation with custom downward transport, not calibrated grains, drag or deposited ground dust.
- **Fire loop:** a coherent simulated sequence can need a deliberate head/tail blend. Crossfade in linear light and inspect several cycles for ghosting, overlap and an endpoint pause. Export one cycle without duplicating its terminal first frame. Do not call a one-shot seamless because its player repeats.

An accepted test may retain weak rolling, broad folds, coarse billows or imperfect loop behavior. Keep those limits attached to reuse. Free-only tools and GPU-only execution were constraints of particular investigations; enforce them when the current request requires them rather than inventing universal restrictions.

## ChatGPT image generation and other generated source art

Use the available imagegen skill and built-in tool for requested raster generation/editing. It is useful for source-art exploration, distinctive surface character, stylized accents and visual variants when procedural construction would be needlessly rigid. Preserve the user's reference and requested invariants; do not make them approve every routine tool call.

Prompt for the intended consumer: isolated effect or surface, useful framing and silhouette, desired lighting or unlit appearance, seamless tiling if required, and real transparency when required. Inspect the actual alpha, edges, repeat and appearance; a prompt is not verification. A black background is not an opacity mask.

Do not treat a visually plausible generated normal, roughness, velocity field or PBR set as calibrated data. Prefer deriving data from a shared procedural structure or explicit geometry and validate it. Independently generated frames, or a sheet with plausible frames, need separate temporal and layout validation before use as animation. Cropping and packing cannot repair incoherent motion.

Record prompt, tool/model metadata available, references and selected output hashes. Keep the selected original: seeds or a repeated prompt do not guarantee reproduction across services or revisions. Built-in image generation is distinct from the locally tested SDXL route; local inference timing, GPU evidence and repeatability do not transfer to ChatGPT image generation.

## Hybrids and reusable layered shockwaves

Keep the strengths of each source explicit. Authored structural alpha can remain authoritative while generated RGB adds detail, but matching alpha does not prove physically valid lighting or coherent material channels. The investigated SDXL refinements did not consistently improve smoke or shockwave art; do not add them by default.

The accepted modular shockwave combines:

- Fast irregular broad radial streaks with continuous soft opacity.
- Slower perimeter billows using transformed/crossfaded accepted simulated imagery.
- Fewer, larger irregular soil fragments for weight and mud rather than fine sand.

Keep these layers independently editable and show them alongside the combination. The reusable shockwave is a fresh procedural composition of simulated imagery, procedural cards and mesh fragments; it is not a fresh radial CFD solve.

Investigated fixes matter when the same symptom appears: screen-space dither caused fine speckle, corrected by blended transparency rather than blur; competing animation/lifetime envelopes caused late opacity/brightness rebound, corrected with explicit forward atlas progression and one output fade. Diagnose the actual path before applying either fix elsewhere. An overall monotonic fade is required only when the intended envelope calls for it; local moving forms may legitimately fluctuate.

## Backend failures and scope

Material Maker export failures and limited VFX Texture Lab automation/art results were local observations, not permanent product rankings. Prefer already demonstrated recipes instead of reopening that evaluation. Recheck a tool only when the task needs it and a changed environment/version justifies the effort.

Use deterministic postprocessing for packing, channel extraction and validation where appropriate. Label those stages accurately; Python-generated fallback art must not be presented as Blender, fluid-solver or image-service output. Preserve existing installations and accepted outputs; environment repair is a separate scope decision when it would affect shared tools/models/drivers.
