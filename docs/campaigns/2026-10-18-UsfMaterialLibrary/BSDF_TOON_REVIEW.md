# Native BSDF examples and upstream Toon decision

Historical initial audit below. Subsequent user-authorized UE5.8.4 integration is verified by the editor build and103 focused Unreal tests; see ENGINE_UPGRADE.md for current state, unrun C# tests and SDK-validation limits. New native BSDF/Toon examples remain unimplemented.

## Historical Hazelight upstream availability audit (superseded by ENGINE_UPGRADE.md)

The user directed checking/fetching Hazelight's original repository. A live ls-remote query of
Hazelight/UnrealEngine-Angelscript returned angelscript-master at
d811c292432fe125ed597ad704653750b2977e6f. A direct normal-Git fetch completed and updated the
existing origin-hazelight/angelscript-master tracking ref from c2935a433c96 to that commit.
Its subject is a merge of epicgames/release; its release parent is
f45729c0b6f097038e72b52d366a62baeca3275b, titled "5.8.4 release". This confirms a newer Hazelight
integration baseline is available. The working engine remains5.7.4 on its existing branch;
no checkout, merge, rebase, build, engine source change or project reassociation was performed.

The fetch used tree:0 filtering and no FETCH_HEAD write. Upstream trees/blobs remain omitted;
no-lazy-fetch reads of upstream Build.version and material headers could not resolve those
omitted objects. Exact upstream file contents/native Toon declarations have therefore not been
verified through this fetch; the native5.8 Toon claim is established by Epic's primary docs.

Operational side effects disclosed to the user: normal Git's promisor consolidation created
a12,100,513,587-byte pack in the existing engine object database despite the small48,583-byte
incoming metadata pack. The owned pack-objects child reached about14GB RAM and finished before
the attempted targeted stop; no process was actually stopped. No pack files were manually
deleted. origin-hazelight is now configured as a promisor remote with tree:0 filtering.
An initial unredacted remote listing also exposed embedded credentials in tool output. Their
values are deliberately not retained here; the user was advised to rotate them. A credential
extraction/API fallback was rejected by automatic approval review and was not executed; the
successful alternative used the configured remote's normal Git authentication.

## Requested result

Add real Substrate BSDF examples, keeping texture-backed surface authoring live in .ush. Toon
must use Epic's native response to actual lights, without postprocess banding, unlit/manual
light direction or a CK-maintained custom engine shading model. Continue working offscreen.

## Verified engine and source facts

- Selected engine Build/Build.version is5.7.4; project targets DX12 SM6 with Substrate enabled and
  r.Substrate.ProjectGBufferFormat=0 (Blendable). No5.8 native Toon BSDF/Profile types are present
  in the searched local engine source. No engine upgrade or backport has been performed.
- Existing Toon is MSM_CkUsf (EngineTypes.h721). Engine ShadingModels.ush includes the plugin
  CkUsf_Toon.ush and dispatches CkUsf_ToonBxDF. The plugin starts from DefaultLitBxDF and changes
  diffuse cosine gain using two custom-data controls; specular remains Default Lit. It is real
  per-light deferred shading, but it is the maintained-fork approach the user now excludes.
- Local engine HLSLMaterialTranslator.cpp1493 onward limits this CK bridge to a single static
  legacy-root model with Blendable GBuffer. Mobile/forward/Nanite shader paths fall back to
  Default Lit; the path tracer explicitly substitutes Default Lit. Existing demos are preserved,
  not relabeled as Epic's upstream native Toon feature.
- CkUsf_Generator.cpp745 onward connects Custom outputs to legacy root material properties.
  CkUsf_ShadingModelContract.cpp36-53 rejects explicit FrontMaterial graphs and Adaptive for the
  current source-model route. Existing library looks compile through Substrate's legacy
  conversion; they are not demonstrations of direct Slab authoring or independent closures.
- Local MaterialExpressionSubstrate.h253 has a genuine Slab BSDF, and417 has Simple Clear Coat.
  Native Hair/Eye/Water and operator nodes also exist. Their presence alone does not add them to
  CkUsf's current generator/reload contract.

## Linked articles checked against primary documentation

The user's5.5 overview describes Slab authoring and composition, but current5.7/5.8 GBuffer
behavior must be checked separately. Epic's current overview explicitly documents simplification
and Blendable feature limits. A graph containing two slabs does not prove two independently
evaluated layers survived compilation.

The StraySpark article's central native-Toon claim is supported by Epic's5.8 release notes:
Substrate NPR Shading is experimental and uses Blendable GBuffer, with native Toon BSDF and
Toon Profile, direct-light and GI controls. Toon therefore does not require an Adaptive switch.
This is a specific upstream BSDF feature; Substrate does not make arbitrary material Custom
HLSL a programmable per-light lighting loop.

