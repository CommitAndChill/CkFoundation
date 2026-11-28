# Opal native glints

2026-10-22. The user rejected the flat, painted appearance and missing sparkle in
OpalPolish, then rejected the visible handmade facet diagnostic. The latter remains
under OpalFire diagnostic names and is excluded from the gallery. The user explicitly
approved a bounded CkUsf native-glint extension, compatibility/reload tests, and an
incremental C++ rebuild. No engine-source changes or default renderer migration.

## Implementation

`_AdaptiveBaseGlints` defaults off and is valid only on the resolved AdaptiveVertical
graph. It adds `BGlintValue` and `BGlintUV` outputs and connects native lower-Slab pins
16/17 only when enabled. Upper glint pins stay disconnected. Existing disabled
output lists, wiring and signature text remain unchanged. Changing this structural
option requires explicit regeneration; numerical `.ush` edits retain the material.

Generation checks actual platform glint support under Adaptive. Opposite-profile
classification excludes valid Adaptive authoring under Blendable without pretending
to verify the target renderer's future capabilities. Reload checks exact pin identity,
types, masks, opt-in and provenance before applying a batch. Compiled statistics now
expose the feature mask, material type, `HasGlint`, and `ComplexSpecial` in addition to
closure count and payload uints. The native topology still has two retained Slabs.

Read-only Astra review found no blocking correctness issue. Focused real-RHI tests
cover generation, native features, default-off compatibility, renderer restrictions,
opposite-profile admission, tampered graph/output/signature rejection, mixed-batch
atomicity, numeric glint density/UV reload and exact restoration. These are focused
checks, not a repository-wide, packaged, or performance claim.

## Current execution status

- Incremental C++ build succeeded through UnrealToolbox.
- Adaptive gate: 8/8 passed, zero failed/skipped/contaminated, 59 seconds test duration.
  `Saved/Logs/OpalGlints-AdaptiveGate.log` and its process receipt retain evidence.
- Blendable CkUsf compatibility gate: 91/91 passed, zero failed/skipped/contaminated.
  Both gate process receipts report exit 0; raw logs have no error-severity,
  ensure, fatal, AngelScript compile failure or shader compilation failure signals.
  Deliberate rejection-test warnings and existing startup warnings remain in the logs.
- Native opal artwork/render tuning and final visual evidence remain pending.

## Render diagnosis and visual feedback

The first native-glint SceneCapture2D rendered black 8-pixel tiles at density .6;
density 1 rendered normally. A source review identified that UE 5.8 excludes
SceneCapture views from the scene-level special-tile allocation mask, while the
per-view classification can still emit those tiles. An identical primary-view
render works (`OpalGlints-Study04-Primary.png`) while the scene capture remains
black (`OpalGlints-Study05-SceneCapture.png`). No engine code was changed. The
owned offscreen studio now has a native primary-viewport screenshot path.

The user judged Study10's fine native glints better but too much like ordinary
glitter. Native glints remain a reusable pipeline capability; they are insufficient
as the main opal effect. The next shader study targets coherent, irregular angular
color patches under the smooth polish. Neither the earlier facet diagnostic nor
these glint studies have been promoted to a final gallery.

## Corrected optical target and geometric depth probe

The user explicitly clarified that the target is a thick, clear exterior enclosing
opal color/flashes at different depths. A two-Slab opaque material on one surface
does not establish that geometry. New additive OpticalOpalCore/Shell definitions and
masters now provide a recessed opaque two-Slab core and a separate transmitting
polished exterior. Both retain the original five texture maps read-only. Geometry
reuses the closed AmberHero body, with principal-axis core recess of 3 mm. All source
core vertices lie behind every exterior triangle plane with a conservative minimum
clearance of 2.447 mm. This is not uniform wall thickness. Exact evidence and a
reproducible calculation are OpalDepth-GeometrySeparation.json/.py.

OpalDepth-Study01..04 establish assembled/core-only/shell-only and a second viewing
angle. The initial core incorrectly spread synthetic spectrum color into diffuse,
creating broad rainbow blobs. Study05/06 remove that contribution: original texture
hues and filled patch shapes drive reflection; optical-domain orientation modulates
activity. Fine glints remain disabled (density 1). This is an artist approximation of
angular color response, not a diffraction solver. Astra reviewed both directions.

The first construction session encountered a deferred-generation readiness assumption
and a float3/float4 texture-sample typo, both corrected explicitly. Its log is iteration
history, not a clean final gate. The compiled Core reports 2 closures/16 uint with native
Glint/ComplexSpecial retained; Shell reports 1 closure/4 uint, both without shader errors.
Runtime glint feature retention does not prove sparse sparkle at density 1.

