# Batch 3: contrasting texture-backed materials

Status: delivered locally and technically verified 2026-10-19. User artistic review remains pending. User explicitly approved the bounded CkUsf SceneColor generator extension after the first texture-only frosted candidate left opaque background detail sharp. No artistic acceptance is inferred.

## Material roster and source identity

| Material | Texture source | Defining treatment |
|---|---|---|
| Velvet | Poly Haven velour_velvet, CC0 | Native Cloth fuzz over red velour color, DirectX normal and roughness maps |
| Walnut | Poly Haven walnut_veneer_02, CC0 | Recorded veneer grain with warm tint and native ClearCoat finish |
| Granite | ambientCG Granite002A, CC0 | Mineral color/normal/roughness with adjustable polish |
| Brass | ambientCG Metal009, CC0 machining finish | Reused machining maps plus independently authored brass conductor reflectance and directional tangent/anisotropy |
| Frosted glass | ambientCG Plastic010, CC0 matte finish | Reused normal/roughness finish, native reflective interface and optional screen-space opaque-background blur; not an optical glass scan |
| Carbon fiber | ambientCG Fabric004, CC0 authored carbon weave | Actual texture weave under native ClearCoat; dielectric, excluding the upstream all-metallic map |

Exact URLs, author/license receipts, source channel hashes and source candidacy decisions are retained in CkTests/Resources/CkUsf/Batch3Textures/Manifest.json. All 26 recorded channels decoded and verified; selected source maps total 54.17 MB. Runtime uses 17 texture assets. Original velvet sources are 1024x985; the importer retains immutable sources and records its power-of-two runtime resize. Other sources are 1024-square.

All six .ush entrypoints expose texture sampling, UV scale/rotation, normal strength, roughness and color. Specialized controls are FuzzWeight/FuzzColor, GrainContrast/FinishWeight/FinishRoughness, MineralContrast/Polish, FinishVariation/Anisotropy/BrushRotation, TransmissionAmount/GlassCoverage/BlurRadius, and CoatWeight/CoatRoughness respectively. Clean intact defaults; no mandatory wear.

## Bounded renderer extension

_SurfaceSceneColor defaults false. Only Translucent SurfaceLit DefaultLit or ThinTranslucent may opt in. Production generation adds native SceneColor RGB and ScreenPosition.ViewportUV graph inputs, assigning decoded linear RGB to In.SceneColor and coordinates to In.SurfaceScreenUV. Mesh UV is independent. The generated capability define cannot be supplied by author defines. Default-off graphs add none of these inputs/nodes or scene-copy usage.

SurfaceSceneColor.ush uses 64 fixed symmetric Gaussian-weighted disk taps, converts viewport UV to buffer UV, clamps taps to the view bounds, and decodes each HDR tap before averaging. The decoder already removes pre-exposure. Radius defaults to 8 and is clamped to 12 render-buffer pixels; display width follows screen percentage. The frosted look modulates radius with its roughness texture, replaces sharp opaque background with blurred emissive, and retains the native ThinTranslucent reflective interface.

_SceneColorReplacesBackground is a separate opt-in requiring SceneColor. It suppresses a second material-local fog application to copied opaque color. Other SceneColor consumers retain fog defaults. Flag/layout/define/wiring/fog changes require master regeneration; source-only edits remain reloadable. Reload rejects incompatible changes before mutation.

This is a screen-space opaque-scene approximation. It cannot reconstruct offscreen or occluded content, blur later translucent layers, or model thickness-traced scattering. Carbon uses an isotropic clear-coat lobe over recorded cross-tow texture; no per-fiber directional shading is claimed. Material API statistics are not GPU timing evidence.

## Validation ledger

- Initial offscreen studio imported six masters/17textures and captured all six. Initial frost candidate/control retained as Batch3-FrostPane-{Native|TAA}-{Close|Gameplay}-{Candidate|Control}.png and Batch3-Frost-Limit.png. Both controls remained sharp, prompting the explicitly approved extension.
- Static Astra review found no blocking source issue before build; it is not compilation or rendered verification.
- First incremental build failed because the new ensure macro required an explicit CkEnsure.h include. Original log retained as Saved/Logs/UsfLibrary-Batch3Build-MissingInclude.log. Corrected incremental build passed in 79.71s, exit 0.
- Initial 25-tap Cartesian kernel passed source/control response but rendered a visible grid. Retained all 11 frames plus report in Saved/UsfMaterialGallery/Batch3-SparseKernel. Astra reviewed actual captures and recommended the fixed symmetric 64-tap disk, radius 8/maximum 12. Final native/TAA close panes retain 12.7%/11.8% of covered horizontal edge gradient; uncovered mean RGB changes are 0.00073/0.00152. Zero-radius versus bypass mean RGB error 0.00301. Shared-helper edit response 0.04323, recovery error 0.00135; same MID/master, exact source/parameter restoration. Final broader gate pending.
- The first six-entrypoint source proof stopped before editing because a generic brightness threshold rejected dark carbon (mean 0.00644 with visible weave variation). Original six baseline frames/report retained in Batch3-DarkCarbonBaseline. Carbon uses a source-appropriate baseline minimum 0.002; recovery tolerance is now relative and tighter for dark surfaces, preventing blank recovery from passing. This was a harness correction, not a production shading fix.

