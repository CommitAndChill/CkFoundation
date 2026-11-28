# USF material library progress

## Latest steering — preserve earlier artwork; correct incident-light response

2026-10-22: user clarified that grey regions can flash at other light angles, then explicitly
selected the earlier pastel artwork. Its attached RGB pixels exactly match the original in
OpalPolishV2 and OpalFireV1 (SHA256 7d66d3ff5ca042e4ffa170eeb844f18aaac921a4bba2d591030b2d41b59660b6).
Do not generate replacement color art or permanently exclude neutral areas by coverage/chroma.
The newer OpalLayeredFireV1 maps and native captures are unaccepted diagnostic studies.

Those five new maps imported as 2048-square textures with two retained native closures.
Moving-view captures exposed shared-normal body faceting and localized highlights. A smooth
body plus view-only activation removed faceting but did not establish the required incoming-
light behavior. No such approximation is promoted as the final opal. The exact live source
edit/restoration proof passes (OpalLayered-Reload-FireStudy-Proof.json, four captures, retained
actor/material/MID/topology identities); this verifies editing, not art acceptance.

Source inspection and Astra consultation recommend an opt-in three-Slab graph: clear A,
oriented colored reflection B, smooth cloudy transmitting C. The proposed extension keeps
one mesh/material, existing graphs and the selected RGB artwork; removes view-only color
activation; and starts visual acceptance with a fixed camera and moved light. See
PLAN_OPAL_INTERNAL_DOMAINS.md. The user subsequently approved the bounded three-Slab extension,
compatibility tests and incremental build. The incremental build now passes, followed
by fresh Adaptive 16/16 and Blendable 99/99 gates (exit0, no skipped/contaminated tests).
The new three-layer fixture retains three closures with no budget simplification.
The new native three-layer study now has a saved material preset, six final control
captures, a 48-frame fixed-camera moving-light sequence and a passing rendered `.ush`
edit/restoration proof. Gallery: `Saved/UsfMaterialGallery/Opal-Three-Layer-Gallery.html`.
See GATE_OPAL_INTERNAL.md for exact evidence, diagnostic failures and appearance limits.
All three closures remain active on one Body mesh; no separate shell or emission.
The approved RGB bytes and 61 protected files remain unchanged. The final owned offscreen
editor (PID128764, session f7c4e8be-9726-4dbf-9d19-d576d96a5b81) quit normally and is absent.
Native output is ready for visual review; reference fidelity and perceived interior depth
are not declared accepted. No old gallery was replaced and no Git publication occurred.

## Active direction correction — one mesh with native layered opal

2026-10-22: the user challenged the separate-shell construction because the purpose is
to showcase Substrate layering on one mesh, as with car paint. Stop advancing the
three-surface assembly as the intended material. Preserve its gallery and captures as
construction studies. Before this clarification, the studio helper gained a reversible
1 mm core / 0.5 mm middle principal-axis inset control; that change has only static and
geometry-math checks and has not been rendered or accepted.

After the user's single-mesh guide and approval to proceed, source inspection confirms
the native translucent forward path evaluates both retained Slabs. This supersedes the
earlier opaque lower-diffusion proposal. The additive NativeTranslucentVertical graph
uses two SimpleVolume Slabs, two independent TransmittanceToMFP conversions and Vertical
Layering with parameter blending disabled. AThicknessCm supplies the polish; root
SurfaceThickness supplies BThicknessCm for the body. Independent reference thicknesses
keep extinction calibration separate from actual optical thickness.

Implementation and incremental UnrealToolbox build pass. The fresh Adaptive gate passed
12/12, including all four LayeredTranslucent production tests; Blendable CkUsf compatibility
passed 95/95. Both have exit 0, zero skipped/contaminated tests. The new graph retains two
requested/effective closures without budget simplification in the real-RHI gate, with
exact A/B MFP/thickness bindings and atomic identity-preserving numerical reload coverage.
These are technical gates, not artistic approval or the final look's rendered reload proof.
Evidence: Saved/Logs/OpalLayered-AdaptiveGate.log, OpalLayered-BlendableGate.log and their
matching Saved/UsfMaterialGallery/OpalLayered-*-process.json receipts.

The initial single-mesh study reused the protected OpalPolishV2 maps. A new independent
OpalLayeredFireV1 source set now preserves original 1254-square generated RGB bytes and
supplied prompt, with five aligned maps, 191 connected chromatic regions and 23.33%
binary coverage. Numerical domain slopes extend across neutral gaps without flattening
at coverage edges; polish normal is flat and roughness is constant (31/255, approximately
0.12). Completed runtime 2048-square import remains to be checked for this source set.
Authoring script: Tools/UsfMaterials/author_opal_layered_fire_maps.py; manifest:
Plugins/CkTests/Resources/CkUsf/OpalLayeredFireV1/OpalTextures.json. The current art iteration
uses one body actor/material with transmitting native layers, without a separate shell,
emission or native glints. Reference/comparison video frames are preserved under
Saved/UsfMaterialGallery/OpalReferenceVideo and OpalComparisonVideo. Final native visual
controls, changed-view behavior and rendered source-edit/restoration proof remain pending;
no new result is user-approved. See GATE_OPAL_LAYERED.md for exact scope and limits.

Layer thickness models optical transport, not generated shell geometry or measured
front-to-back ray distance. Texture offsets provide approximate perceived internal depth.
Native scattering/transmission, visible colored flashes and actual artistic quality must
be verified separately from compilation. Old studies and approved material assets remain
protected. No engine or default renderer configuration changes are needed.

## Active correction — thick clear exterior with buried opal color

2026-10-22: the user clarified that the optical target is a visibly thick clear exterior
enclosing color and flashes at different depths. The earlier opaque two-Slab polish
and native-glint studies do not meet that visual goal; the user judged the latter ordinary
glitter. The approved native-glint extension is implemented and its focused Adaptive
8/8 and Blendable 91/91 gates pass. These are technical results, not art acceptance.

