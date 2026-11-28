# Substrate hero material references and proposed recreation gate

2026-10-21. Research/proposal only; no renderer migration or material recreation claimed.
User requests complex online tech-showcase references and faithful CkUsf recreations.
This complements the breadth library; it does not replace it.

## What is actually new

The previous native velvet, walnut, resin carbon and copper batch exercises native inputs,
but clearcoat, cloth and conductors also have legacy equivalents. The next showcase should
demonstrate retained BSDF layers, colored transmission through an upper layer, independent
layer normals/roughness and view/light-dependent behavior. Avoid claiming every optical
effect was impossible with custom shaders before Substrate.

Epic explicitly distinguishes parameter blending from layering matter:
https://dev.epicgames.com/documentation/unreal-engine/overview-of-substrate-materials-in-unreal-engine?application_version=5.8

## Primary reference roster

| Priority | Reference | Recreation target and visible test |
| --- | --- | --- |
| First | Epic Automotive Substrate Materials | Candy metallic finish: flake base, absorbing pigmented middle layer and clear top coat. Changing middle-layer thickness should alter filtering of the base response while retaining the top reflection. Separate layer-normal and roughness controls. |
| Next | Epic Electric Dreams opal | Patterned iridescent mineral beneath a polished absorbing surface. Internal color patches must move/respond differently from the surface highlight under orbiting light and camera. Optional dust only after the clean material succeeds. |
| First | NVIDIA Lightspeed / Adobe Zorah slubbed silk | Textured uneven yarns with distinct anisotropic specular lobes and optional dust coverage. Warp/weft direction, lobe width and intensity need separate inspection. |
| Next | NVIDIA Lightspeed / Adobe Zorah lustrous wood | Texture-backed figured wood with chatoyancy and a distinct surface finish. Grain-dependent interior shimmer must remain distinguishable from the outer finish. |
| Later | NVIDIA Lightspeed / Adobe Zorah patterned gemstones | Asterism with a light-dependent star beneath a polished gemstone surface. Research a physically defensible orientation-map model before promising fidelity. |

Primary URLs:

- Automotive article, layered single/dual/triple coats and advanced optical features:
  https://www.unrealengine.com/news/get-over-280-production-ready-automotive-substrate-materials-for-ue-5-7-free-on-fab
- Electric Dreams official sample documentation, including the opal material:
  https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine
- Epic's GDC demonstration transcript posted by Unreal Engine for Design & Visualization;
  describes the opal base, polished absorbing layer and optional dirt/dust:
  https://www.linkedin.com/posts/unreal-engine-for-design-visualization_electric-dreams-and-the-rivian-r1t-ue52-activity-7048668197977247744-Dv1I
- Zorah production interview: multiple Substrate slabs, slubbed silk, asterism and wood
  chatoyancy utility maps:
  https://blog.adobe.com/en/publish/2025/04/03/bringing-zorah-to-life-how-substance-3d-tools-powered-nvidias-ambitious-tech-demo

These are reference targets, not inspected downloadable graphs. Zorah's neural acceleration
is a separate technology; the recreation targets native Substrate appearance and must not
claim NVIDIA Neural Materials execution or performance. Reference artwork is not a texture
license. No packs/projects were downloaded. Use existing licensed maps where suitable and
independently authored utility textures; record exact provenance for every added asset.

## Verified local renderer boundary

- DefaultEngine.ini selects Substrate with ProjectGBufferFormat=0 (Blendable).
- CkUsf_ShadingModelContract.cpp currently rejects Adaptive for native surface graphs.
- Engine RenderUtils.cpp defines ProjectGBufferFormat as read-only; Blendable forces one
  closure. Adaptive additionally depends on platform support and closure/byte budgets.
- Engine SubstrateTranslatorCommon.cpp rejects CK's legacy-root Toon shading model under
  Adaptive. Do not claim the entire existing gym, including Toon, is compatible.
- Important distinction: UE's native Substrate Toon BSDF has Adaptive packing/unpacking
  (SubstrateExport.ush:885, Substrate.ush:4974). The native Toon contract currently rejects
  Adaptive in CkUsf, but this is not an engine-wide native Toon limitation. Its eligibility
  could be extended and tested separately; it cannot be combined into Slab layering operators.
- ConfigCacheIni.cpp supports startup -ini:Engine:[Section]:Key=Value overrides. A temporary
  startup profile is source-supported but not yet executed or proven end-to-end here.

## Proposed bounded implementation, pending renderer approval

Use the existing checkout and an explicit offscreen Adaptive launch profile, retaining the
normal Blendable launch and its native Toon stations. No engine Toon port, global default
change, alternate checkout or build-artifact deletion is included.

1. Prove the startup profile on a minimal dedicated scene without loading Toon assets.
   Verify effective Adaptive mode, closure/byte limits and unchanged persistent config.
2. Extend the existing public-node CkUsf generator with two bounded Adaptive-only templates:
   a vertical stack and two-slab horizontal mixture, without parameter blending. Prove two
   retained slabs first; expand the vertical template to three for the full candy finish
   only after retention and the additional visual contribution are demonstrated.
   Preserve existing graph contracts and masters; use new assets and a dedicated roster.
   Numeric .ush outputs feed public Substrate slabs/operators; no private FSubstrateData.
   All sampling, normals, roughness, color, UVs, thickness and specialized numeric controls
   stay live-editable. Topology/schema changes remain explicit regeneration operations.
3. Establish candy paint and multi-lobe silk first, using separately inspectable layer controls.
   Confirm retained closures rather than trusting node count or a visually plausible blend.
4. Capture orbit-light videos, macro/gameplay views, layer-isolation comparisons and same-MID
   .ush edits with exact source restoration. Measure actual material cost before making
   performance claims. Recheck the normal Blendable/Toon route after the showcase.

Launch count and compatible focused tests will be specified after the approved contract is
implemented. No editor was launched during this reference research.

Read-only architecture review recommends this bounded profile and contrasting first pair.
The proposed closure topology is not a claim to reproduce the unavailable reference graphs.
Existing CC0 satin maps can support silk, but bespoke slub/orientation detail must be authored;
existing walnut maps can support later wood, but are not measured figured-wood optical data.
