# Industrial enamel family

Scope note, 2026-10-19: this is an optional aging/layering example. PLAN.md contains the revised breadth-first texture-backed library plan; this family is neither its visual template nor an expansion prerequisite.

The first material-library slice uses one generated IndustrialPaint master and
four gym MIDs. It preserves the approved studio lights, EV100 3.5, open rows,
USD specimen, named Specimen/CoreBase slots and native Toon workflow.

## Authoring

`Source/CkUsf/Shaders/CkUsf/IndustrialSurface.ush` owns reusable paint, primer,
steel, rust and dust color/roughness responses, region weights and detail normals.
`Looks/IndustrialPaint.ush` samples/combines those functions. Its ClearCoat source
declaration is the material's shading-model contract. The LookDefinition and
ordered artist parameters are in `Script/CkUsf/CkUsf_Industrial_Assets.as`.

The generator's existing numeric contract is sufficient. No generator, native
engine, Common.ush contract or renderer-setting change is made by this slice.
Paint, primer, exposed steel and rust are selected surface regions. Only steel
contributes metallic reflectance. Rust amount thresholds a separate oxidation
field; it does not uniformly turn all exposed metal into partially metallic rust.
Paint/primer loss thresholds share one damage field, keeping their transitions
ordered. Dust attenuates the underlying weights; all five weights sum to one.

Wetness uses the existing ClearCoat lobe and slight porous-surface darkening.
Dry materials explicitly output zero ClearCoat. This is not a general native
multislab implementation, water simulation, thickness model, or independent coat
normal. Blendable's single-closure budget and native Toon coexistence are retained.

## Reusable data

| Texture | Dimensions | Linear RGBA channels |
| --- | --- | --- |
| IndustrialDetail | 1024 square | Steel machining height, enamel orange peel height, rust pitting height, dust variation |
| IndustrialPlacement | 256 square | Damage susceptibility, oxidation, directional grime, macro discoloration |

Both are original procedural data. Immutable PNGs, channel preview and receipt
are under CkTests `Resources/CkUsf/IndustrialPalette`; the deterministic generator
is the host's `Tools/UsfMaterials/generate_industrial_palette.py`. The receipt
records seeds, Fourier bands, hashes, distributions and periodicity checks. No
external texture rights are inferred from the shader-ball license.

Imported assets live under `/CkFoundation/CkUsf/Textures/Industrial`. Preparation
preflights both source hashes/dimensions and both existing asset ownership markers
before modifying either. Import uses sRGB off, Masks compression, alpha retained,
Wrap U/V and SimpleAverage mips. Existing assets with an unknown source hash are
rejected. The preparation tool never replaces an unowned import.

`SurfaceTiling` is independent of `MaskTiling` and `MaskOffset`. One shared detail
and placement pair serves all presets; a final high-resolution texture set is not
required per specimen. These masks describe clustered UV damage, not curvature or
cavity. The standard specimen's original valid charts and repaired UV faces remain
unchanged. A future mesh-specific edge-wear bake is a separate trustworthy input.

## Artist presets

Colors are linear values. All presets use surface tiling 3, normal strength 0.65,
and red-oxide primer `(0.24, 0.065, 0.035)`.

| Control | Factory teal | Workshop ochre | Coastal blue | Abandoned olive |
| --- | --- | --- | --- | --- |
| Paint RGB | .025/.22/.19 | .48/.23/.035 | .035/.12/.30 | .12/.15/.045 |
| Wear | 0 | .45 | .68 | .88 |
| Rust amount | .02 | .30 | .70 | .95 |
| Dust | .02 | .10 | .18 | .52 |
| Scratches | .08 | .40 | .26 | .15 |
| Wetness | 0 | 0 | .32 | 0 |
| Paint roughness | .30 | .42 | .55 | .62 |
| Placement tiling | 1.2 | 2.2 | 1.8 | 1.2 |
| Placement offset | 0 | .29 | .61 | .87 |

DebugView 1 shows ownership, 2 roughness, 3 metallic, 4 placement and 5 tangent
normals. Restore zero for the normal lit material. Factory Wear=0 guarantees no
primer/steel/rust exposure. Grain/scratch response is restrained and mip-filtered.
UV-space central differences produce tangent microdetail, avoiding the existing
screen-derivative-as-tangent helper. They are not true geometric displacement.

## Observed cost and limitations

Desktop deferred, UE5.7.4, Substrate=1, ProjectGBufferFormat=0, real D3D12 SM6.
The editor material-statistics API reported 445 pixel instructions, 296 vertex
instructions, 5 samplers and 2 UV scalars for the final numeric shader. Its texture
sample counters reported 0 pixel/3 vertex; those counters do not account reliably
for these Custom-HLSL samples. Source contains five detail fetches and one placement
fetch. Do not call the material texture-free based on that counter.

Actual ListTextures output recorded BC3/PF_DXT5: 1408 KB for detail with 11 mips,
128 KB for placement with 9 mips; both fully resident in the observed scene.
Total 1536 KB is an allocation observation, not a GPU timing or benchmark. One
master/four MIDs does not itself imply batching or a draw-count reduction.

At extreme distance, thresholding averaged placement mips changes coverage;
fwidth and fading cannot restore the lost mask distribution. Gameplay-distance
captures were inspected, but this slice does not claim a universal coverage-
preserving mask hierarchy. Chips alter color/roughness/normal regions without
silhouette displacement or physical thickness. No anisotropic lobe, independent
coat normal, Adaptive GBuffer, path-traced, forward or mobile proof is included.

## Live iteration and evidence

Ordinary `.ush` saves reload affected loaded masters and existing MIDs, including
transitive IndustrialSurface dependencies. L remains the manual look reload.
Global lighting was not changed; its G command remains explicit. New parameter
schemas require deliberate asset regeneration through the preparation tool.

The host's `verify_industrial_reload.py` checks source edit/recovery, existing
object equality, preset overrides and rendered pixels in one process. It restores
exact source bytes and records output under `Saved/UsfMaterialGallery`. It must run
in one source-mutating lane. Concurrent unknown source changes are preserved.

The initial AutomationLibrary screenshot route selected an active editor
viewport and yielded invalid sky-only proof images; that attempt is not counted.
The accepted route sends HighResShot through the PIE player/GameViewportClient
and rejects an empty baseline before editing. Current gate/evidence results and
the earlier unresolved Python GC crash are recorded in PROGRESS.md.

This family is the first expansion slice. Concrete, directional composite,
vehicle armor and wider organic/architectural families remain separate work.
The user owns final artistic acceptance.