New additive OpticalOpalShell/Core assets use separate geometry. A native primary-view
offscreen capture shows the clear margin around an inset core; source geometry verifies
3 mm principal-axis recess and at least 2.447 mm conservative separation. This is not a
uniform wall-thickness claim or traced entry/exit refraction. Study05 restores original
irregular texture colors after rejecting a synthetic rainbow-band study. Fine glints
remain disabled while depth is evaluated. A sparse opaque island probe was rejected for
dark polygon boundaries. The current middle layer instead uses a separate transmitting
native Slab on inset geometry, preserving the deeper core. It remains a construction
study: an extra inner glass rim and broad color pattern still prevent final opal acceptance.
Core/Shell live source changes and restoration pass; an initial weak middle-layer color
probe failed its image threshold, and a stronger independent probe passes without lowering
the threshold. All shader bytes were restored; 61 prior protected files remain unchanged.
The construction gallery is `Saved/UsfMaterialGallery/Opal-Depth-Study.html`: 21 actual
native captures include a decoded 16-frame changing-view clip. It is explicitly a study,
not a final opal. All three materials have source change/restoration evidence. The studio
logged one handled viewport API ensure; its unpaired override-removal calls were corrected
on disk and static-checked, without another boot. No new result is artist-approved.
See GATE_OPAL_GLINTS.md.

## Current state — additive opal polish ready for visual review

2026-10-22: improved only the opal while protecting the user-accepted seated-glass
display with a 33-entry hash baseline. Two offscreen boots: initial art trials rejected
fracture-like derived normals; the final V2 study uses original generated 1254x1254 RGB
artwork unchanged, five aligned new maps and 21 broad authored orientation regions.
Explicit UE stretch-to-power-of-two builds coordinated 2048x2048 mip-enabled textures.
New V2 definition/master preserves the earlier study and reuses only the AmberHero body.
Saved defaults, eight final/control captures and byte-restored same-MID source reload
pass; compiled material evidence reports two native closures / 12 uint per pixel.
The final receipt passes 39 PNGs, a fully decoded 24-frame 8 fps / 3-second clip, all
33 protected hashes unchanged and clean closure of both owned processes. The result remains an
opaque artistic approximation and milkier than the illustrative target; user artistic
approval is open. No C++/engine/default-config change or new automation result.
See GATE_OPAL_POLISH.md for exact measurements, source provenance and completion records.
Gallery: `Saved/UsfMaterialGallery/Opal-Polish-Gallery.html` (eight local references verified).

## Current state — seated glass and machined aluminum ready for visual review

2026-10-22: addressed the hovering-cover, missing rainbow-bevel and rounded-housing
feedback with additive V5 geometry, two new live-editable shaders and three new material
roles. The 18 mm glass now rests over a continuous rim with a 0.2 mm fit seam, no posts
and 2.1 mm panel clearance. Housing shoulders are flat with small straight chamfers.
RGB thin-film reflectance provides the bevel rainbow; true refractive dispersion and
traced internal optics remain outside this approximation. Opal/panel are read-only reuse.
Final sources were frozen before the fourth offscreen boot. Saved-default checks,
visible controls, byte-restored same-MID shader reloads, 39 native captures and a fully
decoded 24-frame clip pass in the final receipt. All 13 protected files are unchanged.
Both owned processes are closed with a clean retained log. No C++/engine/default-config
change, broad automation suite or publication. Gallery:
`Saved/UsfMaterialGallery/Optical-Seated-Gallery.html`; GATE_OPTICAL_SEATED.md records
reproduction, the four-boot investigation, actual measurements and visual limits.
User artistic review and full mockup parity remain open.

## Current state — optical depth refinement ready for visual review

2026-10-22: additive V4 glass has a real 18 mm section, crisp 1.5 mm chamfers and 8.9 mm
panel clearance; opal adds buried orientation/parallax under two native Slabs. Final
controls, same-MID byte-restored shader probes, 48 motion frames and two decoded clips
are reconciled in the 75-capture receipt. Seven protected hashes remain unchanged.
Initial saved defaults were captured before overrides; final art was verified after
warm source reloads within the sixth boot, not as final-source startup appearance.
Gallery: `Saved/UsfMaterialGallery/Optical-Depth-Gallery.html`; GATE_OPTICAL_DEPTH.md
records limits and reproduction. No C++/engine/default-config change or publication.
User visual approval/full mockup parity remain open. Both owned processes are closed;
retained final log has clean shutdown and zero matched error/ensure/script diagnostics.

## Current state — opal and coated display ready for review

2026-10-22: user accepted Amber-Polish and requested the next two concept materials.
Delivered additive native two-Slab opal and a transmitting coated glass cover over an
illuminated instrument panel. All twelve maps and texture sampling/response remain
live-editable through `.ush`. Two offscreen Toolbox boots verified saved defaults,
native closure counts, shader reload/restore on existing MIDs, ten fresh control frames
and two moving-light clips. Both owned editors closed. No C++/engine/default renderer
change or publication. See GATE_OPTICAL_PAIR.md and
`Saved/UsfMaterialGallery/Optical-Pair-Gallery.html`. Final visual review is pending.

The older entries below are chronological history; the original amber entry has been
superseded by the accepted fidelity and polish work recorded later in this document.

## Current state — amber inclusion specimen ready for review

2026-10-21: refined tail-light accepted visually; user asked to continue. Added a contrasting
polished amber cabochon with actual eight-pair twig/leaflet and separate seed geometry at
different depths. Original authoring generator, five named-role OBJ/MTL sources, seven
authored maps and five additive look definitions/masters; no C++ or engine changes.
Current source totals16480triangles, zero degenerates; all cap faces point+X.
The first source look was too yellow with oversized light reflections; selected a warmer
palette and smaller light-source radii after actual Unreal trials. The convex cap reports
one compiled native closure. Fresh rendered proof passes/restores:82 captures, two decoded
motion clips, optical controls, cap/inclusion visibility and three shader edits/recoveries.
All68 protected prior files and default config unchanged; owned editor/Toolbox closed.
Gallery: `Saved/UsfMaterialGallery/Amber-Inclusions-Gallery.html`.
This remains a staged single-cap/opaque-backing specimen, not traced solid amber; final
artistic review is pending. No new automation-suite result, engine edits or publication.
See GATE_AMBER_SPECIMEN.md for exact scope, measurements and limitations.

## Current state — additive optical finish refinement ready for review

