# Single-mesh transmitting opal

Latest status: the two-Slab extension and rendered live editing are technically verified,
but this art study is not the final opal. The user selected the earlier OpalPolishV2/OpalFireV1
RGB artwork and corrected permanent-coverage assumptions. See PLAN_OPAL_INTERNAL_DOMAINS.md
for the subsequently approved three-Slab separation, now in implementation. Preserve the selected color artwork.

2026-10-22. User approved proceeding after supplying the single-mesh Substrate guide.
This supersedes the proposed opaque lower-diffusion correction and the separate-shell
construction as the intended opal. Existing studies and source assets are preserved.

## Scope and engine evidence

Append an opt-in NativeTranslucentVertical graph to CkUsf, following the existing
NativeTranslucentSlab and AdaptiveVertical creation, signature and reload boundaries.
Two native SimpleVolume Slabs are joined by Vertical Layering without parameter blending.
The upper clear polish and lower cloudy transmitting body each have independent
transmission calibration and actual optical thickness. Numerical inputs, texture sampling,
normals, roughness, color and depth-pattern offsets remain authored in .ush.

Read-only engine consultation and lead inspection establish that
SubstrateForwardLighting.ush evaluates each retained BSDF in the native translucent path.
The translator starts the bottom Slab thickness from root SurfaceThickness; the Vertical
operator temporarily supplies its thickness to the top Slab. No engine change is required.
Compilation budgets can simplify the material, so generated nodes alone are not evidence
of retained layers. Raster behavior does not establish equivalent Lumen, export or
ray-tracing behavior.

Source references are SubstrateTranslatorCommon.cpp:1803,
MaterialExpressionSubstrate.cpp:2970, SubstrateForwardLighting.ush:163-176 in the registered
UE 5.8.4 checkout. Existing project Blendable defaults remain unchanged; the studio uses
its approved startup-only Adaptive profile.

## Implementation and verification sequence

1. Extend graph admission, numeric outputs and exact native creation/validation with
   default paths preserved. Verify parser/renderer restrictions, incompatible authoring,
   graph tamper rejection and atomic reload behavior through focused production tests.
2. Incrementally build through UnrealToolbox. Fresh test discovery may use two editor
   boots (NullRHI and real RHI) to register the new tests; reuse that discovery afterward.
   First test boot runs the Adaptive tests;
   require two requested and effective closures, no simplification or compile errors.
   Second fresh boot runs Blendable CkUsf compatibility/rejection coverage.
3. One additional fresh boot launches the owned offscreen studio. Use one identity body mesh and
   one generated material/MID; original OpalPolishV2 source maps remain read-only.
   Verify visible coating, body transmission/scattering controls, colored patches under
   changing light/view, complete lights-off, source edit/restoration and preserved identity.
   Warm numerical iterations share this studio. Close the owned editor afterward.

## Visual acceptance and limits

The goal is a thinner smooth polish with irregular colored flashes in a light-permeable
milky body. Uniform metallic glitter, broad painted rainbow blobs and a detached glass
shell are rejected directions. No emission is used to substitute for illumination.

Optical layer thickness does not create geometric shell separation or a traced internal
volume. View-dependent texture offsets are an authored depth approximation. SimpleVolume
is an approximate participating-medium response, not opaque screen-space diffusion or
an opal diffraction solver. Technical gates do not establish visual approval or performance.

## Execution

Incremental UnrealToolbox build succeeded. Adaptive gate passed 12/12 tests, including
all four new LayeredTranslucent tests, with zero skipped or contaminated tests. These
assert two requested and retained closures, no budget simplification, exact thickness
and MFP wiring, atomic invalid-batch rejection and preserved MID/node identities during
numeric and included-helper source reload. Log scan found no fatal, ensure, AngelScript
or material/shader compiler errors. Blendable compatibility then passed 95/95, zero
skipped/contaminated, exit 0; its matching log and process receipt use the BlendableGate
suffix. Native art checks remain pending. Evidence: Saved/Logs/OpalLayered-AdaptiveGate.log and
Saved/UsfMaterialGallery/OpalLayered-AdaptiveGate-process.json (exit 0).

All 61 protected files still match OpalGlints-ProtectedBaseline.json before the studio.
Changes are local; no rendered result or completion is claimed yet.

## Current technical gate and new artwork record

