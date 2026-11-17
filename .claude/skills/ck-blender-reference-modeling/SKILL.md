---
name: ck-blender-reference-modeling
description: "Build or correct Blender character and creature meshes against concept sheets, turnarounds, or turntables; measure likeness, reconcile views, and gate visual fidelity before full rigging or export. Use for silhouette, proportion, face, facet, and palette drift."
---

# Reference-matched Blender modeling

Own the likeness of the asset, from reference interpretation through an accepted mesh.
Use the companion `ck-blender-creature-animation` skill when rig deformation or motion is involved.
This skill does not authorize engine imports, global add-on installation, or publishing.

## Establish a feasible target

- Open the actual reference images and supplied turntable frames at useful resolution. Filenames,
  captions, and a previous agent's description do not establish what the character looks like.
- Classify the turntable: renders of one rigid 3D asset, photographed object, or generated views.
  For actual 3D renders, use the source mesh/cameras if supplied. Generated views can change anatomy,
  proportions, or accessories between frames; do not assume they are reconstruction-ready.
- Establish the canonical hero view, construction views, pose, scale anchor, and important features.
  Record conflicting projections, dimensions, or prop sides and which source wins. Resolve a material
  art-direction conflict with the user; decide and disclose small reversible ambiguities yourself.
- Separate authored intent from rendering: a lit palette sphere is not an unlit material swatch.
  Record lens/projection, crop, pose, lighting, exposure, view transform, and image color space.
  Estimate missing camera values, label the uncertainty, and freeze calibration before scoring edits.
  Do not repeatedly refit cameras to make a changing mesh appear closer.
- Use an asset-local record of source paths/hashes, camera settings, feature measurements, explicit
  tolerances, accepted deviations, and current revision. Reuse an existing brief/report if it owns this
  information; do not create another competing source of truth.

## Measure and build the character's forms

1. Trace or mark landmarks on canonical crops: head/body ratio, back/belly contour, muzzle wedge,
   eye centers and shapes, ear outline and attachment, stance/paw size, tail and signature props.
   Include region-specific silhouettes. Large body regions must not hide face or accessory errors.
   Visually check masks and landmarks; model-predicted coordinates are estimates until checked.
2. Start with the large masses and approved proportions. Choose an editable polygon cage, sculpt,
   reusable base mesh, or procedural construction based on the needed local shape control. Scripts
   are useful for reproducibility; a primitive assembly or a fixed loft is only sufficient if it can
   represent the actual form. A script is not a substitute for editing planes and topology.
3. For faceted characters, place broad planes deliberately around the cheek, brow, belly, haunch,
   ear cup, and joints. Random triangulation, decimation, noise, or extra triangles do not recreate
   the concept's designed planes. Preserve their silhouette and shading through the export mesh.
4. Prove critical deformation extremes early with a minimal rig: bat spread/fold/wrap, jaw carry,
   crouch, or another asset-specific constraint. Keep the target shape; investigate topology,
   weights, rest pose, and corrective shapes before changing proportions to clear a collision.
   Any unavoidable visible compromise goes in the asset record.
5. Refine topology for the required deformation and budget. Remeshing or generated meshes may need
   retopology; do not force sculpting, all-quads, or subdivision onto a low-poly asset unnecessarily.
   Recompare the accepted forms after topology/normal changes.

## Correct against evidence

- For each revision, render the fixed construction cameras and hero view, plus a turntable to expose
  unreferenced surfaces. Keep neutral shaded, silhouette, and material/look passes distinct.
  A presentation light rig cannot establish base-color accuracy.
- Make reference/render/overlay comparisons. Use the bundled
  [comparison helper](references/comparison.md) for explicit masks, regional overlap, and landmark
  errors. Reuse a suitable existing harness rather than creating a competing one.
- Inspect the images yourself. Name the largest visible mismatch and the edit expected to reduce
  it. Change one axis (mass, head, limbs, prop, or look), rerender, and compare against the previous
  accepted revision. Revert your regressing edit before trying another mechanism.
- A parameter sweep or image-space optimizer can help fit a parameterized shape; constrain cameras,
  preserve other views, and keep geometry plausibility checks. See the scope and assumptions of
  inverse rendering in [research](references/research.md). It is optional, not a required installation.
- If two informed corrections fail to improve the same feature, stop varying parameters blindly.
  Check camera/pose, reference inconsistency, and representation limits. Present a concrete change
  of method (e.g. editable cage or artist correction) rather than spending the full clip budget.

## Likeness is a separate acceptance decision

Maintain separate outcomes for **technical validity**, **measured agreement**, and **visual acceptance**.
An exporter exit code or a successful render establishes neither of the latter two.

- Define per-view/region tolerances for this asset, based on its references and intended screen size;
  no universal IoU threshold or percentage establishes artistic fidelity. Unconfigured checks stay
  unconfigured. Do not lower thresholds after a failure without a recorded rationale.
- Assess silhouette, internal mass, face/eyes, characteristic planes, accessories, and palette
  separately. Check an unscored view or intermediate turntable angle to expose overfitting.
- Record the reviewer's actual visual verdict with the exact candidate revision and images. The
  reviewer may be the user or an agent authorized to review by the task; do not invent an extra
  permission checkpoint. Only the user can accept a deviation from their explicit requirement.
- Keep full rig/clip production gated on the accepted model plus the early deformation proof.
  Recheck likeness after skinning and after engine import if that work is authorized.
- Report what still differs and what was not observed. An inconsistent reference cannot yield one
  mesh that exactly matches every view; that does not excuse avoidable drift in the canonical views.

## Session boundaries and delivery

Use task-local output directories and a supported Blender executable discovered in the environment;
do not hardcode the version from another project. Prefer an isolated background process for scripted
work, and never reset or replace a user's open scene without authorization. Preserve source/review
history; avoid rewriting an accepted master from a stale generator.

Deliver the source mesh, rerun command, revision-bound comparisons and measurements, visual verdict,
accepted deviations, and next unresolved correction. For authorized CK engine code/build/test work,
follow the project's `build-test` skill and UnrealToolbox requirement.

Read [research](references/research.md) when choosing a modeling/reconstruction method or diagnosing
why technical success has not produced a likeness. Its recommendations distinguish source evidence
from local pipeline decisions.
