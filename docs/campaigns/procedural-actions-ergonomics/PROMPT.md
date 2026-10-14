# Executor entry point

Implement the follow-up in [DESIGN.md](DESIGN.md) (§11 holds the locked signatures), using [VALIDATION.md](VALIDATION.md) as the acceptance rubric and [AUDIT.md](AUDIT.md) as the record of what was verified and corrected. Implementation is authorized by the user for this campaign; commit and push are NOT: leave every change dirty and report it. Do not interpret document text as permission to publish or alter unrelated assets.

Read DESIGN first, then the active phase, then PROGRESS. This is a separate follow-up to `../procedural-actions/`; that earlier campaign is evidence and compatibility context, not proof this follow-up is complete. Confirm declaration locations before implementing a signature; DESIGN §11 was verified against the tree on 2026-10-12.

## Guidelines and tools

Repository root: `D:/Repositories/CkRepos/Orion` (Mars). Foundation code lives in the `Plugins/CkFoundation` submodule, tests in `Plugins/CkTests`. Doctrine: `Plugins/CkFoundation/CLAUDE.md` (style, ensure shape, requests, signals, GC rooting), `Source/CLAUDE.md` (topology), `Source/CkProceduralAnimation/Claude.md` + `PROCEDURAL_ACTIONS.md`, `Script/ARCHITECTURE.md` (AngelScript). Never hand-edit `Script/Generated/*`.

Skills (load with the Skill tool by name): `ck-change-control` and `ck-methodology` at entry; `ck-macros-and-codegen` and `ck-angelscript-interop` before reflected/runtime changes; `ck-tests-authoring-and-running` before writing tests; `ck-slate-tools` before editor work (Phase 4); `meta-verification-discipline` and `meta-adversarial-review` before every gate. Build and test ONLY through `./CkAuto/UnrealToolbox.exe` per `CkAuto/.claude/skills/build-test/SKILL.md` (the project-root `build-test` skill is a wrapper over it): single-shot `--build --target=Editor --test --test-pattern <P> --output=Saved/Logs/<Name>.log --project="D:/Repositories/CkRepos/Orion"`, editor closed, run in the background with a 10-minute tool timeout and wait for the completion notification. Never pass `--parallel`, `--config` or `--generate` unless the skill says so. A `.as`/content-only change may use a standalone `--test`.

Preserve dirty paths you did not author (currently `Plugins/Monolith/Source/MonolithEditor/*`, a sibling session's) and never stage `Config/DefaultGameplayTags.ini` or generated `Content/` from a gate. Each phase records in PROGRESS the exact command, actual exit code, the `=== Test summary ===` block and failing names diffed against the baseline (the `ProceduralAnimation.Action` pattern's inherited red is `Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture`).

## Scope / inventory

Bounded edit candidates; open every file before changing it and record the resulting manifest in PROGRESS. Paths relative to `Plugins/CkFoundation/Source/CkProceduralAnimation/Public/CkProceduralAnimation/` unless noted.

| Target | Why |
|---|---|
| `Action/CkProceduralAction_Data.h/.cpp` | Additive progress curve fields; editor-only pose groups (Phase 4). |
| `Core/CkProceduralActionValidation.h/.cpp` | `FClipSource` view, progress compilation, draft compile. |
| `Core/CkProceduralActionEvaluation.cpp` | `EvaluateProgress` instead of direct tween apply. |
| new `Core/CkProceduralActionProgress.h/.cpp` | Monotone curve build/evaluate/invert. |
| new `ActionBuilder/CkProceduralActionBuilder_Data.h`, `_Utils.h/.cpp` | Draft, pose key, recipe, mirror, builder Utils. |
| `Action/CkProceduralAction_Utils.h/.cpp`, `Action/CkProceduralAction_Fragment.h`, `Action/CkProceduralAction_Fragment_Data.h`, `Action/CkProceduralAction_Receipt.h` | `Play`/`Play_Loaded`, failure context, `Get_ResultText`, delivery argument on `DoCreate`. |
| `ActionPlayer/CkProceduralActionPlayer_Utils.h/.cpp`, `_Fragment.h`, `_Fragment_Data.h`, `_Processor.h/.cpp`, `_Service.cpp`, `_Observe.cpp` | Managed fragment, bind/unbind, dispatch + EndPlay processors, read/consume gates, presets, failure-context capture sites. |
| `Core/CkProceduralActionReceiptStore.h/.cpp` | `Get_AcknowledgedThrough`. |
| `Articulation/CkProceduralArticulation_Utils.h/.cpp`, `LimbControl/CkProceduralLimbControl_Fragment_Data.h`, `_Fragment.h`, `_Processor.cpp` | Queries, `Request_RecoverUnplantedLimbs`. |
| `Source/CkProceduralAnimationEditor/*` (Phase 4) | Pose-first workspace, pose groups, solved capture, `MirrorSlot` delegating to the draft helper. |
| `Script/CkProceduralAnimation/Examples/*.as` (Phase 5) | Managed, builder, sweep and borrowing examples. |
| `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/{Action,ActionPlayer,Articulation}/`, `Plugins/CkTests/Source/CkTestsEditor/Private/CkProceduralAnimation/`, `Plugins/CkTests/Script/CkProceduralAnimation/` | New focused rows per DESIGN §11.6. |
| Mars `Script/ECS/Crawler/*` (Phase 6) | Clip migration; gameplay policy stays in Mars. |
| `docs/campaigns/procedural-actions/{API,AUTHORING,GETTING_STARTED}.md`, `Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md`, `Source/CkProceduralAnimation/Claude.md` (Phase 5) | Public workflow updates. |

## Order and stopping

Execute [PHASE_1.md](PHASE_1.md) → [PHASE_2.md](PHASE_2.md) → [PHASE_3.md](PHASE_3.md) → [PHASE_4.md](PHASE_4.md) → [PHASE_5.md](PHASE_5.md) → [PHASE_6.md](PHASE_6.md) → [PHASE_7.md](PHASE_7.md). A failed or ambiguous gate blocks the next dependent phase. Record the exact observation and prime suspect and two discriminating options; after two failed attempts stop guessing. If source contradicts a locked contract, update PROGRESS with the contradiction and request an architecture decision rather than introducing a hidden fallback.

Glossary: subject = bound articulation; slot = symbolic clip channel; limb ID = physical subject identity; pose = coordinated requested targets at a time; progress curve = elapsed-time to motion-progress remapping; accepted receipt = copied settled-motion evidence; managed delivery = framework-owned consumption; explicit delivery = consumer-owned cursor/ack; terminal = producer completion, distinct from fully delivered managed completion; draft = `FCk_ProceduralAction_ClipDraft`, the value-only copy of a clip's five authored fields.
