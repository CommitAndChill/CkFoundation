---
name: ck-blender-creature-animation
description: "Rig, skin, animate, or correct Blender creatures and characters against pose sheets and motion references, including quadruped contacts, bat wing folding, timing, and Blender-to-engine visual parity. Use when clips are technically valid but motion or deformation misses the design."
---

# Reference-directed creature animation

Preserve the accepted character while producing the intended performance. For mesh likeness,
load `ck-blender-reference-modeling`; for the supporting production evidence, consult its
[research reference](../ck-blender-reference-modeling/references/research.md).
Do not require another model pass when the current task is a bounded animation correction.

## Establish the motion contract

- Inspect the accepted mesh, rest pose, existing rig, actual reference poses/video, and target-engine
  clip contract. Distinguish mandatory bone names, parents, sockets and units from negotiable
  control-rig choices. Do not freeze invented anatomy before testing deformation.
- A turnaround describes appearance. A pose board gives key shapes, but does not uniquely define
  timing, spacing, weight, or overlap. Use supplied motion footage; where absent, choose and label
  an inferred performance instead of claiming motion fidelity to a still image.
- For the clip being made, record the key beats and times, contact intervals, body path, facing,
  duration/FPS, loop convention, root-motion/in-place choice, and intended gameplay speed. Reuse
  the existing manifest. Start with a representative clip before authoring a whole clip library.
- Separate physical requirements from stylization. Species footage informs joints and timing;
  the approved character reference owns exaggeration. No automatic switch to realistic anatomy.

## Fit the rig to the performance

Place pivots and deformation loops for the accepted anatomy. Keep a stable export skeleton where
required and use controls/IK or corrective shapes when they materially simplify the intended motion.
Check the evaluated mesh, not just bone transforms.

| Asset | Early proof to observe |
|---|---|
| Quadruped | planted stance, crouch, stride extremes, spine bend, turn and carried prop |
| Bat/winged creature | spread, up/downstroke, reduced-area recovery, half fold, wrap, inverted hang |
| Face/props | eye/eyelid readability, jaw opening and bite contact, ear attachment, straps and prop clearance |

Preserve volume and intentional planes where the design needs them. Fix bad joint placement,
topology, weights, or pose controls before adding arbitrary delays, hiding geometry, or shrinking
the character to pass an intersection check. Meaningful membrane folds can touch; classify and
inspect contact pairs instead of treating either all intersections or all same-wing contacts as benign.

## Block, play, correct, then polish

1. Block the important poses and breakdowns with deliberate timing. Stepped keys are useful when
   they make beats easier to judge; use the interpolation suitable for the task. Compare the key
   poses to the sheet using matched cameras and the modeling skill's comparison harness.
2. Play the rough clip at its actual frame rate. Judge weight shift, anticipation, action, recovery,
   and readability at the intended gameplay camera. Contact sheets supplement playback; they
   cannot establish motion quality on their own.
3. For locomotion, evaluate foot trajectories in world space during stance, body travel per cycle,
   and phase relationships. For in-place clips, simulate the intended mover speed. For flight,
   inspect wingtip/wrist arcs, wing area through recovery, body response, and fold transitions.
4. Polish arcs, spacing, overlap, and secondary motion after the main action reads. Periodic/sine
   curves may suit a layer or a stylized cycle; identical smooth oscillation on every channel does
   not establish a convincing gait or flap. Preserve purposeful holds and asymmetry.
5. Check intermediate frames and loop seams in pose AND velocity; verify the exporter's duplicate
   end-frame convention so import does not create a hitch. One-shots must recover or transition
   into their intended state. For additive clips, verify the actual reference pose and composition.
6. Render normal-speed playback and selected slow-motion/diagnostic views. Inspect them and change
   one named cause per revision. Stop after two ineffective corrections and reconsider the motion
   target, rig/skin, or reference interpretation.

## Export and observe the consumer

- Bind evidence to the exact source revision, clip names, time range, FPS, export settings and
  consumer asset paths. Read transforms, bone/slot contracts and evaluated pose samples after a
  fresh round trip. Sample meaningful contacts and extremes, not arbitrary easy frames.
- Compare the source deformation to the imported mesh/clip. Import success, frame counts, normalized
  weights and clean loop numbers do not establish appearance or performance.
- When engine work is authorized, observe the actual animation graph, blends, playback rate, root
  handling, sockets/carry prop, materials, lighting and gameplay camera. Review rough representative
  motion in-engine early enough to avoid polishing against the wrong mover or shader contract.
- Keep source animation quality, export parity and runtime integration as separate verdicts. If
  runtime is unavailable, identify it as unverified; do not silently claim engine-ready performance.
- Use the project's UnrealToolbox/build-test workflow for required CK build/test automation.
  This skill grants no additional permission to import assets, overwrite loaded packages or publish.

## Delivery

Provide the changed clips/source, before/after playback and pose comparisons, measured contact/seam
results, exact revision and rerun command, and remaining visible differences. A reviewer must actually
observe playback before accepting motion; automated metrics remain supporting evidence. Follow the
task's existing review authority without adding ceremonial approval requests.
