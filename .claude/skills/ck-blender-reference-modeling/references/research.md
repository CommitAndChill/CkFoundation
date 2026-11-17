# Research behind the Blender fidelity workflow

Researched 2026-11-13. Primary sources below support the indicated observations. The workflow and
acceptance policies in these skills are engineering recommendations, not claims that a source
guarantees exact reconstruction or a particular AI model's output quality.

## Production evidence

1. **Blender Studio: [Modeling Stylised Assets on Pet Projects](https://studio.blender.org/blog/modeling-stylised-assets-on-pet-projects/)**
   (2023-05-25). The production describes art-director draw-overs and repeated shape refinements;
   its wing example establishes function and motion before finalizing the design. This supports
   measured visual iteration and early deformation proofs. It does not imply procedural modeling
   is unsuitable; the article also discusses Geometry Nodes tradeoffs.
2. **Blender Studio: [Sprite Fright: Animation Tips Learned The Hard Way](https://studio.blender.org/blog/sprite-fright-animation-tips-learned-hard-way/)**.
   Describes gathering performance references, analyzing animation frame by frame, early feedback,
   and keeping blocking simple. Recommendation: approve major poses and timing before secondary detail.
3. **Blender Studio: [Animating for Project DogWalk](https://studio.blender.org/blog/animations-for-dogwalk/)**
   (2025). Describes rough action blocking and a cyclic workflow exporting animation to the game
   engine for evaluation. Recommendation: test representative rough motion against the actual
   mover/camera/blending contract before polishing an entire clip set. Its Godot pipeline is an
   example, not a Blender-to-Unreal importer recipe.

## Geometry, projection and appearance

4. **Blender Manual: [Remeshing and Retopology](https://docs.blender.org/manual/en/latest/modeling/meshes/retopology.html)**.
   Describes automatic remeshing and manual retopology for deformation. Uniform remeshing is not a
   guarantee of useful animation edge flow. Choose local control/edge placement to preserve the
   actual form and bend behavior; use the installed version's manual for exact UI/API details.
5. **Blender Manual: [Displays and Views](https://docs.blender.org/manual/en/5.0/render/color_management/displays_views.html)**.
   Describes distinct view transforms and the effect of exposure/tone mapping on displayed color.
   Recommendation: record transforms and separate neutral material evaluation from final lighting.
   Sampling a shaded swatch does not recover its unlit material value.
6. **NVIDIA, CVPR 2022: [Extracting Triangular 3D Models, Materials, and Lighting From Images](https://nvlabs.github.io/nvdiffrec/)**
   ([code](https://github.com/NVlabs/nvdiffrec)). Demonstrates optimization of mesh geometry,
   materials and illumination against multi-view observations. Its
   [NeRF dataset loader](https://github.com/NVlabs/nvdiffrec/blob/main/dataset/dataset_nerf.py)
   reads camera angles and transforms. This supports image-space fitting as a concrete optional approach;
   it does not establish that inconsistent generated concept panels can be reconstructed exactly,
   or supply a production skeleton, deformation topology, or animation performance.
7. **OpenAI: [Images and vision — limitations](https://developers.openai.com/api/docs/guides/images-vision#limitations)**.
   Documents limits in precise spatial localization and possible incorrect descriptions.
   Recommendation: verify image-derived landmarks, inspect rendered evidence, and avoid treating
   fluent descriptions as measured geometry. This is not a comparative Claude/Codex benchmark.

Access note: Blender Studio/Manual material was available through the search service's indexed
page text; direct page opens returned HTTP 402. NVIDIA and OpenAI pages were opened directly.
No paid course content or benchmark of the new skills was obtained. Consult primary pages when
version-specific behavior matters.

## Local evidence and resulting decisions

The initiating host was Mars/Orion; the reusable skills must work without those asset folders.
Read these only when auditing that host, not as prerequisites in another consumer:

- `Tools/Blender/mars_monsters/pantry_rat_r02/scripts/compare_sheet.py`: its `main()` records
  per-view IoU then prints `PANTRY_RAT_OK` without a likeness threshold. A successful comparison
  run means execution succeeded. The new helper labels metric outcomes separately from visual
  acceptance and leaves missing tolerances explicitly unconfigured.
- The rat's `R02_BRIEF.md` and `PROGRESS.md` already request overlays and retain rounds; copying
  their prose is insufficient. Per-region checks and a recorded visual verdict make acceptance
  explicit. IoU is overlap of projected masks, never a percentage of character fidelity.
- The rat progress records disagreeing scale labels and spoon placement across the sheet. Resolve
  canonical targets first; do not optimize one mesh against contradictory constraints indefinitely.
- The bat's `PROGRESS.md`, Phases 3/3b, separates intersections and export checks from persistent
  visual differences. A clean skeleton or zero cross-part intersections cannot substitute for
  likeness or playback review.

## What these additions can and cannot establish

The skills encode a repeatable process and reusable comparisons. They do not add a neural 3D
reconstructor, Blender connector, artist judgment, or more spatial capability to the session's
model. Existing Blender access is used as available; do not install arbitrary add-ons as a cure.
Their effect on asset quality still needs a real pilot: one corrected asset, fixed references and
cameras, before/after comparisons, and an observed visual verdict before scaling up production.
