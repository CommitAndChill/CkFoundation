# Asset contracts and acceptance evidence

Choose checks relevant to the consumer and the requested claim. Do not generate every possible channel or run a full engine suite for a texture-only task.

## Channel and color contract

Record each channel's meaning, color/data interpretation, bit depth, ranges, packing and material decode. Separate visual color, grayscale luminance, opacity, relative height, normals and flow; they are not interchangeable.

- Color may be sRGB-encoded; alpha is linear opacity. Composite and resize with the declared straight/premultiplied convention, using linear-light color where appropriate. Premultiply for filtering/compositing and recover straight color safely if the destination requires it.
- Data exports use Raw/Non-Color interpretation and no display tone mapping. Disable denoising for scalar/vector emission exports; the investigated default denoiser changed encoded vectors and edges. Keep a known-value probe when transfer or signs are uncertain.
- Opacity tone shaping is an explicit artistic operation, separate from color-space interpretation. The investigated `perceptual` curve was a chosen look; do not silently interpret alpha as sRGB or apply the curve again on import.
- Lit smoke can retain baked light direction and shadows. Neutral RGB plus authored opacity serves a different purpose. Choose based on the target lighting response and expose baked limitations.
- Document additive fire separately: RGB emission and alpha extinction/coverage may serve different roles. Do not assume every effect's alpha has identical meaning.
- A higher-bit-depth container or EXR made from an 8-bit source does not restore precision. Record master precision, reduction/filtering and actual decoded precision used by checks.

For flow/distortion, specify coordinate basis, sign, neutral value, decode, units and intended amplitude. One investigated analytic RG field used neutral 0.5 and `RG*2-1`; it was unitless UV advection, not physical fluid velocity. Validate the current consumer rather than imposing this encoding everywhere.

## Alpha and normal contracts

Inspect final textures over black/dark, light and checker backgrounds, including low-alpha edges and target screen size. Check clipping, rectangular borders, unintended baked background, halos, speckle and flicker. Keep useful RGB at transparent edges where the sampling/compression path needs it; inspect actual engine mips rather than assuming a clean PNG proves import quality.

For tangent/card-space normals:

- Define the basis, normal convention and intended lighting model. The accepted billboard basis was image-right/image-up/toward-viewer, DirectX primary and OpenGL reference; verify the current material's decode. Green was already inverted for DirectX, so do not flip twice.
- Treat normal RGB as data, with sRGB off and an appropriate normal-map import/compression path. If alpha is needed, confirm that the chosen platform format retains it; opacity was separate in the investigated recipe.
- Test an interpretable U/V slope or sloped mesh when generating a new basis/export path. Check sign, length within precision-aware tolerance, and normal/opacity alignment.
- Use neutral encoded `(128,128,255)` outside opacity support for the investigated RGB8 card export. For filtered/downsampled overlaps, combine vectors using coverage-aware weighting and renormalize before encoding; do not blend encoded colors as if they were albedo. Account for runtime normal compression/reconstruction.
- Density-relief normals approximate the projected billows/streaks; they do not reconstruct fluid geometry. Soil mesh-face normals have a different provenance. A flattened combined normal approximates the layered result and needs visual lighting review.
- For the accepted Blender normal-pass method, matched white-emission references recover the vector with `normalize(2*Cnormal-Cwhite)` in its declared linear pipeline. Normal/white alpha must agree; do not reuse this formula blindly for an unrelated pass setup.

Lighting previews must decode the final exported textures. CPU Lambert response checks the exported data; Unreal lighting needs actual Unreal evidence. If no normal map is required by the selected material, omit it rather than adding an unvalidated channel.

## Flipbook contract

Record frame count, dimensions, grid, first-frame index and UV origin, chronology, playback rate/duration, one-shot/loop policy, color/alpha convention, and SubUV interpolation policy. Example: 64 frames at 24 fps last 64/24 seconds; 2048x2048 with an 8x8 grid gives 256x256 cells before gutters. Choose these values for the use case, not by precedent.

Validate:

