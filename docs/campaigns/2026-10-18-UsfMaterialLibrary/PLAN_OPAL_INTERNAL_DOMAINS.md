# Proposed single-mesh opal with independent internal reflection and medium

2026-10-22. USER APPROVED the bounded three-Slab extension, exact earlier texture reuse,
compatibility tests and incremental build. Implementation is active; engine-source changes
are outside scope. Existing graphs, studies and sources remain intact. Execution evidence
and current status belong in PROGRESS.md; proposed counts below are not passing results.

## Corrected requirement and reason for the extension

The whole opal must be capable of colored flashes. An area that appears grey from one
view must not be permanently excluded by a static artwork coverage mask. The current
OpalLayeredFireV1 artwork contains 191 connected chromatic patches and approximately
23.33% binary coverage; that is a source-description result, not the desired permanent
eligibility of the final opal. The existing expression
`Coverage = Data.r * chroma_gate * FlashStrength` must not determine which surface areas
can ever participate in internal fire.

Latest explicit user steering: the earlier attached pastel texture already has the right
colors and texture. Preserve that approved original RGB artwork byte-for-byte and pixel-
for-pixel; change only its native light/view-dependent dullness, brightness and sparkle.
The supplied clipboard image was compared as decoded RGB: it is pixel-identical to
`Plugins/CkTests/Resources/CkUsf/OpalPolishV2/Source/OpalOriginal.png` (also preserved in
OpalFireV1). Original PNG SHA256 is
`7d66d3ff5ca042e4ffa170eeb844f18aaac921a4bba2d591030b2d41b59660b6`.
Do not generate replacement color art, recolor it, extend
its palette through gaps, or infer approval of the newest 23% coverage artwork. New
numerical orientation/roughness fields may be authored separately if needed and must
remain coordinated with that unchanged original color texture.

The current enum8 NativeTranslucentVertical has two SimpleVolume Slabs, each with one
normal shared by its reflection and participating-medium response. Applying internal
domain orientations to B's native normal can facet the cloudy body. Keeping B's normal
smooth and replacing its reflection by a view-dependent color term avoids that faceting
but does not supply an independently oriented native internal reflection lobe.

The proposed bounded correction separates three roles on one mesh/material:

- A: smooth clear external polish.
- B: oriented colored internal reflection, with negligible diffuse/scattering contribution
  and near-clear transmission so the deeper body remains visible.
- C: smooth cloudy transmitting medium, with its own optical calibration and thickness.

This is an authored approximation of internal domains. It does not add diffraction,
dispersion, traced entry/exit refraction, geometric interior layers or a separate shell.
The supplied reference/comparison video frames remain the visual target, under
`Saved/UsfMaterialGallery/OpalReferenceVideo` and `OpalComparisonVideo`.
The response must come from incident lighting and the native BRDF. No view-only angular
color-activation gate, baked highlights or emission is proposed. The source colors remain
the approved pastel artwork; shader controls may modulate reflection intensity, not
substitute a synthetic rainbow palette.

## Proposed public contract

Append enum9 `NativeTranslucentStack3` without renumbering enum0..8. Its exact leading
source declaration is `// @ckusf surface_graph NativeTranslucentStack3`. Use a separate
`FCkUsf_TranslucentStack3Output` and `CkUsf_DefaultTranslucentStack3()`; do not extend the
existing two-layer struct or output list.

Proposed flat field order and types:

| Layer | Fields |
|---|---|
| A | `float3 ADiffuseAlbedo`, `float3 AF0`, `float ARoughness`, `float3 ANormal`, `float3 AEmissiveColor` |
| B | `float3 BDiffuseAlbedo`, `float3 BF0`, `float BRoughness`, `float3 BNormal`, `float3 BEmissiveColor` |
| C | `float3 CDiffuseAlbedo`, `float3 CF0`, `float CRoughness`, `float3 CNormal`, `float3 CEmissiveColor` |
| A optics | `float3 ATransmittanceColor`, `float AReferenceThicknessCm`, `float AThicknessCm` |
| B optics | `float3 BTransmittanceColor`, `float BReferenceThicknessCm`, `float BThicknessCm` |
| C optics | `float3 CTransmittanceColor`, `float CReferenceThicknessCm`, `float CThicknessCm` |
| Shared | `float RefractionStrength` |

