# USF material library plan

Native BSDF continuation: GATE_ADVANCED_BSDF.md / ADVANCED_BSDF.md cover the delivered ruby
velour, varnished walnut, resin carbon weave and copper surface studies. PROGRESS.md is the
current evidence/status authority; the batch roster below records the earlier breadth plan.

Revised 2026-10-19 following the user's breadth and texture-library clarification and subsequent approval. Supersedes the industrial-hero-first sequence. Moving evidence belongs in PROGRESS.md; first-batch implementation/limits are in BATCH1.md. Approval is scope approval, not artistic acceptance of later renders.

## Balanced starting roster

Four mixed batches of six materials. Clean, intact defaults throughout. Later choices may change after sourcing and visual review; silk, leather and tiles remain required in batch 1. The user's full catalog is the expansion backlog, not an immediate all-items commitment.

| Batch | Proposed materials | Visual contrast |
| --- | --- | --- |
| 1 | Silk; pebbled leather; glazed ceramic tile/contrasting grout; finished oak; brushed aluminum; matte molded plastic | Directional textile, organic grain, glaze/grout, wood, reflective metal, polymer |
| 2 | Fine cotton/poplin; suede; terracotta floor tile; marble; hard rubber; clear thin glass | Dry weave, nap, porous ceramic, veined stone, soft highlights, transmission. Cotton poplin replaces the proposed heavy canvas after source download restrictions. |
| 3 | Velvet; walnut; granite; brass; frosted glass; carbon fiber | Grazing fuzz, dark wood, mineral grains, colored metal, blurred transmission, composite weave |
| 4 | Knitted fabric; patent leather; porcelain tile; terrazzo; translucent plastic; cork | Yarn relief, glossy coating, dense ceramic, aggregate, scattering/transmission, cellular structure |
| 5 | Denim; concrete; clean brick; satin stainless finish; plywood edge; cardboard | Delivered locally; BATCH5.md records source interpretations and evidence |
| 6 | Linen; smooth leather; glazed mosaic; coated pine; limestone; galvanized steel | Delivered locally; BATCH6.md records selected defaults and evidence |
| 7 | Technical synthetic weave; slate; copper; glossy plastic; candle wax; glass-fiber composite | Approved and in progress; polyester source and approved photo-derived fiberglass explicitly labeled |

Beginning with batch 7, add environment surfaces alongside mixed families. Environment1 compares
the same height-backed gravelly soil using UV-only POM and actual vertex displacement on separate
owned ground patches. Subsequent mixed batches should include grass-covered ground, dirt and mud
with source-appropriate height and wetness; photographed ground and actual grass geometry are
distinct representations. Preserve the individual-material quality priority and optional aging.

The remaining requested fabrics, leathers, ceramics, woods, stone/construction, metals, polymers/rubber, glass/translucent and specialty materials remain candidates for later mixed batches. They require their own visual contracts rather than universal aging presets.

## Batch 1 visual contracts

BC = base color; N = tangent normal; R = roughness. These are input targets; do not claim a source contains maps before inspecting it. Record missing channels and obtain/author justified replacements.

| Material | Defining detail and texture inputs | Specialized live controls/shading |
| --- | --- | --- |
| Silk | Fine directional weave, shallow relief, orientation-dependent lustrous highlights; BC/N/R plus direction data where useful | Thread scale/orientation, anisotropy, normal strength, highlight roughness. Glossy tint or Cloth fuzz alone is insufficient. |
| Pebbled leather | Irregular grain cells, fine pores, correlated color/roughness variation; BC/N/R, optional height/AO | Grain scale/strength, dye tint, roughness range. No mandatory cracks/grime; avoid deep reptile-like cells. |
| Glazed ceramic tile/grout | Smooth glaze with subtle waviness/variation, recessed coarse matte grout; BC/N/R, useful height and tile/grout mask | Tile size/layout, grout width/color/depth, independent normal/roughness and glaze weight. Do not coat grout; inspect layout on a supplemental flat sample as well as the approved ball. |
| Finished oak | Long recognizable oak grain and pores under a restrained finish; BC/N/R, useful height/AO | Grain scale/orientation, tint, pore normal, finish roughness/coat weight. No end-grain claims without corresponding detail. |
| Brushed aluminum | Neutral metal reflectance and fine coherent directional brushing; N/R/direction, BC only for justified variation | Brush scale/direction, anisotropy, roughness, metallic reflectance. Avoid exaggerated grooves, painted lines or mandatory corrosion. |
| Matte molded plastic | Quiet color, fine manufactured stipple, broad dielectric highlights; N/R and suitable BC | Mold texture scale, shallow normal strength, tint, roughness; metallic zero. Avoid chalk-flat diffuse or leather-sized grain. |

All six expose texture sampling, UV scale, normal strength, roughness and color in .ush using existing bindings and ordered parameters. Specialized properties belong to the appropriate material, not a mandatory universal control set.

## Texture sourcing and import

Prefer curated, appropriately licensed PBR sets suited to each material. Candidate CC0 sources verified 2026-10-19: https://polyhaven.com/license and https://docs.ambientcg.com/license/. No specific texture set is selected/downloaded by this plan. Respect download/API terms separately; website preview images are not material maps.

