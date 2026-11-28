# Amber inclusion specimen

2026-10-21. Ready for artistic review. User approved the refined tail-light and asked to continue.
Next contrasting study: polished amber cabochon with actual botanical inclusions.

The neighbor is the existing NativeTranslucentSlab optical path, preparation ownership
receipts and offscreen source/capture bridge. This slice adds authored geometry, textures,
definitions and `.ush` sampling/optics without new C++, engine changes or renderer migration.
Existing tail-light and all earlier gym work remain preserved. No publication or desktop input.

Plan:

1. Original source:11x8x2.7cm asymmetric convex cap, actual paired fern leaflets, twig and
   separate seed wing. Five named-role OBJ/MTL groups and seven original maps, generated
   deterministically with editable Python. Check zero degenerates, +X cap winding, UVs,
   dimensions, source hashes, correct linear/sRGB and normal convention.
2. Native cap uses independent reference and specimen thickness, IOR-derived F0, color
   absorption and weak PNO. Texture sampling, tint, normals, roughness, UV and optical
   properties stay `.ush` editable. Opaque inclusions are two-sided and texture-backed.
3. One iterative offscreen studio and one fresh final validation boot, existing binaries.
   Inspect actual Unreal front/oblique/macro and cap-hidden/inclusion-hidden views; compare
   thickness/roughness, source entry/shared-helper changes and exact recovery. Record
   stable captures before comparison, object identity, compiled closure and prior-file hashes.
4. Separate gallery with light and restricted camera sweep. Actual pixels and decoded clips,
   not concept renders. The user performs artistic acceptance.

Representation limit: one convex exterior optical sheet over a hidden opaque rear support.
The dark twig and pale seed occupy different geometric depths. Acceptance is front +/-30deg;
no rear optical correctness, traced solid interfaces, bubbles, caustics or photometry claim.

## Final evidence

Original source manifest SHA25631d45195d2182d9873885058c394c18bb3a832f44314c5f215c55f6b8e943fa0.
Five roles total16480triangles, zero degenerates. Body11360 triangles all face+X;
Backing480, Botanical3456, Seed384, Stem800. Seven decoded maps have pinned container and
pixel hashes. Amber maps2048x2048 (normal, roughness, R16 thickness, linear near-unity tint);
botanical maps1024x1024 (sRGB base color, normal, roughness). Generator and17 source artifact
hashes independently checked. Thin botanical geometry and rear-facing backing are two-sided.
The generator/OBJ sources are editable; no Blender installation or external texture license
is required. These are authored textures, not scans or a traced botanical fossil.

Two owned Toolbox studios used existing binaries, no build. First actual render was too
yellow with large white light-source reflections. Selected linear reference transmission
(.97,.70,.32),EV4.8, source radii.8/1.2/1cm after lookdev. Preparation deliberately migrated
only the exact known first Body palette(.97,.78,.40); unrelated authored values are preserved.
Fresh studio121988 ran the final proof, unique run b0328bbf8b22.

Amber-Proof.json passes/restores:82 unique captures, zero cleanup errors. Starting display
noise .00256; directional thin signed response+.10580 and thick-.09827 exceed .00768
threshold; recoveries .00115/.00103. Roughness changes .04380 and absorption .13549,
recoveries .00233/.00235. Entry/helper/inclusion source changes .31035/.13547/.06756,
recoveries .00303/.00129/.00147 within declared .02 bound. These are fixed tone-mapped
display-RGB differences, not a photometric Beer-Lambert fit. Original source bytes,
scalar/vector overrides, texture bindings, master/MID/front identities and visibility restore.
The native cap reports a complete shader map and one closure; no whole-frame performance
claim follows from that resource statistic. The inclusion probe changes aggregate pixels
and reloads four opaque masters; each opaque consumer's individual response is not isolated.

Proof SHA256a508d6a51acb5f14940df75629ea4b96d3558979a81640f0e068ac7cbce7ab61.
Artifacts: Saved/UsfMaterialGallery/Amber-Inclusions-Gallery.html, Amber-Proof.json,
Amber-Preparation.json, Amber-GalleryInspection.json and Amber-FinalStudio.log.
Gallery checks all capture/source hashes,1600x1200 PNGs, native resource readiness,
visibility controls, material identities, local references and full motion decoding.
Motion comprises24 light positions and12 camera positions within approximately+/-25deg.
PNG pixels were inspected directly. Automated browser layout remains unverified under the
existing local-file policy restriction; no workaround attempted.

All68 protected prior file hashes match after source restoration and editor exit. Original
tail-light artifact inspection still passes. No Error/ensure/AS compile/fatal/reload rejection
markers were found in the final studio log. Owned editor/Toolbox exited. DefaultEngine.ini
SHA256 remains20D012DFF0397DA2198FB8AF0E7B19DADE2F8F204D10C54340F80C5EEB579438.
Prior core automation results remain historical; no new automation-suite result is claimed
for this shader/content-only slice. Work is local, with no engine edits or publication.

### Fidelity correction outcome

The first gallery was rejected by the user; its 82-capture functional result is not
artistic acceptance. Astra reviewed the original concept and the actual renders.
New hero sources: Tools/UsfMaterials/author_amber_hero.py, AmberHeroV1 mesh manifest,
CGBookcase Autumn Leaf31 CC0 source archive/Receipt.json, AmberHeroLeaf.ush and
prepare_amber_hero.py. Hero has closed pebble geometry, photographed masked foliage,
59 suspended bubble surfaces, 65 fragments, no backing and elongated highlights.

Incremental Toolbox build passed; fresh OpticalTailLamp focused gate passed 7/7,
zero skipped/failed/contaminated (43s test phase), actual exit 0. The log has no
Error/ensure/AS compile failure markers. Three new graph7 tests cover native
F0-derived refraction, generation/compile, atomic rejection and live F0 reload.
The four graph6 siblings still pass. This is a focused contract result only.

Graph7 NativeRefractiveSlab failed actual visual acceptance: a displaced image of
the body appears inside its own silhouette. Captures NativeIOR-0/Low/Neutral and
Separated-0 preserve diagnostics. StandardSeparated=1 plus capture SeparateTranslucency
did not fix it; source inspection ruled out TAA/MSAA as the proposed extra gate.
No engine edits or project renderer-setting migration. The final hero uses the
existing graph6 PNO body; graph7 remains an opt-in, visually unaccepted experiment.
The next studio binds persistent prepared assets after deferred regeneration,
avoiding the first graph7 run's stale /Engine/Transient parent.

Actual review: Saved/UsfMaterialGallery/Amber-Fidelity-Review.html and
AmberHero-Fidelity-Final.png. This is improved authored content, not achieved
concept fidelity. Thick-volume lensing, traced multiple interfaces, natural
contact/shadows and final photographic polish remain open. No performance claim,
full campaign rerun, publication or artistic approval is inferred.
