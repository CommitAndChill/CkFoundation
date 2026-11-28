# Three-layer single-mesh opal

2026-10-22. User approved the bounded NativeTranslucentStack3 extension. The exact
earlier OpalPolishV2 RGB artwork is preserved; new numerical data provides 550
full-area optical orientations without a chroma/coverage exclusion. This is an
authored reflection approximation, not measured diffraction or geometric crystals.

## Implementation and technical evidence

Enum9 adds a separate 25-field output contract. Native A over (B over C) separates
clear polish, oriented colored reflection, and a smooth cloudy transmitting body.
Three independent MFP conversions and two non-blended Vertical operators preserve
the three BSDFs. Existing graph contracts remain separate. No engine-source changes.

Incremental UnrealToolbox build passed (34 actions). Fresh real-RHI Adaptive gate
passed 16/16, and fresh Blendable CkUsf compatibility passed 99/99. Both report zero
failed/skipped/contaminated tests and process exit 0. Three-layer fixture and numeric
reload compiles request and retain three closures, without budget simplification;
observed payload is 20 uints. This is not a GPU timing/performance claim. Fresh editor
logs contain no fatal, ensure, AngelScript compile, shader or material errors.

Evidence under Saved: Logs/OpalInternal-AdaptiveGate.log and
Logs/OpalInternal-BlendableGate.log; UsfMaterialGallery/OpalInternal-*-process.json.
The Adaptive summary is at log lines 16246-16251 (16 passed, duration 1m 1s);
Blendable is at lines 60180-60185 (99 passed, duration 3m 36s). The corresponding
process receipts explicitly record exit 0 at 21:20:33Z and 21:25:02Z. These clean
gate logs precede the separate optional shader-debug crash described below.

## Rendering diagnosis, preserved evidence

Native screenshot capture succeeded, but the three-layer result was matte and lacked
the expected crisp outer-coat reflection. Capture completion is not appearance approval:

- `Saved/UsfMaterialGallery/OpalInternal-Initial.png` and `.json` preserve the initial
  native three-layer result.
- `Saved/UsfMaterialGallery/OpalInternal-Stack3Compare.png` and `.json` preserve the
  three-layer comparison. The actual material reports three closures, 20 uints,
  no glints and no compile errors.
- `Saved/UsfMaterialGallery/OpalInternal-Layered2Compare.png` and `.json` preserve the
  older two-layer material's crisp response on the same Body actor and studio rig.
  This is an appearance control, not a replacement for the approved three-layer design.
- `Saved/UsfMaterialGallery/OpalInternal-SmoothC.png` and
  `OpalInternal-SmoothC-Probe.json` record the C-only roughness probe. Smoothing C
  restored gloss in that probe; the source was restored to the exact original SHA256
  `cdb1536bc9c5ab133dc7a041034b397d2dd90ed4c3221e660d752b4fa876cb12`.
  This observation does **not** establish a C-to-A roughness alias or its root cause.

The generated master export at
`Plugins/CkFoundation/Content/CkUsf/GeneratedLooks/M_CkUsf_Look_OpticalOpalInternal.ckexport`
has the intended 25 Custom outputs. Each native Slab consumes its own roughness at
input 3, F0 at input 1 and normal at input 5; F90 input 2 is disconnected. The Custom
wrapper assigns all 24 additional outputs and preserves parameter order. The two
Vertical operators form A over (B over C), without parameter blending. This source
and export audit found no skipped roughness or incorrect F90 connection.

An optional warm shader-debug validation attempt crashed after enabling
`r.ShaderCompiler.DebugValidateJobCache=true`: the assertion was `CachedJobName` in
`Engine/Source/Runtime/Engine/Private/ShaderCompiler/ShaderCompilerJobCache.cpp:2130`.
The preserved log is `Saved/UsfMaterialGallery/OpalInternal-ShaderDumpCrash.log`
(assertion at lines 3698 and 3705). This diagnostic attempt did not invalidate or
replace the earlier automation results, and no engine-source change was made.

A subsequent source-only dump with job-cache validation off succeeded; receipt:
`Saved/UsfMaterialGallery/OpalInternal-ShaderDumpSourceOnly.json`. The real native
preprocessed HLSL is retained at:

`Saved/ShaderDebugInfo/PCD3D_SM6/M_CkUsf_Look_OpticalOpalInternal_91f5ef17e3f27517/Default/CkIskmVF4/BPPSFCachPointIndLP/0_164bf4914e396f0e/BasePassPixelShader.usf`

That dump is a diagnostic smooth-C variant (`O.CRoughness=.025`, line 14172), not
the original `.35` body. Its wrapper keeps A and C assignments separate (lines
14198 and 14208), its Slab calls consume independent A/B/C roughness locals
`Local10`/`Local15`/`Local20` (starting at 14522/14555/14588), and nested native
Vertical calls remain at 14619-14620. This rules out an alias in the inspected
generated HLSL; it does not prove final lighting evaluation or the visual root cause.

The approved earlier RGB artwork and protected prior assets remain outside this
diagnostic mutation scope. No engine extension, separate shell or new physical
diffraction implementation is part of this campaign.

## Final native study and reusable preset

