# Procedural action clip editor, redesigned

Status: design for review, 2026-10-13. Nothing in this document is implemented. The mockup it describes is
published separately; this file is the engineering contract behind it.

## 1. Why

The clip editor shipped on 2026-10-12 is correct but not usable the way Unreal's animation editors are: no time
ruler, poses as buttons, keys as counts, a hand-drawn gizmo, three playback clocks, four copies of the selection, and
a 2,705-line toolkit wiring them by hand (audit, §7 of this file). The maintainer's brief: redesign from a UX-first
mockup, keep the experience lean, and make the code simpler, not bigger. The colleague's needs: pose a subject with
the standard gizmo (in the editor and in a level), record poses, assemble a clip with blend states on a proper
timeline, and draw on Control Rig and Persona where they fit.

## 2. Goals and non-goals

Goals
- A new attack is authored end to end with the gizmo and the timeline, no raw Details.
- Everything on screen is in seconds and section terms; phase-normalized time never reaches the user.
- One editor model, one write path, one transport. The editor module shrinks.
- The same posing tool works in the asset editor and on a subject placed in a level.
- Every existing clip asset opens unchanged and plays identically.

Non-goals
- Sequencer or MovieScene integration (frame-number sequences, bindings and tracks the data model does not have).
- A pose asset type (see §5.2).
- Changing what a clip can express at runtime: the compiler, evaluator and player are not part of this design,
  except the one data hygiene change in §5.1.

## 3. The editor, as the user sees it

