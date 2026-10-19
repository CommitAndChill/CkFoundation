# Gate: native Toon lighting and live iteration

Written 2026-10-18. Status and evidence are maintained only in PROGRESS.md. This campaign lives under the existing tracked documentation allowlist.

Entry: inspected legacy material enum/compiler/GBuffer/light dispatch, path-tracing packed IDs, shader include hashing, and development retry behavior. Existing material look reload is already delivered. Engine has an unrelated Iris edit which must be preserved.

1. Engine registration, compiler metadata, custom-data transport, BxDF dispatch, stable optional plugin shader mapping, enum compatibility and path-tracing fallback -> verify incremental editor build, compile coverage and focused archive tests.
2. CkUsf authoring/generator/validator and shipped Toon look -> verify valid generation and rejection of unsupported domains/blend modes/Substrate, preserving existing enum values and ClearCoat outputs.
3. Explicit shading-model command enables development retries and recompiles changed dependency consumers -> verify the production command is exercised by the same-process real-RHI test.
4. Real lit-mesh capture, BxDF edit, explicit reload, pixel change, exact source restoration and recovery -> verify same process/MID, measured readbacks and original shader bytes restored. Invalid global HLSL is not injected unattended because it enters an interactive repair dialog.

Plan two fresh editor test boots at most initially: first integrated build/test and one focused correction/final gate if needed. Build failures do not consume an editor boot. Reassess from evidence before additional boots. Do not delete build artifacts or create another checkout.

If the mesh stays black, inspect the material compile result and GBuffer/lighting dispatch before changing thresholds. If edits do not change pixels, inspect transitive shader hashing and compiled consumers before adding delays. If the same symptom survives two attempts, consult the existing Astra reviewer before a third edit.

Exit: record actual build/test scope, pixel/PID evidence and limitations; restore all test shader edits and temporary settings; inspect fresh logs for ensures and script failures; review engine/plugin diff and preserve Iris. No unsupported platform or whole-repository regression claim.