This is 25 fields. ADiffuseAlbedo is the primary float3 Custom output; the remaining 24
are additional outputs named `CkUsf_<Field>`, preserving the order above. Layer optical
transmittance clamps to 0.0001..0.9999; every reference/actual thickness clamps positive;
PNO strength clamps 1..1.1, as in the existing optical contract. No existing output names,
indices, types, constructor values or signature text change.

Proposed constructor values follow the current two-layer neutral constructor: diffuse
and emissive zero; F0 float3(0.04); tangent-space normal (0,0,1); roughness A=0.1/B=0.2/C=0.2;
transmittance float3(0.95); reference/actual thickness 0.1cm; refraction strength 1.0.
These are API neutral defaults, not the final opal recipe. The recipe must explicitly
make B near-transparent with zero diffuse/emission and a thin optical section; C owns
cloudiness and uses a smooth normal. B must still transmit: an opaque MSS_None middle
Slab would defeat the purpose by blocking C. The proposed fixed topology therefore uses
three SimpleVolume Slabs and three independent native TransmittanceToMFP conversions.

Renderer and authoring admission: enabled Substrate, Adaptive GBuffer, desktop deferred
platform support, and a minimum three-closure budget. Inherited shading/blend/translucency
overrides are required; the graph fixes colored-transmittance blending,
SurfacePerPixelLighting, thin-surface thickness, PNO and BeforeDOF. No directional/fuzz/
native-glint options, legacy profiles, scene color or WPO. This does not change project
renderer defaults. No direct generation is allowed under Blendable; validated bulk
authoring may be classified as OtherRendererProfile, as with enum8, without certifying
future capacity or rendering.

## Exact native graph and thickness ownership

```text
FrontMaterial -> OuterVertical(Top=A, Base=InnerVertical, Thickness=AThicknessCm)
InnerVertical(Top=B, Base=C, Thickness=BThicknessCm)
A.SSSMFP <- MfpA(ATransmittanceColor, AReferenceThicknessCm)
B.SSSMFP <- MfpB(BTransmittanceColor, BReferenceThicknessCm)
C.SSSMFP <- MfpC(CTransmittanceColor, CReferenceThicknessCm)
root SurfaceThickness <- CThicknessCm
root Refraction <- RefractionStrength
```

Both Vertical nodes have parameter blending disabled. Every Slab is native SimpleVolume,
with no SubsurfaceProfile or SpecularProfile. The outer Vertical's base must be the inner
Vertical, not B or C directly. All eight new native nodes (three Slabs, three conversions,
two Vertical operators) must be distinct, owned by the generated master and present in
its expression collection. Create/connect all local nodes before publishing the graph;
one required failure must reject the whole construction. Use TObjectPtr and prerequisite
checks before examining node inputs.

The existing engine inspection established root thickness as the bottom Slab's starting
thickness and each Vertical operator as the thickness owner for its top subtree. The
nested mapping above is the proposed application of that rule; the first real-RHI gate
must confirm three requested/effective closures without budget simplification before art
iteration. Node count alone is insufficient. No engine modification is proposed.

## Concrete implementation touch list

All paths below are relative to `Plugins/CkFoundation/Source/` except the test file.