Layout (Persona's shape): toolbar; Channels | Viewport | Details; Timeline; Transport.

Toolbar: Save; Subject (the preview config, shared per creature); Record pose (S); Solved / Authored; + Section;
+ Slot; Issues count.

Channels: Body, then the clip's slots each bound to a limb of the subject by a dropdown, then the subject's other
limbs listed as "under the gait", then the subject summary (rig, gait, floor). A tick per channel chooses what Record
covers. Click selects the channel's current key.

Viewport: `SEditorViewport` on `FAdvancedPreviewScene`. The subject drawn with the preview config's meshes and
chain segments; the rest pose dashed. Click a limb to select it (hit proxies). The standard editor gizmo
(W/E/R: move, rotate, bend guide) moves the selected channel; releasing writes the key through a command with undo.
HUD: mode, time, section, receipts.

Details (`IDetailsView` on a selection proxy object): a key (position, orientation, bend, reach policy, pose name,
space read-only), a section (name, duration, easing or custom curve, "starts from current pose"), or the clip
(subject, length, windows, entry blend). Issues below, each naming the field; clicking one selects it.

Timeline (custom Slate, one widget): a ruler in seconds (`SSimpleTimeSlider`), a Sections row (phases as bars; drag
a boundary to change a duration; the easing shape drawn inside; a hollow start marker when the section starts from
the current pose), one track per channel with keyframe diamonds, an Events lane (markers as diamonds, windows as
bars, draggable), and a Poses row naming recorded frames. Playhead drag, zoom and pan. Keys drag in time within
their section.

Transport: `SScrubControlPanel` (play, step, loop, scrub, zoom) plus the time in seconds, frame, and section
fraction; snap to 1/60 s by default.

Curves tab: `FCurveEditor` with one curve per channel axis over the clip's keys, writing through the same commands.
Section timing is an easing preset only (§4.3 G2); there is no separate progress curve to edit.

Keyboard: S record, Delete, Ctrl+Z/Y, Space play, F frame subject, Home/End.

### 3.1 Flows

New attack: create the clip (pick the subject and a starting shape) → add sections and drag durations → add a slot
and bind it → scrub to a section's end, pose with the gizmo, S → repeat per section → mirror the slot → pick
easings → play Solved → fix issues → save. Nine actions, no Advanced panel (the mockup lists them).

Edit an existing attack: open the clip; it opens on its subject; click a diamond; drag the gizmo; S.

Pose in a level: enter the Procedural Pose mode; pick a placed preview subject or spawn one; choose the target clip,
section and time; pose; Capture. Play the clip over the placed subject through the production solver.

## 4. Decisions

D1. Blend in and out are not new data. A section's easing is its blend in; the next section's easing is its blend
out; a section that starts from the current pose (no time-0 key, the carried-start rule) needs no blend at all. The
timeline draws the easing shape inside the bar so it reads like a montage section.

D2. A pose is a keyframe row, not an asset. A recorded pose is the set of keys at one time plus a name in the Poses
lane (today's pose group). Reuse across clips is copy/paste of a pose. One asset per clip and one preview config per
creature, shared by its clips (today: one preview per clip, paired by a user setting and a naming convention).

D3. One editor model. `FCk_ProceduralActionEditorModel` owns the selection (one of: key, section, event, clip,
channel), the playhead and transport state (time, playing, looping, Solved/Authored) and a revision counter.
Widgets read it through attributes and write it through its methods; nothing else holds a selection or a clock. The
solver world and the session become playback sources the model polls.

D4. One write path. Every clip mutation is an `FCk_ProceduralActionAuthoringCommands` command built on the runtime
builder (`FCk_ProceduralAction_ClipDraft` + `Write_Into`), inside one `FScopedTransaction`, bumping the model's
revision. The curve editor, the gizmo, the timeline and the details proxy all call commands. The hand-written copy
and modify bodies and the curve adapter's direct writes go.

D5. One time conversion. The session owns `ElapsedToKey(Seconds) -> (PhaseId, KeyTime)` and `KeyToElapsed`, where
key time is the eased progress at that instant (keys are sampled in progress space; markers and windows stay in raw
phase fraction, which is linear in elapsed time). Capture, selection, the gizmo and the diamonds all use it. This
fixes the confirmed bug where capture stores a pose at the raw fraction and the gizmo at the eased one.

D6. Sections start from the current pose by default. Recording at a section's end writes that section's key at 1
and nothing at the next section's 0. A start key is authored only to force a jump. This removes the duplicate pose
at every boundary and is what the carried-start rule was added for.

D7. Posing is one Interactive Tools Framework tool. `UCk_ProceduralPose_Tool` (a `UInteractiveTool` with a
`UCombinedTransformGizmo`) edits one channel's endpoint, orientation or bend guide against a subject frame and
commits through a command. The asset editor hosts it through `FAssetEditorModeManager`; the level mode hosts the
same tool. If hosting the tools framework in the asset editor proves heavier than expected in the first spike, the
fallback is the viewport client's built-in widget, with the level mode alone on the tool; that fallback is a decision
the maintainer makes, not the executor.

D8. The level mode needs a placed subject. `ACk_ProceduralAction_PreviewActor` carries a preview config and draws
the subject at its rest pose in the editor world (the same mesh placement the viewport uses). Posing reads its rig
and frames; capture writes into the chosen clip; "play here" runs the solver world and draws its result over the
actor. Nothing in the level is simulated.

D9. The viewport does not display the solver world. The solver world stays a private game-preview world; the
viewport draws its snapshots, as today. Merging the two is the heavy option and buys nothing the user sees.

D10. Test-visible surface. Widgets expose stable `Tag`s and the toolkit exposes its model, so the editor tests drive
the model and read the widgets, not button text.

## 4.1 Reuse before building (maintainer's rule for this work)

Every panel is an engine or Ck component the user already knows; only the timeline's track lanes are custom Slate.

| Panel | Built from | Familiar from |
|---|---|---|
| Transport | `SScrubControlPanel` (KismetWidgets) | Anim Sequence, Montage |
| Time ruler, playhead, zoom | `SSimpleTimeSlider` (ToolWidgets) | Sequencer-style editors |
| Track lanes, sections, events | custom Slate, drawn with `FAppStyle` brushes and `CkStyle` tokens | Persona's track panel |
| Curves | `FCurveEditor` + `SCurveEditorPanel` | Sequencer, Control Rig |
| Viewport, gizmo | `SEditorViewport`, `FAdvancedPreviewScene`, the engine widget | every asset editor |
| Details | `IDetailsView` on a selection proxy | everywhere |
| Channels tree | `STreeView` with the engine's row style | Skeleton tree, Control Rig hierarchy |
| Icons, toggles, tones | `FCkIconStyle` / `ECk_Icon`, `CkStyle::`, `CkDebugger.ToggleChip` (CkEditorTools, CkGameplayDebugger) | the Ck debuggers |

No new style set, no new icon brushes unless `ECk_Icon` lacks one, and then it is added to the manifest, not
registered ad hoc.

## 4.2 What Epic would cut: assets and hoops

Today a creature needs a preview config asset per clip, hand-filled with limb bindings, segment frames, a floor,
locomotion and slot bindings, before the editor shows anything. None of that is authoring; it is setup the editor can
derive. So:

- A clip's editor-only subject is the pair `(rig, gait)` for a walking creature, or a layout for a dedicated one,
  held on the clip (`_EditorSubject`, §5.3). Limb bindings, segment frames, the floor, the presentation and the
  locomotion defaults are derived from the rig's rest pose and clearance at open time. The editor shows the creature
  with no further setup.
- `UCk_ProceduralActionPreview_Config` stays only for scenarios that are genuinely authored (obstacles, floor
  removal, custom locomotion). It is optional, picked from the toolbar, and never required. The Mars seed command
  stops writing preview assets; the four it wrote are deleted with it.
- Creating a clip asks one question: the subject. The starting shape is a toolbar action afterwards (Body lean,
  Reach, Sweep), not a creation step.
- A game needs exactly one thing per attack: a clip reference. The crawler already has that.

## 4.3 Gardening the runtime surface

An audit of the action API (83 UFUNCTIONs, 108 reflected types; the one game consumer uses 21 functions, every one
through `Create_FromLoadedClip` with explicit receipts) found a third of the surface used by nothing but its own
tests, and six mechanisms that each exist twice. The decisions, with the consumer impact:

| Decision | Survives | Goes | Impact outside tests |
|---|---|---|---|
| G1 One way to play a clip | `Create_FromClip(Scope, Player, PlaySpec)`; `PlaySpec` holds a soft clip (resolved synchronously when already loaded), bindings and a `Delivery` enum: `Explicit` (the caller reads and consumes receipts) or `Auto` (the player acknowledges, releases and fires `OnFinished`) | `Create_FromLoadedClip`, `Play`, `Play_Loaded`, `LoadedPlaySpec`, the managed listeners (`OnManagedRecords`, `OnManagedAcceptedPose`), `ManagedPage`, `ManagedRecordsPerTick`, `Get_IsManagedDelivery` | Mars changes one call; the examples swap `Play_Loaded` for `Create_FromClip` with `Auto` |
| G2 One timing control per section | the seven easing presets | custom progress curves (`ProgressMode`, `ProgressKey`, `ProgressCurve`, `Set_PhaseProgress`, the monotone cubic) and the editor's progress plot | none; every asset stores the default mode |
| G3 One mirror | the builder's `Mirror_Slot` (the editor's command) | play-time `_Mirrors` and `ActionPlayer_Mirror.cpp` | none |
| G4 One tuning | the player's tuning; a play may not override it | `TuningMode`, the play spec's tuning, `Request_UpdateTuning` | Mars drops one redundant line |
| G5 One recovery | `Request_RecoverUnplantedLimbs` | `Request_RecoverLimbs` | the editor's scenario switches |
| G6 One player spec | `FCk_ProceduralActionPlayer_Spec` built directly; `Add` rejects a clip that borrows a walking limb on a player whose support mode is Disabled | the presets and `BorrowingSpec` | Mars builds the spec it already builds, without the repack |
| G7 One motion budget | `Sampling` | `MotionBudget` (the same two numbers) | Mars stops copying one into the other |
| G8 Two capacity knobs | `MaxConcurrentInstances`, `RecordsPerInstance`; the rest derived | the other eight fields | none set by anyone |
| G9 Track options nobody uses | reach policy, bend keys | anchor bindings and `AnchorAtStart`, `ObserveCrossing`/clearance response, `RequiresTerminalOrientation`, `AuthoredRest` with explicit rest target | editor fields and one Mars test |
| G10 Actions release themselves | an action whose terminal record has been acknowledged (explicitly consumed, or `Auto`) releases; `Request_Release` stays for early teardown | the consumer's release step | Mars drops its release call and four bookkeeping fields; the attack's owner becomes the crawler, not a scope entity per attack |
| G11 Manual pose streaming (flagged for the maintainer) | nothing | `Create_Manual`, `ControlSpec`, `PoseBatch`, `LimbTarget`, `SubmitPoseBatch`, `End`, the editor's manual preview, the Noncombat examples, the Mars long-limb demo (912 lines, attached only by tests) | the only streaming path goes; it is in git if a game ever needs per-frame targets |
| G12 Legacy body-pose action offset | the player's body channel | `Request_SetActionOffset`, `Get_ActionOffset`, `Get_ActionSample` on BodyPose, after confirming the only writer is the walker-without-a-player path | none |

After G1 to G12 the surface is roughly: Action 1 create, 3 requests, 3 reads, 2 signal pairs, 2 diagnostics; Player
`Add`, `Read_Receipts`, `Get_RetainedReceiptIdentities`, `Request_ConsumeReceipts`; Articulation 10 reads and 1
recovery; Builder 15. Standing up one attack: compose the player once, one `Create_FromClip`, read and consume each
frame, cancel when needed, one asset per clip, no callback, no scope entity.

Kept on evidence: explicit receipts (the crawler must read at its own processor slot after the probes; the gameplay
debugger reads them too), `Get_RetainedReceiptIdentities` (debugger), `Get_ResultText`, `Validate_Data`, the builder.

## 5. Data model changes (runtime module, small, migrated)

5.1 Track space moves to the slot. `FCk_ProceduralAction_Slot` gains `_Space` and `_AnchorId`; the per-phase
`FCk_ProceduralAction_LimbTrack` loses them. The compiler already requires one space per slot; this removes the
loop that rewrites every phase, the `SpaceChangesAcrossPhases` diagnostic, and a class of invalid assets. Migration:
compatibility version bump; `PostLoad` copies the first track's space onto the slot. The content hash folds the slot
space once instead of per phase, so compiled hashes change for every clip with slots; receipts stamp the hash per
play, so nothing persisted breaks. The builder's `Set_SlotFrame` keeps its signature.

5.2 Pose groups stay editor-only metadata on the clip, but every key-time edit goes through commands that update
the group (D4), and the join tolerates 1e-6 like the gizmo does. No pose asset.

5.3 The clip remembers its subject. `UCk_ProceduralAction_Data` gains an editor-only `FCk_ProceduralAction_EditorSubject`:
a soft rig and a soft gait (walking), or an articulation layout (dedicated), plus an optional soft scenario preview
config. The user-settings pairing map and the sibling `_Preview_DA` lookup are removed. Existing clips: the toolkit
fills the subject from the old pairing or sibling preview on first open (rig and gait copied out of it) and marks the
asset dirty.

Nothing else in the runtime data or the player changes.

## 6. Module plan

Keep as is: `AuthoringSession` (compile, evaluate, staging), `PoseGroups`, `SolvedCapture`, `SolverWorld`,
`Preview_Config`, `PreviewSpace` (absorbs `ObservedReach`), `Factory`, `AssetDefinition`, the engine-independent half
of `GuidedCreation`.

Rewrite: `AuthoringCommands` collapsed onto the builder path (D4); the toolkit as a thin host (layout, tabs, undo
client, save) of about 400 lines; the viewport (engine scene, hit proxies, tool host); the timeline; the details
proxy; the curve adapter as multi-channel curve models through commands; the creation dialog (subject + starting
shape only).

New: `EditorModel` (D3), `SCkProceduralAction_Timeline` (the one timeline widget), `SCkProceduralAction_Transport`
(wraps `SScrubControlPanel`), `UCk_ProceduralActionSelectionProxy` (details), `UCk_ProceduralPose_Tool`,
`UCk_ProceduralPose_EdMode`, `ACk_ProceduralAction_PreviewActor`.

Delete: the `RecordedStore` shim, the three test files inside the module (moved to CkTestsEditor), the
`GIsAutomationTesting` log hooks, the timeline's "key 0/1" tools, `AuthoredPlot` and `ProgressPlot` (the curve editor
shows both), the user-settings pairing, the sibling preview lookup.

