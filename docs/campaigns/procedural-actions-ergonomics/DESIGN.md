# Procedural actions follow-up: poses, curves and managed playback

Design of record, revised 2026-10-12 after the audit in [AUDIT.md](AUDIT.md). The 2026-10-12 draft's
claims were checked against the code; corrections are listed there and applied here. §11 holds the locked
signatures every phase implements. No implementation or runtime verification is claimed by this document.

## 1. Acceptance contract

The author chooses a subject, specifies key poses, adjusts timing/curves, previews the solved motion and saves a reusable clip. The programmer plays that clip without implementing another animation clock or receipt cleanup loop. Expert consumers retain explicit accepted-motion delivery and acknowledgement.

The user's central requirement is: "specify key poses and then with the curves have everything blend together nicely." This package addresses the gap between the current execution machinery and that experience. [VALIDATION.md](VALIDATION.md) is the executable acceptance rubric; tests are authored by the executor, never claimed to exist by this document.

Required demonstrators: a body pullback/lunge/recovery; a dedicated appendage sweep; a walking-limb sweep with successful return and a diagnosed failed return; a noncombat reach. All use the existing production runtime. No demo may move mesh transforms directly or substitute browser interpolation for Solver output.

## 2. Verified starting point

See [AUDIT.md](AUDIT.md) § Confirmed for the evidence table. Summary of what the code is today (paths relative to `Plugins/CkFoundation`):

- `Action/CkProceduralAction_Data.h`: `UCk_ProceduralAction_Data` holds `_CompatibilityVersion` (1), `_BodyChannel`, `_Slots`, `_Phases`, `_Markers`, `_Windows`. A phase has `_Id`, `_Duration`, `_Easing` (Linear/Quad/Cubic presets), `_BodyInterpolation`, `_BodyKeys`, `_LimbTracks`. A limb track has `_Slot`, `_Space`, `_AnchorId`, `_Interpolation`, policies and position/orientation/bend keys with normalized `_Time`.
- `Core/CkProceduralActionValidation.cpp`: `Compile(const UCk_ProceduralAction_Data&, FCompiledClip&, TArray<FDiagnostic>&)` is the one compiler. It rejects on any diagnostic, requires keys at 0 and 1 on every non-empty key array, a constant space per slot across phases, resolves markers/windows on elapsed phase time, inverts the phase easing to place key critical times, and hashes the content.
- `Core/CkProceduralActionEvaluation.cpp`: phase progress is `Apply(Easing, elapsed/duration)`; every track samples at that progress; Smooth is a per-axis bounded cubic; rotation is normalized slerp.
- `Action/CkProceduralAction_Utils.cpp`: `Create_Manual`, `Create_FromClip`, `Create_FromLoadedClip` share `DoCreate`, which reserves the identity before creating the entity. The loaded path compiles and binds synchronously and, on rejection, publishes `Finish` and commits `Finished` inside the call.
- `ActionPlayer/*`: the player fragment owns `FReceiptStore _Receipts`, `_Actions`, `_PendingRecoveries`. `Observe` (FGroup_Gameplay_Camera) publishes accepted poses; `Service`/`HandleRequests` (FGroup_Transform_Derived) and `ReconcileOwnerLoss` (FGroup_Physics) write terminals; `Read_Receipts`/`Request_ConsumeReceipts` are the explicit consumer API.
- Editor: `FCk_ProceduralActionAuthoringCommands` (transactional per command, incl. `MirrorSlot` for position/bend tracks), `FCk_ProceduralActionAuthoringSession`, `FCk_ProceduralActionSolverWorld` (production preview), `FCkProceduralActionEditorModes` (Authored/Solver/Recorded), the toolkit, timeline and viewport widgets.
- Mars: the crawler body attack is `Create_Manual` + a game phase clock (`Mars_Crawler_Processor_AttackTick.as`) submitting body offsets; `Mars_Crawler_Processor_AttackResolve.as` drains receipts explicitly.

Historical gate: Mars `Saved/Logs/ProceduralActions-Commit-R48.log` reported 132 selected, 131 passed, 1 failed, zero contaminated; the failure `Ck.ProceduralAnimation.Action.Editor.MountedToolkit.TwoTabsModesAndCapture` is on no known-red list and is inherited. That run is the baseline of record for the `ProceduralAnimation.Action` pattern; each phase gate diffs failing names against it.

## 3. Locked architecture

1. Keep `UCk_ProceduralAction_Data`, its compiler, evaluator, claim system, publisher and bounded receipt store. Add authoring/convenience layers; do not add a second player.
2. Whole-pose authoring expands into the existing body/limb tracks. There is one persistent motion representation and one compiler: `Compile` takes a non-owning source view (`FClipSource`) that both the asset and the runtime draft produce. Pose grouping metadata is editor-only.
3. Managed delivery and explicit receipt consumption are mutually exclusive per action identity, selected at creation and immutable.
4. Subject assembly is reusable across clips. Visuals, anatomy, clip motion, runtime tuning and test scenarios retain separate ownership.
5. The game remains responsible for intent, targeting, hit tests, damage, interruption decisions and cooldown. Root locomotion remains with its existing owner.
6. No new runtime module, editor dependency in runtime, game-specific runtime subclass, background Jolt work, global registry or direct fragment writes outside declared friends.
7. Existing public functions, asset behavior and explicit consumers retain their contracts. New APIs are additive. Published identifiers are not silently renamed.