1. Every expected frame exists, is nonempty and has matching dimensions/channels. Check meaningful progression while allowing legitimate repeated/blank frames.
2. Every atlas tile matches the expected decoded source frame after the explicitly recorded conversion/resampling. Verify all tiles, row/column order and orientation; one correct corner does not prove the layout.
3. Opacity extraction matches source alpha, and grayscale color retains the intended alpha. Grayscale RGB luminance is not an opacity substitute.
4. Borders and padding suit the requested motion and sampling path. Fixed camera/framing must not crop an expanding plume or introduce a camera zoom that looks like motion.
5. Animation preview frame count, timing and total duration match the source. GIF centisecond timing/palette merging can differ; retain a fidelity master and an accurate-cadence video where needed.
6. Birth, evolution, decay and the tail match the brief. Do not force zero alpha on every one-shot: some approved windows end with a faint residual. Conversely, intentional fade-to-zero must not rebound or overlay the next pulse.
7. A seamless loop has reviewed head/tail motion over several cycles, no duplicated endpoint pause and no obvious crossfade ghosts. Compare seam change to ordinary adjacent variation; a scalar metric alone cannot approve motion.

## Unreal import, material and lifecycle

Check the actual asset's sRGB, compression, alpha retention, sampler/filtering, mip behavior, texture group and blend/lighting settings. Source PNG re-export equality is distinct from decoded platform mip validation. For atlas mips, use suitable dilation/gutters, mip generation or another demonstrated sampling strategy; an uncompressed no-mip audition is not a production optimization default. Include gutters in the UV layout/packing contract if present.

Confirm SubUV frame progression and endpoint mapping at ordinary playback speed. Verify the relationship among material animation age, particle lifetime, emitter duration and system loop/retrigger policy. An editor timer that restarts a preview is not gameplay looping. DesiredAge capture is not wall-clock cadence evidence. Verify saved defaults through reconstruction/reload.

For materials, inspect useful surfaces and light directions, repeats, grazing angles and intended camera distance. For Niagara, inspect actual screen coverage, layers, bounds/culling and overlapping bursts. Do not change shared framework behavior or protected implementation details just to force a preview to match.

## Runtime performance evidence

Record the scenario and hardware: effect scale/distance, resolution/quality, simultaneous effects, relevant lighting and warm/cold conditions. Measure applicable GPU/CPU timings and memory with the project's supported profiling tools. State whether evidence is on the target or a proxy. Backend render/bake time measures authoring throughput, not game performance.

Inspect translucent screen coverage/overdraw and sorting, material work/samples, particle/emitter counts and update cost, texture format/mips/resident memory, and practical bounds/scalability. Texture memory should reflect actual imported format/mips, not PNG file size. For scale, an uncompressed 2048x2048 RGBA8 base level is 16 MiB before mips; use the actual asset measurements for the final claim.

Tune the bottleneck and compare under equivalent conditions. Preserve useful silhouette and timing. Budget decisions that require a material loss of the accepted look go to the user with visible alternatives and measurements. Without runtime measurements, report design choices and pending profiling rather than 'optimized'.

## Reproduction and evidence handoff

Keep only what the requested deliverable needs, with enough evidence to regenerate and inspect it:

- Editable `.blend`/node graphs, shader sources or generator, versioned recipe/config, seeds/parameters and reproduction entry point.
- Required caches, source imagery and linked dependencies, with resolvable relative relationships where possible. Do not delete caches needed by delivered scenes.
- Backend/add-on/runtime version, actual device where relevant, selected source/output hashes and native execution receipts. Declare procedural, simulated, generated and reused contributions independently.
- Source masters versus game exports, channel/atlas contract, preview media and focused validation results. Reopen saved sources and replay representative frames; compare decoded pixels separately from container bytes when metadata differs.
- The user's exact selected version and approval scope, plus recorded known defects and unverified runtime/performance claims.

Generated source art may only be reproducible as a retained selected original plus prompt/settings. A fresh seed proves a particular variation, not every parameter combination. Match framing/exposure/cadence in comparisons; do not beautify review media without declaring the transformation.

Use short status statements such as: 'asset checks passed; motion approved for this test; runtime not tested; performance not measured.' User acceptance can close a test without falsifying internal failed criteria. A later explicit acceptance supersedes older pending prose for that artifact, while its technical limitations remain.
