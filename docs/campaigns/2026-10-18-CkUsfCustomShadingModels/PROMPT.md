# Legacy custom shading models

Written 2026-10-18. Current evidence lives in PROGRESS.md. Retire these planning documents after delivery; the permanent contract is in Source/CkUsf/Claude.md.

Implement the first CkUsf native lighting model without Substrate. An opaque desktop deferred mesh must receive Unreal lights and shadows through the new Toon BxDF. Artists must be able to edit its lighting HLSL and explicitly reload it in the same editor process.

Locked decisions: use the legacy MSM_CkUsf engine family with a plugin-owned Toon function; two UNorm8 custom-data controls are threshold and softness. Automatic reload remains limited to material looks. Shared lighting reload uses RecompileShaders Changed with development-mode retry enabled. No automatic global reload or transactional shader-map replacement is part of this slice.

The supported acceptance target is opaque, desktop deferred, ordinary static meshes. Substrate, forward/mobile, translucent/Nanite lighting, Lumen custom sampling, and path-traced Toon are outside this gate. Path tracing must still compile, with an explicit DefaultLit compatibility fallback. Existing material enum serialization and no-plugin engine startup must remain compatible.

Success means the focused real-RHI test records bright -> dark -> restored native lighting after editing the plugin BxDF, retaining the same editor PID and MID. A build alone is insufficient.

Reference patterns: existing ClearCoat material compiler/GBuffer plumbing; DefaultLitBxDF; CkUsf's existing generator and ShaderReloadUpdatesExistingMID test; CkUsf's real-RHI outline scene-capture fixture.
