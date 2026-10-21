# CkTextureGeneration

Portable source-asset authoring for Niagara VFX and CkUsf materials. The canonical [workflow and user checkpoints](WORKFLOW.md) apply before choosing a backend. Read [techniques](references/techniques.md) for method selection and [asset contracts](references/asset-contracts.md) before export/import.

This is an offline tool directory inside CkFoundation, following the existing Tools/CkPixel pattern. It adds no editor or runtime module. CkParticlesEditor continues to own Niagara templates/systems; CkUsfEditor continues to own look materials. The plugin's [ck-vfx-authoring skill](../../.claude/skills/ck-vfx-authoring/SKILL.md) connects source-asset creation to their existing execution rules.

## Available recipes

| Recipe | Entry point | Scope and evidence |
|---|---|---|
| Procedural masks, flow and PBR | [blender/procedural/reproduce.ps1](blender/procedural/reproduce.ps1) | Native Shader Nodes, correlated material channels and continuous evolution. Historical technical evaluation; individual recipe files do not imply artistic approval. |
| Fresh simulated smoke | [blender/fresh-smoke/README.md](blender/fresh-smoke/README.md) | Accepted physical recipe, new seeded GPU gas bake, separate lit/unlit render. Requires installed Continuum Flow/OpenCL. |
| Modular shockwave | [blender/modular-shockwave/README.md](blender/modular-shockwave/README.md) | Procedural streaks and mesh soil plus transformed/crossfaded accepted simulated source imagery; matching normals are approximations. No fresh radial fluid solve. |
| Existing-frame packaging | [pack_flipbook.py](pack_flipbook.py) | Exact decoded PNG8/16 RGBA tiles, original-alpha opacity, preview and manifest. `--preview-only` omits atlases. |
| Generated source art | [techniques guidance](references/techniques.md) | Use the available imagegen tool/skill. No account credentials, model checkpoint, local diffusion service or installation is bundled. |

Python helpers require Python 3.10+ with NumPy and Pillow; [requirements.txt](requirements.txt) declares external packages. They are not installed automatically. The native procedural generator runs inside Blender and needs no external Python packages. Historical execution used Blender 5.2.1; fluid scripts target the demonstrated Continuum Flow 0.4 API. A compatible installation, selected GPU and actual new run must be checked on the current machine.

Explosion, fire-loop and settling-dust lessons are consolidated in the technique reference. Their original scene-dependent experiments are historical evidence, not additional standalone executable recipes in this package.

Source code/configs and required shockwave input textures are portable. Runtimes, virtual environments, generated runs, `.blend` exports and simulation caches stay local. Use an explicit new output directory outside the plugin source tree. Existing destinations are refused. Original accepted experiments remain untouched.

## Run

Run the selected PowerShell script with explicit absolute paths. For procedural generation:

```powershell
./blender/procedural/reproduce.ps1 -BlenderPath <absolute-blender-executable> -Spec <absolute-recipe-json> -OutputPath <new-absolute-output-directory> -PreflightOnly
```

Remove `-PreflightOnly` to execute the selected native recipe. Specs live under `blender/procedural/recipes`: erosion, flow, painted-metal, smoke-billowy, coherent-flipbook and normal-probe. Copy and edit a config for a variant; output paths are supplied at runtime rather than stored in shared configs. The demonstrated procedural backend deliberately renders on CPU; this is not a fallback for the GPU fluid recipe. No output from one method is attributed to another.

For already approved RGBA frames, using the chosen Python environment:

```text
python pack_flipbook.py --input <frames-directory> --output <new-directory> --fps 24 --columns 8 --rows 8
```

Filenames sort chronologically; use zero-padded indices. Grid size must equal frame count. No resizing, gutter insertion or tone changes occur in the packed PNGs. Use `--color-space linear` when RGB is linear data rather than sRGB color; alpha remains linear opacity. Preview compositing is linear-light and separate from native master pixels. Source precision is retained in PNGs, including 16-bit RGBA. Review GIF timing is quantized to centiseconds and repetition is only a player convenience. Mips/compression and a production atlas-gutter strategy remain import/runtime decisions.

For new directions generate motion previews before atlas packing unless the user has already authorized the later stage. The fluid/shockwave launchers default to previews and expose explicit packing options described in each recipe. After approval, package the existing frames rather than generating new motion: smoke uses the root packer and shockwave provides `--finalize`. A preflight checks paths/configuration and preview dependencies; it does not certify the add-on, GPU, rendering, visual quality or game performance.

## Verification and provenance

Run `python -B -m unittest discover -s tests -v` for portable processing checks and `python -B -m unittest discover -s blender/modular-shockwave/tests -v` for the fixture-only preview/finalization contract. Set `TEMP`/`TMP` to a writable external directory if the host's managed temp is restricted. PowerShell launchers support preflight without generation; their files are also parse-checked during migration. [source-lineage.json](source-lineage.json) and each recipe's lineage record identify original source hashes and changes. [Investigation evidence](references/investigation-evidence.md) records the originating chats, acceptance scopes and limits. [Migration progress](../../docs/texture-generation-migration/PROGRESS.md) records the checks actually executed after extraction.

No full fluid bake, fresh migrated visual acceptance or Unreal runtime validation follows from source migration. A native procedural smoke test, if recorded in progress, establishes only that tested family/configuration. The workflow keeps technical checks, human visual approval, runtime behavior and measured performance separate.
