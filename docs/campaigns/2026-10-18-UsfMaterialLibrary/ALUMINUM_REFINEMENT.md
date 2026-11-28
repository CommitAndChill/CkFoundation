# Linear aluminum refinement — 2026-10-19

User reported the brushed aluminum looked strange. Matched fixed-camera production renders isolated the cause: flattening the normal left the large patchwork intact; disabling anisotropy removed it. The circular direction field strongly altered environment reflections. Engine Substrate Rect direct lighting ignores anisotropy, but environment evaluation modifies reflection normal/roughness using it; the earlier direct-light limitation did not imply anisotropy had no effect in the ordinary studio.

The active finish now has coherent linear brushing. Unchanged CC0 ambientCG Metal009 DirectX normal and roughness supply parallel machining; the source is explicitly brushed steel. Neutral Metal051A base color remains, with independently authored aluminum metallic reflectance. This is a reusable finish recipe, not a scanned linear-aluminum dataset. A 4x4 constant axial map RGB(255,128,255) aligns the tangent to the horizontal machining. Defaults: normal strength .65, roughness .7*map+.1, anisotropy .55. Texture sampling, UV scale/rotation, strength, roughness, tint and brush direction remain editable through the existing .ush interface.

`CkTests/Resources/CkUsf/Batch1Textures/AluminumLinear_v2/Receipt.json` pins all four active map sources, license URLs, dimensions/conventions and generator hash. Host `source_linear_aluminum.py` reproduced the receipt/direction byte-identically twice. `prepare_linear_aluminum.py` validates hashes and ownership, imports three new assets without replacing original textures, and regenerates only LibraryAluminum outside PIE. Batch preparation also imports this override before generating its six masters. The initial Manifest.json remains historical and is not silently rewritten.

Evidence under host `Saved/UsfMaterialGallery`:

- `Aluminum-Diagnosis.json`: baseline / flat normal / isotropic / rougher / neutral comparison, all overrides restored.
- `Aluminum-Preparation.json`: three runtime asset paths and source hashes. Actual capture state confirms the three new texture bindings and the retained aluminum BC.
- `Aluminum-Refined-{Close,Macro,Gameplay}.png`: actual ungraded PIE renders, same approved ball and original studio lights, temporary TAA/100%. `Aluminum-BeforeAfter.png` places unchanged screenshot pixels side by side. Gallery/contact sheet use the revised finish; original captures are retained.
- `Aluminum-LiveReloadProof.json`: passed in PID46700; same master/MID, all bindings/overrides preserved. Debug specimen mean changed .364891 → .034163 after a .ush source edit and recovered to .364867. Exact source bytes restored, SHA256 `4e0db080d51505034b92886b29871d5e4d19185c09e89f2a92bfb1197bb79e34`.
- `Batch1-Aluminum.exit.json` / `Saved/Logs/UsfLibrary-Aluminum.log`: fresh offscreen real-RHI `GeneratesUsableMasters` passed 1/1, zero skipped/contaminated, actual exit0, 48s. No C++ rebuild or global Toon reload was needed.

Two fresh editor boots for this refinement: one offscreen diagnosis/iteration studio, one focused final gate. PIE shutdown restored AA=0 and screen percentage87, then explicit Python/UE GC and clean quit succeeded. No desktop input/focus automation. No engine/cache changes, cook/package, checkout creation, commit or push.

Independent art review finds the circular patchwork resolved and the revised finish plausible. Remaining dark reflections and specimen UV seams are visible presentation limits; this is not measured BRDF equivalence or user artistic acceptance. The previous full-run DDC crash and isolated passing Toon retry remain recorded in BATCH1.md; this focused result does not resolve that unrelated native failure.
