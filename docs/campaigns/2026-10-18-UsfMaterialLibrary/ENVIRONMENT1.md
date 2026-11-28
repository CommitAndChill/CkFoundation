# Environment 1: height-backed gravelly sand / soil

2026-10-19. Retained generic candidate; superseded artistically by WOODLAND.md.

The user rejected these gravel surfaces as unimpressive. Do not present them as the selected
environment. All source/import/capture evidence remains preserved. V4 corrects combined importer
position-Y and texture-V conversions; asymmetric rendered UV evidence confirms U+X/V+Y.
The first endpoint image-equality proof failed and restored state. Repeated identical captures
also vary. Backbuffer-quantization override32 did not materially reduce that variance (interior
ratio0.989), so that hypothesis is not established. No numerical tolerance was relaxed.

User requested environment families including grass, dirt and mud, especially height, POM and
displacement. Initial aligned CC0 gravelly_sand remains under Environment1Textures and its saved
candidate assets/captures. Its fine soil relief was visually too subtle for this demonstration.
The selected source is the coarser Poly Haven gravel_floor under Environment2Textures, with
versioned T_EnvironmentGravelV2 textures driving the same EnvironmentGravel master/two stations.
Both have actual BC/N/R/16-bit Height/AO. No old source bytes or candidate textures are replaced;
grass-covered ground and mud belong in subsequent mixed rosters. Vegetation cards/blades and
wet-mud layering are separate material/geometry problems, not a generic noise variation.

Read-only renderer audit and bounded Astra review confirm the current separate WPO entrypoint
accepts the same textures/parameters. No new native outputs or renderer settings are needed.
Current surface contract does not expose PixelDepthOffset, MP_Displacement or Nanite tessellation.
Existing Common POM is a reference; this new look owns a bounded refined gradient-sampled march,
leaving old helpers byte-identical. This is not Nanite tessellated displacement.

Two additional permanent ground stations preserve all shader-ball specimens and lighting:

- POM: relative height shifts all sampled material maps. Flat silhouette, scene depth and shadows.
- Displacement: explicit texture mip drives real vertices of a unit-scale 230 cm, 256-square-cell
  owned non-Nanite grid. 131072 surface triangles plus a separate neutral lower slab. Height-driven
  shading slopes compensate for WPO not recalculating mesh normals. Source normal detail is
  independently adjustable. GPU displacement leaves collision unchanged.

Both use white=top, black=deep, with authored relief in centimeters (not a calibrated scan scale).
Full geometric relief suppresses full POM to prevent double application. Relief is bounded to
12 cm; source and import bounds include allowance. POM has bounded steps, crossing interpolation,
explicit pre-march gradients and grazing/footprint fade. Unit unmirrored UV basis U+X/V+Y is
authored and imported explicitly, including the OBJ importer's V conversion.

Verification -> expected observations:

1. Height zero matches flat control. Asymmetric UV diagnostics verify the imported basis;
   opposing camera views verify the POM shift sign.
2. POM changes texture appearance, while depth debug remains equal to flat.
3. WPO changes silhouette and rasterized depth; zero source normal strength isolates height-derived slopes.
4. Near/far and sampled camera views expose mip/stepping/bounds behavior; no continuous-motion or GPU
   performance acceptance from still images or compiler statistics.
5. .ush pixel sampling and VS height edits update the same two MIDs; exact byte/parameter recovery.
6. Separate shadow/collision/edge limits are reported honestly. The regular height field cannot
   create overhangs or independent gravel geometry.

Texture/geometry recipes and receipts live under CkTests Resources/CkUsf/Environment1*.
Selected texture receipts live under Environment2Textures. Grid v1 lacked MTL definitions; v2
corrected names but retained the old defaultMat slot and exposed zero-area lower-slab UV charts.
After a bounded Astra consultation, v3 supplies valid slab UV charts, checks every used LOD
section by named slot and preserves the unused defaultMat without an index-remap guess. All
versions/failures remain. Successful v3 import uses only CoreBase/Specimen, no new tangent
warnings. Initial soil preparation/master backup/captures are in Environment1-InitialSoil.
Implementation and proof share the batch-seven two-boot budget. No C++ rebuild, Adaptive change
or engine patch is planned.