| File | Bounded symbols/change |
|---|---|
| `CkUsf/Public/CkUsf/LookDefinition/CkUsf_LookDefinition.h` | Append enum9 only; preserve all existing reflected values/options. |
| `CkUsf/Shaders/CkUsf/Common.ush` | Add the separate 25-field struct/default constructor. |
| `CkUsfEditor/Public/CkUsfEditor/Generator/CkUsf_ShadingModelContract.h/.cpp` | Add dedicated `Is_Stack3OpticalSurfaceGraph`; parse/type/primary/24-output code; renderer/authoring checks. Keep `Is_OpticalSurfaceGraph` scoped to single-Slab enums6/7, and do not route enum9 through fixed enum8 construction/validation. |
| Same contract files | Introduce one minimum-closure-count helper used by strict admission and opposite-profile classification: existing Adaptive4/5 and optical8 require 2, new9 requires 3, existing optical6/7 require 1. Preserve previous results for all enum0..8. |
| `CkUsfEditor/Public/CkUsfEditor/Generator/CkUsf_LookValidator.cpp` | Reuse full source/signature/platform checks; use the shared closure-count helper when classifying other-profile authoring. Unsupported three-closure capacity on the active Adaptive profile remains Invalid. |
| `CkUsfEditor/Public/CkUsfEditor/Generator/CkUsf_NativeSurfaceGraph.cpp` | Add `Create_Stack3OpticalGraph` / `Validate_Stack3OpticalGraph` dispatch before the fixed enum8 path. Use a new explicit layer-prefix binding helper for A/B/C; do not stretch `Get_AdaptiveBindings(bool InLayerA,...)` into a C selector. Reuse Connect_Outputs/Input_Matches and existing signature format machinery. |
| Same native graph file | Extend recorded-signature eligibility and optical/root settings capture to enum9. Capture all node identities, classes, GUIDs, edges/masks, three MFP calibrations, both Vertical flags and root thickness/refraction; existing graph signatures remain byte-identical. Root expected class is Vertical, with an exact nested subtree check. |
| `CkUsfEditor/Public/CkUsfEditor/Generator/CkUsf_Generator.cpp` | Recognize the new Custom output-type call in reload primary-node discovery; retain source owner/provenance and all-or-nothing preparation/application. |
| `CkUsf/Claude.md` | Document enum9, three roles, independent normals, nested thickness ownership and approximation limits. |
| `Plugins/CkTests/Source/CkTestsEditor/Private/Test_Usf_Stack3Translucent.cpp` | New focused tests; preserve existing enum8 fixture/test expectations. Register beneath `CkTests.UnitTests.CkUsf.AdaptiveBsdf.Stack3Translucent`. |

Content integration follows only after contract approval and a passing technical gate:
use independent look/definition/master names, one identity body actor and a separate
owned recipe. Reuse the exact earlier user-approved color artwork as the color texture,
without RGB appearance edits. Its colors may be sampled for B's intrinsic colored F0;
normal orientation and roughness determine the native incident-light/view response.
Do not use static RGB chroma or Domain.R to permanently set fire eligibility to 23% of the
object. If a new numerical field set is needed, write it into a separately owned namespace
only after checking for existing files. Set DomainMap.R=1 across the whole image (a QA
contract, not an appearance mask); G may encode bounded optical-depth variation and B
bounded roughness/metadata, not a view-angular activation phase. Give the full surface
coherent optical orientations, including neutral artwork areas. Keep the original color
PNG unchanged, preserve its original source/prompt provenance, and record deterministic
numerical field seed/region tables, normalized-vector QA and aligned import settings.
Nearest-domain orientation extension is permissible as numerical field construction; it
must not become nearest-site palette filling or replacement color artwork. C remains
smooth while B carries the independently oriented colored reflection.

## Bounded verification and acceptance sequence

Four focused production tests are sufficient for the initial extension:

1. RendererAndAuthoringAdmission: enum9 append/type/field count; active Adaptive capacity
   three admits, capacity 0..2 and disabled/Blendable reject; inherited options and
   malformed source reject before package/material mutation; other-profile bulk authoring
   classification remains non-runnable; enum0..8 defaults/output contracts are unchanged.