2026-10-21: user liked the first actual tail-light and asked to continue. Added a separate
three-map authored tooling set (rounded optical zones, shallow pillow/fan profiles,
continuous smooth perimeter and thicker webs) plus three new owned lens/reflector/housing
finish assets. Original TailLampV1 resources, definitions and masters are preserved.
The new entrypoint calls the existing live-editable lens sampling/optics function.
Matched trial captures show a smoother cover and calmer reflectors; small pillow-cell
detail remains subtle at the main distance. No C++ or engine changes; existing binaries.
Fresh offscreen rendered proof passes/restores:78 capture records/76 unique PNGs, two motion
clips, matched comparison, LED and source-edit assays. All57 checked original files are
unchanged, default config unchanged, owned editors exited. Gallery:
`Saved/UsfMaterialGallery/Optical-TailLamp-Refined-Gallery.html`.
The normal-flatten assay is close to temporal noise; source recovery is byte-exact while
rendered recovery meets the declared.025 display-RGB bound, not noise-floor agreement.
The first unsettled comparison report is retained. No new automation-suite result is claimed
for this content/shader-only slice. See GATE_OPTICAL_REFINEMENT.md for evidence and limits.

## Current state — optical tail-light prototype ready for review

2026-10-21: user approved building the tail-light direction after reviewing illustrative
concepts. Work is offscreen in this checkout. The selected bounded extension is an opt-in
Adaptive native transmitting Slab, with separate calibration and specimen thickness,
colored transmission, IOR-derived F0 and independently controlled Pixel Normal Offset. The authored lens is one optical sheet
over actual reflector/LED/housing geometry; screen-space refraction is not traced optics.
Actual Unreal gallery: `Saved/UsfMaterialGallery/Optical-TailLamp-Gallery.html`.
Fresh Toolbox gates pass4/4 Adaptive and86/86 Blendable CkUsf tests, zero failures/skips/
contamination. Rendered proof passes with preserved object identity and exact entry/helper
source restoration;96 unique capture files and two decoded orbit videos are retained.
The default renderer config is unchanged; owned processes exited and changes remain local.
This first prototype still needs user artistic review against the more polished concept.
Browser layout inspection was blocked by local-file policy; direct media/ref checks passed.
See GATE_OPTICAL_TAILLIGHT.md. Earlier Adaptive pair accepted as visually promising by user;
silk's intended two-highlight behavior was not clear enough in its presentation.

## Current state — bounded Adaptive hero pair ready for review

2026-10-21: user explicitly approved a separate offscreen Adaptive startup profile and bounded
CkUsf layered-graph extension. Added two retained-Slab templates and texture-backed
dyed-metal paint / two-lobe slubbed silk. The normal Blendable launch, existing native Toon
and prior material assets remain the compatibility baseline. No permanent renderer change,
engine patch, publication or alternate checkout. References: SUBSTRATE_HERO_REFERENCES.md.
Contract and evidence: GATE_ADAPTIVE_BSDF.md. Gallery:
`Saved/UsfMaterialGallery/Adaptive-BSDF-Gallery.html`.

Candy paint layers an absorbing dyed medium over a neutral conductor; satin/slub silk mixes
two independently oriented anisotropic responses. Both retain two compiled closures, with
48/36bytes material payload respectively. Licensed texture backing and authored slub utilities
remain live .ush sampling/math. Silk intentionally enlarges yarn detail about2.9x; its full-ball
yarn identity remains subtle. These are reference-inspired studies, not calibrated scans,
supplied reference assets, a native flake population or neural rendering.

Final incremental build and82/82 focused real-RHI CkUsf tests pass under Blendable (3m30s).
Six Adaptive tests pass in a separate fresh editor (40s), including the reverse profile reload
case. Zero failures/skips/contamination. The first compatibility gate exposed a loaded-material
profile interaction; bulk reload now excludes fully validated opposite-profile native masters
without hiding malformed source/contracts/topology. The prior failing Toon pipeline passes.
Direct preparation stays strict; opposite-profile Legacy without provenance still rejects.

Rendered proof passes/restores:79 captures,48 orbit frames, entry/shared-helper live edits,
isolated coat/thickness/lobe controls, exact original source bytes and restored material/rig
settings. Two3-second videos probe/decode correctly. Canonical display copies preserve decoded
pixels and original proof while excluding UE export allocation bytes after PNG IEND; no engine
patch. The owned editors are closed, original DefaultEngine.ini SHA is unchanged, and work
remains local. User artistic acceptance is pending.

## Current state — advanced native BSDF batch ready for review

2026-10-21: added NativeRubyVelvet, NativeVarnishedWalnut, NativeResinCarbon and NativeCopper.
Explicit Slab fuzz and native SimpleClearCoat extend the existing opt-in native graph contract;
old default layouts/signatures remain unchanged. The four licensed texture-backed surfaces
and every sampling/numeric surface control remain live .ush authoring. Shared-normal coat,
fixed coat F0 and fixed Blendable fuzz roughness are documented limits. No renderer or engine
source change. Gallery: `Saved/UsfMaterialGallery/Advanced-BSDF-Gallery.html`.

Incremental build and76/76 focused real-RHI CkUsf tests pass,3m7s, zero failed/skipped/contaminated.
Nine new tests cover native fuzz/coat pins, malformed options and batch-atomic/identity reload.
Rendered proof passes/restores with zero cleanup errors:28 inspection/control/source captures,
48 orbit phases and four independently probed3-second videos. Native node/master/MID identities,
shader bytes, material overrides, rigs and quality settings are verified. The owned editor quit
after restoring startup state; original DefaultEngine.ini hash remains. Changes remain local;
no user artistic approval is inferred. See ADVANCED_BSDF.md and GATE_ADVANCED_BSDF.md.

A local obsolete module-manifest advertisement stalled the first showcase startup; noninvasive
stacks proved a missing entry point in the old DXT DLL. Two receipt-excluded texture-format
advertisements were removed from backed-up generated metadata. All DLLs/artifacts retained.
The next boot succeeded. Preparation/proof tooling issues were corrected before the final
rendered run; failed diagnostics are preserved, not counted as successful proofs.
The earlier first-pair and engine publication record below remains historical evidence.