Expected size: the module goes from about 11,800 lines to roughly 7,000 (inferred from the inventory; the gate is
the measured count at the end of Phase 2, reported, not promised).

## 7. Audit findings this design answers

- Four selection copies, three transports, four refresh routines → D3.
- Three write paths (builder drafts, hand-rolled commands, curve adapter direct writes) → D4.
- Raw versus eased key time in capture versus gizmo (confirmed bug) → D5.
- Track space per phase but constant per slot → 5.1.
- Pose groups joined on exact float time → 5.2.
- Duplicate pose at every phase boundary → D6.
- Rotation only editable in raw Details → the gizmo's rotate mode writes orientation keys.
- Tests that drive button text → D10.

## 8. Phases (two executor passes, one gate each; the architect reviews each)

The maintainer's time budget: hours, not days. The editor works today, so each pass is gated once at its end, not
after every edit. The level posing mode (D7, D8) is deferred to a later pass; the asset editor ships first.

Pass A, foundations: 5.1 (slot-level space), 5.3 (subject on the clip, derived preview), the editor model (D3), the
single write path (D4), the one time conversion (D5, with a regression test for the key-time bug), carried-start
sections (D6), tests moved out of the module, dead code deleted, the Mars seed script and content updated. The old
widgets keep compiling against the model. One gate: `ProceduralAnimation` plus `CrawlerMelee`.