Rejected alternatives: a Slate-only builder excludes AS/native users; a second pose asset/player duplicates authority; replacing receipts with signals loses retained evidence; automatic name-based retargeting can move the wrong appendage; defaulting every borrower to CountOnly hides a consequential policy; unrestricted arbitrary progress curves can reverse time/window ordering; direct pose tweening bypasses coherent settlement; a transient `UCk_ProceduralAction_Data` per `Build_Into` (GC churn, second code path) loses to the compiler view; managed records as CK signals (payload retained on the entity and replayed to late binders) lose to per-identity delegate lists.

## 4. Designed authoring journey

### Subject setup, once

Choose or create a reusable `UCk_ProceduralActionPreview_Config`. Guided setup offers Body Only, Dedicated Articulation and Existing Walker (`_SubjectMode` Standalone/Walking plus the body-only case of an empty layout). Show visual limb selection, stable IDs, lengths/rest geometry, solver capabilities and binding diagnostics. Walking setup reuses existing walker composition and explicit support policy. No second topology store.

The existing preview config remains an editor fixture, **not** a runtime spawn asset. Reuse a value mapping/export helper for layout, bindings and player configuration; document what game assembly must supply. Do not advertise automatic import of arbitrary actors or meshes as implemented. No guessed anatomical names or segment lengths. A subject setup validation summary must distinguish complete assembly from merely selected meshes.

### New motion, immediately editable

Choose Blank, Body Lean, Reach or Sweep, then participants. Recipes produce ordinary clip data with finite durations, explicit slots and neutral/rest-based starting poses. They contain no damage semantics. Prepare/Execute/Recover are editable named phases; Blank starts with one Motion phase. Recipe parameters expose amplitude, direction, duration and curve preset; they never silently enable borrowing.

Preview configuration is chosen/reused during creation; asset path/name selection precedes writes. Cancel creates neither asset. Successful paired creation is one undoable operation where supported, with explicit cleanup if saving either asset fails. Existing example assets remain compatible.

### Pose, key, curve, play

Default workspace:

```text
[Subject] [Motion recipe] [Save]                     [Play solved preview]
+-----------------------+-----------------------------+--------------------+
| Body and limbs        |                             | Selected pose      |
| participant selection |        3D viewport          | body / endpoints   |
|                       | endpoint + bend handles     | time / transition  |
+-----------------------+-----------------------------+--------------------+
| Timeline: named pose keys, phase durations, markers/windows               |
| [Capture pose] [Duplicate] [Delete] [Mirror to selected limb]              |
+-------------------------------------------------------------------------+
| Actionable issue summary       [Requested motion] [Advanced diagnostics]  |
```

Pose editing operates on session-owned requested targets. It never writes the live/private Solver entity directly. Drag body offset or selected endpoint/bend handles, then Capture Pose. Editing an existing pose changes that key; no hidden capture of interpolated samples. Capturing at an occupied time requires explicit Update Pose or Duplicate at a different time.

A whole-pose capture covers the declared participating channels atomically. Unselected limbs remain under ordinary control, not implicit zero targets. Required participants missing from a captured pose reject the operation and identify the missing channel. A dedicated **Key selected channels** advanced operation permits sparse tracks without pretending it is a whole pose.

Stable editor pose IDs label grouped keys. Metadata references phase/time/channels, stores no second set of transforms and is stripped from runtime builds (`WITH_EDITORONLY_DATA`). Moving, duplicating or deleting a pose updates its referenced keys and metadata in one transaction. Advanced track edits refresh grouping; broken groups show a diagnostic instead of capturing unrelated keys. Old clips derive temporary groups from matching key times and are unchanged until the author explicitly edits them.

Capture defaults to requested targets. **Capture solved pose** is a separate command requiring a healthy, settled, matching-revision Solver snapshot; it converts to declared track spaces, validates the complete conversion and records capture provenance in editor metadata. It cannot copy world-space values into rest-normalized keys or copy a stale Recorded snapshot as a current pose.

The default Play executes the production Solver preview. Requested preview remains available for fast authoring; recordings, support geometry, sequencing and budget counters live in Advanced diagnostics. Inspector shows selected content; raw full asset Details becomes Advanced. Bindings, spaces and solver choice appear when selection/setup requires them, not as mandatory manual steps for every key.

Invalid drafts remain saveable. Play is disabled with a field-linked explanation. Playback restarts explicitly after changes; old recordings remain readable but cannot supply current geometry. Save clip and preview configuration through their separate asset ownership, with a combined convenience action that reports each result.

## 5. Shared construction and pose operations

Add runtime `ActionBuilder/CkProceduralActionBuilder_Data.h` and `ActionBuilder/CkProceduralActionBuilder_Utils.h/.cpp` under `Source/CkProceduralAnimation/Public/CkProceduralAnimation/`. Editor commands wrap the same pure value operations in Unreal transactions. The exemplar is the existing Action data/Utils plus transactional `FCk_ProceduralActionAuthoringCommands`, not a fluent UObject with private lifetime rules.

Reflected value contracts (exact layouts in §11.1):

| Type | Required values | Optional grouped values |
|---|---|---|
| `FCk_ProceduralAction_ClipDraft` | none | The asset's five authored fields, value-only (`_BodyChannel`, `_Slots`, `_Phases`, `_Markers`, `_Windows`). |
| `FCk_ProceduralAction_LimbPoseTarget` | Slot, Position | Orientation mode + value; bend mode + value. |
| `FCk_ProceduralAction_PoseKey` | Time, participating limb targets | Body mode + value. No live entity handles. |
| `FCk_ProceduralAction_RecipeSpec` | Recipe enum, participant slot IDs | Motion parameters and timing groups; no mesh, damage or support ownership. |
| `FCk_ProceduralAction_MirrorSpec` | Source slot, destination slot, plane | none |
| `FCk_ProceduralAction_ProgressCurve` | Mode | Normalized progress keys (CustomCurve only). |

