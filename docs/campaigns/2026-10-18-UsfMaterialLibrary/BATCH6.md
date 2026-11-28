# Batch 6: linen, smooth leather, mosaic, pine, limestone and galvanized steel

2026-10-19. Status: batch 6 is implemented locally and technically verified. This extends the user's breadth-first mixed-library request. Artistic approval is separate.

## Contract

Six contrasting materials follow the existing .ush -> LookDefinition -> generated master -> showcase MID workflow. BC/DirectX-normal/roughness maps remain sampled in each .ush with live tint, UV scale/rotation, normal strength, roughness and specialized controls. No added dirt, rust or damage. No new C++/renderer extension or presentation light was needed.

Linen uses Cloth fuzz; smooth leather uses a fine-grain dielectric response; mosaic separates native glaze from matte grout with a source-aligned mask; pine uses a restrained ClearCoat finish; limestone uses a rough dielectric response; galvanized steel uses actual zinc-spangle finish textures with authored neutral conductor reflectance. Verified source identities, source-authored variation and non-measured properties are documented below.

Neighbor patterns are LibraryDenim/Cloth, LibraryLeather/dielectric, LibraryPorcelain/masked glaze and grout, LibraryWalnut/ClearCoat, LibraryConcrete/roughness contrast and LibraryStainlessSteel/source conductor variation. LibrarySurface and all previous shaders/assets remain protected. The opening receipt protects **318** earlier asset/shader/definition hashes: SHA256 **149dd082dff8cb7158c3a9e023fdaddbbce6f73e30586709e4a75a4e3120c2ca**.

## Sources and licenses

