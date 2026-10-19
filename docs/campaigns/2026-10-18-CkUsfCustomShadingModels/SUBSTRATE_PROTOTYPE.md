# Substrate Toon prototype

Date: 2026-10-18. Status: implemented and verified for the scope below.

## Scope

Desktop deferred Substrate with Blendable GBuffer (`r.Substrate=1`, `r.Substrate.ProjectGBufferFormat=0`), opaque CkUsf Toon materials authored through the existing legacy root inputs. Keep `// @ckusf shading_model Toon`, surface-output band controls, automatic material reload, and explicit `Ck_Usf_ReloadShadingModels` for shared lighting changes.

This is a bridge into Substrate lighting, not a new layerable BSDF node. Adaptive GBuffer and explicitly connected FrontMaterial graphs remain unsupported. Forward/mobile, Nanite, indirect stylization, and path-traced Toon are outside acceptance.

## Source findings

- HLSLMaterialTranslator.cpp deliberately uses legacy material compilation for hidden conversion with Blendable GBuffer. Existing native Toon already writes its model ID and two band controls. Rebuilding Substrate topology/conversion is unnecessary for this route.
- DeferredLightPixelShaders.usf reconstructs a Substrate BSDF from that GBuffer and evaluates it through SubstrateDeferredLighting/SubstrateEvaluateBSDFCommon. The legacy Toon BxDF alone does not affect this lighting path.
- CkUsf must retain SINGLE classification. SIMPLE shaders force the model ID to Default Lit.
- The bounded implementation needs compiler metadata, transient reconstructed Toon fields, and a per-light diffuse hook. No Adaptive storage layout or new closure is needed.
- Adaptive would require persistent marker/control transport, packing/unpacking, byte budgeting, classification, simplification policy, and evaluator/export support. That is a separate architectural slice.

## Verification plan

The initial prototype verification used launch-only renderer overrides. CkPlugins now saves Substrate and Blendable GBuffer in Config/DefaultEngine.ini as its project defaults. Exercise actual generated CkUsf materials, complete shader-map readiness, two independent colored lights, source material reload, shared lighting reload, same MID/master/process identities, exact-byte shader recovery, and unsupported-configuration rejection. Record actual results and limitations here before claiming completion.

The baseline did not establish missing bands under Blendable. The initial cold compile hit Toolbox's idle watchdog and respawned; that owned run was stopped. Its retry reached the test but asserted at MaterialExpressionSubstrate.cpp:750. Review found an abbreviated comma-separated INI override: Unreal accepted Substrate=1 but silently ignored the format setting, leaving Adaptive=1. This establishes an unsupported Adaptive crash, not a Blendable render result. The implementation now rejects unsupported configurations before topology generation. The final gate uses two complete INI overrides and asserts actual Substrate enabled, Blendable enabled, and numeric project format=0 before creating the material.

The first incremental build found a plugin API-name mistake (GetShadingModel); it was corrected to IsShadingModelFromMaterialExpression. Tests did not start on that failed build. The replacement gate is Saved/Logs/Usf-SubstrateToon-Ready.log.

That replacement build passed (53.12 seconds), but its automatically parallelized test startup was interrupted by the same idle-output watchdog while shader workers were active. No rendering verdict is claimed for it. Toolbox v1.65 exposes no supported idle-timeout flag or persisted field. A normal Toolbox editor launch completed cold shader preparation without the test watchdog. The temporary host INI was restored byte-for-byte after closing that editor. Fresh discovery and a single-editor raw gate then passed **8/8**, zero failed/skipped/contaminated, in 3m28s (`Saved/Logs/Usf-SubstrateToon-Proof.log`). The gate used `--test --no-live --parallel=1 --no-nullrhi --known-reds=off --discover-fresh --test-pattern=CkUsf.ShadingModelDeclaration` with the complete renderer overrides below. The raw verdict treats every failure as a failure; no known-red entry was changed.

The eight tests include all 13 production-generated model shader maps, parser/settings/transition/rejection coverage, reload identity checks, the new support matrix, and the new Toon rendering/reload sequence. Shader maps were complete on both game and render threads before pixel readback. No ensures, AngelScript compile errors, shader compile failures, or source/CVar restoration errors appear in the focused gate.

## Measured rendering and reload proof

Real RHI, Substrate enabled, Blendable enabled, project format=0, PID 41496, unchanged generated master and MID `/Engine/Transient.MID_M_CkUsf_Look_ToonPipeline_0`:

| Stage | Linear mean RGB | Edit-to-verified capture |
| --- | ---: | ---: |
| Baseline | 1.014648 | — |
| MID threshold at one | 0.507324 | — |
| No direct light | 0.000000 | — |
| Source tint at one quarter | 0.253662 | 1.801 s |
| Exact material source restored | 1.014648 | 0.219 s |
| Shared lighting gain zero | 0.000000 | 137.065 s |
| Exact shared lighting source restored | 1.014648 | 12.487 s |

Two differently directed red/green lights additionally prove per-light band separation: both illuminate at threshold 0.25; threshold 0.75 suppresses green while red remains within 5%; threshold one halves red. The actual PerLightBothBands.png and PerLightRedBand.png captures were inspected. Captures are under `Saved/Automation/ToonPipeline/P41496`; structured measurements are in `Saved/ShaderReloadDelivery/SubstrateToon-Proof.result.json`. These times include completion/readiness, not the earliest visible frame, and are observations from one run rather than a performance benchmark.

