# Seated optical display — rendered refinement gate

2026-10-22. The user identified a hovering glass cover, missing rainbow bevels and an overly rounded aluminum housing. This additive refinement uses the existing offscreen Adaptive profile and existing binaries. The prior optical depth study, opal and accepted amber are preserved. Visual approval and full agreement with the illustrative concept remain open.

## Result and limits

- V5 glass retains an 18 mm section and 1.5 mm flat 45-degree chamfers. A continuous annular metal seat replaces the discrete posts. Its top is 0.2 mm below the glass underside; the instrument panel is 2.1 mm below the glass. This is an intentional nonintersecting fit seam, not contact simulation.
- The aluminum housing has broad flat shoulders, straight side faces and small straight chamfers with explicit normal creases. Rounded corners in plan are retained. Directional texture detail, normal strength, roughness, tint and anisotropy remain editable. Studio reflections are still pronounced; visible corner segmentation and remaining differences from the concept are not claimed resolved.
- `OpticalSeatedGlass.ush` adds RGB thin-film reflectance strongest on the bevels and at 20% strength on sidewalls. It samples the existing coating texture and varies thickness and response with viewing angle. The rainbow is coating interference, not wavelength-dependent refractive dispersion. There is no traced entry/exit transport, internal reflection or caustics claim. Neutral Pixel Normal Offset does not produce physically bent transmitted background.
- Face transmission uses 1.8 cm thickness, with a separate artistic 7 cm edge path. The inherited 0.33 cm transmission-reference calibration remains distinct from specimen thickness. No prior 3.5x edge-path multiplier is used.
- Low side lights and the existing engine preview environment make the continuous glass wall and its seat visible. The rig is a transient studio scene with high-quality front-layer translucent reflections; it is not a calibrated lighting setup or saved gym map.

Gallery: `Saved/UsfMaterialGallery/Optical-Seated-Gallery.html`. Final and macro PNGs are actual 1600x1200 Unreal captures. The original illustrative target is labeled separately. The orbit clip is 24 native frames at 8 fps, not interpolated video or a performance measurement.

## Verified evidence

Final-source startup session: editor PID **119008**, Toolbox PID **117360**, UUID `65ecc656-0697-4492-8937-93b9df05096e`. Final art sources and rig were frozen before this fourth boot. This differs from the earlier optical depth slice, which finished after warm source edits.

`OpticalSeated-SavedDefaults.json` records persistent-master-derived MID defaults before overrides. Maximum scalar/vector error is `4.768371586472142e-08`. All five roles retain texture backing. Compiled native closure counts are two for the read-only opal and one for glass/panel/housing/trim. Graph-0 opaque roles correctly do not have an explicit native front-material node.

`OpticalSeated-FreshProof.json` contains eight captures on unchanged master/MID identities. Differences below are mean absolute RGB differences on the 0–255 scale:

| Assay | Difference | Relevant repeated-frame noise |
|---|---:|---:|
| Coating off, fixed bevel region | 6.373 | 0.678 |
| Glass hidden, whole frame | 6.784 | 0.655 |
| Aluminum UV/normal/roughness/tint controls, whole frame | 13.884 | 0.655 |
| Defaults recovered, whole frame | 0.696 | 0.655 |

The bevel region is fixed at `[680,570,1490,810]`. The first verification script used a whole-frame coating threshold and failed because this localized difference was diluted by temporal noise elsewhere. The corrected assay compares the bevel against repeat noise in the same pixels. The proof was rerun warm without changing material or scene sources. This is a focused visible-response check, not a physical accuracy test. With all point lights, skylight, environment, reflection-capture brightness and panel emission disabled, the captured RGB maximum is exactly zero.

`OpticalSeated-LiveReload.json` verifies both new source files through the actual watcher on the existing master/MIDs. The glass probe changes texture UV sampling, normal strength and roughness; the aluminum probe changes texture-backed color. Glass change/recovery MAE is **7.374 / 0.683**; aluminum is **7.679 / 0.622**. Each change and restore reports zero failed masters. Both source files were restored byte-for-byte to their final-startup contents.

`OpticalSeated-MotionProof.json` records 24 distinct captures with a camera sweep and one circulating light. `OpticalSeated-EncodedMotion.json` records a 1600x1200 H.264 clip, 24 decoded frames / 8 fps / 3 seconds. The final receipt independently decoded the complete clip again. Selected motion frames and the final still were visually inspected; browser layout inspection was not performed.

