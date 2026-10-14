# Gate 7 — Integration, consumer examples and adversarial release audit

## Entry

Gates 1–6 accepted. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Framework/docs/examples, CkTests gyms; host crawler migration only after framework acceptance; scoped optimization/correction files from audit.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Ship native/AS/BP noncombat examples: standalone inspection arm, borrowed carrier reach, body-only lean, two-limb gesture. Demonstrate the same public library surfaces.
2. Migrate crawler body jab through the player with exact accepted-pose behavior comparison. Add an explicit long-limb sweep demonstration; consumer performs full-segment/world-blocking hit resolution and per-target/window deduplication outside framework.
3. Measure 1/10/100 subjects at 2/4/8 links using existing stats/Insights; record CPU, allocation, query and bounded storage. Optimize demonstrated costs without weakening public contracts.
4. Independent adversarial reviewers cover ownership/order/edge cases, performance, guideline/macro/AS/BP conventions, ergonomics, missing features, editor lifecycle and consumer collision. Main reads every finding, fixes supported findings and records rejected findings with evidence.
5. Run final focused checks after fixes, then one full toolbox gate on final sources. Verify actual logs/process exits and known-red delta. Run public-environment/editor/runtime matrix and visual continuous motion.
6. Update permanent module docs, examples and PROGRESS with concrete evidence. Do not call pending editor/non-editor/AS-off/network checks passed or claim regression-free from compile alone.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate7.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

All mandatory VALIDATION rows have their required evidence, actionable review findings addressed, final build and gate current; user-facing editor/playback/examples fully functional.

## Fences

No performance numbers without measurement, tip-only sweep called full limb collision, unexplained known-red additions, hidden scope cuts, commit or push.

