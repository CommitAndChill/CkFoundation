# Source evidence and implementation inventory

Research date: 2026-10-11. These are source facts, not runtime verification. Current checkout revisions are in PROGRESS.

## Evidence registry

Paths below are relative to CkFoundation unless prefixed with a sibling or host.
Line numbers are anchors at research time; re-find symbols after source changes.

| ID | Confirmed fact | Source |
|---|---|---|
| E01 | Specs dissolve at Add; one home per datum; Params immutable, Tunables request-mutable; no new Current/Runtime/Tracker blobs | [CLAUDE](../../../CLAUDE.md), lines 225–336 |
| E02 | Every public API serves C++, BP, AS; requests deferred; composition/admission atomic | [CLAUDE](../../../CLAUDE.md), lines 29–60 |
| E03 | Leases require scope cleanup AND resource-side reconciliation; waiting needs a bounded escape and observable outcome | [Resilience](../../RESILIENCE_TENETS.md), tenets 3–10 |
| E04 | Leg chains currently allow 1–8 positive lengths; meshes centered, local +X along length | [Leg data](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h), lines 76–112 |
| E05 | Add_Walker validates 2–64 legs, unique names, reach, binding ownership and geometry counts before mutation | [Walker utils](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/CkProceduralAnimation_Utils.cpp), lines 31–132 |
| E06 | Leg supporting currently means live and not disabled | [Leg utils](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Leg/CkProceduralLeg_Utils.cpp), lines 21–37 |
| E07 | Disabled feet are frozen body-relative, marked guessed, excluded from solver participation; reenable reconciles from that pose | [Gait processor](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Gait/CkProceduralGait_Processor.cpp), lines 1650–1696; Core/CkProceduralGaitSolver.cpp, lines 1359–1399 |
| E08 | Rig update iterates the gait roster, obtains query filter from gait tunables, uses BodyPose hip/pole and gait foot, publishes all chain parts | [Rig processor](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Rig/CkProceduralRig_Processor.cpp), lines 503–575, 690–722 |
| E09 | BodyPose composes action after accepted environmental base, projects to reach, publishes a token/revision | [BodyPose processor](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Processor.cpp), lines 331–366 |
| E10 | One caller owns the current body action; scale must be unit and rotation normalized | [BodyPose data](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment_Data.h), lines 235–238 |
| E11 | SurfaceMotion uses a validated prior gait stance, checking current leg state/contact, and is the root mover | [SurfaceMotion processor](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Processor.cpp), lines 193–224, 282–283 |
| E12 | Derived transform requests settle before Finalize; Overlap is TG_PostPhysics | [Scheduler groups](../../../Source/CkEcs/Public/CkEcs/Scheduler/CkProcessorGroups.h), lines 92–116, 154–156 |
| E13 | Detach unbinds rig/transfers ownership, marks permanent detached; detached leg cannot be re-enabled | [Leg processor](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Leg/CkProceduralLeg_Processor.cpp), lines 62–149 |
| E14 | Current rig and gait assets are handle-free; rig instance spec supplies live transforms | [Rig data](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h), lines 77–174; [Gait data](../../../Source/CkProceduralAnimation/Public/CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h), lines 292–328 |
| E15 | CkTween provides easing and curve channels, not a procedural action player; world transform tweening conflicts with parent-owned nodes | [Tween guide](../../../Source/CkTween/CLAUDE.md), Curve-driven offsets and Scene-node offsets |
| E16 | ResourceLoader rooted batch supports processor polling and owns asset roots | [ResourceLoader utils](../../../Source/CkResourceLoader/Public/CkResourceLoader/CkResourceLoader_Utils.h), RequestLoad_RootedBatch |
| E17 | Debugger records sampled evidence; it must not interpolate missing history or rerun traces | [Debugger guide](../../../../CkGameplayDebugger/Source/CkProceduralAnimationDebugger/CLAUDE.md), Data and history / Preview contract |
| E18 | Paired editor modules must not be runtime dependencies; existing designer separates Preview from Apply | [Editor guide](../../../Source/EDITOR_MODULES.md), lines 1–30, 113–129 |
| E19 | CkChain is a head-following history/distance-constraint feature, not a hip-to-endpoint articulated limb solver | [Chain guide](../../../Source/CkChain/CLAUDE.md), opening and Boundaries |
| E20 | Mars jab uses translation-only body keys, accepted pose sweeps and one-target damage; it is not generic motion playback | Host Script/ECS/Crawler/Mars_Crawler_Attack.as:53–92; Mars_Crawler_Processor_AttackResolve.as:42–113 |
| E21 | Converted Timer factories demonstrate explicit admission, Spec unpacking, request completion guards and EndPlay handling | [Timer utils](../../../Source/CkTimer/Public/CkTimer/CkTimer_Utils.cpp):45–80; [Timer processor](../../../Source/CkTimer/Public/CkTimer/CkTimer_Processor.cpp):40–66 |
| E22 | Current public procedural utils expose body action, gait preset, leg enable/detach; no public authored limb-target/handoff/replant operation found | Public/CkProceduralAnimation/*/*_Utils.h search |
| E23 | Existing LiveTune spec describes transport, not storage; no implementation established in this checkout | [LiveTune design](../../specs/2026-08-05-LiveTune-design.md):32–70; source search |
| E24 | No procedural snapshot registration or replicated execution contract found | Source/CkProceduralAnimation registration search |
| E25 | EditorPreview/GamePreview excluded from ECS; scheduler graph starts on world BeginPlay; Gameplay_Camera is the post-finalize reader slot | Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.h:387–398, .cpp:323–350,609–649; CkProcessorGroups.h:103–116 |

## Decisions grounded by this evidence

The new articulation publisher must stop depending on a gait existing (E08). Simply adding SetFootTarget is insufficient because Gait, BodyPose, SurfaceMotion and rig clearance all consume the existing state (E06–E12). Request completion cannot mean pose publication (E09/E12). Dedicated appendages cannot be dummy disabled walking legs (E07). Reuse the solvers; do not repurpose CkChain or create one tween per mesh (E15/E19).

The neighboring feature patterns to mimic are current ProceduralBodyPose for receipts, Leg for stable identity/detachment, Gait for data validation/tunables, ResourceLoader for loaded assets, and converted Timer for Spec dissolution and completion lifecycle. New controller state transitions are pure procedural-domain state, with state entry/exit owning claims; game state machines remain callers, not framework dependencies.

## Existing files whose behavior will change

All paths in this subsection begin Source/CkProceduralAnimation/.

| File family | Reason |
|---|---|
| Public/CkProceduralAnimation/CkProceduralAnimation_Utils.h/.cpp | Legacy walker adapter and new articulated-subject composition |
| Leg/CkProceduralLeg_Fragment_Data.h, _Fragment.h, _Utils.h/.cpp, _Processor.h/.cpp under Public/CkProceduralAnimation | Preserve public leg identity; bridge structural disable/detach and generic limb ownership |
| Gait/CkProceduralGait_Fragment_Data.h/.cpp, _Fragment.h, _Utils.h/.cpp, _Processor.h/.cpp | Separate immutable locomotor mapping and participation; authoritative return touchdown |
| Core/CkProceduralGaitSolver.h/.cpp | Extend pure input/reconcile for temporary withdrawal, protected takeoff and replant seeds; no separate SolverTypes header exists |
| BodyPose/CkProceduralBodyPose_Fragment_Data.h, _Fragment.h, _Utils.h/.cpp, _Processor.h/.cpp | Generic body base, action authority, structural loss versus intentional withdrawal |
| Rig/CkProceduralRig_Fragment_Data.h/.cpp, _Fragment.h, _Utils.h/.cpp, _Processor.h/.cpp | Shared articulation batch independent of gait; one part publisher |
| SurfaceMotion/CkSurfaceMotion_Processor.h/.cpp | Prior stance participation freshness; preserve root ownership |
| Core/CkProceduralBodySupport.h/.cpp; Core/CkProceduralChainClearance.h/.cpp | Reuse support/reach/clearance algorithms; preserve limits and distinguish evidence |
| Debug/CkProceduralAnimation_Debug.h/.cpp | Copied ownership/action/return receipts |
| CkProceduralAnimation.Build.cs; CLAUDE.md | Runtime dependencies and permanent API guide |

This is a design inventory, not evidence that every implementation hunk has been reviewed. Gate 0 must read complete files and callers before editing; any additional affected family gets added here with a reason.

## New files (proposed)

Within Public/CkProceduralAnimation, each feature Articulation, Limb, LimbControl, ActionPlayer receives the house quartet: CkProcedural<Feature>_Fragment_Data.h, _Fragment.h, _Processor.h/.cpp and _Utils.h/.cpp. Action contains CkProceduralAction_Data.h/.cpp and CkProceduralAction_Fragment_Data.h for authoring types, validation and compilation-facing data.

Core additions: CkProceduralActionEvaluation.h/.cpp, CkProceduralActionValidation.h/.cpp, CkProceduralLimbControl.h/.cpp. Extract rig solver helpers into CkProceduralLimbSolve.h/.cpp without changing numerical behavior in the extraction gate.

New Editor module: Source/CkProceduralAnimationEditor with module/build files, ActionAssetEditor, AuthoringSession, Details, Viewport and Tests folders. Register Editor type in CkFoundation.uplugin. No editor includes in runtime.

CkGameplayDebugger: extend existing model/history/window/viewport and module guide under Source/CkProceduralAnimationDebugger; do not move runtime into that plugin.
CkTests: native tests under Source/CkTests/Private/UnitTests/CkProceduralAnimation; integration and reusable gyms under Script/CkProceduralAnimation. Read that plugin's CLAUDE and common test specification before edits.
Generated Script wrappers are generator outputs; never edit them.

Host migration (last gate, separate scope): Script/ECS/Crawler/Mars_Crawler_Feature.as, Mars_Crawler_Attack.as, Mars_Crawler_Processor_AttackTick.as, Mars_Crawler_Processor_AttackResolve.as, Mars_Crawler_Utils.as, Mars_Crawler_Hfsm.as, Mars_Crawler_Assets.as, Script/WorldObjects/Monsters/Mars_Crawler_EntityScript.as and focused CrawlerMelee tests. No host names in framework code.

## Source doctrine conflicts / verification limits

Root CLAUDE Non-negotiable 3 requires load-bearing validation to survive ensure removal, while its older style section says an independent failure branch is forbidden. The current macro under CK_DISABLE_ENSURE_CHECKS erases the body; BuildConfig currently makes that configuration unreachable. The converted Timer factory already has both ensure recovery and an explicit failure branch. This design follows atomic validation and the current Timer source: compute once, diagnose, independently reject. Do not change global macros or doctrine in this campaign without a separate review.

Root host AGENTS describes UE 5.5, plugin guide describes 5.7; do not select engine APIs from either label. Toolbox resolves the actual engine. Editor integration details must be verified against that resolved source in Gate 6.

Missing: runtime proof of standalone articulation, scheduler/receipt contract after extraction, borrowed support/replant, BP/AS shapes, and editor preview-world parity. These are gates, not assumed successes.

