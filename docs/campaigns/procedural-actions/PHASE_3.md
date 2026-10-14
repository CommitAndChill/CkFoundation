# Gate 3 — Gait borrowing, support and recovery

## Entry

Gates 1–2 accepted. Verify actual status/revisions in [PROGRESS.md](PROGRESS.md). Read [RESEARCH.md](RESEARCH.md), the applicable complete source files and nested guidance. Load ck-change-control, ck-tests-authoring-and-running and build-test before code/test work; public data also requires ck-macros-and-codegen and ck-angelscript-interop. Tooling requires ck-slate-tools and ck-gameplaydebugger-extension.

## Ownership and contract

Gait/Core gait solver, Leg, BodyPose, SurfaceMotion and LimbControl with focused tests.

[ARCHITECTURE.md](ARCHITECTURE.md), [DATA_MODEL.md](DATA_MODEL.md), [API.md](API.md) and [AUTHORING.md](AUTHORING.md) own the designed contracts. Do not invent a competing API or reinterpret a proposed signature as compiled. If the inventory cannot fit a bounded executor session, split checkpoints with disjoint file ownership before editing; keep the same gate acceptance.

## Steps

1. Add independent gait participation/contact generation; keep structural disabled/detached semantics and legacy MinLegsToWalk behavior unchanged.
2. Acquisition preserves current contacts until all resources can commit. Reject Unplanted/Returning limbs and incoherent support; capture RootAtStart at commit.
3. Protect aggregate trusted support before every takeoff lane. Emit emergency request without taking off; action service closes/cancels, then later service permits degraded emergency stepping. Bound ordinary support hold.
4. Invalidate both prior reach stance and fitted feet-plane evidence. BodyPose uses structural loss for damage sag and participants for conform/reach.
5. Implement actual-endpoint recovery via existing foothold/reservation checks; ownership transfers only at trusted touchdown. Persist original policy/deadline through scope death. Failure becomes Unplanted, explicit retry only.
6. Run biped-last-contact, multi-borrower, mid-swing, floor removal, slope, forced-root motion and detach tests plus focused gate.

## Verification and decisions

Use the focused command below after a complete source checkpoint; main thread owns build/test serialization and all writers pause during each run:

```powershell
./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --no-live --discover-fresh --output=Saved/Logs/ProceduralActions-Gate3.log --project="D:/Repositories/CkRepos/Orion"
```

Expected: build exit 0, fresh AS compilation, a nonzero discovered/executed relevant population, and no NEW failures versus recorded known reds. Exact future test counts are not invented. Record actual counts/names and mtimes. If compile fails, repair this checkpoint before proceeding. If a NEW failure reproduces only with this change, restore the offending step then diagnose. If a proposed engine/scheduler/API contract is disproven, stop the dependent work and amend/review the design. Anything else unexplained: STOP, record observation and blocker in PROGRESS; do not weaken acceptance. An unavailable environment is pending evidence, not a passing skip.

## Exit

V06–V12 pass; no stale cached support, no healthy-leg damage sag, no immortal return/hold and no implicit reborrow after failure.

## Fences

Do not borrow by Disable, restore stale plant trust, reset timeout on cancellation, introduce another root mover or bypass emergency ordering.

