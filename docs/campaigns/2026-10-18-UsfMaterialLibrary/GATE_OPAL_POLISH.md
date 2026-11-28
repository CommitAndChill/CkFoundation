# Opal polish — additive native two-Slab study

2026-10-22. The user requested an improved opal while preserving the accepted seated-glass display. This additive study uses the existing offscreen Adaptive renderer and existing binaries. Final artistic approval remains open: the result is milkier than the illustrative target, and the optical model remains an opaque approximation.

## Gate status — technical evidence complete, visual approval open

Two owned offscreen boots were used. The first prepared the original study and supported live art trials; its derived fracture-like normals were rejected. The second loaded the selected V2 artwork, new textures, new definition/master and final saved defaults. Current final session: editor PID **121180**, UUID `92df5ae1-3050-4da9-a9bb-96922e884c91`.

Saved-default, final-control, live-reload and motion reports pass. The independent `OpalPolish-Receipt.json` passes and reconciles 39 PNGs, source and asset pins, preserved bindings, encoded motion, all 33 protected entries and clean session closure. User artistic acceptance remains separate and pending.

Gallery: `Saved/UsfMaterialGallery/Opal-Polish-Gallery.html`; its generator verified eight local references. Actual Unreal captures and the motion clip are distinguished from the generated source artwork.

## Sources and representation

- Selected original artwork: `Plugins/CkTests/Resources/CkUsf/OpalPolishV2/Source/OpalOriginal.png`, with the actual generation prompt in `Source/ImagePrompt.txt`. This is generated RGB artwork, not a mineral scan or calibrated optical dataset. Its SHA-256 is `7d66d3ff5ca042e4ffa170eeb844f18aaac921a4bba2d591030b2d41b59660b6`.
- `author_opal_polish_maps.py` and `OpalTextures.json` record five aligned maps: original color, domain data, domain normal, outer-polish normal and roughness. Color is byte-identical to the original image. The utility maps are numerical artistic estimates. The authoring keeps **21 broad connected palette regions**, merging 88 small regions; broad interiors use one seeded tangent tilt rather than brightness-derived fracture normals or tiny glitter cells.
- Source images remain **1254x1254**. All five imported textures explicitly use `STRETCH_TO_POWER_OF_TWO`, SimpleAverage mip generation and Clamp addressing. Preparation verifies completed platform dimensions **2048x2048**, separately from source dimensions. This stretches the coordinated maps over the full UV domain; it does not alter the stored source PNGs. Twelve mip levels are expected from the built extent, but actual mip-count/resident-mip verification is not claimed.
- Only color uses sRGB. Both normal maps use normal-map compression and no green flip; data and roughness are linear mask textures. New paths begin `/CkFoundation/CkUsf/Textures/Optical/OpalPolish/T_OpalPolish_`.
- New definition/master: `DA_OpticalOpalPolishV2` and `M_CkUsf_Look_OpticalOpalPolishV2`. The AmberHero body mesh is reused read-only. Previous study definitions, masters and maps remain preserved; no recipe migration is performed.

`OpticalOpalPolish.ush` declares `AdaptiveVertical`. The upper native Slab supplies the smooth outer polish and a slightly milky transmitting medium; the lower Slab supplies broad body color and direction-dependent colored reflection. The lower F0 varies with the domain/spectral controls. Both emissive outputs are zero. Preparation and every recorded final/control/reload capture report **two compiled native closures / 12 uint per pixel**, with no compile errors; this is compiled-material evidence, not a GPU timing measurement.

The shader samples explicit filtered mip levels for the buried color/domain response, keeps domain orientation independent of the flat outer polish, and uses view-dependent UV offsets for the artistic internal-depth effect. This is not traced solid opal, volumetric multiple scattering, measured mineral interference or a physically reconstructed internal structure. The apparent buried layers are texture sampling within an opaque two-Slab material. Remaining milkiness and target mismatch are visual limits, not resolved by the technical gate.

## Verified controls and reload

`OpalPolish-SavedDefaults.json` passes before overrides; maximum scalar/vector default error is `2.38418579323607e-08`. `OpalPolish-FreshProof.json` passes eight final/control captures with unchanged master/MID and texture bindings. Values below are mean absolute RGB differences on the 0–255 scale:

| Assay | Difference | Repeated-frame noise |
|---|---:|---:|
| Flash off | 1.906402 | 0.551791 |
| Internal-depth UV/layer mix off | 1.808113 | 0.551791 |
| Outer coat roughness 0.45 | 3.560112 | 0.551791 |
| Defaults recovered | 0.591149 | 0.551791 |

