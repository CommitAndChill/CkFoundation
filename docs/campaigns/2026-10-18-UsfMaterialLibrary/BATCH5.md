# Batch 5: denim, concrete, brick, stainless finish, plywood and cardboard

2026-10-19. Status: batch 5 is implemented locally and technically verified. This extends the approved breadth-first library. Artistic acceptance remains the user's.

## Contract and approach

Six clean, contrasting materials follow batch 4's LookDefinition -> generated master -> library showcase MID path. Every look retains texture-backed color, DirectX normal and roughness sampling in its own .ush, with live tint, UV scale/rotation, normal strength and roughness controls. No wear/dirt is imposed. No C++ or renderer-mode change was needed.

| Look | Defining feature | Native response / special controls |
|---|---|---|
| LibraryDenim | Actual diagonal cotton twill | Cloth; FuzzWeight/FuzzColor |
| LibraryConcrete | Smooth cast mineral surface and small pores | DefaultLit; pore roughness contrast |
| LibraryBrick | Clean fired brick and aligned source mortar | DefaultLit; firing/color variation, UVAspect |
| LibraryStainlessSteel | Neutral conductor reflectance and fine source microfinish | DefaultLit metallic; bounded color variation, polish |
| LibraryPlywood | Alternating exposed veneer edge layers | ClearCoat; optional finish weight/roughness |
| LibraryCardboard | Unprinted paper fibers and corrugated relief | DefaultLit; fiber color contrast, low dielectric specular |

Stainless alloy is not measured or certified by the finish source. Plywood is an edge-layer material, not a veneer-face substitute. Brick's source aspect is 2:1 (1024x512); UVAspect repeats V twice by default. No height-map silhouette displacement or independent mortar segmentation is claimed.

The runtime source contract is 18 textures + 6 masters = 24 new assets. All earlier 287 protected asset/shader/definition hashes were checked before this slice; opening receipt SHA256 af1a98ed4bb6cb0b1250ccc7218d9113c22f56fd4b32e3ca43adfbadfce609a5.

## Sources, licensing and source limits

