# Validation — definition of done

All commands from `D:/Repositories/CkRepos/Orion`, toolbox only. Say which pattern produced each
number; a focused green is not the full gate.

## Per-phase gates (already in each PHASE file)

| Pattern | Baseline on 2026-10-13 | Expected after |
|---|---|---|
| `ProceduralAnimation.Action.Editor` | 62 rows, 62 green | 62 − deleted + new, all green |
| `ProceduralAnimation` | 446 rows, 439 green; reds: 4 listed + `CirclesCylinderUpAndDown`, `OccludedPlantSteps` (flaky, green alone) + `SpiderCourseTraversal` (pre-existing, unlisted) | same names red, nothing new |
| `CrawlerMelee` (host) | 38/38 | 38/38 (+ `PincerClawSeveredMidStrike`) |
| `Crawler` (host) | all green | all green |
| `IsmRenderer` | 9/9 | 9/9 |
| `Jolt.Query` | green | unchanged |

## Final gate

`./CkAuto/UnrealToolbox.exe --build --target=Editor --test --no-live --discover-fresh --project="D:/Repositories/CkRepos/Orion"`
(~45 min). Exit 0 with every failure on the known-reds list. Paste the `=== Known reds ===` block
and `NEW FAILURES` (must be empty) into PROGRESS.md.

## Three environments

- C++: the suites above.
- AngelScript: the host compiles (`Script/ECS/Crawler/*.as`, `Script/Editor/Mars_CrawlerAttackClips.as`)
  and the three `Plugins/CkFoundation/Script/CkProceduralAnimation/Samples/*.as` compile against the
  new `Add_Walker` spec; `utils_procedural_articulation::Request_ClearFault` and
  `utils_procedural_action_editor::Get_EditorData` exist in `Script/Generated`.
- Blueprint `[EDITOR-VERIFY]`: in a Blueprint graph, place `Add Walker` (one Spec pin),
  `Request Clear Fault`, `Create From Clip` (Delivery defaults to Auto). Nodes compile.

## `[EDITOR-VERIFY]` (maintainer)

1. Phase 3: open the four crawler clips, confirm subject under the sidecar and labels on key rows, save.
2. Phase 4: in Solved mode, drag a claw with the gizmo and release — the output log shows one
   `CkProceduralActionSolverWorld` creation, not two; scrub back, re-solve is instant.
3. Rotate (E) and bend (R) gizmo modes write orientation/bend keys (Details shows the change).
4. Undo a subject change: meshes refresh. Undo a rename: selection stays on the section.
5. PIE Room 5: sever a pincer claw mid-strike — the crawler keeps posing its other legs; kill a
   crawler mid-attack — behaviour unchanged by this campaign (collapse is a separate feature).

## PR hygiene

- Cherry-pick `907508cbe` (runtimemesh perf) onto `dev` as its own PR before this branch's PR.
- PR body names the CkEcs change (EntityScript spawn-cancel widening on teardown; the
  `GamePreview` opt-in) and the CkJolt query options (no production consumer yet) as separately
  reviewable items; route per `ck-change-control`.
- No `AutomationGate.json` edits from this repo; a red proven pre-existing goes to CkPlugins by PR.
