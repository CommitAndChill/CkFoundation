# USF studio progress

Updated 2026-10-18. Status: imported and running; user approved the upright,
open gallery and brighter presentation. Publication verification is in progress.

The user asked for Astra-assisted improvements to the USF Materials gym, better
material examples and a standard material preview mesh without the Unreal logo.
They explicitly require live editing for all materials and agreed to perform
verification when explicitly asked.

Astra inspected the gym and recommended neutral local studio lighting, one
consistent shader ball and six authored starters. It identified the unbound global
outline as a presentation problem. A second review caught backwards image planes,
scaled light positions outside attenuation range, a shadow-obstructed rim, unsafe
import retry behavior and temporary startup settings being persisted by gym travel.
The source addresses these findings; subsequent runtime evidence is recorded below.

Implemented source:
- Foundation: CkUsf_Studio_Assets.as and five Studio*.ush entrypoints. Ordered/grouped
  parameters, static world-projected detail with derivative fading, existing
  DefaultLit/ClearCoat/Subsurface contracts. Existing Toon shader remains unchanged.
- Tests: StudioShowcase, standard specimen path with neutral core/base,
  lights configured in world space after actor scaling, fixed exposure, plane-based
  image looks, outline off by default. N/B frame six source-authored materials, T/U
  restore Toon source, F frames selected specimen, L/G preserve reload commands, O
  toggles outline. Prepared constant-node comparison materials are not instantiated
  on normal startup; the older comparison exec command initializes them on demand.
- Host Tools/UsfMaterials: Open-Studio.ps1, open_studio_in_editor.py and
  prepare_shader_ball.py. Setup resolves an upstream revision, preserves attribution
  and hashes, converts the GLB scene to a two-slot OBJ, validates before saving and
  supports retry from its source receipt without overwriting an existing mesh.
- Tests Resources/CkUsf/UsdShaderBall: license/provenance description. The actual
  model and generated studio master assets are NOT present yet.

Chosen candidate: Khronos/USD Working Group standard shader ball, explicitly
CC BY 4.0 for the asset. Credits: Chris Rydalch, Andre Mazzone, Thomas Anagnostou
and Eric Chadwick. It is not an MIT asset. Logo absence, actual mesh geometry, UV
orientation and imported slot ordering remain unverified until inspection.

Local checks: Python syntax, PowerShell parser, synthetic GLB conversion covering
hierarchy transforms, inverse-transpose normals, mirrored winding, material slot
ordering and rejection of ambiguous material selection passed. The first synthetic
test attempted an inaccessible sandbox temporary path; the same fixture was rerun
successfully under the selected checkout's Saved directory. No Unreal/editor test
was run, and no commit/push was made for this task.

Execution limitation: automatic approval review repeatedly failed because its
selected reviewer was at capacity. It did not judge these actions unsafe. As a
result Git-state inspection, external mesh download and GUI launch were not
executed. Existing source was edited with narrow patches; no Git mutation or
unrelated cleanup was attempted. The user chose to perform verification explicitly.

Human entry: run Tools/UsfMaterials/Open-Studio.ps1, then outside PIE enter
`py "D:/Repos/CkPlugins_3/Tools/UsfMaterials/open_studio_in_editor.py"` in the editor
console. Confirm import, no logo/fallback materials/errors, six material appearances,
outline off/on and source save/recovery without restarting. Send the setup error
or observed visual discrepancy if any step fails. Record their result here before
marking this slice accepted.

## First real import: material ordering and UV repair

The user executed setup and supplied the real import log. Download succeeded at
upstream edc7c9e67c639d230715049ee31f9a96a6babbbe; source SHA256 is
ab123a15d70c4553adcabb16bef45587ad17e3c00da6b54640624a7287a8b52e.
Unreal imported CoreBase before Specimen, which exposed the setup's incorrect
ordering assumption. It also warned about degenerate tangent bases and near-zero
binormals. Offline inspection of the downloaded model's original OBJ established
6,912 nondegenerate triangles with collapsed UVs; normals had unit length.

Source repair: all specimen/core assignments resolve material-slot names, including
source-MID capture and diagnostic switching. Version-2 conversion splits only
unmapped corners and projects them onto a nondegenerate dominant plane. It writes
a new SM_UsdShaderBall_Uv2.obj, preserving the original OBJ and original GLB. Named
slot validation accepts either ordering. The importer reimports only the asset
whose source matches this recorded gallery source, using the original source hash;
it preserves an existing on-disk UAsset under Saved before reimport and saves only
after named-slot validation. The OBJ now explicitly emits smoothing groups while
preserving imported vertex normals. Unreal's actual smoothing/tangent warning
behavior remains to be checked; no warning is silently filtered.

