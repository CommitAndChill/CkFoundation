# Design audit (2026-10-12, Fable 5.1, before implementation)

Every load-bearing claim in the 2026-10-12 DESIGN draft was checked against the working tree at
CkFoundation `b23668049` / CkTests `96ef615b` / Mars `6a43f51` (all on `feature/procedural-anim-ext`,
source clean). Paths below are relative to `Plugins/CkFoundation` unless a repository is named.

## Confirmed

| Claim | Evidence |
|---|---|
| `BindTo_OnFinished` exists and is a producer signal | `Action/CkProceduralAction_Utils.h:126-131`; fired from `FFragment_ProceduralAction::CommitState` (`Action/CkProceduralAction_Fragment.cpp:13-24`) |
| Loaded clips compile/copy synchronously, including a synchronous terminal rejection | `Action/CkProceduralAction_Utils.cpp:142-207` (compile at 174, `Finish` + `CommitState(Finished)` at 196-198) |
| AS already constructs a transient clip through setters | Mars `Script/Tests/CrawlerMelee/Mars_AutoTest_CrawlerMelee_LongLimbLoaded.as:67-110` |
| The crawler body attack is a manual action driven by a game phase clock | Mars `Script/ECS/Crawler/Mars_Crawler_ActionAdapter.as:16-27`, `Mars_Crawler_Processor_AttackTick.as:388-395` (446 lines) |
| Clip Details and preview Details sit together beside the transport | `Source/CkProceduralAnimationEditor/Private/Toolkit/CkProceduralAction_EditorToolkit.cpp:192-230` |
| Authors create phases, slots, markers and windows one by one | `Private/Widgets/SCkProceduralAction_Timeline.cpp:40-70` |
| New clips are bare data objects | `Private/Asset/CkProceduralAction_Factory.cpp:24-26` |
| Presets were proposed, not implemented | `docs/campaigns/procedural-actions/AUTHORING.md:112` |
| Smooth = per-axis bounded cubic; rotation = normalized slerp; easing is phase-level; key critical times are inverted through the easing; markers/windows resolve on elapsed phase time | `Core/CkProceduralActionEvaluation.cpp:176-181, 208, 234, 319`; `Core/CkProceduralActionValidation.cpp:130-145 (InverseEasing), 463-490 (ResolveTime)` |
| R48: 132 selected / 131 passed / 1 failed / 0 contaminated, `MountedToolkit.TwoTabsModesAndCapture` | Mars `Saved/Logs/ProceduralActions-Commit-R48.log:24849-24860`; that test is on no known-red list (reported `NEW FAILURES`), so it is an inherited red, not this follow-up's |
| Identity is reserved before the action entity exists | `Action/CkProceduralAction_Utils.cpp:51-56` (`ReserveUnbound` / `Reserve` precede `Request_CreateEntity`) |
| Signal broadcast stores its payload and frame on the entity even with no listener; a same-frame bind with the default policy replays it | `Source/CkEcs/Public/CkEcs/Signal/CkSignal_Utils.inl.h:153-163, 466-470` |

## Corrected in DESIGN.md

| # | Was | Now | Why |
|---|---|---|---|
| X1 | "the already-observed loaded-clip synchronous terminal path losing its callback" | The signal is not lost: a `BindTo_OnFinished` in the same frame replays the terminal payload. What is lost is *stream* delivery ownership: a managed registration made after `Create_FromLoadedClip` returns would follow the synchronous `Finish`. Managed `Play` therefore registers delivery inside the shared creation path, before any terminal can be committed. | `CkSignal_Utils.inl.h:153-163` |
| X2 | §8 mirror "first implement explicit pose/recipe expansion" with no mention of existing code | The editor already has a transactional `FCk_ProceduralActionAuthoringCommands::MirrorSlot` (`Private/Authoring/CkProceduralActionAuthoringCommands.cpp:710-784`): position/bend keys and tangents only, body/root-local only, no orientation. The runtime draft `Mirror_Slot` becomes the single geometry (adds orientation as `q' = (w, -S v)`), and the editor command delegates to it. | one geometry, no duplication |
| X3 | New test namespace `Ck.ProceduralAnimation.Ergonomics` | Feature namespaces: `Ck.ProceduralAnimation.Action.{Builder,Progress,Mirror,Managed,Diagnostics}`, `Ck.ProceduralAnimation.ActionPlayer.Presets`, `Ck.ProceduralAnimation.Articulation.Query` | a campaign name is a process breadcrumb; test names ship |
| X4 | PROMPT.md: skills under `C:/Users/Adam/.codex/skills/` | Ck skills: `Plugins/CkFoundation/.claude/skills/<name>/SKILL.md`; meta skills: `C:/Users/Adam/.claude/skills/<name>/SKILL.md`; build: `CkAuto/.claude/skills/build-test/SKILL.md` | stale environment |
| X5 | VALIDATION commands hardcode `--parallel=1 --editor-args=-nocefaccelpaint` | Dropped. The build-test skill forbids a hardcoded width except to pin a measurement; the CEF workaround was an R45 diagnostic and R48 ran the mounted test without it. | `CkAuto/.claude/skills/build-test/SKILL.md` § Parallel lanes |
| X6 | `Build_Into` "validates/compiles a pinned candidate" with no mechanism | `Compile` gains a non-owning source view (`ck::procedural_action::FClipSource`); both the asset and the draft produce it. No transient UObject, no asset layout change, one compiler. | DESIGN §3.2 "one compiler" |
| X7 | New `FCk_ProceduralAction_BuildReport` | Dropped; builder functions return the existing `FCk_ProceduralAction_ValidationReport` (`Advisory = false`). | reuse before invention |
| X8 | Dispatch budget "player-capacity group ... default 32" unnamed | `FCk_ProceduralAction_Capacity::_ManagedRecordsPerTick = 32` (construction group, validated by `ValidateCapacity`) | capacity is a construction group |
| X9 | §9 "Preserve existing combat tuners" verbatim | `CancelSeconds` becomes the clip action's `_DedicatedReturn` (tuning override); the cancel ease is the limb-control return, not `RecoverEasing`; `PoseTimeoutSeconds` is a manual-only watchdog and stops applying; the offensive window spans all three posed phases (`Get_IsOffensivePhase`, Mars `Mars_Crawler_Attack.as:33-37`). These are approved behaviour changes, listed in PHASE_6. | the clip player owns the return |
| X10 | Managed records/completion as CK signals | Per-identity dynamic delegate lists in a player-owned `FFragment_ProceduralActionPlayer_Managed`. A CK signal retains its last payload on the entity (a copied page) and replays it to late binders, which §7 forbids. | `CkSignal_Utils.inl.h:162` |
| X11 | Progress fields hashed unconditionally | Hashed only when `_ProgressMode == CustomCurve`, so every existing clip keeps its content hash (`ContentRevision` stamps in receipts and tests). | T1 |

