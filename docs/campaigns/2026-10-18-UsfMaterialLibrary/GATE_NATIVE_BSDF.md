# Native BSDF first slice

Status: implementation and focused gate verified, 2026-10-21; native asset preparation and rendered
proof pending. Moving results belong in PROGRESS.md.

Use the existing source-selected shading contract and batch-wide reload preflight as the neighboring
patterns. Add opt-in direct Slab and native UE5.8 Toon expressions connected to FrontMaterial;
leave legacy model selection intact. No private Substrate HLSL structures or replacement lighting.

Files: LookDefinition/Common output types, unified header parser and validator, generator and a
native graph helper, focused editor tests, two textured examples and the existing offscreen studio.
Keep all old material assets and the approved logo-free shader ball presentation.

1. Resolve and validate native graph selection before material creation.
   Verify malformed/mixed declarations, incompatible domain/settings and invalid profile rejection.
2. Feed numeric Custom outputs into engine-owned native expression pins.
   Verify exact expression class, FrontMaterial and ordered output connections, including optional
   anisotropy connectivity and tangent-space normals.
3. Reload existing native masters in place after whole-batch preflight.
   Verify topology/profile/texture-dimension changes reject without mutation, while numeric entrypoint
   and transitive edits preserve master, Custom, BSDF and MID identities/overrides.
4. Build and run compatible focused coverage through Toolbox. Announced allowance: up to3 fresh
   discovery/gate boots plus1 offscreen showcase boot. Build2 passes; one actual native gate boot
   passes67/67 with real RHI, offscreen rendering, exit0 and3m13s. No additional native gate boot
   was consumed. Asset preparation and showcase captures remain pending.
5. Create NativePorcelainSlab and NativeCeramicToon using existing licensed BC/N/R maps. Capture
   matched close/gameplay views and actual moving-light response before expanding the roster.

Limits: first slice opaque SurfaceLit and one BSDF, without a separate WPO function. Blendable retains renderer simplifications.
Native Toon profile ramps/hatching remain asset data; texture sampling, UV scale, normals,
roughness, color and numeric BSDF inputs are editable through .ush. No independent multilayer or
path-traced Toon claim. New topology or profile bindings require explicit regeneration.

Evidence: `Saved/Logs/NativeBsdf-Build2.log`, `Saved/Logs/NativeBsdf-CkUsf-Gate1.log` and their
process receipts under `Saved/EngineUpgrade/20261020-UE584`. The67-test gate includes six new
native contract/topology/reload tests and compatible existing CkUsf coverage, zero failed/skipped/
contaminated. Diagnostic config is restored byte-for-byte. Engine publication is independently
verified at origin/main-ck `55c44c498c272fcc1f911d1c90e9885184b026f3`; focused C#83/83 passes
in35s. These results do not establish rendered native showcase acceptance.
