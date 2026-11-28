# Optical depth — rendered refinement gate

2026-10-22. The user requested closer fidelity to the illustrative opal and coated-display target. This is an additive content/shader refinement using the existing offscreen Adaptive profile. Final artistic approval is separate; the result is still not a complete match to the mockup.

## Current result and limits

- Display: real **18 mm** glass section, **1.5 mm** flat 45-degree chamfers with explicit normal creases, and **8.9 mm** clearance above the recessed instrument panel. V4 preserves canonical outward source winding and independent face-corner normals. Its glass imports as `SM_OpticalDepthBevel_GlassCover`; the other three unchanged meshes retain their V2 asset paths.
- The glass uses one native Slab and neutral Pixel Normal Offset. `EdgePathCm=9` is multiplied by 3.5 in the shader: **31.5 cm effective artistic edge path**, not measured geometric thickness or a physical transport result. There is no traced entry/exit refraction or caustics claim.
- Opal: original generated color artwork and estimated artist data under two native Slabs. Forty-eight domain cells per UV unit on each axis provide buried orientation variation; `InternalDepthUV=.016` and `InternalLayerMix=.25` approximate internal parallax. The smoother outer scattering veil and polished highlight remain distinct from the buried pattern. This is an opaque appearance approximation, not a spectral diffraction or volumetric transport solver.

User-facing artifact: `Saved/UsfMaterialGallery/Optical-Depth-Gallery.html`. The illustrative reference is clearly separated from the actual Unreal renders.

## Verified evidence

Final evidence session: PID **127372**, UUID `7295d30c-ac90-402f-b2d5-ab975ed55da6`.

`OpticalDepth-SavedDefaults.json` records persistent-master-derived MID defaults **before overrides**; maximum scalar/vector error is `4.768371586472142e-08`. Later art shader edits were applied through the live source watcher in this same fresh boot. The final appearance is therefore **verified after warm reload within a fresh boot**, not a startup capture of the final shader source. `final_proof()` preserves the earlier defaults artifact instead of replacing it after the art edits.

`OpticalDepth-FreshProof.json` now contains both complete subjects. Native compiled counts are two closures for opal and one for glass/each opaque role. The graph-0 roles correctly have no explicit native front-material node. Mean RGB differences below use the **0–255** scale:

| Subject | Repeat noise | Control differences | Recovered difference |
|---|---:|---|---:|
| Opal | 0.872 | Flash off 6.624; depth off 5.681 | 0.745 |
| Display | 0.758 | Uncoated 1.050; cover hidden 9.331 | 0.781 |

The coating endpoint is subtle relative to temporal noise. Opal lights-off is exactly black after 120 settling frames; the earlier single-pixel 1/255 residue is not counted as exact-zero evidence. Display lights-off retains 23,401 nonzero pixels, maximum 250, establishing the separate emitting graphic.

`OpticalDepth-LiveReload.json` contains both entrypoint probes and byte-exact recovery on unchanged master/MID identities. Opal edit/recovery MAE is **7.036 / 0.789** against repeat noise **0.662**; glass is **17.735 / 1.219** against **0.687**. Each changed/restored watcher completion reports zero failed masters. Independent read-only checks verified all 20 final-control/reload PNG hashes, session identities, and restored shader hashes.

`OpticalDepth-MotionProof.json` records 24 distinct captures per subject on the same PID/MIDs. `OpticalDepth-EncodedMotion.json` records two 1600x1200 H.264 clips, each 24 fully decoded frames at 8 fps / 3 seconds. These are appearance sequences, not performance measurements.

`OpticalDepth-Receipt.json` reconciles **75 capture records / 75 unique PNGs**, both source probes, the final macro, motion, native statistics and seven protected files. Independent read-only verification confirmed every PNG/evidence hash and both encoded-video hashes. The refreshed `Optical-Depth-Gallery.html` exists. User visual approval remains pending, including the remaining gap to the illustrative target.

`OpticalDepth-FinalSession.json` confirms editor PID 127372 and Toolbox PID 119264 are absent, with clean `LogExit` / log-closed markers and zero matched Error, ensure, fatal, AngelScript or traceback diagnostics. `OpticalDepth-Final.log` is a retained hash-verified log copy. Both owned processes are closed.