2. ExactTopologyAndRetainedClosures: three Slabs/three MFP nodes/two Vertical nodes and
   every exact layer/calibration/thickness/refraction binding. Use real RHI, completed
   game/render-thread shader maps, empty compile errors, requested 3/effective 3 and the
   actual compiled budget-simplification flag false. Log payload and material feature/type
   results; do not assert an unmeasured performance benefit or a guessed payload value.
3. TopologyPreflightIsAtomic: swapped A/B/C roles, either parameter-blending flag, bypassed
   inner operator, shared MFP or Slab, swapped reference/actual thickness, wrong root C
   thickness, pin masks/output types, unsupported connected features, extra nodes and
   signature tamper all reject with errors and empty prepared work. Preserve StateId,
   Custom code, roots, expression identities and MID parent/override. Restoring the exact
   graph must make preparation runnable again.
4. ProductionBatchAtomicityAndNumericReload: one invalid master rejects a mixed batch
   before any material changes. Numeric entry and included-helper edits/restoration on
   valid masters preserve master/MID/native node identities, exact topology and all three
   compiled closures. Exercise the real Ck_Usf_ReloadShaders path rather than a copied
   application routine.

The lead must declare editor-boot scope before Toolbox. Use incremental in-place build,
then one fresh Adaptive `--test-pattern=AdaptiveBsdf --no-nullrhi` gate (existing 12 plus
the proposed 4; expected 16, not a recorded result), followed by one fresh Blendable
`--test-pattern=CkUsf --no-nullrhi` compatibility/rejection gate (expected 99 if discovery
contains only these four additions). Test-discovery boots, if cache invalidation requires
them, must be accounted for separately before launch. No Cook/Package/Shipping work.

Only after those gates pass, launch the approved owned offscreen studio for one-mesh
visual verification. Separate the native controls: A polish, B reflection off/orientation
variation, C cloudiness off/smooth-normal control, full lights-off, and matched moving
light/view. The first artistic check holds the camera fixed and moves one light; formerly
quiet regions must become colored without faceting C. Prove rendered source edit/restoration on the same material/MID separately
from the automation reload test. A changing-view sequence must demonstrate changing
active regions across the body; verify the source no longer contains a permanent 23%
eligibility mask. Finite views cannot prove every surface point will flash from every
configuration, so report the sampled view/light range and remaining blind regions.
Verify that the selected original color PNG remains byte/pixel-identical, native lights-
off extinguishes reflection, and changed illumination/view changes response without a
view-only angular color gate. New color-art generation is explicitly outside this plan.
CTO/user visual approval is still required; compiled layers and native screenshots alone
are not approval.

## Review risks and stop conditions

- Three closures may exceed a platform/quality budget even where enum8 works; strict
  admission and actual compiler retention are separate checks. Stop on simplification
  instead of hiding it with a lower expected count or changing engine budgets silently.
- Nested thickness ownership and shielding must follow the engine's subtree handling.
  Stop if B/C become opaque or thickness is accumulated incorrectly; do not compensate by
  unrelated scalar tuning before identifying the graph/engine cause.
- A native Slab normal still controls that Slab's volume response. Separating B from C
  isolates the dominant cloudy medium; B must remain nearly non-scattering. This is not
  an engine-level independent reflection/scattering-normal API for one Slab.
- Full-surface optical domains can still look metallic, tiled or painted. Increasing
  static color/brightness or adding emission is not proof of the required light/view
  response. Review rendered controls before further representation changes.
- Three layers and 24 additional Custom outputs increase payload, compilation and shading
  work. Field/output count is bounded and known; exact runtime payload/timing must be
  measured rather than inferred from node count. No performance claim is proposed.
- Existing two-layer signatures, saved definitions and protected artwork stay unchanged.
  A migration or in-place replacement would broaden this plan and needs separate approval.
