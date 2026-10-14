# Author, tune and observe

`CkProceduralAnimationEditor` now provides clip and preview-config asset creation, a paired asset toolkit, a transactional authoring session, and Authored, Solver and Recorded views. This page distinguishes the controls present in source from the remaining release requirements. The mounted-editor workflow and full verification protocol are still being tested; source presence alone is not acceptance evidence.

## Current editor quick start

**Pose-first workspace (default).** Creating a **Procedural Action Clip** from the Content Browser first asks for the subject (reuse a preview config, or create a Body Only, Dedicated Limbs or Walker config next to the clip) and the starting motion (Blank, Body Lean, Reach, Sweep); Cancel creates nothing, and nothing is saved until you save. The editor opens on the workspace: **Body and limbs** (tick the channels a whole pose covers), the viewport, **Selected pose** (staged values with -1/+1 nudges), the pose timeline (one chip per named pose, per phase), **Capture pose**, **Update pose**, **Duplicate pose**, **Move pose here**, **Delete pose**, **Capture solved pose** (pause the solved preview first), **Mirror to selected limb**, the phase timing panel (duration and preset easing) and the issue list. **Play solved preview** runs the production Solver; **Requested preview** plays the authored targets instead. A clip that does not compile stays saveable; Play is disabled and its tooltip names the first field to fix. **Save clip + preview** saves both assets and reports each result. The same construction is available in code through the pose builder (`UCk_Utils_ProceduralActionBuilder_UE`, AngelScript `utils_procedural_action_builder`; see [API.md](API.md#9-pose-builder)).

**Starting from wherever the limb is.** Delete the pose at the start of a phase when a move should leave from the limb's or body's current position rather than from a fixed key (a claw lifting off its planted foot). A track with no key at 0 moves from where the previous phase left it, or, in the first phase that moves it, from the subject's pose when the action starts. Every track still needs its key at 1, and bend tracks keep their key at 0.

**Which preview opens.** A clip opens on the preview config you last picked for it. With none picked on this machine, it opens on the preview saved beside it, `<Clip>_Preview_DA` for `<Clip>_DA`, so a clip and its preview can be shared together.

**Advanced (collapsed by default).** The steps below build a clip one phase, slot, key and event at a time with the raw Details, the per-track tools, the X/Y/Z key curves and the Recorded receipts; they remain the way to make sparse, per-channel edits.

1. In the Content Browser's Animation category, create a **Procedural Action Clip** and open it. The clip owns phases, body and limb tracks, and events. Runtime action tuning comes from the player's spec, not from this clip asset.
2. Click **Create Preview Config** to create a sibling `_Preview_DA`, or choose an existing config in the picker. The config owns the disposable preview subject, topology, slot-to-limb bindings, surface, appearance and **Playback | Action Tuning** for Solver runs. Choose Standalone or Walking as appropriate. A body-only clip can use a config with no limb bindings. For an obstructed standalone preview, set the limb topology's Swivel clearance, the standalone Jolt clearance filter, and a bounded static obstacle; keep the walking support floor separate. Save the clip and preview config separately; their selection association is per-user editor state, not a runtime clip reference.
3. In **Authored**, add a named phase and duration, then add body keys or a named limb slot, select its phase and slot rows, and add endpoint keys. The Clip Details panel and X/Y/Z curve panel edit key values. Scrub within the selected phase before using **Move endpoint key** or **Move bend guide** in the viewport. Use **Bind selected slot** to map the slot to a physical limb ID in the preview config.
4. Read the status and field diagnostics. Authored shows requested geometry only. **Solver (restart)** creates a new private production-world preview and advances it on editor frames; **Recorded** browses copied receipts with sequence/gap labels. **Play/Pause**, **Reset**, **Frame**, the time slider and previous/next receipt controls are in the right panel. Restart Solver after changing the clip or preview subject.
5. Save both assets through the normal editor save action. An invalid draft may be saved for later correction, but its Play control is disabled and it is not marked validated. There is currently no separate **Validate** button: the authoring session compiles and reports diagnostics as edits are refreshed.

## 1. Editor boundary

The implemented asset toolkit pairs runtime `UCk_ProceduralAction_Data` with a reusable authoring session model. `CkProceduralAnimationEditor` is registered in `CkFoundation.uplugin`; runtime does not include editor headers or depend on CkGameplayDebugger.

Reuse shared style/icon tokens through CkEditorTools/current editor-style conventions and existing editor toolbar integration. Reuse Unreal Details and curve editing where verified against the resolved engine source. Do not add a graph editor for a linear, phased timeline. The PathNetwork designer's separated Preview/Apply session is the local transactional precedent.

The live debugger remains a DeveloperTool reading copied accepted runtime snapshots. The editor may share neutral value-model/geometry helpers, but must not import the debugger plugin to make authoring work.

## 2. Target layout reference

```text
[Clip asset] [Rig preview] [Validate] [Save]     [Authored | Solver | Recorded]
+---------------------+--------------------------------+-----------------------+
| Subject / limb tree |                                | Selected track/key    |
| Body                |          3D viewport           | Space / slot mapping  |
| LeftFore            | requested path: dashed         | Position / rotation   |
| RightFore           | accepted path: solid           | Reach / clearance     |
| Support limbs       | ghost poses / joint pivots     | Units / easing        |
| Slot bindings       | reach / contacts / reservations| Validation messages   |
+---------------------+--------------------------------+-----------------------+
| [Play] [Pause] [Frame] [Reset] [Time] [Rate] [Loop preview]                  |
| Phase lanes   | Prepare          | Act         | Settle                   |
| Body track    | o-----------o----|---o---------|------------o             |
| Limb tracks   | o---------o------|------o------|------------o             |
| Marker/window |        *        | [--------)  |                           |
+---------------------------------------------------------------------------+
| Source revision | Preview revision | Constraint errors | Timings/budgets  |
+---------------------------------------------------------------------------+
```

This is a target workflow wireframe, not the current toolkit or a screenshot. The current toolkit uses clip and preview Details on the left, with mode controls, status, viewport, timeline, curve editor and receipt controls on the right. Source now draws a dashed requested endpoint chord, solid settled endpoint chord, reach sphere, support overlays and preview floor/obstacle boxes; their mounted appearance and interaction still require the editor protocol. The wireframe's Rate and Loop controls are not implemented. Display actual human-friendly names in the UI; implementation fragments and private token fields belong in diagnostics.

## 3. Three modes with truthful evidence

### Authored

Pure clip evaluation without world simulation is implemented. Scrub to inspect the requested pose, tracks and binding. Requested geometry only; no 'grounded', 'collision safe' or 'accepted' badge. The status states that solved, contact and accepted evidence is unavailable. Preview mesh and its appearance-only scale are separate from motion data.

### Solver

The implemented Solver host uses a disposable private production world, not the selected live game entity. It runs the ECS/Jolt composition, control and publication path through normal world ticks. The preview config exposes a separate bounded list of static Jolt obstacle boxes, distinct from the walking support floor, and standalone articulation clearance settings. A preview with Swivel clearance needs a Jolt filter; obstacle placement alone does not enable clearance. Preview readiness and failures appear in mode status. Contacts and blocked returns must come from the production query path; the authored viewport alone is not solver evidence. The native `SolverWorld.ObstacleQuery` and `SolverWorld.ReachProjection` tests passed in R43; the complete mounted-editor/walking-surface protocol remains pending.

R44 source separates live settled subject geometry from receipt geometry: Solver observes the latest healthy settled pose through return/recovery, while Recorded retains the selected historical receipt. Solver does not fabricate a receipt interval or a continuous path for ordinary gait return. Detached or missing bound parts invalidate stale live geometry. The live floor overlay follows production Jolt readiness; Recorded hides environment geometry because the receipt does not record an environment timeline. These R44 changes still need their post-edit gate and visible walkthrough.

Scrubbing backwards recreates the fixture from its canonical seed and resimulates to the requested time with fixed input/step schedule. Stateful gait cannot be reconstructed by setting a clock backwards. Seek is bounded/cancellable; never block UI indefinitely. A fresh revision or changed surface invalidates the simulation cache. No claim of bitwise determinism across hosts.

The R44 preview-only **Scenario** fields add a bounded environment schedule, separate from clip motion/events and action tuning. Set **Observation Horizon Step** to a positive count (maximum 3,600) to keep observing after action terminal; zero retains the ordinary stop-at-terminal workflow. **Scenario Events** permits at most 64 events: Remove Floor, Restore Floor, Recover Limbs, and Detach Limb. Steps are 60 Hz simulation counts after action readiness, not wall-clock seconds; subject/floor preparation consumes none. Events use unique ascending positive steps strictly before the horizon. Floor events only affect the preview's own configured floor. Limb events name walking-rig IDs; detach accepts one ID. An invalid schedule rejects before creating a private world. Backward seek replays the schedule captured at Start.

For a recovery study, use a walking config with coherent sloped floor contacts, end the clip while its limb is displaced, remove the floor during return, allow the bounded return to fail, restore the floor, and explicitly recover the unplanted limb. Leave enough observation steps for actual trusted touchdown after the recovery request. An admitted request alone is not evidence of recovery. The action's terminal outcome remains visible independently of the preview's observation horizon. This new lane is source-ready work, not yet a certified R44 runtime or visual result.

The live toolkit ticks its private world at most once per actual editor frame, on the game thread, with a fixed simulation delta. During a Solver time-slider drag, the thumb shows the requested time while the production world stays paused; releasing the pointer requests one seek to the final target. Keyboard slider changes and explicit Frame/Reset requests are serviced on the next editor tick. Seek replay then progresses over editor frames and exposes progress/cancel. Do not reuse `FTestWorldWrapper` in the toolkit: resolved engine `AutomationCommon.cpp:220-233` increments global `GFrameCounter` per test tick and `:199-215` restores it on teardown; `TickTaskManager.cpp:2617-2626` suppresses duplicate actor ticks in a frame. The test helper proves world feasibility only. Live preview must not mutate the process-global frame counter. Faster simulation seeking requires a separately proven process/snapshot architecture; pure Authored scrubbing remains immediate.

### Recorded

The implemented Recorded view holds copied receipts, with previous/next navigation and record detail text. Missing sequences remain explicit gaps. Selecting a record changes the observed viewport pose while its source clip and preview-config revision still match. Source draws requested dashed and settled solid endpoint chords for continuous observed intervals; these lines are endpoint motion, not collision sweeps. After either source changes, the old receipt data and provenance remain readable, but pose geometry and config-derived overlays are hidden until a new Solver recording is made; the editor will not interpret old receipts with the new subject config. Editing requires returning to Authored and cannot mutate the recording or live subject. Mounted verification of those overlays and the remaining diagnostics is still required. The mode shares the debugger's evidence semantics, not its live mutation authority.

## 4. Authoring operations

These are the one-by-one operations of the Advanced path (see the note under the quick start). The current timeline exposes named phase and slot creation, phase duration and reorder/remove controls, body and endpoint key insertion, selected slot frame, explicit mirror-to-destination controls, a text field for binding a slot to a physical limb ID, marker/window creation and draggable half-open event edges. Event-edge snapping, curve editing, viewport endpoint/bend controls and undoable commands are implemented in source. The mirrored destination must be declared; it is not inferred from symmetric naming.

The R44 **Rename selected slot** command updates the slot declaration, every phase's matching tracks, and the selected preview config's binding in one undo transaction. Missing, empty, duplicate or ambiguous IDs reject before mutation. Other preview configs and game bindings are independent consumers: update their slot bindings explicitly when changing a published clip's slot identity. This command's new regression awaits the R44 gate.

The following are release refinements or behaviors that still need full mounted-editor verification:

- Verify phase reorder and marker/window identity through save/reopen and real mounted Undo/Redo.
- Verify the source-implemented topology picker and its capability text through mounted selection.
- Verify real hit-proxy endpoint/bend dragging, coordinate conversion and Undo/Redo; show segment lengths, reach sphere and singularity warning.
- Verify explicit mirror and retarget operations in a mounted workflow; preview requested/accepted errors before accepting a binding.
- Verify marker/window dragging, snapping and half-open boundary preview through routed Slate input.
- Compare variants by duplicating clip assets, not editing the same active runtime object.
- Topology changes require rebuilding the preview; active instances retain old compiled revisions.

Named identity is not the array index. Commands must preserve IDs when reordering rows and must diagnose references affected by a removal. Invalid drafts remain saveable for Details correction; Play stays disabled until the runtime compiler accepts the clip. Undo must restore IDs as well as values.

## 5. Tuning and asset workflow

The toolkit presents clip motion/events and the separate preview assembly in distinct Details panels. The clip owns motion and event content; the preview config owns topology, bindings, surface, preview meshes, Walking composition settings and Solver-only Action Tuning. Solver validates that tuning against the production player rules and pins it into its private player's spec. A live game's action tuning is supplied independently by its player's spec; changing the preview config never edits the clip or a live subject. Appearance-only mesh scale must not change solver segment lengths or endpoints.

Changes are local transactions on the edited asset. The authoring session runs the runtime compiler on refresh and displays field-addressed diagnostics in status. Save refreshes validation but does not refuse serialization of an invalid draft. Invalid drafts remain editable; Play is disabled and no valid-content badge is shown. Runtime playback pins its prior compiled revision; editor edits never mutate it in place.

'Restart preview with changes' is explicit. 'Apply to selected live subject' is absent from release 1; the debugger remains an evidence tool plus its existing explicit enable/detach requests. A later live-tuning integration must use request transport and authority checks, not per-frame reads of mutable DataAssets. Existing LiveTune documentation is a proposal, not a dependency we can assume.

Loaded preview meshes/materials are rooted by the editor session. Closing the tab cancels loading/simulation and releases the world, delegates and asset roots. No static raw widget/world pointer, per-frame world rebuilding, or background work calling Jolt outside its allowed thread.

## 6. Diagnostics that change authoring decisions

Required diagnostic presentation at release (the current status and Recorded detail cover only part of this list):

- requested versus accepted body/endpoint, with reach projection delta;
- ownership and control state for every limb;
- eligible trusted support count and projected support region where configured;
- current gait reservations and proposed return destination;
- geometric crossing versus query failure as separate outcomes;
- root frame and slot frames, detached/missing bindings;
- sample/receipt sequence, content revision, pending publication and backlog;
- action state/reason plus remaining acquisition/return timeout.

Red means invalid/rejected; amber means projected/constrained/degraded; normal accepted pose is not automatically 'safe for damage'. Color must have icons/text equivalents.

Practical presets exist as builder recipes (`Make_FromRecipe`: Blank, Body Lean, Reach, Sweep) that produce ordinary clip data, not special runtime action subclasses; choosing them in the editor is part of the planned workspace. The feature must remain usable by a robot arm, a machine and a creature.

## 7. Verification

Production simulation preview is a release requirement because requested motion alone hides the main failure modes. The toolkit, private world and focused tests exist; full real editor acceptance is still in progress. It includes undo/redo, keyboard focus, viewport picking, scrub after edit, dirty-state prompts, two asset tabs, PIE coexistence, closing while loading and world teardown. The protocol in `VALIDATION.md` remains the acceptance gate.

A recording used to assess motion must identify fixture, rig/clip revisions, frame rate and actual input. Screenshots prove appearance only; a continuous run without per-frame capture establishes motion. The user judges the intended feel; automation proves the state and geometry contracts.

Engine-specific toolkit, curve and preview calls have been compiled against the toolbox-resolved engine. A compiled API and focused fixture do not by themselves verify every routed Slate interaction or continuous visible motion.

