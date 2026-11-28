# River pebble reference and orbit inspection

2026-10-19. Delivered locally and technically verified; user visual acceptance remains separate.

The user rejected WoodlandV3 and supplied F:/undefined - Imgur.mp4. Its target is densely packed,
rounded mixed-size pebbles with varied mineral colors and meaningful relief. The reference frame
sheet is Saved/UsfMaterialGallery/ReferenceOrbit/ReferenceSheet.png. Do not present the earlier
woodland art as accepted; all previous sources/assets/galleries are preserved.

Selected source: Poly Haven ganges_river_pebbles by Amal Kumar, CC0. Actual alternatives were
compared: dry_river_pebbles was too angular/brown; clean_pebbles too small/uniform. Five aligned
2K source maps retain publisher bytes, MD5/size/SHA, metadata, license and author-method evidence.
The published216cm scan is shown as a complete authored80cm patch. Normal/height fitting suggests
5.48848cm height amplitude at that presentation width; this is a consistency estimate, not a
calibrated scan-depth claim. Source height remains16-bit; there is no height inferred from color.

The source BC PNG is16-bit with sRGB/gAMA metadata. This engine ImageCore.h94–109 defines16/32-bit
image formats as linear; onlyG8/BGRA8 use gamma-space conversion. The initial native import looked
washed out. A matched native comparison using nearest16-to8-bit quantization of the SAME encoded
RGB, with no gamma transform/resize/grading, restored the stone palette. New BC8 texture/name and
Manifest-runtime-v2 preserve the original16-bit asset/manifest/master backup and all four other
original map identities. Quantization maxencodederror .001953155 is below .5/255. The before/after
captures and trial receipts remain. This is a local import adaptation, not an engine patch.

EnvironmentRiverPebbles .ush supports live texture sampling, normals, color, roughness, UVs,
relative relief,8–96 bounded POM steps(default64), separate WPO and vertexmip(default0). It reuses
the verified80cm256-square-cell non-Nanite grid without resaving it. Two ground stations use this
look; approved shader-ball geometry/presentation and earlier material assets remain unchanged.
The full source normal is used once, avoiding double slope shading. POM changes UV appearance,
not native raster depth/shadows/silhouette. WPO moves actual vertices but leaves collision intact.

One reusable Point inspection light follows the selected studio station. I toggles it, P pauses,
K resets phase; N/B select stations. The original fixed studio rig is default. During inspection,
only selected key/fill/rim become0/15%/0; exact saved intensities return on off/retarget/respawn/EndPlay.
Point is used because LightRendering.cpp1033–1038 rejects anisotropic Rect permutations. Orbit is
20degrees/s, groundradius100cm/height70cm/40cd/sourceRadius6cm/attenuation200cm, ball210/90/120cd/
12cm/260cm. Actual material trials selected these levels. One light and bounded reach avoid adding
simultaneous dynamic shadow lights to every station. No GPU/CPU performance acceptance is claimed.

RiverPebbles-Proof uses32captures/4reloads with same two master/MID pairs, independent texture and
VS source edits, repeated-debug noise measurements, relative rendered recovery and exact source/
state recovery. Initial UV signal at the beauty camera fell below3x measured repeat noise; its
failed/restored receipt/images remain in RiverPebbles-InitialUvNoiseFailure. The numeric UV check
uses a stronger declared oblique inspection pose65cm/30cm; thresholds were not loosened.

OrbitLight-Proof passes:47native captures including36phase samples, actual world-time tick,
pause/reset, cardinal geometry, ground/aluminum/silk retargets, material identity preservation,
exact all-rig restoration and exclusion of actual neighbor bounds from the attenuation sphere.
The preview is encoded4fps/9s from36real Unreal frames:2x nominal speed, phase-sampled rather
than real-time footage or a performance measurement. OrbitLight-LifecycleProof separately passes
actual production respawn, active EndPlay/GC and warm reopen with the default rig restored.
The fresh real-RHI GeneratesUsableMasters gate passes1/1, zero failures/skips/contamination;
fresh log has no fatal/ensure/script/material-compile failures. Offline reconciliation passes
32frame hashes, source/asset provenance,6active new binaries and389protected earlier files.
Its first attempt exposed a verifier schema mismatch: reused-grid preparation intentionally
stores the original binary identity rather than duplicating import settings. The verifier now
matches it exactly against WoodlandPreparation and validates that original full import receipt
and the unchanged protected mesh binary. Runtime sources/assets were unchanged by this correction.
Gallery: Saved/UsfMaterialGallery/RiverPebbles-Gallery.html; preview: OrbitLight-Preview.mp4.

Exactly two fresh editor boots were used: warm offscreen studio and final real-RHI generation
gate. All bridge work is serialized; AS edits occur outside PIE after GC. No C++ rebuild, desktop
input, live Blender, engine/config changes, cook/package, cache deletion, extra checkout or Git
publication. Astra consultation was attempted but unavailable at model capacity.
