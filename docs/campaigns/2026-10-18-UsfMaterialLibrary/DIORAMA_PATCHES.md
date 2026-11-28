# Grass, dirt and mud diorama patches

2026-10-19. Delivered locally and technically verified; no user visual approval yet.

The user explicitly permits the existing verified authoring tools as an exception to the
updated game-vfx skill's missing CkTextureGeneration package in this selected checkout.
Do not fetch another checkout, install a package, or use the live Blender instance.

Three distinct non-repeating80cm compositions: flowering grass with sparse leaves/gravel;
loose dirt with clustered curled leaf litter, tiny flowers and gravel; wet mud with shallow
pools, embedded gravel and sparse leaves. Texture-backed source detail and modeled forms
produce aligned BaseColor, DirectX normal, roughness, geometry height and AO maps. BaseColor
uses8-bit sRGB for the verified engine import path; geometry height retains16-bit precision.

Reuse the existing immutable dense grid and established POM/WPO shader contract. Add six
stations after the existing roster, retaining river-pebble stations and the approved shader
balls. All sampling, UVs, normals, roughness, color and relief remain editable through .ush.
Use the existing selected-station orbit light; it stays disabled during source-control proofs.

Completed verification: inspected source beauty/maps before import;96native/TAA/debug/control
captures prove sampled UV and actual vertex-height responses and12same-MID pixel/vertex reloads
with exact source/state recovery. Nine additional captures prove selected-station orbit response
above3xrepeat noise for all3looks, with exact rig and material recovery. One warm offscreen studio
boot and one fresh focused real-RHI generation gate were used, without C++ rebuild.
GeneratesUsableMasters passes1/1 in1m5s, zero failures/skips/contamination. Fresh log has no fatal,
ensure, script-compile or material-compile failures. Independent offline reconciliation passes
all105frame hashes/responses, source/calibration/provenance,18newasset binaries and396protected
earlier source/asset hashes. Grid binary unchanged. Gallery: Saved/UsfMaterialGallery/Diorama-Gallery.html.

POM affects sampling only; WPO affects existing vertices without changing CPU collision.
Topmost heightfields cannot reproduce separate blade undersides, leaf overhangs or all flower
silhouettes. These texture materials are reusable ground patches; no foliage-geometry system,
renderer extension or performance acceptance is implied. All work stays offscreen.

SelectedV2 source art and maps are complete: grass5.26245093cm, dirt2.87664306cm and mud3.54227126cm
actual geometry ranges. Mud pool/rim mean roughness is .20550/.65899, with smoother pool normals.
Packed blends, source/calibration checks and15aligned maps/15EXR masters are preserved. Initial
V1 wiry grass and weakly differentiated dirt/mud remain rejected internal art drafts.

Native full-height grass trials produced connected columns under leaves/petals. A bounded
creative review and matched native trials selected shallow1cm grass relief and vertex mip2;
source range and full-height/filtered candidates remain intact. This is deliberately compressed
textured ground cover. Dirt/mud retain full source relief. Presentation is recorded separately
in diorama_settings.py and the import receipt, without rewriting source data/calibration claims.

The first warm open happened before explicit diorama preparation and the existing missing-master
guards rejected the six new specimens. PIE ended and GC ran; the owned18assets were imported,
then all8ground stations loaded in the same editor. No crash or guard bypass. Future direct
bridge runs must call diorama_prepare outside PIE before open; bootstrap already does so.

Final studio state: diagnostic/source controls restored, orbit off, AA0/screen87. Owned warm
PIE ended, GC completed and editor quit before the fresh gate. No desktop input, live Blender,
engine/config changes, C++ rebuild, cook/package, cache deletion, extra checkout or Git publication.
Unrelated Iris source SHA256 remains69813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92.
The user skill exception is task-local; it does not claim the missing canonical package was installed.
