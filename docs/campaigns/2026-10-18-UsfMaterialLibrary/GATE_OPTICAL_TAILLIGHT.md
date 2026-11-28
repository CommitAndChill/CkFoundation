# Optical tail-light showcase

2026-10-21. Status: first Unreal prototype ready for artistic review. User approved the tail-light concept after
reviewing a generated optical-material concept sheet. That sheet is an artistic target,
not an Unreal render or a promise of path-traced optical fidelity.

## Scope

One unbranded automotive lamp with an independently modeled red molded lens, recessed
metal reflectors, housing, and switchable LED emitters. Texture-backed sampling and all
numeric optical/material controls remain in CkUsf `.ush`. Preserve the existing gym,
Blendable launch, Adaptive pair, unrelated dirty work, and original project settings.
No engine changes, publication, alternate checkout, or desktop input.

## Research and approach

Neighbor: the existing native Adaptive graph generator and `adaptive_bsdf_session.py`
offscreen studio. Existing AdaptiveVertical/Mix contracts are opaque. Native optical
transmission needs a separate opt-in graph, not a disguised opaque layer stack.
Inspect public engine Slab, TransmittanceToMFP, SurfaceThickness, and refraction settings.
Consult Astra before selecting the optical contract. Surface reflection and interior
depth are separate contributions; interior geometry is not claimed as new Substrate work.

1. Add the smallest native translucent Slab contract and production-path validation.
   Verify public native pins/settings, malformed authoring rejection and strict reload
   topology, alongside existing opaque/profile compatibility.
2. Author reproducible lens/reflector/housing meshes and optical texture maps with source
   receipts. Verify mesh bounds, winding/UVs, texture channel settings and asset ownership.
3. Capture a transient offscreen lamp with LED-off/on states, lens isolation/interior
   inspection, alternate views, and moving light. Inspect actual images before expanding.
4. Change lens `.ush` optics and a shared helper in the same editor, preserve master/MID
   identity, restore exact source bytes and verify rendered recovery. Record limitations.

## Verification budget

Planned editor processes: one offscreen iterative studio after incremental C++ build,
one fresh Adaptive focused gate, and one fresh Blendable compatibility gate. Discovery
may require a separate Toolbox-owned editor; record actual boots and explain additional
iteration boots before launching them. No pre-change rerun of the already recorded
82/82 Blendable and6/6 Adaptive focused baseline merely to repeat it.

## Acceptance

Actual Unreal images with legible outer-lens reflection and visible recessed interior;
LED state changes; `.ush` source edit/recovery evidence; focused fresh test results;
honest raster/refraction and artistic limits. User performs artistic acceptance.

## Selected implementation and observed pivots

The appended enum6 is `NativeTranslucentSlab`. It uses the public Slab and
TransmittanceToMFP expressions, SimpleVolume, colored transmission, per-pixel forward
surface lighting, thin-surface wall thickness and tangent normals. Calibration thickness
is separate from the specimen wall thickness. IOR computes dielectric F0 in source.

The first IndexOfRefractionFromF0 experiment visibly dragged foreground housing/floor
across the lens. A matched Refraction-show-flag off/on comparison isolated distortion
while retaining the specular response. Replaced it with the engine's native Pixel Normal
Offset mode. Its root strength uses1 as neutral; `.ush` supplies1+DistortionStrength,
clamped1..1.1 by the generator. It affects pixel-normal deviation relative to vertex normal;
it does not trace the curved sheet or solve entry/exit optics. Validation records the exact
root connection, mode, MFP calibration, wall thickness and remaining native topology.

Manual SceneCapture does not register a normal player streaming view in this source path.
Timed residency is requested for only eight actual lamp textures on each capture and cleared
on owned-editor quit. `InvestigateTexture T_TailLamp_LensNormal` confirmed2048x1024/12mips.
The rendered fine prismatic breakup became visible after this change. No global streaming
switch or persistent texture setting was changed. The source maps remain2048x1024 originals.