A separate normal editor (PID 19228) opened the actual USF Materials gym with Substrate=1 and format=0. Saving the real look declaration switched **DefaultLit → Toon in 1.000 s → DefaultLit in 0.422 s**, on the same sphere/master/MID with the BandThreshold override intact. No reload command was issued for those transitions. Material statistics completed the new shader work before recording each result. Evidence: `Saved/ShaderReloadDelivery/SubstrateAuto-Launch.result.json`, `SubstrateAuto-Reload.result.json`, and archived `SubstrateAuto.Editor.log`. The original source bytes were restored, PIE ended, the generated master was saved consistent with the original DefaultLit source, and the verification editor closed. The host renderer INI was restored again.

The live gym logs also contain CkAbility logger-registration and null loading-screen-widget errors. Both exact messages already appear in the pre-task `Saved/Logs/CkPlugins-backup-2026.10.18-22.58.00.log`; they were not changed in this shader prototype. The focused automation gate has no such errors.

Restoration SHA256 receipts: host INI `BC18361DD0D9985A2A97CCEBB8AAA2CC04F5896965FAE15D03F66F01746AFC9B`; user's DefaultLit Toon.ush `CCA57F97B2D8381B5F3934AFB82B5A411426F25223F02FB745159EB20D967F3D`; implemented shared lighting source `7CE3904D9738947007689209102F7EC2FEEA7497C12AA807B1EBD8A0B251A882`. The unrelated Iris checksum remains `69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92`.

Not verified: shadowed rendering, mixed DefaultLit/Toon tiles, human visual acceptance of the gym, forward/mobile/Nanite/path tracing, no-plugin startup, or a fresh Adaptive boot after adding the admission rejection. The pure support matrix verifies Adaptive/explicit-front/multiple-model/absent-bridge rejection; Adaptive and native layered FrontMaterial authoring remain unsupported.

## Engine changes

- HLSLMaterialTranslator.cpp: reject unsupported Toon configurations before topology generation; classify the Blendable legacy-root model as SINGLE; append a real versioned key component to Substrate material and translation caches.
- Rendering/CkUsfSubstrateSupport.h: declare the precisely scoped version-1 bridge capability without changing the broadly included EngineTypes.h.
- Substrate.ush and SubstrateRead.ush: transient reconstructed Toon identity and threshold/softness. Other BSDF constructors zero-initialize these fields. Adaptive packed storage is unchanged.
- SubstrateEvaluation.ush: substitute the plugin-authored diffuse band gain for the ordinary cosine response during direct-light integration.
- Engine-owned CkUsf_Toon.ush fallback: identity gain if the optional plugin shader source is absent. No-plugin startup remains unverified.

## Authoring and reload

Keep the leading `// @ckusf shading_model Toon` declaration in the look entrypoint, and set O.ToonThreshold/O.ToonSoftness normally. Material look edits use automatic reload or Ck_Usf_ReloadShaders. Shared ShadingModels/CkUsf_Toon.ush lighting edits use Ck_Usf_ReloadShadingModels, which enables shader-development retries. The shared pure gain helper is used by both legacy and Substrate paths.

To enable the renderer, set r.Substrate=1 and r.Substrate.ProjectGBufferFormat=0 and restart once. CkPlugins saves these settings as project defaults. Standalone consumers can use the following full launch-only arguments for the prototype automation:

```
-CkUsfSubstrateProof
-ini:Engine:[/Script/Engine.RendererSettings]:r.Substrate=True
-ini:Engine:[/Script/Engine.RendererSettings]:r.Substrate.ProjectGBufferFormat=0
```

After that initial renderer selection, supported source/lighting iteration remains within one editor process.

## Project default adoption

Following the prototype proof, CkPlugins adopted Substrate and Blendable GBuffer as saved project defaults. The real Toon look now starts with `// @ckusf shading_model Toon` and its generated master is saved with CkUsf Toon selected. The DefaultLit source hash above records the earlier proof's exact-byte cleanup, not the final adopted look. Production base color remains `saturate(TintColor)` and shared lighting iteration gain remains `1.0`.

## Delivery verification

After rebasing Foundation and Tests onto fetched dev, the first build exposed missing procedural-animation debugger APIs in the host's older debugger pin. Advancing the clean CkGameplayDebugger submodule to its existing origin/dev resolved that dependency mismatch without source patches. The resumed incremental build passed, and the fresh real-renderer declaration gate passed 8/8, zero failed/skipped/contaminated, in 48 seconds with Toolbox exit 0: Saved/Logs/Usf-Substrate-PublishGate-Ready.log. Project defaults supplied Substrate and Blendable; -CkUsfSubstrateProof asserted them in the production-path fixture.

PID 36032 retained the same master/MID. Baseline mean RGB 1.014648 changed to 0.253662 with source tint and recovered to 1.014648; shared lighting gain zero produced 0.000000 and recovered to 1.014648. Warm edit-to-verified-capture observations were 0.226/0.218 seconds for material edit/recovery and 7.755/7.801 seconds for lighting edit/recovery. These are one-run observations, not a benchmark. No ensures, AngelScript errors, shader compile errors, or restoration errors were found in this focused gate. Discovery reported an unrelated unloadable procedural-animation AutoTest wrapper; no assets were deleted.

The engine implementation is commit c68d75b0cec2 on main-ck. Final production shader hashes are 99F46AD15C0A6F961544208D76AC48B97BF2A1A0C6C6A3C18BBBA25945DBD286 (Toon look) and 7CE3904D9738947007689209102F7EC2FEEA7497C12AA807B1EBD8A0B251A882 (shared lighting). The unrelated Iris file still matches 69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92.