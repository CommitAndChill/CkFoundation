# Phase 7 — final evidence and adversarial acceptance

**Entry.** Phases 1–6 recorded on final source bytes. Freeze source before the final build/test; any repair invalidates the affected gate and the final evidence until rerun.

## Steps

1. Final focused gates on the frozen tree, in this order, each with its own log:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-Final-PA.log --project="D:/Repositories/CkRepos/Orion"
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-Final-Crawler.log --project="D:/Repositories/CkRepos/Orion"
   ```
   Do not run the full suite unless the maintainer asks; do not add to any known-red list.
2. Three environments: C++ (native rows), AS (public-surface rows), Blueprint VM (parity rows) all present in the final logs by name. Native AS-off build: `./CkAuto/UnrealToolbox.exe --build --target=Game --output=Saved/Logs/ProceduralErgonomics-Final-Game.log --project=...` (compile only; record exit code). A cooked route is reported as Blocked unless the existing `CkTests/Tools/ProceduralActionsSmoke` workflow is run exactly as documented there.
3. Measure: builder/compile/expansion cost and steady-state managed dispatch with the existing `Test_ProceduralAction_Performance.cpp` harness style (same scene/count/frames before/after, native timers); report allocation sites and retained counts; no invented targets.
4. Independent adversarial review: a fresh session audits the diff with `meta-adversarial-review` across data/tuner/visual separation, Ck standards, public ergonomics, runtime/editor dependencies, GC/lifetime, scheduling, reentrancy, gaps, compatibility, allocations, replay. Reproduce findings; fix confirmed blockers; rerun affected gates; record dismissed claims with evidence.
5. Final dossier in PROGRESS: explicit file manifest per repo, baseline → final counts and failing names per pattern, build/runtime evidence paths, measured costs, rejected designs, outstanding limits (`[EDITOR-VERIFY]` rows), rollback (`git -C <repo> checkout -- <paths>` for each repo). No commit or push.

## Exit

Every mandatory VALIDATION row has evidence; no new unexplained failure; the inherited mounted red is either fixed and demonstrated or stays an explicit release blocker. Report the narrowest supported completion claim where an environment gate is unavailable.
