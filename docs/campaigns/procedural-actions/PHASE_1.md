# Gate 1 — Articulation and single publication

## Entry

Gate 0 scheduling/public surface proof accepted. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

CkProceduralAnimation Articulation/Limb, Core limb solve extraction, legacy Rig/Leg/BodyPose/walker adapters and native/AS tests.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Capture legacy numerical fixtures before extraction, including 1/2/8-link chains, disabled/frozen foot, detach and body offset. Retain identical input schedules for comparison.
2. Implement sealed Layout and runtime Bindings per DATA_MODEL/API. Compose atomically, 0–64 limbs, 1–8 links; validate scale, unique IDs/handles and writer ownership before creating children.
3. Extract chain solving without numerical behavior change. Eliminate mandatory gait query filter/roster/body-envelope reads for dedicated appendages. Keep legacy facade and same entity identity; no duplicate geometry storage.
4. Make one publisher stage the entire subject candidate and qualify only actual settled results. Implement Operational/Faulted, actual partial-pose diagnostics and no writes after integrity fault.
5. Prove standalone/body-only composition and legacy pose equivalence with meaningful runtime transforms, then run focused gate.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate1.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V01–V04 and V13 relevant articulation cases pass; legacy sampled poses agree within documented existing solver tolerance; no second publisher or fake gait.

## Fences

Do not alter solver math during extraction, mirror leg runtime state, infer chain lengths from meshes, or treat per-part requests as an atomic rendering transaction.

