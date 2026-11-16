# Texture-generation migration progress

## Commit preparation and later execution evidence

2026-10-20: user authorized separate local commits for this tool package and the CkPlugins preview assets. This section supersedes earlier pending-stage statements while preserving their historical scope. The approved smoke and shockwave frames were packed without regeneration; matching shockwave normal exports and preserved source archives were produced outside the plugin tree. Both variations were subsequently imported into the host project under `Content/TextureStudies/MigratedSmokeShockwave_v1`.

The host preview includes native Niagara playback, transparency comparisons and baked/flat/exported-normal lighting controls. Recorded integration evidence includes a successful incremental Development Editor build, saved-map reload, 32 native playback captures, 48 held-frame directional-light captures, three repeated playback cycles without particle accumulation, and 32 review mesh components retaining NoCollision after reload. Twelve approved atlas input hashes and sixteen earlier texture-study file hashes remained unchanged. These checks establish the reviewed preview setup; user acceptance of its Unreal appearance, packaged gameplay and measured performance remain separate and unverified.

Commit preparation reran six portable processing/device-selection tests and the existing-frame finalization fixture. Runtime installations, simulation caches, editable generated scenes, source archives and rendered review media remain external; no editor module is added. Unrelated cookbook work and earlier host smoke studies remain outside these commits. No push is authorized by this request.

## Execution follow-up: fresh variations, preview only

2026-10-19: user authorized fresh smoke and shockwave motion previews, explicitly before atlas packing or Unreal integration. Seed alone changed: smoke90379 and shockwave80479. All physical/visual controls and framing were retained.

- Smoke completed a new Continuum Flow GPU bake with 65 cache samples, actual selected RX 9070 XT (`gfx1201`), OpenCL platform0/device1, flattened add-on slot1. Native bake time was 12.256 seconds for this run; no throughput guarantee. Both lit and opacity passes rendered16 source frames, shown at8fps/two seconds. Initial retail-name selection failed before output/baking because OpenCL exposed duplicated architecture-name entries. The explicit optional platform selector now rejects ambiguity and verifies both exported descriptor and actual device identity; Astra confirmed the installed add-on's index chain. Six processing/device-selection tests pass.
- Shockwave completed native streaks, soil, billows and preview stages, all exit0;32 frames at16fps/two seconds. Required source-art hashes unchanged. This is a fresh seeded procedural composition using existing simulated billow imagery, not a new radial fluid solve.
- Published GIFs preserve two-second total timing with centisecond quantization; accompanying MP4s independently report16/8fps and32/16fps, both2.000 seconds. Smoke opacity is displayed with neutral RGB solely for review; source masters are unchanged. Shockwave display media retains its declared tint/compositing.
- Neither run contains a new atlas. No normal export/finalization, Unreal import, editor boot or engine build occurred. Samples were visually inspected; continuous motion approval belongs to the user. Smoke retains the selected-window tail; shockwave billboard/debris approximations remain.

Run caches, native frames, scenes, receipts and diagnostic logs remain in the requesting chat's work directory. User-facing previews and verification metadata are in its outputs directory. This follow-up supersedes the earlier migration-only execution limitation for the tested preview paths; full finalization/normal replay, production/runtime and performance evidence remain pending.

## Scope and opening state

2026-10-19: authorized source migration only. Working in the existing CkFoundation checkout on `feature/texture-generation-pipeline`, branched in place from `dev`. The opening dirty cookbook README and untracked cookbook documentation are unrelated and remain preserved. Host TextureStudies assets were already staged/untracked and remain untouched.

## Completed source migration

- Canonical workflow, technique selection, asset contracts and investigation evidence moved to `Tools/CkTextureGeneration`; `ck-vfx-authoring` and the development guide route to them. Particle/material implementation doctrine remains in its existing skill.
- Native procedural Shader Nodes generator and six portable configs extracted with explicit output admission. All 13 function ASTs and the shader/render body after the family assignment match the original; exact PNG decoder AST retained.
- Fresh smoke physical/source/force/shader configuration retained, with explicit runtime/GPU selection, run-relative cache references, preview dependency preflight and optional packing. Original experiments are not runtime dependencies.
- Modular shockwave native layer/normal math retained; bundled source textures total 398,050 bytes and match original hashes. Default execution creates motion review; explicit packing or hash-checked finalization uses the recorded frames. Invalid/changed inputs do not start final renders.
- The global game-vfx entry is a checkout-neutral routing wrapper; superseded global reference copies were removed. Shared-settings cleanup committed as `34ef445`; its earlier `a0d3742` and this commit remain unpushed because the available account cannot fetch the settings repository (404).

## Final verification

- Four portable processing tests pass: PNG8/16 exact tiles/order/opacity, preview-only omission, occupied/mismatched/corrupt-input rejection and original preservation.
- One staged integration fixture passes: preview contains no atlas/normals; recorded source/config/frame mutation and missing frames reject actual finalization before renderer launch; exact existing-frame packaging preserves source hashes; occupied outputs reject. Fixture uses invalid scene markers and never invokes Blender.
- Final smoke/shockwave path/config/dependency preflights pass without creating output. PowerShell files parse; all 18 Python files parse; relative documentation links resolve and portable files contain no absolute author-machine paths or credential patterns.
- One actual Blender 5.2.1 native normal-probe render completed: 64x64 RGBA16 output and editable scene; mean RGB differs from calibrated expected DirectX normal by at most 0.00000463. The restricted environment reported a thumbnail-cache write error; the actual PNG/scene were produced and decoded successfully. No add-on or GPU-fluid execution follows from this CPU Shader Nodes probe.
- A separate native Blender save/reopen fixture verifies scene-relative cache-path resolution, including after the complete fixture run was moved. This uses a path marker, not a real VDB simulation or smoke render.
- Astra's bounded review found late preview dependency checks and absolute saved render-cache paths; both corrected and checked. It found no further actionable issue in the reviewed scope.

The first offline-test invocation failed at managed temporary-directory permissions before processing; final tests used a writable external temp directory. A preflight initially rejected all repository-contained outputs; this was narrowed to protect plugin source while allowing explicit caller work/host output locations. These are execution/plumbing corrections, not changes to accepted visual math.

## Delivery and remaining evidence

CkFoundation changes remain uncommitted on `feature/texture-generation-pipeline`; no project commit or push was requested. No editor/runtime module, C++/AngelScript, Unreal assets, installation, engine build or Unreal boot was added/run. Full migrated fluid bake, full native shockwave execution, new human visual acceptance, Unreal import/runtime and performance measurements remain unverified.

Original accepted assets/caches remain in their original locations. Existing cookbook and host TextureStudies dirt remains outside this change. Generated checks and native fixtures live in the requesting chat's workspace, not the plugin source package.
