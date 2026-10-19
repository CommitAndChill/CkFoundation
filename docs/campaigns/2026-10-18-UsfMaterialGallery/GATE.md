# Studio acceptance gate

Written 2026-10-18. Live status and evidence are owned by PROGRESS.md.

1. Inspect current gym, shader authoring contract, screenshot and candidate asset
   licenses. Astra reviews the bounded design before implementation.
2. Implement five new CkUsf looks, source-only studio controls, geometry selection,
   isolated neutral lighting, exposure and opt-in edge outline. Verify source
   signatures, metadata and file syntax locally.
3. Prepare the standard shader-ball asset with recorded license, immutable upstream
   revision and hashes. Verify node transforms, normals, winding and two slots with
   an offline fixture. The first actual download resolves upstream main; human
   inspection must approve that resulting revision/geometry before acceptance.
4. Human opens the selected project through UnrealToolbox and runs the host setup
   Python script outside PIE. It imports the mesh, generates six looks, enters the
   USF Materials gym and frames Toon. No C++ rebuild is needed for this source slice.
5. Human verifies all six materials, silhouette/logo absence, image-plane facing,
   lighting/exposure and outline toggle. Verify one Studio*.ush save and recovery
   in the same PIE session. Inspect fresh logs for shader/AngelScript errors and
   ensures. Import warnings, fallback materials or errors block acceptance.

If import fails, retain source receipts and the unsaved imported object for
inspection. Do not delete/recreate unrelated outputs or overwrite existing assets.
Fix slot order if directed, then rerun setup using its recorded source revision.
If appearance is poor, use the human's observed discrepancy for a targeted next
iteration; a syntax check is not rendered or artistic acceptance.