Final native slice update: persistent Slab and Toon profile/definitions/masters and both gym
stations were created and verified through the offscreen studio. Factory and refined three-level
Toon rendered proofs both passed, including exact shader-byte restoration, same native nodes,
masters and MIDs, unchanged material overrides and restored quality/light state. Gallery:
`Saved/UsfMaterialGallery/Native-BSDF-Gallery.html`. Final proof:
`NativeBsdf-ThreeLevel-RenderedProof.json`; retained baseline: `NativeBsdf-RenderedProof.json`.
Matched close/macro/gameplay captures and24-phase Point-light previews are actual UE renders.
Native Toon diffuse ramp was refined through a saved external native curve; the factory profile
is preserved separately. Textures, specular/GI settings, lights and exposure stayed fixed.
One offscreen showcase boot served both variants. The owned editor ended PIE/restored startup
settings and quit. No user artistic approval is inferred. See NATIVE_BSDF.md for contracts/limits.
This final update supersedes pending-asset/rendered-proof wording in the historical notes below.

The existing engine was upgraded in place to5.8.4 and publication is independently verified by
`ls-remote`: origin/main-ck is `55c44c498c272fcc1f911d1c90e9885184b026f3`, replacing the
approved previous tip `7a4ad7f53ee2d472c778bc7748ca753f396a9108`. The exact approved lease
retry reported that the ref already matched the candidate; the independent remote query establishes
delivery. Protected unrelated edits remain. No association/storage change, extra checkout, cleanup,
cook or package. User-approved temporary serialized builds preserve the5.8 scheduler and query sharing.

VERIFIED: focused C# gate83/83, zero failed/skipped,35s; native BSDF incremental Build2 exit0;
fresh offscreen real-RHI CkUsf gate67/67, zero failed/skipped/contaminated, actual exit0,3m13s.
Evidence: `Saved/Logs/Engine584-CSharp-Focused2.log`, its focused UBT TRX, and
`Saved/Logs/NativeBsdf-Build2.log` / `NativeBsdf-CkUsf-Gate1.log` with process receipts under
`Saved/EngineUpgrade/20261020-UE584`. Temporary diagnostic config is restored byte-for-byte.
The announced native allowance was up to3 discovery/gate boots plus1 offscreen showcase;
only1 native gate boot has been consumed. Showcase asset preparation/rendered proof remains pending.

Earlier upgrade coverage remains103/103 across seven focused Unreal gates, reconciled in
`Saved/EngineUpgrade/20261020-UE584/Unreal-gates-final.json`; the67-test native gate is a later
focused rerun including six added native BSDF tests, not170 distinct tests or a full-suite result.
The original VF collision and failed build attempts remain preserved. Prior156 dependency files
and18 diorama binaries matched retained hashes at that earlier checkpoint.

Native Slab/Toon contract, generator/reload implementation, six focused tests and two ceramic
source examples are authored. NativePorcelainSlab and NativeCeramicToon use licensed Tiles133A
maps with source-aligned grout; native profile/LookDefinition/master assets and matched visual
captures are not yet accepted or claimed complete. The single Slab is not a physical multilayer
glaze; Toon ramps/hatching remain profile asset data. Preserve Blendable, texture-backed numeric
.ush live editing and the approved logo-free ball/lights/exposure. See GATE_NATIVE_BSDF.md and
ENGINE_UPGRADE.md. No broad regression, packaged, performance or native showcase visual claim.
Startup SDK validation under a shared editor hold remains a separate write-path review item.

### Retained diorama batch

Grass/dirt/mud diorama batch is delivered locally and technically verified (2026-10-19).
See DIORAMA_PATCHES.md. SelectedV2 source art/maps and packed blends pass source/calibration
checks; all three native same-object proofs pass96captures/12reloads with exact shader/state
recovery. Grass deliberately uses compressed1cm depth/mip2 to avoid connected foliage columns;
source range remains5.262cm. Dirt/mud retain2.877/3.542cm relief. Orbit followup passes9captures
with all3newground looks, opposed-light response above repeat noise and exact rig/material
recovery. Fresh real-RHI GeneratesUsableMasters passes1/1 in1m5s, zero failures/skips/contamination.
Independent reconciliation passes96source/control frames,9orbit frames,18newbinaries and396
protected earlier files. Gallery: Saved/UsfMaterialGallery/Diorama-Gallery.html.
User explicitly permits existing tools despite missing canonical
CkTextureGeneration package. All work stays offscreen; no user art approval is claimed.

### Retained river-pebble result

The user rejected WoodlandV3 visually and supplied a27.73s pebble reference clip (2026-10-19).
Technical proofs below remain valid within their scope; they are not artistic acceptance.
Delivered locally: dense CC0 scan-backed rounded river pebbles with POM and actual vertex-height
stations, plus one reusable Point inspection light following the selected station. I/P/K toggle,
pause and reset; N/B retarget. Normal studio intensities restore exactly when disabled.
RiverPebbles-Proof passes32frames/4same-object reloads; OrbitLight-Proof passes47captures,
real-time tick/pause, neighbor bounds, retarget and restoration checks. Actual respawn and active
EndPlay/GC/warm-reopen pass separately. Fresh real-RHI GeneratesUsableMasters passes1/1;
independent reconciliation passes6 active new binaries and389 protected earlier files.
Gallery: Saved/UsfMaterialGallery/RiverPebbles-Gallery.html. Video is36actual paused UE frames,
9seconds at4fps,2x nominal orbit speed. All work stayed offscreen; no user art approval claimed.
Astra consultation was attempted but failed at model capacity; no successful consultation claimed.

### Retained previous technical result

Batch7 and the bespoke woodland prototype are delivered locally and technically verified
(2026-10-19). User artistic acceptance is separate. Galleries: Saved/UsfMaterialGallery/
Batch7-Gallery.html and Woodland-Gallery.html. No commit or push was requested or performed.

Batch7 adds technical synthetic weave (polyester-backed stable LibraryNylon ID), slate, clean
copper, glossy plastic, candle wax and user-approved photo-derived fiberglass. Selected fine-weave
v2 and finish trials are retained beside all prior candidates. Same-instance .ush entrypoint/shared
proof passes30captures/14reloads with42 library MID/master pairs and exact seven-source recovery.
Controls pass42frames;36 native/TAA lit views retained. Independent Batch7-Verification.json passes
24 active new assets and all350 protected earlier hashes, bindings, source/capture/restoration/gate.