All work uses the existing checkout, offscreen Toolbox and serialized JSON/Python commands without mouse, keyboard, clipboard or focus automation. No cook/package, cache deletion, engine/config edit, commit or push. Earlier batch evidence and known Python/DDC/HighResShot incidents remain unchanged and are not claimed fixed.

## Final verified evidence

- Batch3-LitCaptures.json: 36 ungraded native Shot close/macro/gameplay views, separate native AA0/87% and temporary TAA2/100%. Exact final AS defaults, including finer diagonal carbon weave and warm walnut.
- Batch3-LiveReloadProof.json: passed in PID 38304; 30 captures/14 reloads. Six entrypoint edits change sampling UV scale and color math; the shared inspection edit affects all six. Same eighteen library masters/MIDs, all scalar/vector/texture bindings preserved, seven byte-exact source restorations. Frost's dedicated helper proof additionally verifies lit SceneColor sampling.
- Batch3-FrostProof.json: passed/restored; 12 actual pane frames, quantitative controls and existing frost MID/master retained. Batch3-FrostEvidence.json binds final image hashes and source identities for gallery/verifier.

- Batch3-ControlProof.json: all six pass, 42 frames; normal strength, UV rotation/scale, roughness and specialized controls respond. Brass's anisotropy pair uses the compatible diagnostic point light; studio lighting and all overrides are restored. Normal inspection uses strength 2 to make subtle maps inspectable, not as the clean default.
- Batch3-Costs.json: pixel/vertex instructions velvet 346/296, walnut 341/296, granite 339/296, brass 334/296, frosted glass 2509/296, carbon 336/296. Six material samplers for the opaque looks, eight for frost; two UV scalars. Authored fetches: 3 per opaque look, 2 finish maps plus 64 SceneColor taps and potentially the declared center sample for frost. Frost is materially more expensive by these shader statistics; no GPU timings or performance acceptance are claimed.
- Batch3-FinalLiveState.json: AA 0/screen percentage 87, preview disabled, diagnostic light restored, all DebugView 0. PIE ended, explicit GC collected 33, and editor PID 38304 quit cleanly with zero remaining shader jobs.

- Batch3-Gate.exit.json / Saved/Logs/UsfLibrary-Batch3.log: fresh offscreen real-RHI CkUsf gate passes 60/60, zero failed/skipped/contaminated, actual exit 0, duration 5m22s. Includes SurfaceSceneColorContract and SurfaceSceneColorRejectsUnsupportedAndChangedLayouts, generation, existing-MID reload, model declaration/compilation and native Toon authoring/global-lighting reload siblings. The two new tests cover both supported models, default-off scene-copy usage, replacement fog isolation, unsupported authoring/reserved defines and atomic rejection of both toggle directions without changing existing graph/MID state.
- Batch3-Verification.json: independent reconciliation passes; 23 new assets (17 textures/six masters), all 26 recorded source maps, 30 final source-proof images, controls and required frost source/capture identities. All 207 protected earlier asset/shader hashes are unchanged. No final ensure, script/material compile failure or fatal error was found. Unrelated engine Iris source retains SHA256 69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92.

The current gate is uninterrupted green. Historical Python, DDC and HighResShot incidents remain separately retained; no native crash/readiness fix is claimed. Final gallery is Saved/UsfMaterialGallery/Batch3-Gallery.html, with Batch3-ContactSheet-TAA.png and linked pane comparisons. Artistic acceptance is the user's. Actual editor-boot budget is five: initial studio; post-build studio; headless discovery; renderer discovery; focused test execution. Separate discovery processes were learned from Toolbox's run log and communicated when observed. No additional C++ rebuild was needed for the kernel/default refinements.