Pass G, gardening (§4.3): runs after A and before B so the surface is final when the widgets are written. G11 and
G12 are confirmed with the maintainer before deletion; everything else is decided. Examples, Blueprint parity rows,
AngelScript rows and docs follow the surface. One gate: `ProceduralAnimation` plus `CrawlerMelee`.

Pass B, surface: viewport on the engine scene and widget (the tools-framework host is deferred with the level mode),
timeline, transport, details proxy, curves tab, thin toolkit, creation dialog; the mounted tests rewritten against
tags and the model (their assertions kept as a checklist). One gate: `ProceduralAnimation.Action.Editor`, then the
`[EDITOR-VERIFY]` click-through from §3.1 by a person.

Later: level posing (preview actor, tool, mode); AUTHORING.md and the field guide rewritten around the new surface.

Each phase ends with the comment audit from `Plugins/CkFoundation/CLAUDE.md` and no campaign breadcrumbs in shipped
code. Slate work follows `Plugins/CkGameplayDebugger/.claude/skills/ck-slate-tools/SKILL.md`.

## 9. Risks, in order

1. Hosting the tools framework in an asset editor viewport (D7). Proven or rejected in Phase 2's first gate.
2. The content hash change in 5.1 invalidating anything that persists a hash. Receipts and tests stamp per play;
   grep for stored hashes is part of Phase 0.
3. The mounted editor tests (about 3,000 lines across four files) are rewritten, not ported. Their assertions are
   kept as a checklist so no behaviour is silently dropped.
4. `SSimpleTimeSlider` is in a Developer module (`ToolWidgets`); it is editor-safe but confirm it links in an
   `UncookedOnly` module before Phase 2 designs around it.