User rejected generic POM gravel; selected WoodlandPatchV3 composes22 curved photo leaves,
flowers, textured stones, twigs, moss and scanned soil into one non-repeating80cm patch. Five aligned
1K maps include actual16-bit geometry height (2.48012793cm range), ray AO and full geometry normals.
Packed .blend,32-bit EXR masters, licensed photo/scan sources and rejected drafts are preserved.
EnvironmentWoodland .ush plus owned256-cell grid drive POM and true vertex-displacement stations.
Ground-only existing Rect placement is closer to the horizontal patch; intensities and approved
shader-ball presentation remain. No separate leaf undersides/overhangs survive a height field;
low-angle WPO views deliberately expose the filled-down edges. POM does not alter raster depth,
silhouette or native shadows; collision remains unchanged by GPU vertex displacement.

Woodland-Proof.json passes32captures/4reloads: same two MID/master pairs/bindings, POM UV response,
actual WPO depth response, pixel sampling/color and VS-height edits, relative visual recovery and
exact source/state restoration. Debug-matched repeat noise is recorded; effects must exceed3x
measured mean noise. No exact pixel-equality claim. Woodland-Verification.json independently
recomputes metrics/images, validates selected maps/EXRs/packedsource/generator,7 new asset binaries,
all350 protected hashes and fresh gate. Earlier generic endpoint failure stays failed/restored;
backbuffer-dither suppression did not establish its cause. No blanket renderer/crash fix is claimed.

Fresh offscreen real-RHI GeneratesUsableMasters passes1/1, zero failed/skipped/contaminated,
actual exit0,1m31s. Both independent reconciliations pass. Exactly two editor boots (warm studio,
fresh gate); owned studio ended PIE, GC collected33, quit, and all four owned UE/Toolbox PIDs exited.
Final AA0/screen87, diagnostics off, parameters and shader bytes restored. No desktop input, live
Blender MCP, C++ build, engine/config change, cook/package, cache deletion, extra checkout or Git
delivery. Isolated Blender CLI CPU4 authored the source. Unrelated Iris hash unchanged.

### Batch 6 retained result

Batch 6 is delivered locally and technically verified: linen, smooth leather, glazed hexagonal mosaic, coated pine, limestone and galvanized steel. Nineteen licensed texture assets and six masters retain live .ush sampling, tint, normals, roughness, UVs and specialized controls. Sources/defaults/costs/limits in BATCH6.md; gallery Saved/UsfMaterialGallery/Batch6-Gallery.html. User artistic approval remains separate.

Final selected-default source proof passes30captures/14reloads with all36 library master/MID pairs and bindings/overrides preserved, exact seven-file recovery. Six control proofs pass42frames with complete restoration; dedicated mosaic glaze/grout proof passes.36 final native/TAA lit views are saved and inspected. Fresh offscreen real-RHI GeneratesUsableMasters passes1/1, zero failed/skipped/contaminated, actual exit0,59s. Batch6-Verification.json passes25newassets/all318protected earlier hashes unchanged and source/control/texture-setting/capture evidence reconciled.

Linen blue BC was auto-detected as a normal map; compression/semantic LOD setup now precedes sRGB in the batch-six importer, with strict saved-setting verification. Initial failure retained. Galvanized normal0.025/roughness scale1.2 were selected through matched source-backed trials and bounded visual review; the complete earlier low-roughness candidate is retained. All final proofs reran after selection. Studio PID59340 ended PIE, GC collected33 and quit; renderer/material/light defaults restored. Two actual boots, no C++/engine/config change or new presentation light, no desktop input, live Blender, cook/package, cache deletion, extra checkout, commit or push. Iris hash unchanged. Earlier Python/DDC/HighResShot incidents remain retained, not fixed.

### Batch 5 retained result

Batch 5 is delivered locally and technically verified: denim, smooth concrete, clean brick, satin stainless finish, plywood edge and cardboard. All six use licensed BC/normal/roughness maps and live .ush texture sampling, normals, color, roughness, UVs and specialized controls. Eighteen imported textures/six masters; exact source identities/defaults/costs/limits in BATCH5.md. Gallery: Saved/UsfMaterialGallery/Batch5-Gallery.html. User artistic approval remains separate.

Source proof passes with 30 native captures/14 reloads, all six entrypoint sampling/math edits plus shared helper, preserved thirty library master/MID pairs and bindings/overrides, exact seven-file recovery. Six control proofs pass with 42 captures and full restoration; 36 native/TAA lit views are saved and the contact sheet inspected. Fresh offscreen real-RHI GeneratesUsableMasters passes 1/1, zero failed/skipped/contaminated, actual exit0, 1m5s. Batch5-Verification.json passes: 24 new assets, all287 protected prior hashes unchanged, source/control/capture evidence reconciled.

Studio PID50868 ended PIE, GC collected33 and quit cleanly; AA0/87%, no diagnostic/preview state and all material defaults restored. Two actual boots, no C++ rebuild or additional presentation light, no desktop input, engine/config change, live Blender, cook/package, cache deletion, extra checkout, commit or push. Iris source hash unchanged. Steel alloy is unverified and its satin response is authored; source normals are nearly flat. Plywood is layered edge, and brick preserves native2:1 map aspect. Earlier Python/DDC/HighResShot incidents remain retained, not fixed. This focused gate does not replace earlier full-module/native-Toon gate evidence.

### Batch 4 retained result

Batch 4 delivered locally and technically verified: knitted fabric, patent leather, porcelain-style glazed tile, terrazzo, translucent plastic and cork. All six are texture-backed and live-editable through .ush. Nineteen imported textures/six generated masters; sources, defaults, costs and limits are in BATCH4.md. Gallery: Saved/UsfMaterialGallery/Batch4-Gallery.html. User artistic approval remains separate; no next batch or layering expansion is implied.

