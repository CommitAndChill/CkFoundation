# Execution gate index

Status lives only in PROGRESS. One executor session handles one gate; split a gate before execution if its measured inventory exceeds half-session context. A gate may have implementation checkpoints, but none can be called complete from compilation alone.

| Gate | Deliverable | Depends on | Design confidence / unresolved proof |
|---|---|---|---|
| [0](PHASE_0.md) | Baseline, public surface, scheduler and preview feasibility proofs | design review | Medium: three environment reflection and production preview world unproven |
| [1](PHASE_1.md) | Standalone articulation + legacy adapter and one publisher | 0 | Medium: substantial extraction, must prove old pose equivalence |
| [2](PHASE_2.md) | Scoped claims and manual dedicated-limb control | 1 | High contract confidence; lifecycle stress still required |
| [3](PHASE_3.md) | Borrow/replant and unified support participation | 2 | Medium: gait reconciliation and prior feet-plane coupling |
| [4](PHASE_4.md) | Handle-free clips, compiler and pure evaluation | 0 | High data contract confidence; reflected/editor key types need Gate 0 |
| [5](PHASE_5.md) | Managed clip playback, receipts and bounded inbox | 2,3,4 | Medium: full scheduling/acknowledgment integration |
| [6](PHASE_6.md) | Real authoring editor + observed live debugger | 1,4,5 | Medium: engine-specific toolkit and production world host |
| [7](PHASE_7.md) | Noncombat exemplars, explicit game migration and final gates | 1–6 | Medium until real motion/collision and all public surfaces proven |

All runtime phases are at least change-control class 3 because they alter shared procedural behavior. Changes to global scheduler, snapshot infrastructure, macros or replication would escalate to class 4 and are outside this design's default scope.

Rollback for each implementation gate: revert only that gate's authored source/content changes while preserving unrelated work. Source-only planning can be removed by deleting this new package when explicitly requested. No commit/push/rebase is authorized by the design request.

