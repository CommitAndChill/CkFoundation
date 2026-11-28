# Advanced native BSDF showcase

2026-10-21. Moving state belongs in PROGRESS.md. Builds on the verified native first pair.

Gate complete; technical/rendered evidence is in PROGRESS.md and ADVANCED_BSDF.md.
User artistic acceptance remains pending.

Research: native generator/contract/validator and Common.ush, adjacent library shaders,
Batch3/Batch7 licensed manifests, UE5.8 public Slab/SimpleClearCoat pins and Blendable
export/read paths. Mimic NativePorcelainSlab and Test_Usf_NativeBsdf. Astra source review
selects native SimpleClearCoat rather than a generic two-Slab stack: its coat export flag
is explicitly supported. Existing source/asset work remains local and preserved.

1. Add opt-in Slab fuzz amount/color and a separate NativeSimpleClearCoat graph/output.
   Verify old layouts/signatures unchanged; unsupported directional combinations reject;
   exact native pins, source declarations and output types match. No renderer/engine change.
2. Add four texture-backed examples: ruby velour, varnished walnut, resin carbon weave,
   clean copper. Verify provenance/hash/settings, source sampling and specialized controls.
3. Incrementally build and run focused compatible CkUsf real-RHI coverage. Plan one run,
   up to three gate/discovery boots if required, then one offscreen showcase boot.
4. Generate only new assets, append gym stations, capture matched close/macro/gameplay and
   Point-light phases. Verify same-instance .ush edits/restoration for all four; fuzz amount
   and coat coverage/roughness contrasts must visibly affect matched renders. Preserve
   shader bytes, overrides, lighting, quality and startup state; quit the owned editor.

Limits: Blendable simplifies general multilayer transport. Native fuzz roughness is fixed;
SimpleClearCoat top F0 is fixed .04 and anisotropy unsupported. With r.ClearCoatNormal=0,
use one shared normal and leave BottomNormal disconnected. No geometric fiber silhouette,
unrestricted multilayer, glint or performance claim. Texture licenses remain CC0 with exact
source identity; authored carbon weave is not a carbon photogrammetry scan. No Git delivery
or settings changes requested for this batch.