`OpticalSeated-Receipt.json` passes and reconciles **39 unique PNGs**: one saved-default capture, eight final controls, six reload captures and 24 motion frames. It validates session/asset identities, request IDs, exact defaults and controls, geometry provenance, source and binary hashes, read-only opal/panel recipes, protected files, the encoded video and final log.

`OpticalSeated-FinalSession.json` confirms both owned processes absent, clean shutdown markers and zero matched Error/ensure/fatal/AngelScript/traceback diagnostics. Retained log: `OpticalSeated-Final.log`, SHA-256 `41b5d4e6ff7071b67ade5b4dcbb8a31f0896620228ca615b22fd5b570b20c62b`.

All **13** opening protected hashes remain unchanged: the persistent renderer configuration, six prior shaders and six prior rendered/receipt artifacts. Preparation treats opal and panel definitions/masters as read-only. No C++ or engine changes, persistent renderer configuration changes, rebuild, broad automation suite, performance benchmark, Git commit or publication are claimed for this slice.

## Sources and ownership

- Geometry generator: `Tools/UsfMaterials/author_optical_display_seated.py`; deterministic source receipt and four OBJ/MTL pairs under `Plugins/CkTests/Resources/CkUsf/OpticalDisplaySeatedV5/`. Positive outward winding, closed edge topology, no degenerate triangles and explicit unit normals are validated before import. The panel source matches V2; only glass/housing/trim are imported to new V5 asset paths.
- Shaders: `Plugins/CkFoundation/Source/CkUsf/Shaders/CkUsf/Looks/OpticalSeatedGlass.ush` and `OpticalSeatedAluminum.ush`. The original glass helper and existing texture maps are reused without modification.
- Authoring: `optical_seated_settings.py` and `prepare_optical_seated.py` under `Tools/UsfMaterials/`. Three new definitions/masters and three new meshes have owned paths. Source pins, recipe identity, exact parameter roster and all destinations are checked before mutation. Prepare before creating the transient scene.
- Session and evidence: `optical_seated_session.py`, `optical_seated_command.py`, `verify_optical_seated.py`, `encode_optical_seated.py`, `finalize_optical_seated.py`, `record_optical_seated.py`, `build_optical_seated_gallery.py` and `Launch-OpticalSeated.ps1`.

## Reproduction

Use the existing checkout with no editor owning this project. State one planned offscreen boot before launching through Toolbox. The launcher uses process-only Adaptive/TAA settings, checks outstanding command ownership and restores its environment. Prepare before scene creation; never regenerate masters bound to the active scene. Use the Toolbox PID from the new launch, not a historical PID.

```powershell
.\Tools\UsfMaterials\Launch-OpticalSeated.ps1
$ownedToolboxPid = [int](Read-Host 'Owned Toolbox PID printed by this launch')
python Tools/UsfMaterials/optical_seated_command.py prepare
python Tools/UsfMaterials/optical_seated_command.py scene
python Tools/UsfMaterials/verify_optical_seated.py defaults
python Tools/UsfMaterials/verify_optical_seated.py final
python Tools/UsfMaterials/verify_optical_seated.py reload
python Tools/UsfMaterials/verify_optical_seated.py motion
python Tools/UsfMaterials/optical_seated_command.py quit > Saved/UsfMaterialGallery/OpticalSeated-Quit.json
python Tools/UsfMaterials/encode_optical_seated.py
python Tools/UsfMaterials/finalize_optical_seated.py --toolbox-pid $ownedToolboxPid
python Tools/UsfMaterials/record_optical_seated.py
python Tools/UsfMaterials/build_optical_seated_gallery.py
```

The receipt depends on the retained opening protection receipt and prior V1/V4 provenance records. A later approved source/recipe change requires explicit ownership and a fresh evidence set; do not bypass a failed pin by silently replacing it. No duplicate editor launch after a timeout.

## Investigation accounting

Four owned Toolbox boots were used: initial V5 geometry/film/light iteration; high-quality front-layer translucent reflections; native preview environment and side-light direction; final frozen-source startup and proof. Additional boots were explained before launch. Earlier iteration logs/defaults/quit records are retained. The final offline receipt passes after clean closure. Astra supplied a bounded read-only film/lighting consultation and Sol independently reviewed the visual correction; those reviews do not replace the native evidence or user approval.
