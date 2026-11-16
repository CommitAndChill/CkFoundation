# Texture-generation source migration

Move the consolidated game-VFX workflow and reusable recipes into this plugin's `Tools/CkTextureGeneration` and connect the existing canonical `ck-vfx-authoring` skill. The accepted module boundary is portable offline tools first; add no empty editor module.

Done means portable source/configuration and required small source-art inputs are present, experiment-local runtime paths are removed, approval/evidence boundaries remain explicit, and focused migration checks run against the final files. Preserve unrelated cookbook changes, host preview assets, original experiments and accepted output. No commit/push of this project is requested.

Non-goals: Unreal code/assets/module changes, environment installation, a new generation service, full fluid rebakes, or production/runtime/visual acceptance. Use existing Tools/CkPixel as the packaging precedent.