`Saved/UsfMaterialGallery/Opal-Three-Layer-Gallery.html` is the new additive review
gallery. It presents actual native 1600x1200 primary viewport frames and a four-second,
48-frame, 12 fps moving-light MP4. No image synthesis, recoloring or interpolated frames
were used for those outputs. Original RGB artwork remains SHA256
`7d66d3ff5ca042e4ffa170eeb844f18aaac921a4bba2d591030b2d41b59660b6`.

The new reusable instance is
`/CkFoundation/CkUsf/Instances/Optical/MI_OpticalOpalInternal`, parented to
`M_CkUsf_Look_OpticalOpalInternal`. Its seven scalar and two vector overrides come from
`Tools/UsfMaterials/opal_internal_preset.json`; all five textures are inherited. The
original definition/master recipe guards were not changed. Preparation verifies exact
ownership, parent, local override roster, all 16 effective scalars, three vectors and
five textures. It rejects enabled base-property overrides and static switch parameters.
Receipt: `OpalInternal-InstancePreparation.json`.

The source uses a smooth A polish with 0.03 cm optical thickness, a separate textured
orientation field for B reflections, and a smooth C interface around the cloudy body.
Native A IOR changes clean white highlights independently; B reflectance changes the
irregular internal flashes; C transmission changes the cloudy contribution. The body
uses roughness .025; this is an authored smooth interface, not a claim that the initial
roughness observation established a renderer defect. All emissions remain zero.

Final shader SHA256:
`7c32de6dc1dd6634331b75b89296cf70cc80645558c5c64856e2db66c8b83149`.
Texture sampling, numerical orientation, roughness, colors and optical thicknesses stay
in `OpticalOpalInternal.ush`. Its optical domains cover the whole surface; there is no
permanent chroma/coverage exclusion or camera-only color activation gate.

## Native rendering and live-source evidence

- `OpalInternal-Final.png` inherits the saved preset without material overrides in the
  capture request. Its receipt records the persistent instance and exact preset hash.
- `OpalInternal-Final-NoFlash.png`, `-LowCoat.png`, `-BodyTransmission.png`,
  `-LightsOff.png` and `-Restored.png` vary only the named material/lighting controls.
  All six retain the same mesh, master and MID, three closures, 20 uints, no glints,
  no ComplexSpecial and no compile errors. Restored matches Final material/rig state.
  The lights-off frame has maximum RGB value zero.
- `OpalInternal-LightSweep.json` records all 48 hashed native frames. Only the first
  Point light moves across the sampled 130-degree arc; camera, material, actor, remaining
  illumination and shader bytes stay fixed. It predates persistent MIC creation but
  uses identical effective material values and shader bytes to the final saved preset.
  The MP4 is encoding of these native frames, not a real-time performance measurement.
- `OpalInternal-Reload-Final-Proof.json` passes four captures in one editor session.
  A C-diffuse `.ush` edit causes mean absolute RGB difference 14.3406, versus repeated
  baseline noise .6402; exact-byte restoration yields .7290. Existing master/MID/front,
  textures, all parameters, actor, camera and rig identities persist. Watcher completion
  reports zero failures. `MaterialEditingLibrary.get_statistics` is used only to finish
  the current shader resource, without explicit recompile, regeneration or asset save.

The earlier `OpalInternal-Reload-AExecution-Proof.json` is a failed diagnostic attempt
(incomplete compiled resource at changed capture); it is retained and is not passing
evidence. The final proof uses the completion barrier and restores the final source SHA.

Two preset-helper preparation failures are retained separately. UE's scalar/vector
setters apply values but always return false in this engine implementation; the helper
now validates full effective readback. A subsequent private StaticParametersRuntime
query was rejected by Python; it was replaced with the public switch-roster API while
retaining all 16 base-override checks. The fully verified, newly owned in-memory instance
was saved only when its exact disk path was absent. No pre-existing instance was rewritten.

Final editor PID128764/session `f7c4e8be-9726-4dbf-9d19-d576d96a5b81` quit normally;
process absence was checked. `OpalInternal-FinalSession.log` retains the resolved helper
error. Its subsequent final capture/reload interval has no fatal, ensure, AngelScript,
shader, material or Python error markers. `OpalInternal-FinalReceipt.json` records the
shutdown, video, preserved-source and 61-file protection checks.

## Appearance limits and review

This is a first native three-layer study, not user-approved final opal fidelity. It
shows separate polish and internal reflecting regions using the selected artwork.
Perceived depth is still modest and the scene does not establish convincing transmitted
contact shadows; dark grazing-edge flecks remain visible. Native optical thickness does
not create geometric shells, traced internal facets or measured spectral diffraction.
Finite sampled light angles do not prove every point flashes under every environment.
The regular/black-opal reference videos remain the artistic target. No engine-source or
default renderer configuration changes, protected asset changes, old-gallery replacement,
Git publication, packaged verification or GPU performance claims are part of this result.

Reproduce with `Launch-OpalInternal.ps1`, then the serialized command helper's `prepare`
and `scene` actions. `capture_opal_internal_final.py` and `render_opal_internal_motion.py`
preserve existing evidence and require fresh output names for a repeat. The normal project
still launches Blendable; this study uses the separate approved Adaptive profile.
