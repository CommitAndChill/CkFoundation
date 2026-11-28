# Optical tail-light refinement

2026-10-21. First additive refinement ready for visual review. User reviewed the actual first prototype positively and asked to
continue. This slice polishes that lamp before multiplying optical examples.

Neighbor: `tail_lamp_session.py`, its existing immutable TailLampV1 resources and native
translucent Slab source. Read the original gate, lens/helper/hardware shaders, original
source generator, settings and preparation code. Current graph already supplies the
required optics; no C++, engine changes or new renderer contract is needed.

Plan and checks:

1. Author a separate deterministic three-map set: rounded optical zones, shallow pillow
   cells/fan tooling, smooth perimeter and thicker support webs. Preserve V1 resources.
   Check map dimensions, normal slopes, bit depth, original hashes and import semantics.
2. Add owned lens/reflector/housing definitions and masters, leaving original assets intact.
   Reuse the live `.ush` sampler/optical helpers; soften cup and housing finishes.
   Check exact source ownership and named slots; inspect matched original/refined captures.
3. Capture front, oblique, macro, lens-hidden, LED-off/on, moving light, material controls
   and source edit/recovery. Check master/MID/texture identity within each variant, complete
   native resource, exact source restoration and actual pixels. The floor stays identical.
4. One iterative offscreen studio and one fresh validation boot are planned. No C++ rebuild.
   Keep old proof/gallery intact; write new run-unique images, report and review gallery.

Baseline: first proof passed/restored; prior focused tests4/4 Adaptive and86/86 Blendable.
Those results predate this refinement and are not new verification of the refined assets.
No unrelated files, desktop input, publication or permanent renderer migration authorized.
Artistic acceptance remains the user's; single-sheet PNO remains screen-space optics.

First fresh comparison rejected: first/refined-repeat difference .03268 exceeded its
observed finish delta .04461/3 noise bound. Original recovery .00536 and all original asset
hashes passed. Directly inspected frames show unsettled specular lighting; three subsequent
identical captures differ .00286/.00275. This establishes a transient capture discrepancy,
not a material failure or a proven renderer root cause. Retained failed report and frames.
Comparison protocol now records a settling frame then a stability pair per finish switch;
requires fixed mean absolute display-RGB difference <=.005 and retains all frames. The
original relative change/recovery threshold remains. Reuse the same fresh editor after
exact restoration; no third boot or material edit is needed for this protocol correction.

## Final evidence and limits

Fresh editor76416 completed the corrected proof, run f69a9ed9edc0:78 capture records,
76 unique PNGs, no cleanup errors, exact source/scalar/vector/texture restoration and
57 preserved original file hashes unchanged. Two owned studio boots total, no rebuild.
Original/refined comparison change .01952 versus temporal noise .00333; original recovery
.00251. Roughness change .02778 and recovery .00338. LED signed increase .00655 exceeds
threshold .00217, with .000219 recovery. New lens compiles to one native closure.

Flat-normal difference .00411 is close to comparison noise .00333. Although above the
script's absolute .001 control bound, this is not decisive isolated evidence of the subtle
normal response. Small pillow cells remain intentionally shallow and visually subtle.
Entry/helper edits change .09246/.11314 and exact source bytes recover. Rendered recovery
.01794/.01673 meets the predeclared .025 bound, not noise-floor agreement. Final versus
initial refined image differs .01734 after the motion sequence. These are tone-mapped
display differences, not calibrated optics or deterministic pixel reproduction.

Proof: Saved/UsfMaterialGallery/TailLamp-RefinedProof.json SHA256
`032079e3dcb1410923e907c4325b7560508c643017edb7b94946a3489eff1927`.
Gallery: Optical-TailLamp-Refined-Gallery.html, with matched comparison, close-ups, LED
states, hidden lens and two fully decoded light/camera clips. HTML local references and
capture/source hashes pass. Browser layout remains unverified under the existing local-file
policy restriction. Windows managed temp storage refused child writes during first encoding;
concat manifests now stay in this workspace, videos are content-addressed and never replaced.

FinalStudio log: TailLamp-Refinement-FinalStudio.log. No Error/ensure/AS compile/fatal/reload
rejection markers found. Owned editor/Toolbox exited; original config SHA remains unchanged
at20D012DFF0397DA2198FB8AF0E7B19DADE2F8F204D10C54340F80C5EEB579438.
Prior4/4 Adaptive and86/86 Blendable tests were not repeated for this content/shader-only
slice; this gate establishes fresh rendered evidence, not a new automation-suite result.
Original proof/gallery still pass their artifact inspection. No publication or engine edits.
