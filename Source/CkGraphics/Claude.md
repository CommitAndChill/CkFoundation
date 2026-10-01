# CkGraphics

**Purpose:** Graphics utilities — rendering helpers beyond ISM, including material instance manipulation and GPU query helpers. Thin utility module; not ECS-Record-patterned.

Render targets are NOT here despite what this line used to claim: replicated render-target pixels and draw calls live in `CkRenderTarget`, and this module has no render-target API at all.

**Depends on:** `CkCore`, `CkEcs`, `CkLog`, `CkVariables`.
**Used by:** `CkIsmRenderer`, `CkOverlapBody`, `CkUnrealComponent`.

---

## Key API

- `UCk_Utils_Graphics_UE` — dynamic material instance creation, render target ops.
- `UCk_Utils_Graphics_UE::Apply_CustomPrimitiveData(UPrimitiveComponent*, const FCk_CustomPrimitiveData&)` — the one
  value-type switch that writes an `FCk_CustomPrimitiveData` onto a primitive component with the matching engine
  setter (Float / Vector2 / Vector3 / Vector4; LinearColor as Vector4). Ensures on an invalid component; does not
  range-check the index (callers own that). Used by `CkUnrealComponent`'s custom-primitive-data request and
  `CkIsmRenderer`'s proxy.

---

## Pattern

Used by modules that need to dynamically change material parameters without a full actor.

---

## Anti-patterns

Don't set material parameters per-frame from a Processor without caching the `UMaterialInstanceDynamic*` — re-creation is expensive.

---

## See also

- `CkIsmRenderer/Claude.md`, `CkVfx/Claude.md`.