## Adversarial review of the corrected design (only what fired)

1. **Lifetime.** A managed-records listener may call `Play` from inside its callback, which adds an entry to the same `TMap` the dispatcher is walking. Fence: the dispatcher snapshots the identity keys (sorted) before any callback and never holds a map reference across one; new entries are eligible next pass.
2. **Lifetime.** After a managed completion the dispatcher releases the live action through `Request_Release`. It must skip an action handle that is invalid or already carries `FTag_DestroyEntity_Initiate` (scope lost), or the request completes `Failed_NotEnqueued` under an ensure every time a scope dies first.
3. **Order.** Terminal records are written in `FGroup_Physics` (`ReconcileOwnerLoss`) and `FGroup_Transform_Derived` (`Service`, `HandleRequests`); accepted poses in `FGroup_Gameplay_Camera` (`Observe`). A dispatcher in `FGroup_Gameplay_Camera` with `RunAfter<FProcessor_ProceduralActionPlayer_Observe>` sees all of a frame's records. The `OwnerLost` terminal written by `FProcessor_ProceduralAction_EndPlay` (`FGroup_EndPlay`) is seen one frame later: acceptable, document it.
4. **Edge.** The synchronous `Finish` in the loaded-clip reject path lands a Terminal record before any dispatch pass; the completion therefore fires on the next `ManagedDispatch` pass, never inside `Play`. That is the M1 guarantee and must be the test.
5. **Hidden coupling.** `FReceiptStore::Consume` removes a stream once `Terminal && EntityRemoved && fully acknowledged` (`Core/CkProceduralActionReceiptStore.cpp:655-656`). The dispatcher must copy the terminal `Result` from the page *before* consuming, or the completion payload reads a removed stream.
6. **Adjacent (flagged, not fixed).** `Action/CkProceduralAction_Utils.cpp:40-43` and every sibling use the retired `CK_ENSURE_IF_NOT { } if (NOT Valid) { }` double shape; new code uses the single shape, existing lines stay. `Articulation/CkProceduralArticulation_Utils.cpp:344-355` rejects `Request_RecoverLimbs` without `CK_ENSURE_IF_NOT` (non-negotiable #3); the new `Request_RecoverUnplantedLimbs` beside it follows doctrine, the old one is left for its owner.

## Not changed

- §3 locked architecture, §4 authoring journey, §6 timing/spatial separation, §7 delivery contracts, §10 compatibility and cost rules stand as written.
- `TOptional` stays out of reflected surfaces: this module already uses enum-mode + value (`_TuningMode`/`_Tuning`, `_HasBend`/`_BendDirection`), which ADJUDICATIONS A1 says to match.

## Post-implementation review corrections (2026-10-12, independent reviewer, confirmed by the orchestrator)

- The claim in §7 that the gameplay debugger observes receipts through a store friend was wrong: `Plugins/CkGameplayDebugger/.../CkProceduralAnimationDebugger_Model.cpp:482` calls the public `Read_Receipts` and stops recording on any result other than Succeeded/Pending/Gap. Sealing reads therefore froze the debugger on every managed action. Fix: reads are unsealed (non-consuming); only `Request_ConsumeReceipts` is sealed.
- `PlayerLost` left the action entity alive under its scope (`ManagedEndPlay` cleared the entry without destroying the action, and a later `Request_Release` cannot succeed without the player). Fix: the EndPlay pass requests destruction of each lost entry's live action.
- `ECk_ProceduralAction_DeliveryStatus::Gap` and the managed page's missing range were unreachable (the dispatcher is the sole acknowledging reader). Fix: removed from the API before it ships.
- The seal was derived from entry presence, which is removed at completion while the stream may be retained a frame longer. Fix: the seal is a bit on the store's stream, set at reservation.

