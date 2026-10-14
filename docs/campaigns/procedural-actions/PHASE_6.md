# Gate 6 — Authoring editor and runtime observation

## Entry

Gates 1,4,5 accepted; Gate 0 production-preview feasibility proven. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

New CkProceduralAnimationEditor module; plugin registration; existing CkProceduralAnimationDebugger model/history/view; tooling tests.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Register paired Editor-only module without runtime or debugger reverse dependencies. Build asset toolkit and reusable session, Details grouping and stable-ID transactions following AUTHORING.
2. Implement phase/track/key/marker/window authoring, endpoint/bend gizmos, explicit mirror/retarget and validation. Keep appearance in separate preview data.
3. Implement Authored pure mode, production Solver fixture and Recorded immutable evidence. Seek rebuilds/resimulates, is bounded/cancellable and invalidates on revision/surface changes.
4. Extend live debugger copied observations for ownership, support, requested/accepted poses, publication faults, streams/backlog and recovery; never rerun traces to fabricate history.
5. Run mounted-widget/model tests and real editor walkthrough including undo/redo, two tabs, save/reopen, GC, close while loading/seek, PIE coexistence and actual continuous playback.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate6.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V24–V26 pass with real observations; solver preview agrees with production fixtures and all UI resource lifetimes terminate.

## Fences

No graph editor, pretend solver preview, live mutation via DataAsset, raw static world/widget pointers or editor dependency from runtime.