The public `MaterialEditingLibrary.get_statistics(master)` submits/finishes missing lazy
shader permutations before the read-only compiled-Substrate query. Merely drawing the
required shader subset did not establish full-map completeness. The strict proof check was
retained. The active lens then reports a complete map and one closure.

Original authored resources: TailLampV1/Manifest.json SHA256
`38b380f1792d22a70e5e326f1d6c34c2ac05a29a9ec0cb2384d419a8094cbb83`.
Seven mesh groups total15166triangles; bounds24x10x4.5cm; zero degenerate triangles;
single outward-facing lens. Packed `.blend`, deterministic generator, maps, OBJ/MTL sources
and explicitly labeled Blender geometry previews are retained. Blender previews use a
transparent mix for source lookdev and are not native Unreal optical evidence.

Two incremental Toolbox builds passed (66.63s initial,11.37s PNO revision). Six owned
studio boots were needed: initial parameter-collision correction, refraction diagnosis,
matched refraction isolation, PNO revision, targeted texture residency, and full-map proof
barrier. They exceeded the initial studio estimate for distinct observed reasons; no desktop
input or user-owned process was used. The unfinished reflector's `Metallic` parameter was
renamed `MetalnessLevel` because the generated output reserves Metallic.

Failed proof reports are retained and are not acceptance evidence. Initial thickness checks
incorrectly reused a baseline from before camera moves and selected a red-only metric for
thin transmission whose dominant response recovers G/B. Independent review changed both
thickness endpoints to a predeclared three-baseline/three-endpoint/three-recovered fixed RGB
mean with within-state temporal-range threshold max(.001,3*range). It proves directional
display response, not a quantitative Beer-Lambert fit from tone-mapped PNGs. Final proof
filenames are run-unique and hash pinned. The IOR endpoint uses a matched broad-fill source
to make dielectric specular visible, then restores the normal studio source radius.

## Final evidence

A third incremental Toolbox build corrected a test fixture's self-referential TArray Add;
production code was unchanged by that correction. The fresh Adaptive real-RHI gate passed
4/4 in46s. The fresh normal Blendable real-RHI CkUsf gate passed86/86 in3m17s, with zero
failures, skips or contamination and known-red filtering disabled. Final logs are retained
as `Saved/UsfMaterialGallery/TailLamp-FinalAdaptiveGate-Passed.log` and
`TailLamp-FinalBlendableGate-Passed.log`. These are focused gates, not a repository-wide claim.

`TailLamp-RenderedProof.json` passes with exact entry/helper source restoration and no
cleanup errors. It verifies native compiled-resource completeness, preserved master/MID
identity, thickness, roughness, absorption, IOR, normal and distortion controls, LED states,
and entry/shared-helper source edits and recovery. The IOR comparison uses matched broad
fill; the separate LED assay uses matched low-light conditions. Display response is not
calibrated photometry. A strict same-editor/source/rig/identity/hash checkpoint reuses the
successful control frames from f1d0534fd188; fresh LED/source/motion captures are from
6a53195211a7. The failed bright-studio LED metric remains retained and is not acceptance.

`Optical-TailLamp-Gallery.html` contains actual Unreal frames, interior inspection, matched
controls and two light/camera orbit videos. Artifact inspection checks96 unique capture
files,41 local references, exact restored source hashes and full video decoding. Browser
layout remains unverified: local-file navigation was blocked by browser security policy;
no workaround was attempted. PNG pixels were inspected directly.

The owned studio and gate processes have exited. DefaultEngine.ini retains SHA256
`20D012DFF0397DA2198FB8AF0E7B19DADE2F8F204D10C54340F80C5EEB579438`.
No engine edits, default-profile migration, publication or user input were involved.
This is a first optical prototype below the generated concept's modeling/photographic
polish. Pixel Normal Offset remains screen-space, with foreground/offscreen limitations;
no traced interfaces, caustics or performance claim. Artistic acceptance is pending.