`Plugins/CkTests/Resources/CkUsf/Batch6Textures/Manifest.json` retains exact official URLs, author/provider attribution, license snapshots, original archives or individual PNGs, source dimensions/coverage, map hashes and transformation receipts. All sources are CC0-1.0: [ambientCG license](https://docs.ambientcg.com/license/), [Poly Haven license](https://polyhaven.com/license), and the official [CGBookcase license statement](https://www.cgbookcase.com/textures). No generic noise replaces the material identity.

| Key / look ID | Official source and author | Actual features and source limits |
|---|---|---|
| linen / LibraryLinen | [rough_linen](https://polyhaven.com/a/rough_linen), colormass photography / Rico Cilliers processing | Actual blue fine linen crosshatch with irregular/slubby threads. The publisher identifies linen; this is not independent chemical certification of flax content. |
| smooth_leather / LibrarySmoothLeather | [Leather038](https://ambientcg.com/a/Leather038), ambientCG / Lennart Demes | Fine smooth orange/brown leather grain, distinct from coarse pebbled leather and the earlier black patent coating. PBRMultiAngle source, without invented grain or added patent response. |
| mosaic / LibraryMosaic | [Tiles071](https://ambientcg.com/a/Tiles071), ambientCG / Lennart Demes | Intact white hexagonal mosaic and aligned grout, PBRProcedural. Local source edge imperfections remain; the glaze mask is a conservative cavity exclusion, not perfect semantic grout classification. |
| pine / LibraryPine | [coated_pine](https://polyhaven.com/a/coated_pine), Charlotte Baglioni scanning / Rico Cilliers processing | Explicitly pine-tagged softwood grain and knots with a coated finish and light source scratches. Not pristine raw timber or an independently certified pine species. |
| limestone / LibraryLimestone | [Tiles139](https://ambientcg.com/a/Tiles139), ambientCG / Lennart Demes | Explicitly limestone-tagged beige/cream fine-stone slabs, PBRProcedural. Subtle source spotting and the slightly-worn catalog tag are retained; no granite substitution or added damage. Geological grade/chemistry is not measured. |
| galvanized_steel / LibraryGalvanizedSteel | [Galvanized Steel 02](https://www.cgbookcase.com/textures/galvanized-steel-02), Dorian Zgraggen / CGBookcase | Actual authored zinc-spangle cells aligned in BC/R/Height and fine finish streaks in Normal. No rust overlay, fabricated spangles, measured coating thickness or alloy certification. |

Selected source data totals **29 channels**: six BC/Normal/Roughness/Height sets, AO for linen/mosaic/pine/galvanized steel, and one derived mosaic TileMask. Runtime binds **19 textures** (six BC/N/R sets plus TileMask) and six masters, for **25 assets**. Unbound source Height/AO remain preserved. The aged Tiles019 candidate was rejected after visible dirty/chipped pieces appeared in its real maps; those originals remain in `mosaic/Maps/` and its archive. Selected Tiles071 and its mask live in `mosaic_clean/Maps/`; all consumers must follow manifest paths.

The original galvanized ZIP endpoint `https://cgbookcase-volume.b-cdn.net/t/GalvanizedSteel02_MR_1K.zip` returned HTTP403 once. The retained official asset page references exact actual map-library PNGs at `https://cgbookcase.b-cdn.net/textures/thumbnails/GalvanizedSteel02_1K/GalvanizedSteel02_1K_<BaseColor|Normal|Roughness|Height|AO>.png`. Despite the upstream directory name, these responses are actual aligned **1024x1024 PNG maps**, not sphere renders or query-downscaled previews. Individual original bytes and SHA256 hashes, official page/license receipts and the download-client reference are retained. No downloaded ZIP or ZIP-integrity claim is made for CGBookcase. The adapter is restricted to these exact selected official URLs and does not broaden the shared source helper's host policy.

## Normal, dimension and transform contract

All six Normal maps are explicitly native **DirectX**: ambientCG NormalDX, Poly Haven `nor_dx`, and CGBookcase's official Available Maps label for its referenced Normal PNG. No unknown convention is relabeled, no green inversion is baked, and import does not flip green. CGBookcase Normal is 8-bit RGBA; the other five source Normal PNGs are 16-bit RGB/RGBA. Pillow decodes RGB/RGBA16 as 8-bit for reported pixel statistics; the original high-precision PNG bytes remain unchanged. BaseColor is sRGB; Roughness, Normal and TileMask are linear data.

Linen originals are **1024x1026**. The manifest explicitly requests runtime-only resize to **1024x1024** for power-of-two mip/streaming compatibility: height scale 1024/1026, approximately 0.195% reduction. This does not overwrite original maps. All other selected maps are 1024-square. No other color/normal/UV transformations or resampling are baked into the publisher source data.

Publisher physical coverage is linen approximately 27.0708x27.1300 cm, pine 74x74 cm, mosaic 45x45 cm and limestone 200x200 cm. Leather reports zero/unspecified coverage; CGBookcase's asset page specifies no coverage. Do not infer centimeters where absent. Source-map and centered-detail sheets were inspected for the actual weave, grain, tile boundaries, knots, fine stone and zinc-spangle identity.

Mosaic TileMask uses actual source AO because absolute Height varies across tilted tile planes and would reject some low tile interiors. For `a=AO/255`, compute `t=clamp((a-.90)/.09,0,1)`, `s=t*t*(3-2*t)`, then `mask=round(255*periodic3x3minimum(s))`. The one-pixel periodic erosion conservatively keeps coating off cavity borders, including local edge imperfections; no procedural hex grid is introduced. Mean mask coverage is **0.8536173**, coverage above half is **0.8545513**, and all source AO at or below0.90 yields exactly zero mask. Source AO SHA256 is `1d89e7801d54c8c2ea706b62b3df099d8e62182d4e5555071aecba81280afc86`; TileMask SHA256 is `5b7609ed0fa82bb85da55ca854cdb1d642d8c5591719574054bb331ddcfe202c`. Canonical runtime binding is `T_LibraryMosaic_TileMask`, sampled with the same UVs as the source maps.

`Tools/UsfMaterials/source_batch6_textures.py` reproduces the selected source/derived outputs through immutable cache checks. A cached rerun matched manifest SHA256 `3f3164a47fe24d609a633fed4072761d02d6c400cc4866171a8633e1acb4957b` exactly. Host checks passed syntax, generator/helper/license hashes, all29 selected map hashes and aligned dimensions, retained originals, native DirectX declarations, and byte-exact reconstruction of the AO-mask formula. These are source integrity checks, not runtime or artistic acceptance.

## Current material defaults

Current authored values are read from `Script/CkUsf/CkUsf_Library_Batch6_Assets.as` and the corresponding six `.ush` entrypoints. Every look exposes BaseColorMap, NormalMap and RoughnessMap, plus TintColor, UVScale, UVRotation, NormalStrength, RoughnessScale, RoughnessBias and DebugView. All UVRotation defaults are0 degrees; all DebugView defaults are0. TintColor defaults to white except galvanized steel. Shared sampling rotates normal slopes back into the mesh tangent basis and clamps scaled/biased base roughness to0.045..1.

| Look / native model | UVScale / NormalStrength | RoughnessScale / Bias | Specialized defaults |
|---|---|---|---|
| Linen / Cloth | 2.3 / 0.45 | 0.85 / 0.08 | FuzzWeight0.12; FuzzColor(0.08,0.07,0.05). |
| Smooth leather / DefaultLit | 2 / 0.25 | 0.85 / 0.08 | GrainContrast1; bounded0.25..3 color exponent; Specular0.5. |
| Mosaic / ClearCoat | 1.8 / 0.12 | 0.6 / 0.04 | TileMask; GroutColor(0.35,0.36,0.35), GroutNormalStrength0.7, GroutRoughness0.88, GlazeWeight0.85, GlazeRoughness0.12. |
| Pine / ClearCoat | 1.6 / 0.3 | 0.75 / 0.08 | FinishWeight0.35; FinishRoughness0.25. |
| Limestone / DefaultLit | 1.6 / 0.45 | 0.85 / 0.1 | PoreContrast1; roughness contrast about0.65 clamped to0.3..1; Specular0.32. |
| Galvanized steel / DefaultLit | 1.5 / 0.025 | 1.2 / 0.15 | TintColor(0.62,0.65,0.7); SpangleContrast0.65; Metallic1. |

Mosaic independently controls grout color, grout normal/roughness and tile glaze. Coating multiplies actual TileMask, leaving excluded cavities matte; inspection modes5/6 expose mask/glaze coverage. Galvanized steel authors neutral conductor reflectance and bounds source color variation using SpangleContrast while preserving actual source cells. Pine uses native ClearCoat over its recorded coated-grain response. These controls do not establish measured optical properties or user artistic approval.

## Verification plan

1. Verify official license/source IDs/maps/normal conventions and mosaic mask alignment -> immutable source manifest, source/detail sheets and reproducible transformations.
2. Check ordered shader/descriptor signatures; import only this batch's 19 textures/six masters outside PIE -> actual approved-ball renders, refine this batch if necessary.
3. Serial source proof -> six sampling/math edits plus shared helper, thirty actual captures/fourteen reloads, same thirty-six library MID/master pairs and bindings, exact source recovery.
4. Dedicated flat mosaic glaze/grout controls plus six common/special controls -> normal/UV/roughness/color and material responses; restore all overrides/lights/renderer settings.
5. Thirty-six native/TAA close/macro/gameplay views, compiler statistics and one fresh real-RHI GeneratesUsableMasters -> independent hashes/evidence/log/actual exit reconciliation.

Actual editor boots matched the two planned: one offscreen studio, then one cached-list fresh gate, without a C++ rebuild. Work stays in the existing checkout with serialized bridge jobs and no desktop input, focus/clipboard control, live Blender, cook/package, cache/artifact deletion, other checkout, commit or push. Earlier Python/DDC/HighResShot incidents are retained, not claimed fixed.


## Retained iteration evidence

The first import stopped before master generation because linen BaseColor failed strict saved-setting verification. The live log records Auto-detected normal map immediately before T_LibraryLinen_BaseColor import; the retained diagnostic shows sRGB false, TC_Default and WorldNormalMap. Engine Texture.cpp validation clears sRGB while normal compression remains active. The original importer set sRGB before compression, so the blue diffuse false positive survived that order. The batch-six importer now sets compression and semantic LOD group before sRGB and validates all saved settings. Retrying reused the owned partial texture and all nineteen imports passed. NPOT runtime resizing was not the cause. Initial failure/diagnostic JSON and studio log remain retained; no engine change or extra boot was needed.

The initial galvanized review at NormalStrength0.2 exaggerated source finish streaks. A matched MID-only0.025 trial retained zinc cells and roughness variation with a more suitable shallow relief. The saved descriptor now defaults to0.025 after ending PIE/GC, then regenerating only batch-six masters. Both candidate images remain in Saved. This refinement does not rewrite source textures or add a wear layer.


A bounded Astra visual review compared close/macro captures with the actual galvanized BC/N/R source. It supported the reduced normal strength but identified faint spangle readability under the narrow highlight. A matched native/TAA trial raised only RoughnessScale0.7 ->1.2, retaining bias0.15 and normal0.025. Irregular cells became visible across the highlight shoulder without new texture or noise. Root selected1.2 and the independent visual review found no further material blocker. The material still reads darkest on the shaded hemisphere; faint source streaks remain. This is internal art review, not user approval.

The complete first candidate's Saved records/captures are retained under `Batch6-LowRoughnessCandidate/`. After adopting the roughness default, PIE ended/GC ran before changing the AS asset default and regenerating only this batch, with no new editor boot. The final mosaic/capture/source/control sequence is rerun against the actual selected defaults; the selected-default records now pass as described below.


## Final technical evidence

The selected defaults passed the complete serial rerun: **30 source-proof captures /14 reloads**, six sampling/math edits plus shared helper, **thirty-six unchanged library master/MID pairs** with preserved bindings/overrides, and exact seven-file source recovery. Six final control proofs pass with **42 recorded frames** and complete restoration. Thirty-six final close/macro/gameplay views distinguish native AA0/87% from temporary TAA2/100%; the regenerated final contact sheet and original views were visually inspected.

The dedicated mosaic proof passes and restores the same existing MID:38,587 resolved tile pixels and2,100 excluded-grout pixels; mean tile coat0.397516, excluded-grout maximum0.0156863, disabled coat maximum0; grout color response0.137491 versus tile0.001633; grout roughness response0.222157 versus tile0.001334. Flat diagnostic UVScale0.5 resolves grout and restores the authored1.8 default. This verifies the recorded mask/controls, not exact chemical composition or perfect semantic segmentation.

| Material | Pixel instructions | Vertex instructions | Samplers |
|---|---:|---:|---:|
| Linen | 346 | 296 | 6 |
| Smooth leather | 337 | 296 | 6 |
| Mosaic | 357 | 296 | 7 |
| Pine | 336 | 296 | 6 |
| Limestone | 334 | 296 | 6 |
| Galvanized steel | 337 | 296 | 6 |

All report two UV scalars. Compiler statistics are not GPU timing/performance acceptance; no mobile, alternative-renderer or geometry-displacement coverage is claimed.

Final live state restores AA0/87%, all debug views0, no preview/diagnostic mode and all material defaults (including galvanized normal0.025, roughness scale1.2/bias0.15). Studio PID59340 ended PIE, Python+UE GC collected33 and quit with normal log closure. The unrelated Iris source retains SHA25669813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92.

Fresh offscreen real-RHI **GeneratesUsableMasters passes1/1**, zero failed/skipped/contaminated, actual Toolbox exit0, duration59s. The cached list spawned one test editor with no discovery/recovery boot. This focused gate is not a new full-module or global native-Toon reload acceptance.

`Batch6-Verification.json` passes: **25 new assets**, all **318 protected earlier hashes unchanged**, all source/control/lit/specialty captures hashed, saved texture color/compression/LOD-group contracts checked, exact source restores and actual fresh gate reconciled. Native Toon, earlier library assets and the approved ball remain protected. Python AST, gate PowerShell parse, gallery local references and root/Foundation/Tests whitespace checks pass. Gallery: `Saved/UsfMaterialGallery/Batch6-Gallery.html`. No desktop input, live Blender, cook/package, cache/artifact deletion, extra checkout, commit or push. Earlier Python/DDC/HighResShot incidents remain retained, not fixed. User artistic approval remains separate.