The required flash and roughness responses exceed the declared `max(0.1, 2 * repeat noise)` threshold, **1.103582**. The depth-off response is also measured above that threshold, but is not proof of physical depth. All lights, skylight, environment and reflection brightness off produce exactly zero RGB maximum, mean and nonzero-pixel count.

`OpalPolish-LiveReload.json` passes an actual texture-sampling source edit and byte-exact restoration through the watcher on the existing master/MID. Repeat noise is **0.559668**, changed MAE **6.956296**, restored MAE **0.706754**. Change and restore report zero failed masters. The restored entry-source SHA-256 is `ba6bbdaa9ee39518c34e7036bc0b398793c7f900982571ff93695ac1f3774a8d`; the independent receipt reconciles preparation, restored source and current bytes.

## Motion, final reconciliation and protection

`OpalPolish-ProtectedBaseline.json` pins **33 entries** protecting the user-accepted seated-glass display and its related accepted artifacts. The new preparer checks ownership and all source/destination pins before importing only five new textures or generating only the new opal master. It does not regenerate prior materials. The final receipt confirms all 33 protected hashes remain unchanged.

`OpalPolish-MotionProof.json` passes seed, 24 native camera/light frames and recovery. Motion recovery MAE is **0.579745**; first-to-last difference is **4.605793**. `OpalPolish-EncodedMotion.json` records `OpalPolish-Motion-5989e252748de1fc.mp4`: 1600x1200 H.264, 24 actual frames / 8 fps / 3 seconds. All frames decode, and the independent receipt fully decodes the clip again. This is a captured camera/light sweep, not interpolated footage or a performance benchmark.

`OpalPolish-Receipt.json` reconciles **39 PNGs** from the same final session: one default, eight final controls, four reload captures and 26 motion captures. It checks actual image hashes, source/transitive dependency pins, new definition/master and five texture binaries, reused body geometry, controls, source restoration, object identities, video and protected files.

`OpalPolish-FinalSession.json` confirms editor PID **121180** and Toolbox PID **129576** absent, clean shutdown markers and zero matched error/ensure/traceback diagnostics. Retained log: `OpalPolish-Final.log`, SHA-256 `3057a4eda78cad9e983f635f923dd6078f66c217f4fa2f61aba5d28eb905d0b1`.

Source/tool entry points under `Tools/UsfMaterials/`: `opal_polish_settings.py`, `author_opal_polish_maps.py`, `prepare_opal_polish.py`, `opal_polish_session.py`, `opal_polish_command.py`, `verify_opal_polish.py`, `encode_opal_polish.py`, `finalize_opal_polish.py`, `record_opal_polish.py` and `Launch-OpalPolish.ps1`. The entry shader includes the existing `OpticalPair.ush` helper; preparation pins transitive source dependencies. Preparation must precede scene creation; never regenerate a master bound to the active scene.

No C++/engine changes, persistent renderer-configuration changes, new automation-suite result, physical-optics validation, performance benchmark or publication are claimed for this slice. Technical evidence is complete; visual approval and closer agreement with the illustrative target remain open.

## Reproduction

Use the existing checkout with no editor owning this project. State one planned offscreen boot before launching through Toolbox. Preparation precedes scene creation. Proof captures refuse overwrite: preserve the completed evidence and use an explicitly owned fresh evidence run rather than deleting or replacing prior proof files. Never regenerate a master bound to the active scene, and do not launch a duplicate editor after a timeout. Use the new launch's actual owned Toolbox PID, not the historical PID above.

```powershell
.\Tools\UsfMaterials\Launch-OpalPolish.ps1
$ownedToolboxPid = [int](Read-Host 'Owned Toolbox PID printed by this launch')
python Tools/UsfMaterials/opal_polish_command.py prepare
python Tools/UsfMaterials/opal_polish_command.py scene
python Tools/UsfMaterials/verify_opal_polish.py defaults
python Tools/UsfMaterials/verify_opal_polish.py final
python Tools/UsfMaterials/verify_opal_polish.py reload
python Tools/UsfMaterials/verify_opal_polish.py motion
python Tools/UsfMaterials/opal_polish_command.py quit > Saved/UsfMaterialGallery/OpalPolish-Quit.json
python Tools/UsfMaterials/encode_opal_polish.py
python Tools/UsfMaterials/finalize_opal_polish.py --toolbox-pid $ownedToolboxPid
python Tools/UsfMaterials/record_opal_polish.py
python Tools/UsfMaterials/build_opal_polish_gallery.py
```
