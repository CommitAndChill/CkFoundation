# Gate 5 — Managed clip playback

## Entry

Gates 2–4 accepted. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Action/ActionPlayer processors and utilities, ResourceLoader integration, BodyPose authority bridge, receipts and tests.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Implement Create_FromClip loading via rooted ResourceLoader batches with finite deadlines and generation checks. No claim before content is compiled and bindings validated.
2. Drive all tracks from one action clock, evaluate against captured start frames, blend from actual accepted source, and wait for qualified terminal publication before committing clock.
3. Use subject-wide candidate validation for concurrent limb actions; no successful subset on a coupled failure. Enforce body subject-exclusivity through return.
4. Publish ordered AcceptedPose/WindowsClosed/Terminal stream records. Consumer acknowledgement is action-identity scoped; closure preserves historical earlier intervals.
5. Prove load/GC/cancel races, revision pinning, narrow windows/hitches, mismatch fail-stop and return outcomes using actual ECS scheduling. Run focused gate.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate5.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V13–V20, V22 and V30 integrated cases pass; request success never masquerades as playback/pose completion.

## Fences

No damage/attack tags, target acquisition, root track, arbitrary UObjects in compiled data, or timing from independent tween timers.