The incremental build result is `=== Build succeeded ===` in
`Saved/Logs/OpalLayered-AdaptiveGate.log` (line 269). The same fresh real-RHI run selected
the existing Adaptive tests plus these four new production tests under
`CkTests.UnitTests.CkUsf.AdaptiveBsdf.LayeredTranslucent`:

- `RendererAndAuthoringAdmission`: opt-in parser/output contract, two-closure Adaptive
  admission, incompatible options and malformed source rejection, existing optical defaults.
- `ExactTopologyAndRetainedClosures`: two transmitting SimpleVolume Slabs, two independent
  transmission calibrations, AThickness on Vertical and BThickness on root SurfaceThickness,
  two requested/effective closures and no budget simplification.
- `TopologyPreflightIsAtomic`: changed topology, shared/wrong MFP calibration, thickness,
  refraction/settings/output/signature and extra-node rejection without material mutation.
- `ProductionBatchAtomicityAndNumericReload`: invalid mixed batch changes no master;
  valid numeric entry/helper edits and restoration preserve master/MID/node identities and
  exact root/input/output contracts while retaining the native compiled layers.

Adaptive: 12/12 passed; Blendable compatibility: 95/95 passed. Both raw summaries report
zero failed, skipped and contaminated tests, with no known-red exclusions affecting the
verdict. Process receipts are
`Saved/UsfMaterialGallery/OpalLayered-AdaptiveGate-process.json` and
`Saved/UsfMaterialGallery/OpalLayered-BlendableGate-process.json`, both exit 0. Compatibility
log: `Saved/Logs/OpalLayered-BlendableGate.log`. These gates establish the tested production
contract, not repository-wide regression freedom, final rendered response or performance.

The new artwork namespace is exclusively
`Plugins/CkTests/Resources/CkUsf/OpalLayeredFireV1`; older OpalFireV1/OpalPolishV2 resources
remain separate. `Tools/UsfMaterials/author_opal_layered_fire_maps.py` preserves the original
RGB PNG byte-for-byte as both `Source/OpalOriginal.png` and `Textures/OpalColor.png`, and
preserves the supplied generation prompt in `Source/ImagePrompt.txt`. Source SHA256:
`8b3c88800dde7811fae3183db156cfffc3b70b03376f8132bde6e29b278467e5`.
`OpalTextures.json` SHA256:
`42616b52d21b7a0b33a564a335bc652541472851f2503ee162ea55275321887c`.

Source QA verifies five aligned 1254x1254 maps, 191 connected chromatic patches in seven
orientation families, 23.3334% binary coverage and 22.6143% mean numerical coverage.
OpalDomains stores coverage/body value/family phase. OpalDomainNormal uses coherent
deterministic patch tilts influenced by neighboring families, extended through neutral
gaps without coverage multiplication; no RGB brightness-gradient normal is generated.
Decoded slopes are 0.100771..0.427443, including a minimum 0.100771 at coverage edges.
Decoded normal lengths are 0.995515..1.004986 after 8-bit quantization. The outer polish
normal is flat; roughness is spatially constant 31/255 (0.121569, nearest 8-bit value to 0.12).
All map hashes, decoded pixel roundtrips, generator/source/prompt pins and original-color
byte equality pass. These are numerical artist fields, not measured mineral data. UE
stretch-to-power-of-two 2048x2048 runtime import is planned; no completed runtime dimension
claim is made for this new set yet.

Reference direction is grounded in the supplied videos' extracted frames:
`Saved/UsfMaterialGallery/OpalReferenceVideo/frame-01.png` through `frame-06.png`, and
`Saved/UsfMaterialGallery/OpalComparisonVideo/frame-01.png` through `frame-06.png`.
They guide irregular patch scale, neutral gaps and changing-view flashes; they do not
establish measured diffraction or renderer equivalence. The intended representation is
one mesh and one material with native transmitting layers, never a separate glass shell.
Physical diffraction, traced entry/exit optics and geometric internal volume remain
outside the contract. Final native visual controls and rendered source-change/restoration
proof for the revised artwork remain pending, along with user visual approval.

The older `Tools/UsfMaterials/author_opal_fire_maps.py` is restored byte-exact to its
pre-existing SHA256 `6f932f52f69696541f2de5d6c1f54d756032a3bf55d7d69a801b9e2a6749d2f0`;
see `Saved/UsfMaterialGallery/OpalLayered-HelperRecovery.json`.
