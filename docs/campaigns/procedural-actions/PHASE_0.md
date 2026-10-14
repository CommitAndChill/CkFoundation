# Gate 0 — Evidence and feasibility

## Entry

Design review closed; no runtime change accepted as verified. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Current feature files in RESEARCH, native public API proof tests, CkTests procedural fixtures; no gameplay migration.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Record root/plugin revisions, dirty paths and AutomationGate known-red lists with mtimes. Preserve unrelated RuntimeMesh changes. Read complete affected files and applicable nested guidance before edits.
2. Prove the proposed reflected data/handle/request shapes with representative native, Blueprint-node and AngelScript call-form fixtures. This may land the real public value types used by Gate 4, but must not advertise unimplemented operations.
3. Demonstrate a one-link standalone chain and a zero-limb body subject through production Transform settlement; inject part loss/foreign write between staging and observation. Evidence must show no successful receipt or clock advancement after mismatch.
4. Prove a disposable preview world can host production ECS/Transform and the opted-in Jolt contact path, and can tear down while loading. Inspect the resolved engine/current local editor examples; do not guess API names.
5. Run the focused toolbox gate and inspect fresh logs. Record observed scheduling and generated AS names. These are experimental proofs, not a new global scheduler design.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate0.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

Representative public values compile in C++/UHT/AS; Blueprint node smoke observed; production ordering/preview proofs have explicit pass evidence. Existing focused failures match recorded known reds.

## Fences

No global scheduler changes, fake Jolt preview, raw engine build commands, guessed wrapper names or handwritten generated code.

