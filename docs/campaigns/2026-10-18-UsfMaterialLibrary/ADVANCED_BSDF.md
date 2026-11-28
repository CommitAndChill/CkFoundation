# Advanced native BSDF surfaces

2026-10-21. Four additional clean, texture-backed examples extend the first native pair.
Implementation, native shader validation and the offscreen rendered proof are complete.
User artistic review remains separate. Gallery: `Saved/UsfMaterialGallery/Advanced-BSDF-Gallery.html`.

| Surface | Native response | Texture source |
| --- | --- | --- |
| Ruby velour | Slab with opt-in fuzz amount/color | Poly Haven velour_velvet, CC0 |
| Varnished walnut | Native Simple Clear Coat, independent base/coat roughness | Poly Haven walnut_veneer_02, CC0 |
| Resin carbon weave | Native Simple Clear Coat over textured dielectric tows | ambientCG Fabric004, CC0 |
| Clean copper | Slab with colored conductor F0 and zero diffuse | ambientCG Metal057A, CC0 |

The new `NativeSimpleClearCoat` graph uses the engine's public
`UMaterialExpressionSubstrateSimpleClearCoatBSDF` directly into FrontMaterial. Its
`FCkUsf_CoatedOutput` exposes DiffuseAlbedo, F0, Roughness, Normal, EmissiveColor,
CoatCoverage and CoatRoughness. Source declaration: `surface_graph SimpleClearCoat`.
The top coat F0 is fixed .04 by the engine. Existing normal settings support one shared
normal; BottomNormal remains disconnected. Directional coat requests reject.

`_SlabFuzz` is an explicit NativeSlab-only opt-in. Additional prefixed Custom outputs
feed native fuzz pins14/15. False-path layouts/signatures remain unchanged; zero fuzz
weight alone does not disable the compiled Cloth feature. Blendable fixes fuzz roughness
and does not preserve Cloth anisotropy, so fuzz+directional requests reject before mutation.
Topology/feature changes require regeneration; numeric .ush edits reload in place after
whole-batch preflight. No engine or renderer-setting change is part of this slice.

Original licensed maps are preserved. Two separately recorded BC8 derivatives address the
known16-bit sRGB import issue: exact encoded RGB quantization, no gamma conversion, grading
or source resizing. Velvet preserves the existing documented1024x985→1024² runtime policy.
All originals, derived files, licenses, metadata and recipes are hash-pinned in
`Plugins/CkTests/Resources/CkUsf/AdvancedBsdfV1/Manifest.json`. Carbon is authored weave
data, not carbon photogrammetry; copper source has no patina or claimed brush direction.

Incremental Toolbox build passed, followed by76/76 focused CkUsf tests, zero failed,
skipped or contaminated,3m7s. The nine added tests include native pin/default compatibility,
unsupported-option rejection, batch-atomic changed-layout rejection, real-RHI compilation,
and fuzz/coat numeric reload preserving existing MID/master/node identity. Evidence:
`Saved/Logs/AdvancedBsdf-BuildTest1.log` and `AdvancedBsdf-Gate1-process.json`.
This is focused coverage, not a repository-wide regression or performance claim.

The offscreen rendered gate passed source edit/recovery and native feature contrasts,
then restored material overrides, source bytes, light state, quality CVars and startup settings.
The gallery and receipts live under `Saved/UsfMaterialGallery`. No user artistic approval
is inferred from technical verification. No Git delivery is requested for this batch.

## Environment and review notes

The first showcase startup stopped before the bridge or asset writes. A noninvasive cdb
snapshot identified Windows loader status c0000139: an obsolete TextureFormatDXT DLL
requested the removed HasAVX2InstructionSupport entry point. Its advertisement remained
in the generated project module manifest although the current5.8 target receipt excluded it.
UBT's same-BuildId merge retains names absent from the current target. A reviewed repair
backed up exact manifest/receipt bytes and removed only TextureFormatDXT and
TextureFormatIntelISPCTexComp advertisements absent from that receipt. Both DLLs and all
other artifacts remain unchanged. Evidence: `ObsoleteTextureManifest/Receipt.json`,
`AdvancedBsdf-StartupStacks.txt`, `AdvancedBsdf-MissingEntry.txt`. This is local generated
metadata reconciliation, not an engine source fix or clean rebuild. The next offscreen
startup succeeded. No extra repository checkout, artifact/cache deletion or renderer change.

Initial texture preflight stopped before writes when an existing normal texture's reported
platform dimensions differed. The engine GetSizeX/Y contract documents placeholders during
compilation and requires completed
platform-data compilation rather than placeholder data. Preflight now invokes the engine's
explicit Editor.AsyncTextureCompilationFinishAll command before rejecting a mismatched size;
it still verifies exact completed dimensions and original sampler/import settings. Preparation
then succeeded for all four native masters. Existing texture properties were not rewritten.

Art review kept velvet/walnut defaults. Matched trials led to carbon UVScale12 and normal
strength.04, reducing oversized embossed cells; copper uses authored F0(.95,.48,.28) with
color-variation.2 for a clearer warm identity. These are presentation choices, not measured
spectral calibration. `AdvancedBsdfRefinement.json` records the two explicit outside-PIE
regenerations. The subsequent proof used the resulting persistent defaults.

A proof-harness membership check initially used an editor-only asset query during PIE.
It stopped before captures and restored its state. The check now validates the mandatory
library and available optional native stations; the driver independently requires all four
requested native specimens. The failure receipt remains preserved separately. No material
or compiled C++ change was needed. The retry uses the same owned editor session.

## Final rendered evidence

`AdvancedBsdf-RenderedProof.json`: passed/restored true, zero cleanup errors;28 studio,
macro, gameplay, edited/recovered and feature-control captures, plus48 captured orbit phases.
All four native nodes, master materials and MIDs remain the same through source edits.
Edited mean absolute RGB differences range.0526–.1571; restored differences.00168–.00458.
Velvet fuzz on/off difference.00532 is deliberately subtle in the studio pose. Walnut coat
coverage and coat roughness differences are.01810/.01645; carbon.02220/.01871.
These are display-space capture comparisons, not calibrated BSDF measurements.

All four previews were independently probed:12 frames,4fps,3 seconds,754x550. They encode
paused Point-light phases, not real-time performance. The successful offscreen editor
PID85948 ended PIE/restored startup state and quit. Config/DefaultEngine.ini retains SHA256
20D012DFF0397DA2198FB8AF0E7B19DADE2F8F204D10C54340F80C5EEB579438.
Three discovery/test boots served the focused gate; one failed startup and one successful
showcase boot were explained above. No C++ or AngelScript edits followed the green gate;
later edits were preparation/bridge tooling, persistent numeric defaults and documentation.