Source proof passes with 30 native captures/14 reloads, six texture-sampling/math edits plus shared helper, same twenty-four library master/MID objects, preserved bindings/overrides and exact seven-file recovery. Near-black patent leather uses sourced roughness for measurable debug proof; its rejected albedo baseline is retained. Six control proofs pass with 42 frames and complete restoration. Native/TAA plastic pane controls and source-aligned porcelain glaze/grout controls pass. Plastic alone receives a bounded local 80 cd Point fill; neighbor bounds/captures, diagnostic restoration and explicit GC retention pass. Runtime setters avoid the editor property mutation that reconstructed the actor and removed that fill.

Fresh offscreen real-RHI GeneratesUsableMasters passes 1/1, zero failed/skipped/contaminated, actual exit 0, 54s. Batch4-Verification.json passes: 25 new assets, all 255 protected prior hashes unchanged, 36 final lit views, source/control/specialty/fill evidence reconciled. Unrelated Iris source hash unchanged. Two actual fresh boots, no C++ rebuild, desktop input, renderer configuration change, cook/package, cache deletion, additional checkout, commit or push. Studio ended PIE, GC collected 33 and quit cleanly. Earlier Python/DDC/HighResShot incidents remain unclaimed as fixes. This focused gate is not a new full-module or native Toon reload acceptance.

### Batch 3 retained result

Batch 3 delivered locally and technically verified: velvet, finished walnut, polished granite, satin brass, frosted glass and carbon fiber. All six are texture-backed and live-editable through .ush. Seventeen imported textures/six generated masters, recorded CC0 source identities and reuse limits, final defaults/costs and retained candidates are in BATCH3.md. Gallery: Saved/UsfMaterialGallery/Batch3-Gallery.html. User artistic approval remains pending.

The explicitly approved opt-in SceneColor generator extension passed incremental C++ build (79.71s, exit 0) and real pane controls. Frost uses 64 fixed symmetric linear-HDR taps, radius 8/max 12, over copied opaque scene color, with separate material-local fog policy. It is a screen-space approximation, not thickness-traced optics. Both new native contract/reload tests pass; unchanged material graphs retain default scene-copy usage. No engine or renderer-mode change.

Final source proof passes with 30 captures/14 reloads, all six entrypoint sampling/math edits plus shared helper, the same eighteen library masters/MIDs, preserved scalar/vector/texture bindings and exact source restorations. Dedicated frost-helper proof and zero-radius/bypass/pane detail controls pass. Six control proofs pass with 42 diagnostic frames and complete restoration. 36 final lit views preserve separate native AA 0/87% and temporary TAA 2/100% captures. Studio ended PIE, GC collected 33 and quit cleanly; renderer/light/material diagnostic state restored.

Fresh offscreen real-RHI CkUsf gate passes 60/60, no failed/skipped/contaminated tests, actual exit 0, duration 5m22s, including native Toon pipeline/global-lighting reload. Independent Batch3-Verification.json passes: 23 new assets and all 207 protected previous asset/shader hashes unchanged; unrelated engine Iris source hash also unchanged. Five actual fresh editor boots: initial studio, post-build studio, headless discovery, renderer discovery and gate execution. The missing-include first build, initial sharp frost, sparse-kernel artifact and dark-carbon harness rejection remain documented. Earlier Python/DDC/HighResShot incidents are not claimed fixed. No desktop input, cook/package, cache deletion, additional checkout, commit or push.

### Batch 2 retained result

Batch 2 delivered locally after the user accepted the linear aluminum refinement and authorized continuation. Six new texture-backed materials: fine cotton/poplin (stable LibraryCanvas ID, not heavy canvas), suede, terracotta floor tile, polished marble, hard rubber and clear thin glass. Seventeen new texture assets/six masters; exact licensed source identities and limits in BATCH2.md. Gallery: Saved/UsfMaterialGallery/Batch2-Gallery.html. Batches 3ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“4 remain pending; batch 2 user artistic approval is not inferred.

Final native game-viewport source proof passed: all six entrypoints plus shared dependency, 30 actual captures, same twelve library master/MID objects, scalar/vector/texture bindings preserved, seven exact source restorations. Six control proofs passed with every override restored. Fresh offscreen real-RHI GeneratesUsableMasters passed 1/1 (48s, exit0), no skipped/contaminated tests. Independent Batch2-Verification.json confirms 23 new assets and all184 protected baseline asset/shader hashes unchanged. Two fresh editor boots, no C++ rebuild, desktop input, cook/package, cache deletion, commit or push.

Retained limitation: an earlier HighResShot shared-helper glass recovery lost its translucent shell while reload reported 0 failures. Native Shot source proofs subsequently passed completely; no engine/readiness fix is claimed. Failed report and22 original frames remain under Saved/UsfMaterialGallery/InitialFailure. Glass uses thin-surface transmission, not thickness-traced optics; optional pane comparison verifies transmission without changing the approved ball. Full iteration evidence, including corrected capture/proof overlap and baseline-check error, is in BATCH2.md.

### Earlier batch 1 evidence (retained)

2026-10-19 local. User approved the revised breadth-first plan. Batch 1 is implemented: crepe satin silk, pebbled leather, glazed ceramic tile/grout, finished oak, linear-brushed aluminum and matte molded plastic. Six generated masters, 25 retained imported texture assets (22 active bindings) and all .ush/AS sources exist. Sources, controls, costs and limits are in BATCH1.md. The approved starting roster has four mixed six-material batches. Batch 2 implementation and verification are recorded below; batches 3ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ4 remain pending.

Batch 1 build passed (70.98s, actual exit0). Live source proof passed in PID39072: 30 captures, all six entrypoints plus shared helper, same six master/MID objects and preserved scalar/vector/texture overrides, exact seven-file byte recovery independently checked. Compatible Point-light direction proof passed for silk/aluminum with normal/roughness variation neutralized; every override and three Rect intensities restored, owned diagnostic Point intensity zero. The opt-in anisotropy/tangent extension preserves default graphs' feature usage. Final artistic acceptance remains the user's.

18 baseline and 18 temporary-TAA preview captures show close/macro/gameplay views. Native project AA=0/screen percentage87 were restored after TAA2/100% previews; no project renderer configuration was edited. Gallery/contact sheet are host Saved/UsfMaterialGallery/Batch1-Gallery.html and Batch1-ContactSheet-TAA.png. No mouse/keyboard/clipboard/focus control; one offscreen real-RHI studio boot through Toolbox, ended PIE and quit cleanly. Python+UE GC passed before and after shutdown; earlier Python crash is not claimed fixed.