Seven protected hashes were independently checked unchanged: normal `DefaultEngine.ini`, accepted `AmberPolish-Final.png`, `AmberSpecimen.ush`, original `OpticalOpal.ush` and `OpticalCoatedGlass.ush`, and both V1 `OpticalPair-*-Final.png` images. No C++, engine, persistent renderer-config change, build, broad automation suite, performance benchmark, Git commit or publication is claimed for this slice.

## Source and reproduction

Geometry: `Tools/UsfMaterials/author_optical_display_depth.py`, `author_optical_display_frontfaces.py` (rejected reversed-winding diagnostic), `author_optical_display_bevel.py`, and `Plugins/CkTests/Resources/CkUsf/OpticalDisplayDepthV4/Geometry.json`. The V2/V3 sources and assets remain preserved. V3 is available only through explicit `prepare(reversed_diagnostic=True)`; it is not the selected render.

Materials: `Plugins/CkFoundation/Source/CkUsf/Shaders/CkUsf/Looks/OpticalOpalDepth.ush`, `OpticalDepthGlass.ush`, and `OpticalDepthHardware.ush`; existing `OpticalDisplayPanel.ush` supplies the code-authored panel. Texture receipts remain `OpticalPairV1/OpalTextures.json` and `DisplayTextures.json`.

Preparation/session: `Tools/UsfMaterials/prepare_optical_depth.py`, `optical_depth_settings.py`, `optical_depth_session.py`, `verify_optical_depth.py`, `encode_optical_depth.py`, `finalize_optical_depth.py`, `record_optical_depth.py`, and `build_optical_depth_gallery.py`. Preparation verifies original/current recipe hashes and the actual parameter roster before bounded owned-default migration. Prepare before creating a scene; never regenerate bound masters in an active scene.

From `D:\Repos\CkPlugins_3`, with no editor already owning the project:

```powershell
.\Tools\UsfMaterials\Launch-OpticalDepth.ps1
$ownedToolboxPid = [int](Read-Host 'Owned Toolbox PID printed by this launch')
$reportedEditorBoots = 1
python Tools/UsfMaterials/optical_depth_command.py prepare
python Tools/UsfMaterials/optical_depth_command.py scene
python Tools/UsfMaterials/verify_optical_depth.py defaults
python Tools/UsfMaterials/verify_optical_depth.py final
python Tools/UsfMaterials/verify_optical_depth.py reload
python Tools/UsfMaterials/verify_optical_depth.py motion
python Tools/UsfMaterials/optical_depth_command.py capture --subject display --name OpticalDepth-Display-Macro.png --controls '{"camera":[10,-18,8],"target":[3,-4,2.8],"fov":34,"settle_frames":32}' > Saved/UsfMaterialGallery/OpticalDepth-Display-Macro-Final.json
python Tools/UsfMaterials/optical_depth_command.py quit > Saved/UsfMaterialGallery/OpticalDepth-Quit.json
python Tools/UsfMaterials/encode_optical_depth.py
python Tools/UsfMaterials/finalize_optical_depth.py --toolbox-pid $ownedToolboxPid
python Tools/UsfMaterials/record_optical_depth.py --macro-record OpticalDepth-Display-Macro-Final.json --editor-boots $reportedEditorBoots
python Tools/UsfMaterials/build_optical_depth_gallery.py
```

The launcher uses UnrealToolbox and process-only Adaptive overrides; it refuses unresolved command ownership. Use the PID printed by that new launch, never a historical PID. The example reports one fresh reproduction boot; record the actual total if additional diagnostic boots occur. Encoding and finalization precede receipt assembly, which now requires both records. Do not duplicate an editor after a timeout.

## Investigation accounting

The lead reports **six** owned Toolbox boots, not six successful gates: initial unsupported standalone hardware graph 1; corrected graph 0 with directional shading; rejected reversed-winding hypothesis; restored canonical winding; V4 crease-bevel geometry and persistent-default migration; final sorting A/B and verification. Sorted triangles did not provide a meaningful improvement and remain **false**. No engine changes were required. The final live art changes occurred after the sixth startup, as distinguished above.




