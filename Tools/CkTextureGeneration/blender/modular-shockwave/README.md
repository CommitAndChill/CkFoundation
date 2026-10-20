# Portable modular shockwave composition

This source migration retains the accepted seeded layer recipe, native shader helpers,
timing and matched normal/white reconstruction. It bundles only two required accepted
1024px source atlases (398,050 bytes total), not scenes, caches or historical run outputs.
`lineage.json` records the original source hashes and investigation identity. This portable
version has passed source/preflight checks only; no migrated render or visual equivalence
is claimed until executed and reviewed.

Execution follow-up, 2026-10-19: seed80479 completed native streak, soil and billow renders plus the 32-frame combined motion preview, with each stage returning zero and source-art hashes unchanged. No new atlas, normal export or Unreal work ran. Human visual approval remains pending.

Run using explicit existing executables and a new absolute output outside the plugin source:

```powershell
& <PythonExecutable> <PackagePath>/blender/modular-shockwave/run.py --python <PythonExecutable> --blender <BlenderExecutable> --output <NewExternalOutputDirectory> --preflight
```

Remove `--preflight` to generate only the native layers and their combined motion preview
at `preview/index.html`. No final atlas or normal render runs by default. Supply
`--config <ConfigPath>` for a new seed or bounded controls. Review this motion before
authorizing the final assets. Blender 5.2.1 was the original renderer; use a compatible Blender EEVEE API.
The host Python requires numpy and Pillow. Package-root `blender_validate.py` supplies
lossless PNG decoding. Preflight validates inputs/config/dependencies without launching
Blender or creating output. Nothing is installed. Occupied output is refused, partial
failed runs and logs remain, and reruns require a new output directory.

After approving that exact preview, finalize its existing output directory:

```powershell
& <PythonExecutable> <PackagePath>/blender/modular-shockwave/run.py --python <PythonExecutable> --blender <BlenderExecutable> --output <ExistingPreviewOutputDirectory> --finalize
```

Finalization verifies the recorded preview-frame, native-frame, source-scene, receipt
and stored configuration hashes before creating final output. It uses the stored
`recipe.json`, ignores the generation `--config` default and never runs the layer
generators. Altered inputs or occupied `review`, `normals` or `verification` outputs
are refused. `--finalize --preflight` runs that gate without rendering or writing.
If full generation/packing is already explicitly authorized, add `--pack` to a new-run
command to perform every stage directly. `--pack` and `--finalize` are mutually exclusive.

The default preview contains native 32-frame/16fps sequences, editable sources, 1024px
selected frame-12 statics, independent layers and combined authoring review frames.
Explicit packing/finalization additionally produces precision-preserving and game
RGBA8 4x8 atlases (256px cells, no gutters),
independent layers, a combined authoring preview, DX primary/GL reference normals, native
alpha masters, source replay evidence and execution receipts. Game atlas opacity uses
the inherited high-byte RGBA/Lanczos path; native-precision reduction is retained separately.
The preview applies declared inherited tints and a fixed background using Pillow
encoded-color multiplication/alpha compositing, following the earlier review-media
convention. It is display-only; it is not linear-light material data or a texture edit.
Its browser replay is authoring review, not measured cadence or gameplay repetition.

Billows reuse accepted simulated imagery in a new radial card composition; no solver
or fresh radial CFD simulation runs. Streaks are native shader cards; soil is seeded
irregular mesh geometry. Dust normals approximate density-height relief; soil uses real
mesh facets. Normal export retains matched-white reconstruction and alpha-weighted
vector reductions. RGB is Non-Color/sRGB off; DX green is already inverted; padding is
neutral (128,128,255), opacity separate. CPU Lambert preview decodes exported normals;
it is not Unreal lighting evidence. Normal basis calibration is inherited, not rerun.

Only the original demonstrated controls/seed had visual approval. Fresh output requires
visual review. Runtime, compression/mips/gutters, performance and production integration
remain separate. Historical accepted-result comparisons and preservation-history scans
were deliberately removed; local source-art hashes and fresh saved-source replays remain.

## Fixture validation

Run from the CkTextureGeneration root: python -m unittest discover -s blender/modular-shockwave/tests -v. This generates sparse synthetic PNG fixtures in system temp, checks preview/finalization gates and exact existing-frame packing, and never invokes Blender or reads accepted run outputs. Set CK_TEXTURE_TEST_TEMP to a writable temporary parent if the caller sandbox restricts system temp. The executable stand-in in launcher preflight is Python; scene fixtures are intentionally invalid and only failing finalization calls omit --preflight.