Reports are the existing `FCk_ProceduralAction_ValidationReport` with `Advisory = false`; no new report type.

`UCk_Utils_ProceduralActionBuilder_UE` exposes uniquely named reflected functions: `Set_BodyChannel`, `Set_SlotFrame`, `Set_PhaseProgress`, `Add_Phase`, `Add_Slot`, `Add_BodyKey`, `Add_LimbKey`, `Add_PoseKey`, `Replace_PoseKey`, `Add_Window`, `Add_Marker`, `Mirror_Slot`, `Make_FromRecipe`, `Make_Draft_FromData`, `Build_Into`. All mutations take the draft by reference (`UPARAM(ref)`) and return a report. Essential arguments precede optional Specs. Times are `FCk_FloatRange_0to1` within a phase and `FCk_Time` for durations; no raw seconds.

`Build_Into(const Draft&, UCk_ProceduralAction_Data* InDestination)` compiles the draft through the shared compiler view before atomically replacing destination content. Failure leaves destination byte-equivalent in authored fields. Do not silently sort duplicates, invent references or clamp invalid inputs; inserting a key at its sorted position inside a strictly-increasing key array is the invariant, not a sort. The destination is caller-owned; document GC ownership and root transient objects while constructing/playing them. An Outer alone does not prove GC retention. Compiled playback remains immutable after successful creation. Build is not Play and not an editor transaction. Bound draft/key/recipe counts using the existing compiler ceilings (`content_limits`) before allocating expanded data; count mirror destinations and grouped pose keys before expansion.

Pose insertion validates all targets before mutation. Coordinate spaces are per slot (`Set_SlotFrame`), never per key. Whole-pose operations materialize the needed limb tracks without resetting keys/events on other tracks. `Replace_PoseKey` differs from `Add_PoseKey`. Mirror expansion shares one pure geometry helper with the editor command.

Document C++, transient AS construction and Blueprint functions, and the AS `asset ... of UCk_ProceduralAction_Data` declaration route separately; execute each supported route. The asset route assigns the private `BlueprintReadWrite` fields directly inside the initializer (`Script/ARCHITECTURE.md` §13) and is gated on a compiled fixture, not assumed. Avoid public reflected overloads, reflected TOptional fields and unbounded opaque fluent state.

## 6. Curve semantics

Separate **timing progress** from **spatial path**. Timing governs synchronized progression of a phase's participating pose channels. Spatial keys/tangents define endpoint paths. A sweep may need an intermediate pose; timing ease alone cannot create an arc.

Retain existing `_Easing` and its behavior. Add to `FCk_ProceduralAction_Phase` a `_ProgressMode` (`Preset`, serialized default, or `CustomCurve`) and `_ProgressKeys`. Preset mode uses the existing easing; custom mode requires `_Easing == Linear` and rejects any other value, so no active field is silently ignored. Custom keys: 2 to 64 finite pairs, strictly ascending times, nondecreasing values, exact endpoints (0,0) and (1,1). Plateaus are permitted; reversals and out-of-range values reject. Compile monotone cubic Hermite interpolation with Fritsch-Carlson tangents into the compiled phase; do not accept arbitrary slopes then clamp the result. One pure evaluator (`Core/CkProceduralActionProgress.h`) serves the compiler's inverse, the runtime evaluator and the editor curve rendering. The content hash includes progress keys only in `CustomCurve` mode, so every existing clip keeps its hash.

The phase's elapsed time remains the action clock. Progress remaps channel evaluation once, not twice. Markers/windows remain located on elapsed phase time using current half-open semantics; easing does not retime their identity or duplicate them. Display marker positions on the elapsed-time timeline. Add a test where a steep custom curve crosses several pose keys/windows in one accepted interval.

Rotation uses shortest-path normalized quaternion interpolation. Smooth spatial interpolation retains current bounded semantics for old content. New recipes use Catmull-Rom tangents derived from adjacent keys, tested for endpoint continuity; `Add_PoseKey` writes zero tangents, and explicitly authored discontinuities remain visible. "Smooth" does not guarantee continuous acceleration across phase boundaries, contact safety or elastic whip dynamics. Offer visible holds/sharp transitions without claiming physical elasticity.

## 7. Managed playback

Add `Play` and `Play_Loaded` to `UCk_Utils_ProceduralAction_UE`, taking scope, player, the existing soft/loaded PlaySpec and a managed completion delegate, returning `FCk_ProceduralAction_ManagedStart` (action handle, independent identity, start report). Bindings/tuning stay in the existing PlaySpec groups; a shorthand cannot infer missing limb bindings.

