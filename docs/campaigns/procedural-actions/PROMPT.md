# Procedural actions and articulated limbs — design and implementation brief

Written: 2026-10-11. Status: proposed architecture, source-reviewed; runtime implementation not started.
Current evidence and execution state live only in [PROGRESS.md](PROGRESS.md).
This package is retired when its implemented contracts have moved into the module guides, or it is explicitly superseded.

## Read this first

Design a reusable procedural animation library for rigid articulated subjects. A walking limb may temporarily perform an authored action and return to locomotion. A dedicated appendage must work without a gait or SurfaceMotion. Both use the same articulation, validation, pose publication, and diagnostics. Body actions compose with environmental support. Actions contain motion and named timing information, never combat logic.

The acceptance specification is [VALIDATION.md](VALIDATION.md). It is a rubric and named test specification, not a claim that tests exist or pass. The user has authorized implementation after design review using tiered delegation, followed by an independent adversarial review and fixes. Proposed declarations in [API.md](API.md) must pass UHT, Blueprint and AngelScript gates before use.

## Package

1. [RESEARCH.md](RESEARCH.md): source facts, examples, boundaries, file inventory.
2. [ARCHITECTURE.md](ARCHITECTURE.md): ownership, scheduling, support, failure, compatibility.
3. [DATA_MODEL.md](DATA_MODEL.md): assets, settings, tunables, bindings, state and visual separation.
4. [API.md](API.md): public contracts and caller flows.
5. [AUTHORING.md](AUTHORING.md): asset editor, preview, diagnostics and tuning.
6. [VALIDATION.md](VALIDATION.md): release rubric and adversarial cases.
7. [PHASES.md](PHASES.md): dependency and confidence index.
8. PHASE_0.md through PHASE_7.md: bounded execution gates.
9. [REVIEW.md](REVIEW.md): decisions, rejected approaches and review disposition.
10. [PROGRESS.md](PROGRESS.md): actual status and next action.

## Locked design decisions

| Decision | Choice |
|---|---|
| Runtime home | Extend CkProceduralAnimation with Articulation, Limb, LimbControl, Action and ActionPlayer features; pure algorithms stay in Core |
| Editor home | New paired CkProceduralAnimationEditor Editor module in CkFoundation |
| Live evidence | Extend CkProceduralAnimationDebugger in CkGameplayDebugger; runtime never depends on debugger/editor |
| Transform ownership | One articulation publisher owns registered segment/terminal/presentation outputs; SurfaceMotion or the host's mover owns the root |
| Locomotion | Optional producer of limb targets; existing Leg/Gait public calls remain compatibility adapters |
| Limb state | Separate topology/liveness, administrative enabled state, gait participation, controller ownership, and contact evidence |
| Actions | Immutable handle-free clip data, named bindings, one authoritative timeline per action instance, explicit accepted-pose receipts |
| Concurrency | Disjoint limb-only actions may coexist; body action is subject-exclusive against other actions; no implicit preemption or action queue |
| Borrowed support | Explicit support admission and bounded replant; no Disable-as-borrow |
| Pose controls | Body offsets and limb endpoint/orientation/bend tracks; rigid kinematics, no promise of physical whip dynamics |
| Root actions | No root-transform track; host supplies movement via its existing mover |
| Gameplay | Selection, target eligibility, hit detection, damage, cooldowns, AI and networking authority remain consumer concerns |
| State lifetime | Scope-owned action instances, resource-side reconciliation, bounded loading/acquisition/publication/return |
| Persistence | Transient procedural execution; reconstruct neutral composition after load. No claimed network determinism or in-flight restore |
| Legacy path | Existing walkers and body-jab behavior retained until explicitly migrated and compared |
| Data updates | Active actions pin a compiled revision; edits affect subsequent instances, not live arrays underneath a solve |

## Scope

Release requirements: arbitrary named rigid appendages within current supported bounds; dedicated and borrowed limbs; neutral/body/limb actions; synchronized accepted samples; curve/key authoring; reversible editor editing; diagnosed admission and terminal outcomes; compatibility tests; noncombat reusable examples.

Not included: an ability system, gameplay attack graph, arbitrary skeletal/Control Rig adapter, dynamic topology during gait life, physics ropes, unconstrained motion matching, general animation blending graph, moving-platform attachment, network prediction, cross-machine deterministic Jolt simulation, or multiplayer hit authority. These are explicit extension boundaries, not undocumented capabilities. Do not resurrect parked flow-graph/ability-trait branches.

## Required doctrine

Read the actual current files, not cached excerpts:
- [Framework doctrine](../../../CLAUDE.md), [source map](../../../Source/CLAUDE.md), [resilience](../../RESILIENCE_TENETS.md).
- [Module guide](../../../Source/CkProceduralAnimation/CLAUDE.md), [AS architecture](../../../Script/ARCHITECTURE.md), [editor modules](../../../Source/EDITOR_MODULES.md).
- Skills: ck-plan-handoff and ck-methodology for execution preparation; ck-change-control at each gate; ck-macros-and-codegen before native public data; ck-angelscript-interop before public-surface proof; ck-tests-authoring-and-running before tests; ck-slate-tools and ck-gameplaydebugger-extension before tooling.
- Build/test through the host's CkAuto/UnrealToolbox.exe. The actual skill in this checkout is CkAuto/.claude/skills/build-test/SKILL.md; the .Codex path in the wrapper is stale.

Resolve source drift before implementing. A required experiment failing is a stop condition: record the actual observation, revise the affected contract, and obtain review. Do not silently weaken acceptance.

## Glossary

- Articulation: fixed collection of named rigid limbs and their presentation bindings.
- Limb: geometry and attachment identity, independent of whether it walks.
- Locomotor member: limb captured into a gait's immutable index mapping.
- Controller: exactly one producer currently entitled to supply a limb target.
- Borrow: bounded transition from gait ownership to authored control, with support removed explicitly.
- Receipt: copied evidence that a particular action sample was solved and its terminal transforms settled.
- Clip: handle-free motion intent and timing data, without gameplay policy.
- Phase: named interval with a positive duration; not a game attack state.
- Window: named half-open interval in clip time; consumers assign meaning.
- Return: framework-owned recovery from actual accepted pose, ending in trusted replant or diagnosed failure.
- Revision: identity of the immutable compiled content used by an action.
- Retargeting: explicit mapping of named slots and coordinate conventions; not automatic scaling.

## Executor kickoff

Read PROGRESS, VALIDATION, ARCHITECTURE, DATA_MODEL, API and RESEARCH, then execute PHASE_0. Verify checkout revisions and unrelated work. Establish the existing gates and architectural proof fixtures. Continue dependency-ordered implementation only after each predecessor's observations support the design. Use tiered delegation with disjoint file ownership; integration and final verification remain in the main thread. Finish with independent adversarial review and address findings. Record commands, counts, failures, source changes and unresolved assumptions in PROGRESS. No commit or push unless separately requested.

