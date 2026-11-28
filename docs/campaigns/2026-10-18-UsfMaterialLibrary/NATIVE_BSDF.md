# Native BSDF first pair

Later extension: ADVANCED_BSDF.md documents native Slab fuzz, Simple Clear Coat and four
additional texture-backed surfaces. This page retains the first pair's original evidence.

The UE5.8.4 baseline is published to CommitAndChill engine `main-ck` at
`55c44c498c272fcc1f911d1c90e9885184b026f3`. This material slice remains local
in the existing host, Foundation and Tests checkouts.

## Authoring contract

An opaque SurfaceLit LookDefinition can explicitly choose NativeSlab or NativeToon;
the leading source declaration is `// @ckusf surface_graph Slab` or `Toon`.
Numeric Custom outputs feed the engine-owned Slab/Toon expression and FrontMaterial.
Legacy looks retain their previous route. Native Toon requires a saved ToonProfile.
The first slice rejects WPO, translucency and mixed native/legacy declarations.

`FCkUsf_SlabOutput` exposes diffuse albedo, F0, roughness, tangent-space normal,
emissive and optional anisotropy/tangent. `FCkUsf_ToonOutput` exposes base color,
metallic, specular, roughness, normal, emissive, pattern UVs and optional direction.
Profile ramps/hatching are native asset data, outside the numeric .ush contract.
Profile binding, texture dimension or topology changes require explicit regeneration.
Whole-batch preflight rejects mismatched layouts before applying any code. Numeric
entrypoint and transitive edits reload the existing master and MID in place.

## Textured specimens

NativePorcelainSlab and NativeCeramicToon share the existing CC0 ambientCG Tiles133A
base-color, normal, roughness and source-aligned tile mask. Grout has independent
color, normal strength and roughness. Porcelain composition is unverified; this is
a ceramic-finish example. One Slab does not constitute a physical multilayer glaze.
Both use the approved logo-free shader ball with a separate neutral CoreBase.

Persistent native assets live under `/CkFoundation/CkUsf/Looks/Native`,
`/CkFoundation/CkUsf/Profiles` and the existing GeneratedLooks directory.
`prepare_native_bsdf.py` refuses to replace existing native masters.
`prepare_native_toon_profile.py` preserves the factory profile in a separate asset
before binding a six-key external color curve: diffuse plateaus .10, .45 and 1,
with narrow transitions at .54–.55 and .79–.80. Specular, GI and pattern settings
remain native factory values. Lights, textures and exposure do not implement Toon bands.

## Verification

Incremental Toolbox Build2 passed. The focused real-RHI CkUsf gate passed67/67,
zero failed/skipped/contaminated,3m13s. Six new tests cover native pin wiring,
invalid declarations/profiles, batch-atomic rejection, shader compilation and
same-instance numeric/transitive reload. This is focused coverage, not a full suite.

Offscreen studio preparation verified actual native expression classes and the saved
profile binding. The factory-profile rendered proof passed with exact source-byte
recovery, stable native graph/master/MID identities, and restored material overrides,
quality CVars, all studio rigs and orbit enabled/paused/phase state. Receipt:
`Saved/UsfMaterialGallery/NativeBsdf-RenderedProof.json`.

The three-level refinement passed the same rendered proof, including exact source-byte
restoration and unchanged graph/master/MID identity. Final receipt and
review page are `NativeBsdf-ThreeLevel-RenderedProof.json` and `Native-BSDF-Gallery.html`
under Saved/UsfMaterialGallery. Both videos were independently probed:754x550,
24 frames,4fps,6 seconds. One owned offscreen editor boot served both profile variants;
PIE ended, startup settings restored, and the owned editor quit successfully.
No user artistic approval is inferred from validation.
Videos encode24 paused light phases at4fps; they are not real-time performance evidence.
Blendable renderer simplifications remain; no independent multilayer or path-traced
Toon claim is made.
