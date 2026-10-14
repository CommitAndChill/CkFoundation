# Gate 2 — Scoped control and dedicated manual actions

## Entry

Gate 1 accepted. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Action/ActionPlayer runtime features, LimbControl, Articulation claim integration and tests; no gait borrow yet.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Implement API typed handles, scope-owned instances, complete resource-set admission, body exclusivity and deterministic subject order.
2. Implement Create_Manual, Ready, complete SubmitPoseBatch, End, Cancel, Release and query/signals. Request completion is operation completion; receipt is settled-pose evidence.
3. Implement immutable capacity reservations and identity-scoped streams. Reserve closure/terminal storage, preserve records after action death, expire diagnosed orphan terminal streams after configured deadline.
4. Dedicated limb return captures actual accepted pose/policy/deadline and runs under articulation even after action destruction. Enforce input/publication timeouts; teardown cannot wait for a tick.
5. Exercise conflict, reentrancy, stale generations, GC/lifetime churn, partial batches and stalled consumer; run focused gate.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate2.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V05, V11–V14, V16 and V30 dedicated/manual cases pass; repeated owner destruction beyond capacity eventually reclaims every orphan reservation.

## Fences

No unchecked SetFootTarget, boolean Force, implicit preemption/queue, direct part tween or manual markers/windows.