All selected assets are from ambientCG under [CC0-1.0](https://docs.ambientcg.com/license/). `Plugins/CkTests/Resources/CkUsf/Batch5Textures/Manifest.json` retains the official asset URLs, publisher attribution (ambientCG / Lennart Demes), creation methods, release dates, license snapshot, original archive and metadata hashes, map hashes, decoded statistics and transform provenance. Selected maps are byte-identical publisher PNGs: no resampling, source recoloring, invented grain or baked UV transformations. No asset is substituted with generic noise.

| Manifest key / look ID | Official asset | Source identity and dimensions |
|---|---|---|
| denim / LibraryDenim | [Fabric069](https://ambientcg.com/a/Fabric069) | Actual blue jeans fabric; the source detail sheet shows diagonal twill threads. 1024x1024, PBRMultiAngle. No geometric thread silhouette is implied. |
| concrete / LibraryConcrete | [Concrete048](https://ambientcg.com/a/Concrete048) | Actual clean smooth concrete with formed surface texture and small pores; no added damage. 1024x1024, PBRPhotogrammetry. |
| brick / LibraryBrick | [Bricks104](https://ambientcg.com/a/Bricks104) | Actual clean red/orange brick and aligned mortar. 1024x512, PBRPhotogrammetry. Both dimensions are power-of-two; preserve the native 2:1 source aspect. |
| stainless_steel / LibraryStainlessSteel | [Metal049A](https://ambientcg.com/a/Metal049A) | Clean neutral silver-colored bare-metal finish, 1024x1024, PBRProcedural. Stainless composition is unverified; neutral conductor reflectance and satin roughness are authored separately. |
| plywood / LibraryPlywood | [Wood089](https://ambientcg.com/a/Wood089) | Actual authored layered plywood SIDE/edge, 1024x1024, PBRProcedural. Not the outer veneer face, and no certified ply count/species/adhesive claim. |
| cardboard / LibraryCardboard | [Cardboard002](https://ambientcg.com/a/Cardboard002) | Clean unprinted brown cardboard fibers with aligned corrugated surface relief. 1024x1024, PBRProcedural. Not a cut-edge cross-section. |

The immutable receipt records **26 source channels**: six BC/Normal/Roughness/Height sets plus AO for concrete and brick. Only **18 BC/Normal/Roughness maps** are runtime bindings. BaseColor is sRGB; normal is DirectX tangent space without green-channel inversion; roughness is linear data. Texture names are `T_<LookID>_<BaseColor|Normal|Roughness>`, and map paths are `<manifest key>/Maps/<channel>.png` under Batch5Textures. Brick has no fabricated mortar mask. Height and AO remain retained source data, without height-map silhouette displacement.

All six assets report physical dimension X/Y/Z as zero/unspecified. In particular, brick's 2:1 pixel aspect does not establish a measured wall coverage. No centimeter coverage or material strength is invented. Source overview/detail sheets were inspected for weave, intact material identity, aligned mortar, plywood layers and unprinted cardboard.

`Tools/UsfMaterials/source_batch5_textures.py` uses the existing Batch2 checked source/image helpers. Publisher archive size/CRC, all six archive/metadata hashes, 26 map hashes/aligned dimensions, normal conventions, license and generator/helper hashes passed host checks. Cached regeneration reproduced manifest SHA256 `15ba62e75ac7f35da034050bc2744dd97d25dd74aeb160e8e5bfd514e17adf6f` exactly. This is source integrity evidence, not rendered acceptance.

Metal049A normal is not literally constant but is effectively flat at ordinary strength. A read-only native RGB16 PNG decode (Pillow truncates RGB16 to RGB8) measured median/99th-percentile/maximum XY slope 0.0001313 / 0.0005343 / 0.0014185, corresponding to a 99th-percentile angle of 0.0306 degrees. XY RMS is 0.0001854 at one texel and 0.0000726 over a 4x4 footprint. Only 1.365% of pixels exceed slope 0.0005; none exceed 0.002. Thus visible finish primarily comes from the authored conductor/satin response and faint source color/roughness variation, not strong source normal relief. Source roughness mean is 0.0338988, standard deviation 0.0057879, range 0.0156863..0.0627451; BC RGB standard deviations are 0.00521/0.00593/0.00698. No source replacement or normal amplification is inferred from this measurement.

## Current material inputs and defaults

Defaults below are read from `Script/CkUsf/CkUsf_Library_Batch5_Assets.as`; shader response is owned by the corresponding six `Source/CkUsf/Shaders/CkUsf/Looks/Library*.ush` files. Every look takes BaseColorMap, NormalMap and RoughnessMap. All default UVRotation values are 0 degrees and DebugView values are 0. TintColor is white for all except stainless steel. Shared sampling rotates UVs about the center, transforms sampled normal slopes back into the mesh tangent basis, and clamps scaled/biased roughness to 0.045..1.

| Look / model | UVScale / NormalStrength | RoughnessScale / Bias | Specialized defaults |
|---|---|---|---|
| Denim / Cloth | 2.4 / 0.45 | 0.85 / 0.08 | FuzzWeight 0.16; FuzzColor (0.07,0.09,0.13). |
| Concrete / DefaultLit | 1.8 / 0.45 | 0.85 / 0.12 | PoreContrast 1; roughness contrast around 0.7, clamped to 0.3..1; Specular 0.32. |
| Brick / DefaultLit | 1.2 / 0.55 | 0.85 / 0.12 | UVAspect 2; FiringVariation 1; source-color exponent bounded to 0.25..3; Specular 0.3. |
| Stainless steel / DefaultLit | 2 / 0.12 | **0.75 / 0.28** | TintColor (0.56,0.57,0.58), MetalColorVariation 0.18, **Polish 0.35**; Metallic 1. |
| Plywood / ClearCoat | 1.6 / 0.3 | 0.8 / 0.08 | FinishWeight 0.25; FinishRoughness 0.3. |
| Cardboard / DefaultLit | 2 / 0.3 | 0.85 / 0.16 | FiberContrast 1; source-color exponent bounded to 0.25..3; Specular 0.2. |

The steel shader bounds source color variation around independently authored TintColor and computes `roughness=lerp(base,max(0.12,base*0.45),saturate(Polish))`. With the actual source roughness distribution and current defaults, the nominal mean output roughness is approximately 0.247 before renderer effects. This provides a satin finish despite the low-roughness source. It is not a measured stainless BRDF or a rendered-performance/quality claim. Brick applies UVAspect to V after the shared UV transform, keeping its actual source mortar layout; it has no independent mortar control. Clean intact defaults do not impose universal wear.

## Verification plan

1. Source/map and ordered HLSL/descriptor validation -> aligned licensed maps and compile-ready bindings.
2. Offscreen import and representative renders -> visually distinct source details under the approved ball/rig, refine only this batch.
3. Serial live source proof -> six sampling/math edits + shared dependency, same thirty library master/MIDs and bindings, exact source recovery.
4. Serial common/special controls and 36 native/TAA close/macro/gameplay views -> restored renderer/material/light state, recorded compiler costs.
5. Fresh real-RHI GeneratesUsableMasters -> actual exit/log/result and independent evidence/hash reconciliation.

Actual editor boots matched the two planned: one offscreen studio and one cached-list fresh focused gate. No C++ rebuild, desktop input/focus/clipboard, live Blender, additional checkout, cache deletion, cook/package or Git delivery. Previous Python/DDC/HighResShot incidents remain retained; no fix is claimed.

## Runtime evidence

The offscreen studio used UE5.7.4 with Substrate enabled and Blendable GBuffer 0. All six imports preserve BC sRGB, linear data, DirectX normals, wrap addressing and generated mips; brick remains 1024x512. Eighteen textures and six masters were generated without regenerating prior library assets. All six native review frames and the final TAA contact sheet were visually inspected. The original approved ball, core/base slots and three-Rect rig remain in use; no new presentation light was added.

The source proof passes with **30 actual native viewport captures / 14 reloads**: six entrypoints change texture sampling UV scale by 1.37 and color math by 0.1, then the shared helper changes all six. All **thirty library master/MID pairs**, scalar/vector overrides and texture bindings remain unchanged. All seven edited source files recover exact bytes. All six original base-color baselines were measurable; no dark-baseline workaround was needed.

Six control proofs pass with **42 frames**. Normal strength, UV rotation/scale, roughness and each specialized response change the rendered result, with full restoration. The normal diagnostic uses strength 2 rather than claiming default normal detail is equally strong across materials; the steel source's near-flat normal remains a documented limitation. Thirty-six final lit views separate native AA0/87% from temporary TAA2/100%, with close/macro/gameplay coverage for each material.

| Material | Pixel instructions | Vertex instructions | Samplers |
|---|---:|---:|---:|
| Denim | 346 | 296 | 6 |
| Concrete | 334 | 296 | 6 |
| Brick | 338 | 296 | 6 |
| Stainless-style metal | 339 | 296 | 6 |
| Plywood edge | 336 | 296 | 6 |
| Cardboard | 337 | 296 | 6 |

All report two UV scalars. These compiler statistics are not GPU timings or performance acceptance. No mobile, alternative renderer or thickness/geometry claim is made.

Final live state restores AA0/87%, all debug views zero, no temporary preview/diagnostic light, and all prior library bindings/settings. Studio PID50868 ended PIE, Python+UE GC collected 33, and the editor quit with normal log closure. The unrelated Iris DataStreamChannel.cpp retains SHA256 69813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92. The fresh real-RHI `GeneratesUsableMasters` gate passed **1/1**, zero failed/skipped/contaminated, actual Toolbox exit **0**, duration **1m5s**. The cached test list required no discovery editor and one test editor spawned. This is a focused generation gate, not a new full CkUsf or global Toon reload gate.

`Batch5-Verification.json` passes: **24 new assets**, all **287 protected earlier hashes unchanged**, thirty same-object library MID/master pairs, 30 source-proof images, 42 control images and 36 final lit views. All recorded source maps, source restores, assets, captures, defaults and actual gate results reconcile. Python AST, PowerShell parse, gallery local links and root/Foundation/Tests whitespace checks pass. Gallery: `Saved/UsfMaterialGallery/Batch5-Gallery.html`. Native Toon, the approved ball and earlier library assets remain protected. No desktop input, live Blender, cook/package, cache deletion, extra checkout, commit or push. Earlier Python/DDC/HighResShot incidents remain unclaimed as fixes. User artistic approval and measured GPU performance remain separate.

Research follows current LibraryKnit's Cloth response, LibraryCork's rough dielectric recipe, LibraryWalnut's ClearCoat finish, the LibrarySurface helper, CkUsf_Library_Batch4_Assets.as, PlayerController station/spawn wiring, and existing source/import/capture/reload tools. Ordered descriptor/HLSL signatures, native import settings and current UE5.7.4/Substrate Blendable state are checked in the actual pipeline.