Only one physical colored interior depth has been rendered so far. The NearUV/DeepUV
samples remain on that core. A bounded second-depth probe adds sparse disconnected
patches with a separate MID of the same Core master; it must demonstrate relative
movement without silhouette chips or conspicuous opaque cutouts before promotion.
The shell uses the existing native PNO thin-surface path: geometric separation is real,
but the shader does not trace the closed region or its exit interface.

### Three-surface construction study outcome

The intermediate opaque-patch probe failed visually: twenty nearly circular pieces
occluded the core and read as dark coins (Study08). Their source/mesh and evidence are
retained as rejected history, with no active scene binding or gallery promotion.

After an Astra review, OpticalOpalFireLayer uses the existing NativeTranslucentSlab
contract on a full midpoint inset body. Original texture hue/chroma and domain orientation
drive colored F0; diffuse/emissive are zero, quiet regions transmit the deeper core, and
native fine glints are disabled. The middle sorts before the exterior explicitly and
casts no shadow. Core remains two closures/16 uint; middle and shell each one closure/
4 uint, with no shader errors. This added no C++ or engine changes.

Study10/11 establish middle off/on. Dark coins are gone and localized green/orange
reflections strengthen, but native unmasked F90 produces an extra continuous inner rim.
The result is suitable as a construction/depth study, not final opal art or mockup parity.
Irregular, convincing internal play-of-color is still outstanding. Do not hide this
behind successful compilation or promote it as the finished material.

The current construction required three owned offscreen sessions: initial shell/core,
the rejected opaque islands, and transmitting middle. The latter retains session
4122ef84-9846-43d6-90fd-bda5f6069b94 (editor120892). No desktop input was used.
The first eight-capture reload report remains failed: Core/Shell changes and recovery
passed, but the weak FireLayer probe's MAE1.689 missed its1.718 repeat-noise threshold.
A separate four-capture whole-layer color probe passes, change3.414>1.572 and recovery
.814<2.358. All three sources were byte-restored with stable masters/MIDs/parameters.
The failure receipt is preserved; no threshold was weakened. This verifies live numeric
source iteration, not optical realism.

This studio log contains one handled viewport ensure: EditorSetViewportRealtime(true)
attempted to remove an override that the script never added. Source inspection confirmed
that the engine API does not set underlying realtime in that branch. The two unpaired
calls were removed from the script, preserving camera/invalidation/native screenshot
requests. That wrapper correction is AST/static-reviewed only, not freshly boot-tested.
No shader/AngelScript compilation failures or fatal errors were found in the audited log;
do not describe the studio as ensure-free. Broad C++ gates were not rerun for this art work.

Construction gallery: `Saved/UsfMaterialGallery/Opal-Depth-Study.html`. Its record validates
21 native primary captures, including a16-frame1600x1200 H.264 clip at6fps, fully decoded.
Second-angle views preserve ordering and the clear perimeter, but do not establish final
convincing opal-domain depth. All61 protected prior files remain byte-identical. The final
owned quit response is OpalDepth-TransmittingQuit.json; retained log is
OpalDepth-Transmitting.log. No publication was requested or performed.

Reproduce with `Tools/UsfMaterials/Launch-OpalDepth.ps1`, then the serial bridge's `prepare`
and `scene` actions. FireLayer starts hidden; set `visible={"FireLayer":true}` explicitly
for the assembled probe. `record_opal_depth_study.py` and
`build_opal_depth_study_gallery.py` preserve existing evidence rather than overwriting it.
`verify_opal_depth_reload.py` supports bounded per-role probes with a new unique prefix.

## New additive art path

`OpticalOpalGlints.ush` uses the original five OpalPolishV2 textures read-only and
the existing AmberHero body. Its clear top remains smooth; the lower macro normal
defaults flat. Native glints supply the microscopic distribution. Glint UVs are
smooth/unwrapped and separate from the shallow offset used for color sampling.
New definition/master destinations are OpticalOpalGlints only; prior assets and
galleries remain protected. This is artistic opaque opal, not spectral diffraction
or traced volume transport. Numeric properties and sampling remain live-editable.

`OpalGlints-ProtectedBaseline.json` preserves 61 prior source/assets/evidence entries.
It explicitly excludes the approved Common.ush output-struct addition from the old
62-entry OpalFire baseline. It does not silently rewrite the historical baseline.

Tool entry points are `Run-OpalGlintsGate.ps1`, `Launch-OpalGlints.ps1`,
`prepare_opal_glints.py`, `opal_glints_session.py`, `opal_glints_command.py`,
`opal_glints_settings.py`, `verify_opal_glints.py`, `encode_opal_glints.py`,
`finalize_opal_glints.py`, `record_opal_glints.py`, and `build_opal_glints_gallery.py`.
Preparation precedes scene creation. Commands are serialized on the owned editor's
tick and UUID bridge; no desktop input or live user editor is borrowed. The scripts
refuse to overwrite completed proof captures. Do not launch duplicate editors after
a timeout or regenerate a master while it is bound to the studio scene.