Fresh focused gate records 56/57 passed, 1 failed due editor death, zero skipped/contaminated; actual Toolbox exit1, duration4m2s. The original native Toon global-lighting reload test crashed in HttpConnectionPool/DDC IO worker threads at DerivedDataRequestOwner.cpp:105 (RequestPtr assertion), distinct from earlier python311 crash. Toolbox automatically resumed the remaining16 tests in a second test editor. Logs/crash preserved: Saved/Logs/UsfLibrary-Test.log and Saved/Crashes/UECC-Windows-B79E8CF440B93D2DCBE52DA1F1BA71FA_0000. Both Toon shader hashes match baseline after the crash.

Unchanged isolated fresh retry passed 1/1, zero failed/skipped/contaminated, actual exit0, 45s. Log Saved/Logs/UsfLibrary-ToonRetry.log, receipt Saved/UsfMaterialGallery/Batch1-ToonRetry.exit.json; PID40284 recovered the same Toon MID from gain0 to meanRGB1.014648. All57 selected tests now have a passing result across the runs, but the original complete run remains 56 passed/1 crashed, not an uninterrupted green gate. Astra identified a DDC request registration/completion mismatch as the concrete assertion meaning; duplicate completion/wrong owner/missing registration/corruption remain candidates, not a proven root cause or cache-corruption claim. No engine/cache/settings edits or known-red change. Four actual new batch editor boots: studio, original gate, automatic gate recovery, isolated retry; original plan was two and both extra boots were explained.

Industrial family remains implemented and technically verified; user artistic acceptance pending. Its review does not block the revised roster. This is not completion of the wider library. Root c66d212, Foundation a232a0288, Tests c33051d4. Initially clean submodules; local continuation prompt preserved. No commit/push authorized or performed.

Verified unchanged gym: Expansion-BaselineClose.png and Expansion-BaselineWide.png under host Saved/UsfMaterialGallery. Live r.Substrate=1 and ProjectGBufferFormat=0. Engine association resolves UE5.7.4. Existing accepted lighting/geometry retained.

IndustrialPaint/IndustrialSurface, one LookDefinition, four gym preset MIDs and the deterministic 1024-detail/256-placement palette are imported/generated and rendered. Final artist settings, costs and limitations are in INDUSTRIAL.md. One native master and two texture assets are new; baseline specimen and six starter/Toon assets are byte-identical to their opening versions after reverting only this session's bootstrap resaves.

Fresh serial real-RHI CkUsf gate: 57/57, zero failed/skipped/contaminated, 2m24s, actual Toolbox exit 0. Host log Saved/Logs/UsfIndustrial-Final.log, exit receipt Saved/UsfMaterialGallery/Industrial-Gate.exit. Matches the continuation's recorded 57/57 focused baseline; no repository-wide regression claim. GeneratesUsableMasters includes the new declared look; native Toon/declaration/global reload tests pass. No shader compile failure, ensure-condition failure or AS compile failure was found in the focused log.

Final live proof: editor PID44560, 20 genuine PIE game-viewport captures, entrypoint edit/recovery and transitive IndustrialSurface edit/recovery. All four MID and master equality checks pass throughout. Every scalar, paint/primer color and both texture bindings remain equal. Source bytes restored exactly; report Saved/UsfMaterialGallery/Industrial-LiveReloadProof.json. Initial PID8480 proof passed the earlier smaller override snapshot; final review added colors/textures/scratches and repeated it in the review process. Temporary source changes are not left in production.

The review editor is left in USF Materials, framing Workshop ochre. N/B include four new presets; neutral CoreBase remains separate. Close, gameplay-distance, roll, metallic and roughness captures are under Saved/UsfMaterialGallery. The initial art review caught broad primer borders and partly metallic rust; thinner borders, smaller Workshop/Coastal clusters and independently thresholded oxidation were rendered before acceptance checks.

## Renderer boundary

Initial inspection found no general anisotropy/tangent output. Approved batch work now adds numeric O.Anisotropy/O.Tangent, gated by LookDefinition _DirectionalShading=false. Only compatible opaque/masked SurfaceLit DefaultLit/ClearCoat opt in; schema changes require regeneration and reject reload atomically. Native Hair remains separate. New compile/wiring/invalid/reload tests passed in the focused run before its later DDC crash; original Cloth/ThinTranslucent coverage remains. Current Substrate Rect integration ignores anisotropy; compatible-light rendered proof is recorded separately.

Engine RenderUtils.cpp enforces one closure/20 bytes under Blendable. Retained arbitrary multislab needs a separate Adaptive decision, which conflicts with current Toon acceptance. Current hero uses surface-region composition and the supported legacy ClearCoat response under Substrate conversion. It does not author a FrontMaterial slab graph. No renderer settings/engine changes.

## Decisions

- 2026-10-19: user's clarification supersedes industrial-first priority. Diversity and texture-backed quality are primary acceptance criteria; four aging presets do not count as four families. Existing procedural packed maps do not fulfill the wider BC/normal/roughness texture library. Revised PROMPT/PLAN and this scope note are documentation only: no editor boot or test. Astra read-only review supports the six-material first batch and a bounded investigation of general anisotropy/tangent wiring; rendered support remains unverified.
- Subsequent approval/execution: six actual licensed PBR texture sets with source-specific maps, not a shared noise palette. Shader-specific responses, independent grout/glaze, oak coat, directional silk/aluminum. Independent source/parameter/import audit found no blocker. Astra art critique led to smaller tile cells, gentler satin normal and higher aluminum roughness; distant views now clear station signs. Rendered directional support is verified under a compatible Point, not the ordinary Rect setup.
- Astra art review: quiet industrial enamel, narrow primer borders, steel-only metallic, rust localized to exposed regions, distinct roughness and granular relief. Four presets: factory teal, workshop ochre, coastal blue, abandoned olive.
- Original procedural packed palette avoids external license uncertainty. Receipt in CkTests Resources/CkUsf/IndustrialPalette; source generator in host Tools/UsfMaterials. This is an initial controlled palette, not proof that procedural assets cover the wider wishlist.
- Avoid current Common.ush NormalFromHeight screen derivative helper; use UV finite differences for tangent microdetail.
- Wear is clustered UV damage, not curvature-driven edge wear; no fabricated curvature claims.
- Source shader 5 detail + 1 placement samples. Material API reports 445 pixel/296 vertex instructions, 5 samplers, 2 UV scalars; its 0 pixel/3 vertex sample counter does not account reliably for Custom-HLSL fetches. ListTextures records BC3 allocations 1408+128 KB with 11+9 mips. No timing benchmark or draw reduction is claimed.