`Play`/`Play_Loaded` register managed delivery inside the shared creation path (`DoCreate` gains a delivery argument; the loaded path's body moves into a shared internal function) before any compile rejection can commit terminal, because the loaded path finishes synchronously (`Utils.cpp:186-199`). No user callback fires before the call returns: a synchronously rejected action has its Terminal record dispatched on the next `ManagedDispatch` pass. Synchronous inability to allocate/validate the outer request returns a failed start report with an invalid action and no callback.

The existing `Create_*` functions retain explicit consumption. Player-owned managed entries (`FFragment_ProceduralActionPlayer_Managed`) retain identity, recipients and a completed flag independently of the scope-owned action entity. The cursor is the store's own acknowledged position (`FReceiptStore::Get_AcknowledgedThrough`); no mirror. Reuse the existing player service and store; no timer per action, no scan of all entities. `FCk_ProceduralAction_Capacity::_ManagedRecordsPerTick` (default 32, validated) bounds records dispatched per player per tick; identities are serviced fairly in ascending order from a rotating start. Receipt transforms remain charged to existing budgets; copied dispatch pages are temporaries.

Managed dispatch reads a copied page from the acknowledged position, delivers it to every recorded listener, then acknowledges exactly that page. With no listeners it still drains and acknowledges. `OnManagedRecords` carries copied records (accepted poses, window closure, terminal); the dispatcher is the single acknowledging reader, so a managed page can never report a gap and the payload carries no missing range (lost records surface as the producer outcome `ReceiptOverflow`). A passive `OnManagedAcceptedPose` subscription projects accepted receipts only; it is ephemeral notification, not retained replay or a damage guarantee. Public `Read_Receipts` stays non-consuming and unsealed for every identity: the gameplay debugger observes streams through it (`CkProceduralAnimationDebugger_Model.cpp` calls `Read_Receipts` and stops recording on any result other than Succeeded/Pending/Gap), and a read never changes ownership.

Delivery ownership is sealed by identity, and the seal is a bit on the receipt store's stream, set at reservation and living as long as the stream (not the presence of a managed entry, which is removed at completion while the stream may be retained for another frame). `Request_ConsumeReceipts` on a sealed identity ensures and completes `Failed_NotEnqueued` without advancing the cursor; `Get_IsManagedDelivery` reads the stream bit and stays true until the stream is reclaimed. `ECk_ProceduralAction_ReadResult::ManagedDelivery` is the result of that rejected consume, never of a read. Multiple managed listeners share copied notifications; one acknowledgement authority. Subscription after playback starts has no historical replay.

Managed completion carries the producer terminal `FCk_ProceduralAction_Result` (Outcome `None` when no terminal was observed, never fabricated as Completed), a distinct `ECk_ProceduralAction_DeliveryStatus` (`Complete`, `Expired`, `PlayerLost`) and the bounded `FCk_ProceduralAction_FailureContext`. Completion fires only after the terminal record and all preceding pages were delivered and acknowledged; the dispatcher copies the terminal result before the acknowledgement that may remove the stream. Then it releases the live action through `Request_Release` when the handle is valid and not pending destroy. Existing `BindTo_OnFinished` remains producer completion and may precede managed completion; it is not emitted twice or retimed.

Reentrant listeners enqueue normal requests; they never mutate the current store iteration. The dispatcher snapshots the identity list before callbacks, copies values before each callback, and revalidates the player handle and fragment presence after each callback before touching storage. A `Play` from inside a callback is eligible on the next pass. Cancel, release, destroy scope/player, unsubscribe and start another action inside a callback are mandatory tests. Recipients are dynamic delegates (weak object references), so a stored delegate never keeps a destroyed owner alive and a dead recipient is skipped by `ExecuteIfBound`. Scope loss suppresses callbacks into dead recipients while player-owned recovery and auto-drain continue. Player destruction fires `PlayerLost` once per uncompleted entry from an `FGroup_EndPlay` pass, requests destruction of each entry's action entity that is still live and not pending destroy (the player is gone, so a later `Request_Release` could never succeed and the entity would otherwise leak under its scope), then clears the entries. If both recipient and player die there is deliberately no callback.

Processor: `FProcessor_ProceduralActionPlayer_ManagedDispatch` in `FGroup_Gameplay_Camera`, `RunAfter<FProcessor_ProceduralActionPlayer_Observe>`, so a frame's accepted poses and the terminals written earlier in that frame are seen together; the `OwnerLost` terminal written by the action's EndPlay pass is seen one frame later. Acknowledgement is a direct, encapsulated store call inside the dispatch pass (the pass is not the publication pass). Measure the resulting one-frame latency against retention budgets.

For critical gameplay delivery use existing explicit receipts. Managed notifications may support noncritical effects; no exactly-once damage promise. Both routes execute the same clip, claim, settlement and return semantics.

## 8. Presets, queries, reflection and diagnostics

Player helpers on `UCk_Utils_ProceduralActionPlayer_UE`: `Make_DedicatedPlayerSpec` and `Make_BorrowingPlayerSpec` return ordinary `FCk_ProceduralActionPlayer_Spec` values. Borrowing takes an explicit `FCk_ProceduralLimbControl_Support` (CountOnly or ProjectedSupportRegion), `FCk_ProceduralLimbControl_Return` and tuning. `Make_CountOnlySupport(MinimumTrustedContacts)` is the clearly named convenience; minimum means trusted planted contacts, not visually standing legs; it ensures on a value below 1. Return duration/deadline remain authored and validated at `Add`. No preset guarantees a reachable plant on unknown terrain. Capacity remains independently grouped.

Subject helpers on `UCk_Utils_ProceduralArticulation_UE`: `Get_IsAnyLimbControlled`, `Get_IsAnyLimbReturning`, `Get_UnplantedLimbIds`, `Request_RecoverUnplantedLimbs`. Controlled means currently action-controlled. Queries copy one coherent subject observation in topology order (`Get_Limbs` order). Recovery gathers the current unplanted set at request drain, not at enqueue; an empty set completes `Succeeded` without claims; incompatible states complete `Failed` and use the existing complete-set `TryRecover` admission. No partial hidden recovery, auto-loop or disabling to bypass support.

Mirror pairs: the draft `Mirror_Slot` (Phase 1) is the one geometry; the editor `MirrorSlot` command delegates to it (Phase 4); a `FCk_ProceduralAction_PlayOptions` mirror-expansion group (Phase 5) reuses it at runtime, expanding into compiled destination tracks before validation/claims while source data stays unchanged. Require source/destination slots and an explicit plane; for the XZ plane the normal is Y. Positions, tangents and bend guides reflect in the declared frame; orientation reflection is `q' = (w, -S v)` for `q = (w, v)` and reflection `S`, a proper normalized rotation, never a negative-scale transform. `LimbRestNormalized` and `AnchorAtStart` tracks reject in the draft (the frame needs subject topology); the runtime expansion may convert them because the subject is known. Never mirror body motion/events implicitly, duplicate windows, overwrite existing destination tracks or assume asymmetric anatomy is equivalent.

Diagnostics: retain machine-readable outcome/recovery/reason fields. Add `FCk_ProceduralAction_FailureContext` (phase, limb IDs, elapsed/limit times, observed/budget counts with a unit name) captured at the failure source on `FFragment_ProceduralAction` and carried on the Terminal record and the managed completion; then `UCk_Utils_ProceduralAction_UE::Get_ResultText`. Unknown reasons render their identifier. Only show measured context that was captured; do not infer "return timeout" from any `RecoveryFailed`. API docs include reason, likely cause and corrective action, and distinguish rejected input, unreachable motion, support loss, delivery gap and failed return. No per-frame FText formatting; format on demand.

## 9. Crawler migration and example quality

Migrate the body attack's authored animation clock/offset evaluation to a compiled clip played through `Play_Loaded`'s explicit sibling (`Create_FromLoadedClip`) with the existing explicit receipt consumer. Generate the clip once per immutable spec revision through the builder; cache the `UCk_ProceduralAction_Data` with correct lifetime (rooted by the owning entity script object, not a fragment). The game starts/cancels playback and owns cooldown/target/hit state. Phase/window transitions derive from the accepted action stream's `PhaseId` and windows, not a parallel elapsed counter. Keep combat cancellation and damage deduplication. Approved behaviour changes are listed in PHASE_6 (cancel return timing, pose timeout, offensive window).

Keep manual actions for genuinely target-driven control. Ship a concise noncombat managed example, a code-builder clip example, a dedicated sweep and a borrowing example. The walking sweep is a fixture/example unless the Mars enemy actually composes the needed anatomy; do not claim a finished whip-spider encounter from a fixture.

## 10. Compatibility, cost and review

Old assets default to existing progress behavior. Persist editor-only pose metadata without changing cooked motion data. Adding fields keeps `_CompatibilityVersion == 1`; missing tagged properties load as defaults, proven by a compatibility test that loads the shipped example assets and compares compiled output to a recorded sample. Existing manual and explicit clip APIs remain operational. Consumer renames/bindings are explicit migrations.

Curve compilation, recipe creation and mirror expansion happen on build/play, not each frame. No per-frame clip compilation, asset IO, full-scene traversal, string formatting or unbounded retained notification buffers. New steady-state dispatch traverses only retained managed identities and bounded pages. Benchmark against the same-source baseline with native measurements; report observed costs and allocation sites without invented numeric targets.

Adversarial failure scenarios to attack: synchronous loaded rejection before callback registration; listener destroying a player mid-page; managed acknowledgement racing an explicit consumer; failed pose capture partially mutating tracks; custom progress reversing/duplicating windows; mirror creating invalid handedness; stale solved snapshot captured into a new revision; named slot rename breaking other configurations; blocked return presented as successful completion; an old clip changing behavior merely by opening it.

## 11. Locked signatures and decisions

House style throughout: trailing return types except UFUNCTION declarations, `CK_PROPERTY`/`CK_PROPERTY_GET`, `CK_DEFINE_CONSTRUCTORS` for essentials, enum-mode + value for optionality, one `UPROPERTY()` per line, `CK_ENSURE_IF_NOT` single shape with recovery in the body, no file-local statics (named namespaces). Executors fill bodies, not designs.

### 11.1 Data (`ActionBuilder/CkProceduralActionBuilder_Data.h`, additions to `Action/CkProceduralAction_Data.h`)

```cpp
// Action/CkProceduralAction_Data.h — additive
UENUM(BlueprintType)
enum class ECk_ProceduralAction_ProgressMode : uint8 { Preset, CustomCurve };

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ProgressKey
{   // _Time, _Value: FCk_FloatRange_0to1; CK_DEFINE_CONSTRUCTORS(_Time, _Value)
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ProgressCurve
{   // ECk_ProceduralAction_ProgressMode _Mode = Preset; TArray<FCk_ProceduralAction_ProgressKey> _Keys;
};
// FCk_ProceduralAction_Phase gains: FCk_ProceduralAction_ProgressCurve _Progress; (CK_PROPERTY)
// UCk_ProceduralAction_Data gains, under #if WITH_EDITORONLY_DATA:
//   TArray<FCk_ProceduralAction_PoseGroup> _EditorPoseGroups;   (Phase 4 defines the struct)

// ActionBuilder/CkProceduralActionBuilder_Data.h
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ClipDraft
{   // FCk_ProceduralAction_BodyChannel _BodyChannel; TArray<FCk_ProceduralAction_Slot> _Slots;
    // TArray<FCk_ProceduralAction_Phase> _Phases; TArray<FCk_ProceduralAction_Marker> _Markers;
    // TArray<FCk_ProceduralAction_Window> _Windows;   all CK_PROPERTY
};

UENUM(BlueprintType) enum class ECk_ProceduralAction_ValueMode : uint8 { Keep, Set };

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_LimbPoseTarget
{   // FName _Slot; FVector _Position; ECk_ProceduralAction_ValueMode _OrientationMode = Keep; FQuat _Orientation;
    // ECk_ProceduralAction_ValueMode _BendMode = Keep; FVector _Bend = FVector::UpVector;
    // CK_DEFINE_CONSTRUCTORS(_Slot, _Position)
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PoseKey
{   // FCk_FloatRange_0to1 _Time; ECk_ProceduralAction_ValueMode _BodyMode = Keep; FTransform _Body;
    // TArray<FCk_ProceduralAction_LimbPoseTarget> _Limbs;   CK_DEFINE_CONSTRUCTORS(_Time)
};

UENUM(BlueprintType) enum class ECk_ProceduralAction_Recipe : uint8 { Blank, BodyLean, Reach, Sweep };
UENUM(BlueprintType) enum class ECk_ProceduralAction_MirrorPlane : uint8 { XY, XZ, YZ };

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_RecipeSpec
{   // ECk_ProceduralAction_Recipe _Recipe; TArray<FName> _Slots; FVector _Direction = FVector::ForwardVector;
    // float _AmplitudeCm = 20.f; FCk_Time _Prepare{0.3}, _Execute{0.2}, _Recover{0.4};
    // ECk_ProceduralAction_Easing _Easing = InOutCubic; ECk_ProceduralAction_TrackSpace _Space = BodyLocal;
    // CK_DEFINE_CONSTRUCTORS(_Recipe, _Slots)
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_MirrorSpec
{   // FName _SourceSlot, _DestinationSlot; ECk_ProceduralAction_MirrorPlane _Plane = XZ;
    // CK_DEFINE_CONSTRUCTORS(_SourceSlot, _DestinationSlot, _Plane)
};
```

Recipe semantics: `Blank` = one `Motion` phase of `_Execute`, no keys. `BodyLean` = body channel enabled (IdentityOffset), phases Prepare/Execute/Recover; body keys identity → `-0.25*Amp*Dir` (Prepare), → `+Amp*Dir` (Execute), → identity (Recover). `Reach` = one slot, phases Prepare/Execute/Recover with position keys rest → rest + `Amp*Dir` → rest, in `_Space`, Smooth with Catmull-Rom tangents; the rest value is the slot's `ExplicitRestTarget` when set, else zero (AuthoredRest with explicit target is required for a draft-only recipe; the report says so). `Sweep` = one slot, Execute phase with three keys (start, apex at `Amp*Dir` rotated 90° about the body up at mid-time, end), Smooth with Catmull-Rom tangents. Every recipe sets `_Easing` on each phase from the spec; no windows, no markers.

### 11.2 Builder Utils (`ActionBuilder/CkProceduralActionBuilder_Utils.h`, `UCLASS(NotBlueprintable)`, Category `"Ck|Utils|ProceduralActionBuilder"`)

All are `UFUNCTION(BlueprintCallable) static FCk_ProceduralAction_ValidationReport`. Draft parameters are `UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft`. A failed call leaves the draft unchanged (validate every input, then mutate).

```cpp
Set_BodyChannel(InDraft, const FCk_ProceduralAction_BodyChannel& InChannel);
Set_SlotFrame(InDraft, FName InSlot, ECk_ProceduralAction_TrackSpace InSpace, FName InAnchorId);   // applies to every existing and future track of the slot; rejects unknown slot, anchor/space mismatch
Set_PhaseProgress(InDraft, FName InPhaseId, const FCk_ProceduralAction_ProgressCurve& InCurve);   // rejects unknown phase, Custom with non-Linear easing, bad keys (same rules as Compile)
Add_Phase(InDraft, const FCk_ProceduralAction_Phase& InPhase);     // rejects duplicate/None id, non-positive or non-finite duration, Phases >= MaxPhases
Add_Slot(InDraft, const FCk_ProceduralAction_Slot& InSlot);        // rejects duplicate/None id, Slots >= MaxSlots
Add_BodyKey(InDraft, FName InPhaseId, const FCk_ProceduralAction_TransformKey& InKey);   // rejects body disabled, unknown phase, occupied time, non-finite
Add_LimbKey(InDraft, FName InPhaseId, FCk_FloatRange_0to1 InTime, const FCk_ProceduralAction_LimbPoseTarget& InTarget);   // materializes the (phase, slot) track with the slot frame; rejects unknown slot/phase, occupied time on any written key array
Add_PoseKey(InDraft, FName InPhaseId, const FCk_ProceduralAction_PoseKey& InPose);       // atomic over body + every listed limb; any rejection leaves nothing written
Replace_PoseKey(InDraft, FName InPhaseId, const FCk_ProceduralAction_PoseKey& InPose);   // requires a key at _Time on every written channel; replaces values only
Add_Marker(InDraft, const FCk_ProceduralAction_Marker& InMarker);  // rejects duplicate id, unknown phase, time outside [0,1]
Add_Window(InDraft, const FCk_ProceduralAction_Window& InWindow);  // rejects duplicate id, unknown phases, end <= start on the resolved timeline
Mirror_Slot(InDraft, const FCk_ProceduralAction_MirrorSpec& InMirror);   // see §8; counts destination tracks before writing
Make_FromRecipe(const FCk_ProceduralAction_RecipeSpec& InRecipe, FCk_ProceduralAction_ClipDraft& OutDraft);   // OutDraft reset on failure
Make_Draft_FromData(UCk_ProceduralAction_Data* InSource, FCk_ProceduralAction_ClipDraft& OutDraft);        // null rejects
Build_Into(const FCk_ProceduralAction_ClipDraft& InDraft, UCk_ProceduralAction_Data* InDestination);         // compile via FClipSource; on success copy the five fields with Set_*; never Modify()/transact (editor wraps)
```

Pure helpers (`namespace ck::procedural_action`, in `ActionBuilder/CkProceduralActionBuilder_Utils.h`): `ReflectVector(FVector, ECk_ProceduralAction_MirrorPlane)`, `ReflectRotation(FQuat, ECk_ProceduralAction_MirrorPlane)` (`q' = (w, -S v)`, normalized), `ComputeCatmullRomTangents(TArray<FCk_ProceduralAction_VectorKey>&)`.

### 11.3 Compiler view (`Core/CkProceduralActionValidation.h`)

```cpp
namespace ck::procedural_action
{
    struct CKPROCEDURALANIMATION_API FClipSource   // non-owning; built by both producers
    {
        int32 CompatibilityVersion = 1;
        const FCk_ProceduralAction_BodyChannel* BodyChannel = nullptr;
        const TArray<FCk_ProceduralAction_Slot>* Slots = nullptr;
        const TArray<FCk_ProceduralAction_Phase>* Phases = nullptr;
        const TArray<FCk_ProceduralAction_Marker>* Markers = nullptr;
        const TArray<FCk_ProceduralAction_Window>* Windows = nullptr;
        FString AssetPath;
    };
    CKPROCEDURALANIMATION_API auto MakeClipSource(const UCk_ProceduralAction_Data&) -> FClipSource;
    CKPROCEDURALANIMATION_API auto MakeClipSource(const FCk_ProceduralAction_ClipDraft&) -> FClipSource;   // AssetPath "<draft>"
    CKPROCEDURALANIMATION_API auto Compile(const FClipSource&, FCompiledClip&, TArray<FDiagnostic>&) -> bool;
    // The existing Compile(const UCk_ProceduralAction_Data&, ...) stays and forwards.
}
```

`FCompiledPhase` gains `FCompiledProgressCurve _Progress` (`TArray<double> Times, Values, Tangents`; empty = Preset). `Core/CkProceduralActionProgress.h/.cpp`: `BuildMonotoneProgress(const TArray<FCk_ProceduralAction_ProgressKey>&, FCompiledProgressCurve&) -> bool`, `EvaluateProgress(const FCompiledPhase&, double InRaw) -> double` (preset → existing tween apply; custom → monotone Hermite), `InvertProgress(const FCompiledPhase&, double InProgress) -> double` (bisection, smallest t with f(t) >= p; replaces `InverseEasing` at the compiler's critical-time step). `Evaluate` and `SampleInterval` call `EvaluateProgress`.

### 11.4 Managed playback (`Action/CkProceduralAction_Fragment_Data.h`, `ActionPlayer/*`)

```cpp
UENUM(BlueprintType) enum class ECk_ProceduralAction_DeliveryStatus : uint8 { Complete, Expired, PlayerLost };
// ECk_ProceduralAction_ReadResult gains: ManagedDelivery

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_FailureContext
{   // FName _PhaseId; TArray<FName> _LimbIds; FCk_Time _Elapsed, _Limit; FName _BudgetUnit; int64 _Observed = 0, _Budget = 0;
};
// FCk_ProceduralAction_Terminal and FCk_ProceduralAction_Record gain: FCk_ProceduralAction_FailureContext _Context;
// FFragment_ProceduralAction gains: FCk_ProceduralAction_FailureContext _Context; (friends unchanged)

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ManagedStart
{   // FCk_Handle_ProceduralAction _Action; FCk_ProceduralAction_Identity _Identity; FCk_ProceduralAction_ValidationReport _Report;
};
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ManagedPage
{   // FCk_ProceduralAction_Identity _Identity; TArray<FCk_ProceduralAction_Record> _Records; bool _Terminal;
};
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ManagedCompletion
{   // FCk_ProceduralAction_Identity _Identity; FCk_ProceduralAction_Result _Result; ECk_ProceduralAction_DeliveryStatus _Delivery; FCk_ProceduralAction_FailureContext _Context;
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_ProceduralAction_OnManagedCompleted, FCk_Handle_ProceduralActionPlayer, InPlayer, FCk_ProceduralAction_ManagedCompletion, InCompletion);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_ProceduralAction_OnManagedRecords, FCk_Handle_ProceduralActionPlayer, InPlayer, FCk_ProceduralAction_ManagedPage, InPage);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_ProceduralAction_OnManagedAcceptedPose, FCk_Handle_ProceduralActionPlayer, InPlayer, FCk_ProceduralAction_Receipt, InReceipt);

// FCk_ProceduralAction_Capacity gains: int32 _ManagedRecordsPerTick = 32;  (ValidateCapacity: 1..receipt_limits::MaxOrdinaryRecordsPerInstance+2, independent of RecordsPerInstance because the per-tick budget spans identities; the dispatcher caps each identity's page at min(remaining budget, RecordsPerInstance+2). Corrected 2026-10-12: the earlier 1..RecordsPerInstance+2 rule would have rejected existing small-capacity configs at Configure/Add.)

// UCk_Utils_ProceduralAction_UE (AutoCreateRefTerm = "InOnCompleted")
static FCk_ProceduralAction_ManagedStart Play(UPARAM(ref) FCk_Handle& InScope, const FCk_Handle_ProceduralActionPlayer& InPlayer, const FCk_ProceduralAction_PlaySpec& InSpec, const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted);
static FCk_ProceduralAction_ManagedStart Play_Loaded(UPARAM(ref) FCk_Handle& InScope, const FCk_Handle_ProceduralActionPlayer& InPlayer, const FCk_ProceduralAction_LoadedPlaySpec& InSpec, const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted);
static FText Get_ResultText(const FCk_ProceduralAction_Result& InResult, const FCk_ProceduralAction_FailureContext& InContext);   // BlueprintPure

// UCk_Utils_ProceduralActionPlayer_UE
static FCk_Handle_ProceduralActionPlayer BindTo_OnManagedRecords(UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer, FCk_ProceduralAction_Identity InIdentity, const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate);
static FCk_Handle_ProceduralActionPlayer UnbindFrom_OnManagedRecords(UPARAM(ref) FCk_Handle_ProceduralActionPlayer& InPlayer, FCk_ProceduralAction_Identity InIdentity, const FCk_Delegate_ProceduralAction_OnManagedRecords& InDelegate);
static FCk_Handle_ProceduralActionPlayer BindTo_OnManagedAcceptedPose(...same shape, FCk_Delegate_ProceduralAction_OnManagedAcceptedPose...);
static FCk_Handle_ProceduralActionPlayer UnbindFrom_OnManagedAcceptedPose(...);
static bool Get_IsManagedDelivery(const FCk_Handle_ProceduralActionPlayer& InPlayer, FCk_ProceduralAction_Identity InIdentity);   // BlueprintPure
```

Internals: `ck::FFragment_ProceduralActionPlayer_Managed` holds `TMap<int64, FManagedDelivery> _Entries` and `int32 _NextDispatchIndex`; `FManagedDelivery` holds identity, `FCk_Delegate_ProceduralAction_OnManagedCompleted _Completion`, `TArray<FCk_Delegate_ProceduralAction_OnManagedRecords> _RecordListeners` (max 8), `TArray<FCk_Delegate_ProceduralAction_OnManagedAcceptedPose> _PoseListeners` (max 8), `bool _Completed`. Added by `UCk_Utils_ProceduralActionPlayer_UE::Add` (empty). `FReceiptStore` gains `Get_AcknowledgedThrough(Identity, int64&) -> bool`, a `ManagedDelivery` bit per stream set by `Reserve*` through a delivery argument, and `Get_IsManagedDelivery(Identity) -> bool`. Processors: `FProcessor_ProceduralActionPlayer_ManagedDispatch` (FGroup_Gameplay_Camera, RunAfter Observe) and `FProcessor_ProceduralActionPlayer_ManagedEndPlay` (FGroup_EndPlay, CK_IF_END_PLAY). `DoCreate` gains `ck::EProceduralActionDelivery InDelivery` and an optional completion delegate; a new private `DoCreate_Loaded(...)` holds the body of `Create_FromLoadedClip`, which forwards with `Explicit`.

### 11.5 Presets, queries, recovery

```cpp
// UCk_Utils_ProceduralActionPlayer_UE (BlueprintPure)
static FCk_ProceduralActionPlayer_Spec Make_DedicatedPlayerSpec(const FCk_ProceduralAction_Tuning& InTuning);
static FCk_ProceduralActionPlayer_Spec Make_BorrowingPlayerSpec(const FCk_ProceduralActionPlayer_BorrowingSpec& InSpec);   // {_Support, _Return, _Tuning}
static FCk_ProceduralLimbControl_Support Make_CountOnlySupport(int32 InMinimumTrustedContacts);   // ensures when < 1

// UCk_Utils_ProceduralArticulation_UE
static bool Get_IsAnyLimbControlled(const FCk_Handle_ProceduralArticulation& InArticulation);   // BlueprintPure
static bool Get_IsAnyLimbReturning(const FCk_Handle_ProceduralArticulation& InArticulation);
static TArray<FName> Get_UnplantedLimbIds(const FCk_Handle_ProceduralArticulation& InArticulation);   // Get_Limbs order
static FCk_Handle_ProceduralArticulation Request_RecoverUnplantedLimbs(UPARAM(ref) FCk_Handle_ProceduralArticulation& InArticulation, const FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate);
// FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs: FCk_Request_Base, no fields, CK_REQUEST_DEFINE_DEBUG_NAME; joins the FFragment_ProceduralLimbControl_Requests variant and is drained by FProcessor_ProceduralLimbControl_HandleRequests.
```

### 11.6 Tests (CkTests)

Native: `Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_Builder.cpp`, `Test_ProceduralAction_Progress.cpp`, `Test_ProceduralAction_Mirror.cpp`, `Test_ProceduralAction_Managed.cpp`, `Test_ProceduralAction_Diagnostics.cpp`; `.../ActionPlayer/Test_ProceduralActionPlayer_Presets.cpp`; `.../Articulation/Test_ProceduralArticulation_Query.cpp`. AS: `Script/CkProceduralAnimation/CkAutoTest_ProceduralAnimation_ActionBuilderPublicSurface.as`, `..._ActionManagedPublicSurface.as`, `..._ActionAssetDefinition.as` (+ `_Assets.as` fixture). Blueprint VM: extend `Source/CkTestsEditor/Private/CkProceduralAnimation/Test_ProceduralAction_BlueprintParity.cpp` with Builder and Play rows. Test paths use `Ck.ProceduralAnimation.<Feature>.<Area>.<Case>` and never a campaign name.
