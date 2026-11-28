# Bespoke woodland height patch

2026-10-19. User rejected V3 visually after technical delivery. Preserved as a rejected candidate; superseded by the dense river-pebble reference direction. Technical evidence below is retained, not artistic acceptance.

The user rejected generic POM gravel and requested a small diorama: leaves, small flowers,
gravel and bespoke detail, without a tiling requirement. This is the current environment priority.

The selected 80 x 80 cm patch has an irregular clearing, curved photographic serrated leaves
(species unverified), partly buried textured stones, moss fronds, twigs and sparse cream flowers
with yellow centers. Quiet soil separates the clusters. Forms are modeled and projected from
one scene into lighting-free base color, DirectX normals, roughness, actual topmost geometry
height and ray AO. Scalar/vector EXR masters, 16-bit height PNG and editable .blend are retained.
The selected1K proof was inspected before integration. Roughness and physical leaf folds are authored.

Runtime: one EnvironmentWoodland .ush look and two small ground presentations, POM and
real vertex displacement, using the same recorded maps and physical scale. All sampling, UVs,
color, roughness, normal strength, relief, POM steps and vertex mip stay editable through .ush.
Clamp sampling protects this non-repeating composition. Preserve approved shader balls and lighting.

A height field records the topmost surface. It cannot retain leaf undersides, overhangs or isolated
flower gaps. POM changes sampled UVs without silhouette/depth/shadow displacement; a subdivided
mesh permits true vertex movement, while collision remains unchanged. These are acceptance limits,
not grounds for replacing recognizable authored detail with generic noise.

Earlier Environment1/2 generic sources, meshes and failed diagnostics remain recoverable. No engine
patch, renderer mode migration, new checkout, desktop input, cook/package or publication is authorized.

## Selected source and runtime integration

Selected V3 is a non-repeating 80 cm patch with 22 curved, photographic serrated leaves
(species unverified), sparse cream flowers, twigs, embedded stone and moss fronds. Earlier
paper-like V1/V2 drafts, initial V3 beauty and prior polish candidates are preserved.
The leaf atlas is ambientCG LeafSet001 (CC0), soil is Poly Haven gravelly_sand (CC0), and stone
color reuses the licensed Granite002A backing. Leaf green/ochre tints and stone darkening are
authored. Soil uses higher-resolution 4K same-scan BC registered to the retained height/roughness;
all final exported maps are aligned 1K. These are composed source textures, not a new scan.

Final portable .blend packs every file image. Data masters use 32-bit EXR; runtime height uses
16-bit PNG with 46173 retained levels. Independent calibration verifies BC transfer and DirectX
normal signs. Geometry-derived height spans 1.1772474 to 3.6573753 cm, range 2.48012793 cm.
The full normal bake already contains leaf/stone slopes, so the runtime does not add those twice.

EnvironmentWoodland.ush owns bounded gradient-sampled POM and its separate WPO entrypoint.
AS defaults UV1, normal1, actual height range, TileSize80, steps48, vertex HeightMip0; maps clamp
at composition boundaries. The owned non-Nanite 256-square-cell grid has 131072 top triangles,
named CoreBase/Specimen slots and six-centimeter bounds allowance. Earlier generic ground assets
remain preserved. Two gym stations now show the selected woodland modes. Existing three Rect
intensities remain 6000/3600/3000; only these new ground stations aim those Rects closer to the
small horizontal patch. Approved ball presentation and all earlier materials are unchanged.

Warm native/temporary TAA captures and same-MID .ush proofs pass32captures/4reloads. Exact pixel equality
is not claimed: repeated native captures vary slightly. Proof controls record repeats in the same
debug mode and require effects above that measured variance, with relative visual recovery plus
exact source-byte/parameter recovery. This does not change the failed generic endpoint result.


Final independent Woodland-Verification.json passes all32 image hashes and recomputed metrics,
selected map/EXR/packed-source provenance,7 active new asset binaries and all350 protected hashes.
Fresh shared real-RHI GeneratesUsableMasters gate passes1/1 in1m31s, actual exit0. No compile errors,
ensures or script compile errors in that focused gate. Compiler statistics: PS417/VS328 instructions,
8samplers/2UVscalars; not GPU timings. Woodland-Gallery.html includes ungraded native/TAA views,
source maps and the separately labeled Blender beauty. Owned studio/gate processes exited.
The regular height field visibly fills raised leaf edges down to soil at low angles; it does not
recover independent leaf undersides. This prototype does not establish motion or performance acceptance.