Epic documents ToonProfile as a per-material asset that should not be changed at runtime.
ToonProfileStruct contains diffuse/specular curves, offsets, hatching and indirect-light controls.
Keep texture sampling, UVs, normals and exposed surface inputs in .ush; profile-owned response
curves remain native profile data unless the actual5.8 source establishes an input-level route.
Do not promise that all profile settings can be driven by .ush or silently add an engine patch.

Primary sources:

- https://dev.epicgames.com/documentation/unreal-engine/overview-of-substrate-materials-in-unreal-engine?application_version=5.5
- https://dev.epicgames.com/documentation/unreal-engine/overview-of-substrate-materials-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US
- https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialExpressionSubstrateToonBSDF
- https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ToonProfile
- https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ToonProfileStruct

Secondary article supplied by user:
https://www.strayspark.studio/blog/substrate-toon-shader-ue5-8-tutorial

## Authorized engine upgrade preflight (2026-10-20)

User explicitly requested proceeding with the engine upgrade. Normal authenticated Git lazy
retrieval of Hazelight target `d811c292432fe125ed597ad704653750b2977e6f` Build.version
confirmed **5.8.4**, superseding earlier merge-title-only evidence. Existing engine stays at
`e628343442a5724ede61a61177fd4a073386110d` pending integration; divergence is 57 local and
38,298 upstream commits. No running editor/build processes were found; recheck before mutation.

Astra reviewed the in-place port. Native Substrate Toon uses GPU ID 13, colliding with CK's
custom GPU ID 13; serialized MSM_CkUsf=13 is a distinct contract. Preserve upstream Toon and
its separate path-tracing namespace; audit another CK GPU ID before adapting the legacy bridge.
Preserve the complete UBT lock protocol and CK compatibility changes. Upstream-equivalent
StaticJIT size fixes and SDeferredToolTip must be checked semantically before dropping patches.

The continuation explicitly forbids stashing the unrelated Iris edit. A narrow user exception
is pending before any rebase; no blanket stash, untracked changes or alternate checkout.
Iris SHA256 remains `69813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92`.
An exact file recovery copy and preflight receipt are in Saved/EngineUpgrade/20261020-UE584.
No engine source mutation, dependency setup, build, boot, cook or publication has run.

## Proposed first BSDF batch

| Example | Defining response | Existing texture source | Native node / current limits |
| --- | --- | --- | --- |
| Glazed porcelain | Dielectric diffuse, neutralF0, roughness and pores | LibraryPorcelain source BC/N/R | Slab; no claim of separate topcoat closure |
| Brushed copper | Colored conductor reflection, near-zero diffuse, anisotropic highlight | LibraryCopper source BC/N/R | Slab; meaningful anisotropy tested with actual moving lights |
| Velvet | Texture-backed pile with colored grazing fuzz | LibraryVelvet source BC/N/R | Slab fuzz; Blendable fuzz roughness is fixed, no simultaneous competing SSS claim |
| Resin-coated carbon fiber | Detailed crossed tows beneath clear-coat response | LibraryCarbonFiber source BC/N/R | Simple Clear Coat; optimized coat model, not arbitrary independent multi-slab transport |

Follow with amber transmission and scattering wax after establishing which native inputs and
scattering modes retain their behavior on the selected GBuffer. True vertical liquid/coating
transport, independent layer normals/roughness, haziness and other Adaptive-only features are a
separate scope decision. No global renderer switch is implicit.

## Proposed authoring boundary for review

Use an explicit opt-in native-BSDF authoring mode; existing LegacySurface remains the default.
Native Slab surface inputs come from a distinct .ush output contract, connected to engine-owned
Slab expression inputs and FrontMaterial. Preserve material/MID identities for ordinary source
edits. Do not return hand-assembled renderer-private FSubstrateData from a Custom node or remove
the existing legacy validation guard globally. Topology/signature changes remain regeneration
events; ordinary texture/math edits remain reload events.

Recommendation after bounded stronger-model consultation: establish the5.8/AngelScript target
and compatibility scope first, beginning with assessment rather than modifying the installation.
One native Slab and one native Toon example are the first implementation gate, before expanding
to the four-example roster. Strongest reason: upstream Toon is a direct requirement and cannot
be proven on this5.7 source; building a second integration target first risks avoidable rework.
Strongest counterargument: a single-Slab5.7 slice is independently useful and avoids an engine
upgrade. Select that explicitly if the engine must stay5.7, with native Toon recorded as blocked.

Before code: decide the5.7 native-Slab slice versus a5.8-compatible engine baseline. Upstream Toon
cannot be honestly delivered on the current source without an engine change. A backport would
remain a maintained engine modification, conflicting with the stated exclusion. No migration,
download, alternate checkout, worktree, rebuild or asset conversion is authorized by this audit.

If the opt-in CkUsf extension is selected, implement its narrow contract and invalid-input guards,
then verify generated native node topology, actual enabled features/simplification and focused
compatibility with existing looks. Prove same-MID .ush edits and recovery on all four samples,
under the existing orbit light, with actual renderer views. Run required incremental C++ build
and focused real-RHI gates through UnrealToolbox only. No editor boots or builds ran in this audit.
