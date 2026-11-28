# Texture-backed mixed batch 1

2026-10-19 local implementation. Six independent clean/intact materials, not weathering presets. User approved the revised plan; user artistic acceptance of these renders remains pending. This batch does not complete the 24-material roster.

## Materials and editable inputs

| Look | Licensed texture source | Defining response / extra live controls |
| --- | --- | --- |
| LibrarySilk | [Poly Haven Crepe Satin](https://polyhaven.com/a/crepe_satin) | Fine crepe satin, authored anisotropy strength/orientation, restrained normal; FiberRotation/Anisotropy |
| LibraryLeather | [ambientCG Leather037](https://ambientcg.com/a/Leather037) | Dark brown leather, irregular grain/pores, correlated roughness; PoreContrast |
| LibraryTile | [ambientCG Tiles032](https://ambientcg.com/a/Tiles032) | Green intact subway tile, glaze distinct from coarse matte grout; GroutColor/NormalStrength/Roughness, GlazeWeight/Roughness |
| LibraryOak | [ambientCG Wood049](https://ambientcg.com/a/Wood049) | Recognizable oak grain and pores beneath a restrained finish; FinishWeight/Roughness |
| LibraryAluminum | [ambientCG Metal009](https://ambientcg.com/a/Metal009) machining N/R + [Metal051A](https://ambientcg.com/a/Metal051A) neutral BC | Linear-brushed authored aluminum, metallic=1, coherent tangent; BrushRotation/Anisotropy. Steel source N/R describe machining, not alloy reflectance. |
| LibraryPlastic | [ambientCG Plastic013A](https://ambientcg.com/a/Plastic013A) | Fine authored plastic detail, clean tinted dielectric response; SpecularLevel |

All six expose BaseColorMap/NormalMap/RoughnessMap, TintColor, UVScale/Rotation, NormalStrength, RoughnessScale/Bias and DebugView. Debug 1 is base color, 2 tangent normal, 3 roughness, 4 metallic. Texture sampling lives in each `Looks/Library*.ush`; shared sampling/normal/direction operations live in `LibrarySurface.ush`. Existing binding/math edits live-reload; changing the texture parameter schema or directional opt-in requires deliberate regeneration outside PIE.

No generic noise substitutes for the source maps. Some publishers' authored sets have procedural creation provenance; this is not a claim that every source was photographed/scanned. No wear/dirt/rust is imposed. Plastic's molded finish is an authored shading choice, not certification of a particular polymer or manufacturing process.

## Provenance and import

29 selected source/derived maps have independently checked SHA256 values in CkTests `Resources/CkUsf/Batch1Textures/Manifest.json`. Source generator is host `Tools/UsfMaterials/source_batch1_textures.py`. It pins cached originals, official asset metadata, upstream authors, license snapshots/URLs, map conventions, physical coverage where supplied and derived-map recipes. Both publishers state CC0: [ambientCG license](https://docs.ambientcg.com/license/), [Poly Haven license](https://polyhaven.com/license). Selected maps are 45.1 MB; retained originals/candidates/provenance total 241.6 MB. Repeated cached generation is byte-identical.

Originally 22 runtime textures and six persistent masters were imported/generated. The aluminum refinement adds three versioned textures (25 retained runtime textures total, 22 active bindings). Its active provenance is `AluminumLinear_v2/Receipt.json`, superseding only the original manifest's aluminum machining maps. BC is sRGB; data are linear; tangent normals use BC5/TC_NORMALMAP with recorded DirectX orientation, no green flip. All use wrap/mips. Extra source AO/height maps are retained but not all sampled. Metallic constants are appropriate to pure regions.

Silk originals are 1024x1042 (26.32x26.79 cm coverage), retained unchanged. The texture build uses explicit 1024x1024 runtime resizing for mip/stream eligibility; the import report records source/runtime sizes. This is about a 1.7% vertical resampling, not invented detail. Oak source coverage is 80x80 cm. Other assets with zero/unspecified upstream coverage have artist-selected specimen tiling, not verified physical dimensions.

Silk rotation was checked in the official Blender material: Non-Color strength/rotation feed Principled anisotropy inputs; one normalized rotation is one full turn. Runtime direction encodes cos(2*angle), sin(2*angle) in RG, confidence in B, avoiding angular-wrap averaging. Exact Blender/Unreal absolute lobe-axis parity is not established; FiberRotation is explicit. Aluminum originally used height-derived circular machining; that version is preserved but superseded after visual review. The active finish uses unchanged Metal009 DirectX normal/roughness and a constant horizontal axial direction, with neutral Metal051A BC and independently authored aluminum reflectance. Tile's glaze mask derives from the exact source height, aligned pixel-for-pixel; coat weight is zero on grout.

## Renderer contract and limits

`_DirectionalShading` defaults false. Only opted-in opaque/masked SurfaceLit DefaultLit/ClearCoat looks add general Anisotropy/Tangent outputs; tangent is tangent-space. Appended outputs preserve existing specialized-model indices. Reload rejects directional layout toggles before graph mutation and requests regeneration. Existing non-directional graphs retain their feature usage. Silk/aluminum opt in; the other four do not.

The engine's current Substrate Rect direct integration explicitly disables anisotropy (`SubstrateEvaluation.ush`, `bHasAnisotropy && !bIsRectLight`). Ordinary gym presentation therefore retains the approved three Rect lights, while a normally disabled owned point component enables a separate compatible-light diagnostic. No engine, GBuffer, Adaptive/topology or project-renderer setting change. Coat response uses supported legacy-root conversion; it does not retain arbitrary physical slabs or an independent coat normal. Height does not displace silhouettes. Shader-ball UV distortion and current reflection quality remain visible limits; this batch does not prove fabric drape, glass transmission or later-family capabilities.

## Evidence

Host evidence lives under `Saved/UsfMaterialGallery` unless noted:

- `Batch1Preparation.json`: runtime import sizes/settings/hashes and six generated masters.
- `Batch1-LitCaptures.json`: 18 actual PIE game-viewport HighResShot captures, close/macro/gameplay. Gameplay camera height was corrected to clear neighboring station signs.
- `Batch1-Preview-LitCaptures.json`: 18 ordinary PIE viewport captures with temporary TAA and 100% screen percentage. Baseline project AA=0, screen percentage=87 were restored. No color grading or generated substitute images. `Batch1-Gallery.html` and `Batch1-ContactSheet-TAA.png` organize these separately from baseline captures.
- `Batch1-LiveReloadProof.json`: passed, PID39072, 30 genuine captures; all six entrypoints and transitive shared helper edited/recovered in the same six masters/MIDs. Every scalar/vector/texture binding preserved; seven source files restored byte-exactly and final hashes independently matched. Base-color debug isolates source response from specular/coat lighting. Initial overly strict albedo-variance precheck rejected clean leather before any mutation; corrected to permit nearly uniform BC with detail in normal/roughness, while still requiring actual pixel edit/recovery.
- `Batch1-DirectionalProof.json`: passed under point light 60 cd; no-AA ordinary viewport captures. With normal/roughness variation neutralized, isotropic/axis0/axis90 differ measurably. Mean absolute display-RGB differences: silk 0.01250/0.01337, aluminum 0.18640/0.20889. All diagnostic overrides restored; Rect intensities restored to 6000/3600/3000, point intensity zero.
- `Batch1-Costs.json`: pixel instructions silk331/leather334/tile353/oak336/aluminum332/plastic332; vertex296 each; sampler stats6 except tile7; two UV scalars. These are material API statistics, not GPU timings. Authored fetches: silk5, leather/oak/plastic3, tile/aluminum4. `ListTextures` output is in `Saved/Logs/UsfLibrary-Studio.log`.
- `Batch1-Build.exit.json`: incremental build exit0; `Saved/Logs/UsfLibrary-Build.log` reports succeeded, 70.98s. Astra/independent source review found no production blocker; original Cloth/ThinTranslucent reload coverage was retained alongside directional cases.
- Fresh serial real-RHI CkUsf run: 56/57 passed, 1 editor-death failure, zero skipped/contaminated, exit1, 4m2s. Existing Toon global-lighting reload hit a native RequestPtr assertion in HttpConnectionPool/DDC IO workers (DerivedDataRequestOwner.cpp:105); Toolbox resumed the remaining16 in a second editor. Unchanged isolated fresh retry passed 1/1, exit0, 45s. All57 have passing results across runs, not one uninterrupted green gate. Logs `Saved/Logs/UsfLibrary-Test.log` and `UsfLibrary-ToonRetry.log`; receipts `Batch1-Test.exit.json` and `Batch1-ToonRetry.exit.json`. Both Toon sources remain byte-identical to baseline. Native DDC request-ownership failure root cause is unproven; no engine/cache repair or known-red exemption.

## Input ownership and validation safety

One offscreen studio boot, through UnrealToolbox, uses process-local `UE-CmdLineArgs` and an explicit startup console script. No startup hook, mouse, keyboard, clipboard or focus automation. The studio ended PIE and quit cleanly; explicit Python+UE GC passed before and after PIE shutdown. Defaults/new gym class were edited only outside PIE with proof references released. This is bounded successful validation, not proof that the earlier native Python crash is fixed.

The second planned boot was the fresh focused gate. Its automatic crash recovery and the explained isolated retry increased this batch to four actual editor boots. No cook/package, cache deletion, additional checkout, commit or push. The approved ball, CoreBase assignment, neutral studio lights and native Toon sources are preserved. Tracked baseline content has no diff; the unrelated engine Iris file still matches its opening SHA256.
