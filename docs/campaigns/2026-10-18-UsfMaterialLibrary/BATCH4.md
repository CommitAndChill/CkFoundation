# Batch 4: knit, coated leather, ceramic, terrazzo, plastic and cork

Status: batch 4 is implemented locally and technically verified. Six texture-backed looks, live source/control proofs, final captures, fresh focused gate and independent reconciliation pass. User artistic acceptance remains separate. Gallery: `Saved/UsfMaterialGallery/Batch4-Gallery.html`.

## Source and asset contract

All six sets come from ambientCG under [CC0-1.0](https://docs.ambientcg.com/license/). Publisher metadata, author attribution (ambientCG / Lennart Demes), official URLs, physical coverage when specified, original archives, license snapshot and SHA256 receipts are retained in `Plugins/CkTests/Resources/CkUsf/Batch4Textures/Manifest.json`. Source maps are aligned 1024-square PNGs: BaseColor is sRGB; Normal is DirectX tangent space without green-channel inversion; Roughness and TileMask are linear data. Original PNGs are not modified.

| Manifest key / look ID | Official source | Identity and treatment |
|---|---|---|
| knit / LibraryKnit | [Fabric085](https://ambientcg.com/a/Fabric085) | Wool fabric with actual interlocking yarn loops visible in the source detail sheet; native Cloth fuzz. Texture detail alone does not create yarn silhouette geometry. |
| patent_leather / LibraryPatentLeather | [Leather026](https://ambientcg.com/a/Leather026) | Smooth black leather grain under an independently authored glossy ClearCoat response. The coating is not a measured patent-leather scan. |
| porcelain / LibraryPorcelain | [Tiles133A](https://ambientcg.com/a/Tiles133A) | Clean white square tile and grout source. Present as porcelain-finish ceramic; porcelain composition is unverified by the catalog. Source roughness contains subtle authored surface variation. |
| terrazzo / LibraryTerrazzo | [Terrazzo013](https://ambientcg.com/a/Terrazzo013) | Actual multicolor aggregate pattern and aligned surface response, with an adjustable polished finish. |
| translucent_plastic / LibraryTranslucentPlastic | [Plastic010](https://ambientcg.com/a/Plastic010) | Clean plastic color and fine surface finish with independently authored thin transmission. This is not an optical scan or measured thickness/scattering model. |
| cork / LibraryCork | [Cork004](https://ambientcg.com/a/Cork004) | Actual beige cork granules and pores. The source does not certify a particular binder or mechanical density. |

The runtime contract is **19 texture assets plus six generated masters = 25 assets**: BC/Normal/Roughness for every look, plus porcelain TileMask. Source Height/AO channels remain available as provenance and are not additional runtime bindings. The importer follows manifest paths; selected plastic files live in `translucent_plastic_clean/Maps/`. Rejected Plastic013A handling marks and Plastic006 scuffs are retained separately and are not selected runtime sources.

Porcelain TileMask is derived from its actual aligned source color, not an unrelated procedural grid: `c=mean(decoded sRGB RGB)/255`, `t=clamp((c-0.70)/0.10,0,1)`, `mask=round(255*t*t*(3-2*t))`. White is tile/glaze; black is grout. Mean coverage is 0.8834073. The source/detail sheets were inspected for actual knit loops, material identity and tile-mask alignment.

`Tools/UsfMaterials/source_batch4_textures.py` verifies publisher archive size/CRC, immutable cached bytes, source dimensions and recorded hashes. A cached rerun reproduced manifest SHA256 `eb7eff2c02018f9f1b0d0766e81b5ead4f7aee981e5a7197903a6e34cf6e034f`. Independent host checks passed for all 27 recorded source/derived channels, dimensions, DirectX declarations, license hash and mask segmentation. These checks do not establish rendered quality.

## Authored material inputs and defaults

The current defaults come from `Script/CkUsf/CkUsf_Library_Batch4_Assets.as`; behavior comes from the six corresponding `Source/CkUsf/Shaders/CkUsf/Looks/Library*.ush` entrypoints and `LibrarySurface.ush`. All use BaseColorMap, NormalMap and RoughnessMap. UVRotation is in degrees and defaults to 0; DebugView defaults to 0. TintColor defaults to white except plastic. Shared sampling rotates UVs about their center, rotates normal slopes back into the mesh tangent basis, and clamps base roughness to 0.045..1 after scale/bias.

| Look / native model | UVScale / NormalStrength | RoughnessScale / Bias | Specialized defaults and behavior |
|---|---|---|---|
| Knit / Cloth | 2 / 0.7 | 0.85 / 0.04 | FuzzWeight 0.25, FuzzColor (0.12,0.12,0.12); saturating native Cloth mask and subsurface color. |
| Patent leather / ClearCoat | 2.2 / 0.055 | 0.65 / 0.08 | CoatWeight 1, CoatRoughness 0.075; dielectric substrate, coating roughness clamped to 0.045..1. |
| Porcelain / ClearCoat | 1.5 / 0.12 | 0.6 / 0.04 | TileMask adds aligned tile/grout separation. GroutColor (0.13,0.14,0.15), GroutNormalStrength 0.7, GroutRoughness 0.88; GlazeWeight 0.9, GlazeRoughness 0.11. |
| Terrazzo / DefaultLit | 1.4 / 0.15 | 0.65 / 0.08 | AggregateContrast 1, Polish 0.7; color exponent is bounded positive, polish interpolates toward reduced roughness with a 0.09 target floor. |
| Translucent plastic / ThinTranslucent | 2 / 0.08 | 0.85 / 0.12 | TintColor (0.38,0.72,0.9), BodyCoverage **0.65**, TransmissionAmount 1, TransmissionTint (0.65,0.85,0.95), GlassCoverage 1, Ior 1.0. Translucent blend and SurfacePerPixel lighting. |
| Cork / DefaultLit | 1.5 / 0.45 | 0.8 / 0.22 | PoreContrast 1.15 adjusts roughness contrast around 0.65, clamped to 0.3..1; Specular 0.32. |

Porcelain interpolates source normal strength and roughness by TileMask, independently tints grout, and sets `ClearCoat=TileMask*saturate(GlazeWeight)`. Thus coating is excluded from resolved grout. DebugView 5 displays TileMask and 6 displays masked glaze weight. Plastic uses native body opacity, ThinTransmittance, ThinSurfaceCoverage and bounded refraction; it does not enable SceneColor copying or claim blurred background/thickness-traced scattering. All defaults avoid added damage or universal aging.

## Plastic station fill and diagnostic findings

The only new presentation light belongs to the new translucent-plastic station. `CkUsfGym_PlayerController.as` creates a movable point light tagged `Usf.TranslucentBodyFill`: **80 cd**, attenuation radius **300 cm**, source radius **15 cm**, lighting channel **1** only, white and without cast shadows. Its world scale is explicitly one; position is the showcase origin plus (-140,-100,140) cm. This is a bounded station-lighting treatment, with no Batch4 C++ or renderer-configuration change.

The first diagnostic used `set_editor_property`, which triggered construction and destroyed the runtime fill. The corrected diagnostic uses runtime setters. `Saved/UsfMaterialGallery/Batch4-FillProof.json` now records `passed=true`, `restored=true` and `gc_retained=true`. Neighbor on/off mean RGB changes are 0.0005243 for brass and 0.0005936 for terrazzo. This is bounded captured-neighbor and lifetime evidence, not a universal zero-influence claim.

`Saved/UsfMaterialGallery/Batch4-PorcelainProof.json` records `passed=true` and `restored=true` on the same existing porcelain master/MID. A flat plane at inspection UVScale **0.5** resolves grout for the diagnostic; it does not replace the authored default UVScale 1.5. The report identifies 41,488 tile pixels and 2,516 grout pixels, zero disabled-coat maximum, coat-grout maximum 0.0156863, grout-color response 0.166822 and grout-roughness response 0.235599. These establish the scoped masked-control behavior and restoration; artistic acceptance remains separate.

## Source proof and retained harness findings

The final source proof passes with 30 native viewport captures and 14 reloads. Each of
the six entrypoints changes UV sampling scale by 1.37 and a texture-backed output by
0.1; a shared-helper edit affects all six. Existing twenty-four library master/MID
identities and scalar/vector/texture bindings remain unchanged; all seven source files
recover their exact bytes. Patent leather uses its sourced roughness channel because
the black albedo debug image quantized almost entirely to zero. The rejected albedo
baseline and restored-source report remain in `Batch4-BlackPatentBaseline/`.

The first grout diagnostic could not resolve enough fully black grout pixels at the
default scale. Its captures/report remain in `Batch4-UnresolvedGroutBaseline/`; the
successful flat inspection uses UVScale 0.5 and restores the authored 1.5 default.
The initial dark plastic and opaque-body comparisons remain in Saved. A matched opaque
endpoint was dark under the three Rect lights but bright under the diagnostic Point
light. That establishes a lighting-response difference, not the exact underlying Rect
implementation limit. The plastic uses no emissive workaround or SceneColor blur.

## Final evidence and execution limits

Thirty-six final lit native/preview captures are saved and the contact sheet has been visually inspected. All six common control proofs pass (42 frames), exercising normals, UV scale/rotation, roughness and specialized response, with full restoration. Plastic pane controls pass in native and TAA captures: body coverage changes mean RGB by 0.07887/0.07582, transmission by 0.10670/0.10263, while uncovered-reference change stays below 0.00235. These are scoped response checks, not measured volume optics.

`Batch4-Costs.json` records current desktop material statistics:

| Material | Pixel instructions | Vertex instructions | Samplers |
|---|---:|---:|---:|
| Knit | 346 | 296 | 6 |
| Patent leather | 336 | 296 | 6 |
| Porcelain-style tile | 357 | 296 | 7 |
| Terrazzo | 339 | 296 | 6 |
| Translucent plastic | 1896 | 492 | 9 |
| Cork | 334 | 296 | 6 |

All report two UV scalars. These compiler statistics are not GPU timings; the native forward-lit translucent plastic is substantially more expensive than the opaque materials. No performance acceptance or mobile/forward/path-traced coverage is claimed.

Final state restores native AA 0/87%, no preview/diagnostic mode, all debug views zero and the plastic fill at 80 cd. PIE ended, Python+UE GC collected 33, editor PID 49168 quit with normal log closure. Fresh real-RHI `GeneratesUsableMasters` passed **1/1**, zero failed/skipped/contaminated, actual Toolbox exit **0**, duration **54s**. The cached test list avoided a discovery editor; one test editor spawned. No fresh full CkUsf or global Toon reload gate is claimed for this batch.

`Batch4-Verification.json` passes: 25 new assets, 30 source-proof frames, 36 final lit frames, six control results, both specialty proofs, local fill restoration/isolation/GC retention, and **255 protected earlier asset/shader hashes unchanged**. It hashes the recorded maps, sources, assets and linked specialty/fill images. Native Toon sources/materials, approved ball and earlier library assets are protected. The unrelated Iris DataStreamChannel.cpp retains SHA256 `69813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92`.

Actual editor boots matched the **two** planned: one offscreen studio for import/iteration and one fresh final focused gate. No C++ rebuild or engine/renderer setting change. Python AST, gate PowerShell parse and root/Foundation/Tests `git diff --check` passed. Work used the existing checkout, offscreen Toolbox and serialized commands, with no mouse/keyboard/clipboard/focus input. No cook/package, cache/artifact deletion, additional checkout, Git commit or push. Earlier Python/DDC/HighResShot incidents remain retained and are not claimed fixed.