Retain per asset: source/author, asset URL/ID, license URL/text, retrieval date, exact downloaded hashes, channel meanings, resolution, normal convention, physical coverage and transformations/derived-map provenance. Preserve editable textures and reproducible import/packing recipes. Licensing must allow project distribution of texture files, not only rendered images.

Use sRGB color and linear data; correct tangent-normal orientation/compression. Pack only compatible data channels. Verify seams, green-channel orientation, physical UV density, mips and distant aliasing. Begin with justified 1K/2K maps; increase for visible benefit. AO must not double-darken BC. Pure metal/dielectric metallic can be constant. Height does not imply silhouette displacement. Procedural detail can complement sourced maps, but cannot silently replace the requested library with noise.

## Gates and execution

| Gate | Exit observations | Status |
| --- | --- | --- |
| Existing industrial slice | Retain implementation/evidence in INDUSTRIAL.md as optional aging example; no expansion prerequisite | Technical evidence recorded; artistic acceptance unclaimed |
| A: Sources/capabilities | Six source sets with provenance, maps meeting visual contracts, directional silk/aluminum path established in current renderer | Implemented; compatible-light directional proof passes |
| B: Individual surfaces | Six generated looks/gym entries; close, gameplay, grazing and orientation comparisons demonstrate distinct identities | Captured/refined; user artistic review pending |
| C: Live workflow/compatibility | Same-instance .ush sampling/math edits and exact recovery; bindings/overrides preserved; controls demonstrated; cost receipts and real-RHI focused gate including Toon siblings | Live proofs pass; 56/57 full run plus 1/1 isolated retry; native DDC crash retained in evidence |
| D: Batches 2-4 | Repeat sourcing, material-specific visual and live-workflow checks per mixed batch; unresolved visual failures stay open | Batch 2 implemented/technically verified (BATCH2.md), artistic review pending; 3 implemented/technically verified (BATCH3.md), 4 implemented/technically verified (BATCH4.md); user artistic acceptance remains separate. Native viewport source proof passes; HighResShot glass recovery issue retained. |
| D5: Next mixed batch | Six clean contrasting texture-backed materials: denim, concrete, brick, satin stainless finish, plywood edge and cardboard; repeat source/live/control/fresh gate evidence | Implemented/technically verified; BATCH5.md. Live sampling/helper/control proofs pass, fresh generation1/1, protected287 hashes unchanged. User artistic review remains separate. |
| D6: Next mixed batch | Linen, smooth leather, glazed mosaic, coated pine, limestone and galvanized steel; licensed sources, live controls and masked mosaic glaze/grout proof | Implemented/technically verified; BATCH6.md. Final live-source/control/mosaic proofs pass, fresh generation1/1, protected318 hashes unchanged. User artistic review remains separate. |
| E: Selected combinations | A few useful combinations and optional aging after convincing individual surfaces; distinguish region blending, coats and retained slabs | Deferred supporting capability |

1. Source and capability audit -> verify actual licenses/maps and renderer support before material-specific implementation. Record sourcing gaps without dropping mandatory silk or substituting generic noise.
2. Mimic StudioLacquer/Studio declarations and Industrial's proven generator/reload integration for wiring -> verify intended bindings, sample types and parameter order. Their procedural appearance is not the visual template.
3. Implement one inspectable material at a time within the mixed batch -> verify defining features against a source/reference sheet at close/gameplay distance; do not grow metal variants instead of finishing the batch.
4. Validate batch -> verify normal/roughness/color/UV/specialized controls and .ush sampling edits in existing MIDs, exact restoration, matched captures and actual costs. Technical checks and user artistic approval remain distinct.
5. Fresh compatible final gate -> use UnrealToolbox with a stated boot budget, normalize source first, inspect actual results/log/exit. Docs-only planning needs no editor boot or test gate.

## Renderer and workflow boundary

Preserve desktop deferred Substrate Blendable, native Toon, approved logo-free shader-ball, Specimen/CoreBase slots, geometry/UV corrections, studio lighting and open gym presentation. Use supported cloth/coat/transmission responses where they visibly improve the material; do not call legacy-root conversion an arbitrary FrontMaterial slab graph. No Adaptive migration or engine work is implicit.

The approved bounded extension now exposes Anisotropy/Tangent through `_DirectionalShading=false` opt-in, preserving existing graphs' feature usage. Compatible-light rendered comparisons pass; test/reload details and remaining limits are in BATCH1.md. Current Substrate Rect direct lighting ignores anisotropy, so the approved three-Rect presentation is not the directional proof. Retained multislab remains a separate decision, not a prerequisite for this roster.

The user also approved the bounded batch-3 `_SurfaceSceneColor=false` and
`_SceneColorReplacesBackground=false` extension. Only supported translucent surface looks
may opt in; unchanged graphs preserve scene-copy usage. Frost uses a texture-modulated
64-tap screen-space opaque-background blur, with explicit optical/occlusion limits and
material-local fog policy. Native contract/reload tests and actual pane controls are recorded
in BATCH3.md; this does not authorize an engine patch or renderer-mode migration.

Use input-independent inspection; never commandeer mouse, keyboard, clipboard or focus. Preserve Python crash evidence and avoid AS schema reinstancing through active Python-connected PIE. No native-crash-fix claim. No commit/push, cook/package, cache deletion or additional checkout is authorized by this plan.