Offline actual-mesh proof: 32,996 triangles, zero collapsed UV faces (minimum
absolute UV determinant 2.0243e-7); exact triangle positions, winding and normals
preserved; all 26,084 originally valid UV triangles unchanged. Python syntax and
synthetic converter regression pass. These do not substitute for Unreal tangent
build verification. Editor interaction remains blocked by reviewer-capacity
failure; ask the user to rerun the same bootstrap command outside PIE and report
its resulting log. Visual assessment is still theirs.
Astra's bounded source review agreed with the UV and name-based slot repairs but
found a retry correctness bug: a failed reimport could reload the old asset and
still mark version 2. Setup now requires positive AssetImportTask.GetObjects
results for the expected target and the repaired source filename before writing
metadata. It also checks the mesh-save result and restores the prior conversion
marker on failure. Offline failure-path checks reject empty/stale import results
and accept only the correct target/source pair. Actual Unreal reimport remains
pending; ask the user to rerun the same editor bootstrap outside PIE.
## Orientation repair: applied in the existing editor

The original converter retained glTF Y-up coordinates in an axis-less OBJ.
Conversion 3 rotates positions and normals together as (-Z, -X, Y), then imports
with KeepXYZAxes (no importer scene conversion). UVs, winding and triangle count
remain unchanged. Actual mesh proof compared all 38,178 positions/normals and
32,996 faces against version 2; synthetic conversion and Python syntax checks pass.

Approval service recovered. Stopped PIE, reimported the owned mesh, saved conversion
3 and generated six look masters, then reopened USF Materials in the same editor
PID 41548. USF_STUDIO_PREPARED logged at 05:16:58 UTC on October 19. All source
looks reported applied. After releasing PIE mouse capture, framing through the
live controller placed the camera at (1640,800,177.58), pitch -6.06, facing Toon.
Saved/UsfMaterialGallery/Orientation3-Final.png shows the specimen upright on its
base. Previous asset retained under Saved/UsfMaterialGallery/BeforeConversion3.

This is orientation/import evidence, not artistic approval or a clean startup
claim: the log also contains a null CreateWidget class error and existing console,
scheduler and material-destruction warnings. A temporary diagnostic Python probe
used an unsupported get_pawn accessor; corrected to get_controlled_pawn and reran.
The user still owns material quality and live-edit visual acceptance. No fresh
editor boot, C++ rebuild, commits or pushes were performed for this repair.

## Open presentation and exposure adjustment

Removed the six studio backdrop mesh components: their oversized black cards
appeared as walls between rows. Kept the shader-ball bases and matched lights.
Raised exposure from fixed EV100 5 to 3.5 (1.5 stops brighter) and doubled the
studio fill from 1800 to 3600 to soften dark sides without raising key highlights.

Applied through a successful full AngelScript reload outside PIE, then reopened
the gym in the existing editor (zero fresh editor boots, no C++ build). A live
production-state probe verified six specimens, no Backdrop components, fill 3600
and both exposure bounds 3.5: USF_OPEN_PRESENTATION_PASS at 05:22:01 UTC October 19.
Saved/UsfMaterialGallery/OpenPresentation.png confirms open rows and a brighter
framed Toon specimen. Final visual brightness acceptance remains with the user.

## Publication checks (2026-10-18 local)

User approved the presentation and requested direct dev delivery. Read-only source
review found no publication blocker; source GLB hash/attribution checked. Python
syntax and synthetic converter regression pass. All three fetched repositories
initially matched origin/dev exactly, so no incoming source changes need a rebuild.

An overly broad real-renderer CkUsf gate auto-split into four lanes (plus one retry
boot), rather than the planned single editor. Result: 51/57 passed, six failed,
zero skipped/contaminated, 3m36s (Saved/Logs/Test-UsfGallery-Publish.log). Failures:
- GymShadingModelsCompileAllLegacyVariants: catalog explicitly rejects Substrate.
- ToonRejectsUnsupportedAuthoringAtomically: expected old diagnostic text.
- ToonReservedOutputNamesAreScoped: older non-Toon-name contract assertion.
- ToonAuthoringGeneratesNativeMaster: editor died during legacy authoring fixture.
- ToonLightingReloadUpdatesExistingMID: refraction cached-expression ensure during
  global recompilation; attribution is unresolved, not claimed unrelated.
- GeneratesUsableMasters: listed known red, debug parameter-name collisions.

The broad run is red and is not an acceptance claim. Legacy fixtures do not load
the new Studio looks or shader-ball directly, but global reload can affect other
loaded materials. Temporary lighting HLSL edits were restored by the test. The
established eight-test ShadingModelDeclaration gate is being run separately in
one fresh renderer editor with -CkUsfSubstrateProof, matching project defaults.

Compatible publication gate: Saved/Logs/Test-UsfGallery-Compatible.log passed 8/8,
zero failed/skipped/contaminated, 49 seconds, with real RHI and Substrate proof.
No AngelScript compile failures or ensures found in that run. This verifies the
supported declaration and live-reload pipeline, not the full CkUsf test population.
The broader red result above remains recorded and unmodified. User presentation
approval plus live six-specimen assertions supply the gallery-specific evidence.
No new C++ source or build configuration changed; existing binaries were reused.
