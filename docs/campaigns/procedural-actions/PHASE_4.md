# Gate 4 — Clip data and pure evaluation

## Entry

Gate 0 reflected value types accepted; independent of later runtime control. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Action authoring data, Core ActionValidation/ActionEvaluation, native tests; add runtime module dependencies only through main integration owner.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Implement handle-free topology/action assets and grouped private reflected value structs per DATA_MODEL. Use stable phase/slot/window IDs, typed time, unit-scale transforms and explicit initial sources.
2. Implement compiler/validator producing immutable native values and field-addressed diagnostics. Validate finiteness, identifiers, key ordering, phase references, capability and supported spaces/easing.
3. Implement one pure evaluator with linear/bounded cubic translation and shortest-arc quaternion slerp; preserve phase/window boundaries and stable exit/point/enter ordering.
4. Provide sampled interval subdivision with per-substep MaxAdvance/travel/angular constraints, MaxSamples and explicit backlog. No per-frame total MaxAdvance cap.
5. Prove invalid data rejection, 30/60/120Hz normal progression, narrow windows, hitches, phase barriers and immutable revisions. Run focused native tests through toolbox.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate4.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V01, V15, V17–V18 and compiler portions of V20 pass; compiler/evaluator contain no live handles, asset loads or gameplay imports.

## Fences

No mutable DataAsset reads per frame, Euler rotation tracks, arbitrary callback payloads, silent normalization or Bounce/Elastic/Back motion easing.