## Validation incident

Initial desktop input attempts interfered with the user's concurrent work. Stopped all mouse/keyboard/clipboard automation. Temporary Content/Python startup hook was removed after starting a session-local Python queue; captures succeeded without desktop input.

PID47204 crashed at 06:36:04 UTC (23:36:04 local) through PythonScriptPlugin/python311 before the pending StopPIE request was consumed. Crash preserved under Saved/Crashes/UECC-Windows-EF4E9BE947E913DCEF516B85DFBD2E22_0000. Astra identified Python GC/wrapper lifetime as a source-grounded candidate, potentially exercised by AS reinstancing, but available unsymbolized frames do not prove the cause. No engine patch was attempted. The replacement persistent-module workflow passed capture, explicit Python+UE GC, PIE shutdown and post-PIE GC with source schema frozen; this is bounded avoidance, not a crash-fix claim.

The first automated screenshot proof was rejected: AutomationLibrary targets the first active editor viewport, so its sky-only images did not prove PIE pixels. HighResShot routed through the actual PIE player/GameViewportClient removed that ambiguity. A fresh nonuniform specimen baseline is required before source mutation. Request IDs prevent stale response acceptance; numerical actor-name sorting preserves preset order after repeated respawns; unknown concurrent source changes are preserved. Final shader hashes match the proof's exact restoration hashes.

Boot accounting: three interactive launches (first input-control attempt, temporary-queue baseline, stable replacement), one fresh serial final gate, and one normal review launch. Every added boot was explained. No C++ build, cook, package, renderer switch, linked checkout, cache/artifact cleanup, commit or push. Original Iris hash still 69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92. Both original Toon HLSL hashes also match the continuation receipts.

## Open items

- User artistic inspection of the first family in the opened gym; no artistic approval is inferred from tests.
- Original Python GC crash root cause remains unproven; stable-schema input-independent validation is available.
- Dedicated industrial panel/edge-wear inputs, chip thickness and exact extreme-distance coverage are not implemented. Current specimen has approved beveled/concave geometry and useful matched light reflections.
- Batch 1 user artistic inspection remains pending; gallery is ready. Later mixed batches are not implemented. The native DDC/HTTP worker crash did not reproduce in the isolated retry, but timing/cache warmth/test-order dependence remain unresolved. Preserve evidence; no engine fix is claimed. Adaptive/multislab remains a separate decision.

## References inspected

Epic Substrate overview, Epic Layering Materials, Adobe Dynamic Material Layering and Bradford Smith's GDC17 Paragon pipeline page were opened. Current Epic page defaults to 5.8; engine 5.7.4 source is authoritative for this implementation. External references inform workflow, not runtime acceptance or license grants.

Aluminum review refinement: coherent linear machining replaces the rejected circular reflection patchwork. Active versioned source receipt, matched captures, same-MID .ush edit/recovery and fresh 1/1 real-RHI generation gate are recorded in ALUMINUM_REFINEMENT.md. Original textures/captures remain preserved; user accepted this aluminum revision and authorized the next mixed batch.

## Amber fidelity correction (2026-10-21, in progress)

The user rejected the initial amber gallery as unlike the illustrative concept.
The old functional proof is historical and does not establish visual acceptance.
Astra identified an opaque rear support, regular twig, missing suspended inclusions,
point-source reflections and PNO's lack of curvature lensing. A separate hero now
uses a closed pebble, CC0 photographed masked leaf, 59 bubble surfaces, 65 fragments
and elongated studio reflections. Prior tail-light assets and graph 6 remain intact.
Graph 7 adds opt-in F0-derived native refraction; focused build/test and final hero
capture are being validated. This remains approximate raster optics, not traced
multi-interface transport or achieved concept fidelity. No publication authorized.

Final correction review is now captured in Amber-Fidelity-Review.html using
AmberHero-Fidelity-Final.png. The persistent graph6 body has a valid one-closure
compiled resource with no compile errors. The final owned studio exited; its log
has no Error/ensure/AS-compile/fatal/reload-failure markers. DefaultEngine.ini stays
unchanged. Graph7 remains visually unaccepted after separation failed to resolve
self-refraction; working PNO hero is the default. See GATE_AMBER_SPECIMEN.md.

## Accepted amber polish (2026-10-21)

User called AmberHero-Fidelity-Final.png "so much better" and authorized further
improvement. Preserve that accepted capture (SHA256 ed79b6829be05b41e4a510ca1d7feec6d0c0e697feb7f6c83218a8dbc95694a0).
One offscreen Toolbox studio, existing binaries, six controlled visual comparisons.
Selected LeafQuiet: rear light source radius .2 -> 1.2cm; leaf TintColor .65 and
TransmissionStrength .45; sediment TintColor (.10,.055,.02), debris cast-shadow off.
The accepted camera/amber material/maps/geometry remain. Lower camera rejected
because the upper body became too dark. Independent image review agreed with the
selected variant. These defaults now live in amber_hero_scene.py; bounded warm
capture controls record leaf settings and shadow states without another editor boot.

Final: Amber-Polish-Gallery.html, AmberPolish-Final.png, AmberPolish-Receipt.json,
AmberPolish-Probes.json, AmberPolish-FinalState.json, AmberPolish-Studio.log under
Saved/UsfMaterialGallery. The accepted image hash remains exact. Six probe hashes
validated; final Body shader resource valid, no compile errors, same master/MID
through probes. Python syntax check passes. No C++/.ush edits or automation rerun;
prior 7/7 optical contract result remains historical. Owned studio closed, no
Error/ensure/AS/fatal markers, DefaultEngine.ini unchanged. No new optical capability,
performance result, or user acceptance of this additional polish is claimed.



