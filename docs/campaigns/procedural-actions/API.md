# Public API and misuse resistance

For the short workflow (compose once, `Create_FromClip` with Auto delivery and `OnFinished`, the pose builder, explicit receipts for damage), see [GETTING_STARTED.md](GETTING_STARTED.md). Sections 8 to 12 cover the outcome table, the builder, delivery and release, the player spec and subject queries.

The public surface is implemented in `Source/CkProceduralAnimation/Public/CkProceduralAnimation/{Action,ActionBuilder,ActionPlayer,Articulation,Limb,LimbControl}`. Native, Blueprint VM and AngelScript focused tests exercise it; [PROGRESS.md](PROGRESS.md) records the current gate and outstanding release evidence. This reference describes callable names, not a certification of every acceptance row.

Reflected values use private UPROPERTY fields and CK accessors. Typesafe handles contain no runtime state. Native callers use the `UCk_Utils_*_UE` classes; generated AngelScript namespaces and handle mixins expose the same reflected operations.

## 1. Features and handles

| Public feature | Typesafe handle | Factory |
|---|---|---|
| Articulation on subject root | FCk_Handle_ProceduralArticulation | Add |
| Generic limb, direct lifetime child of root | FCk_Handle_ProceduralLimb | Created atomically by articulation composition |
| Player on the articulation root | FCk_Handle_ProceduralActionPlayer | Add |
| Scope-owned action instance | FCk_Handle_ProceduralAction | Create_FromClip |

Handles add zero members. State/generation are fragments and receipt values, never embedded into typed handles. Utilities expose Has/Cast/CastChecked/Get_InvalidHandle consistently with the existing feature family.

## 2. Primary call sequence

The standalone consumer assembles direct-child transform parts, composes the articulation, adds one player, and creates an action under a lifetime scope. Existing walker composition supplies its articulation through the legacy adapter; do not call standalone Add over a gait/leg composition. Observe Get_State/Get_Result or OnFinished and use accepted receipts for effects.

```cpp
// Public declarations; the UCLASS/GENERATED_BODY wrappers are omitted.
// UFUNCTION return types use the current house form.

UFUNCTION(BlueprintCallable,
          Category = "Ck|Utils|ProceduralArticulation",
          DisplayName = "[Ck][ProceduralArticulation] Add")
static FCk_Handle_ProceduralArticulation
Add(
    UPARAM(ref) FCk_Handle_Transform& InRoot,
    const FCk_ProceduralArticulation_Spec& InSpec);

UFUNCTION(BlueprintCallable,
          Category = "Ck|Utils|ProceduralActionPlayer",
          DisplayName = "[Ck][ProceduralActionPlayer] Add")
static FCk_Handle_ProceduralActionPlayer
Add(
    UPARAM(ref) FCk_Handle_ProceduralArticulation& InSubject,
    const FCk_ProceduralActionPlayer_Spec& InSpec);

UFUNCTION(BlueprintCallable,
          Category = "Ck|Utils|ProceduralAction",
          DisplayName = "[Ck][ProceduralAction] Create From Clip")
static FCk_Handle_ProceduralAction
Create_FromClip(
    UPARAM(ref) FCk_Handle& InScope,
    const FCk_Handle_ProceduralActionPlayer& InPlayer,
    const FCk_ProceduralAction_PlaySpec& InSpec);
```

These Add methods are in their respective Utils classes, not overloads in one BPFL. A subject has one player. Duplicate composition is diagnosed, not silently returned as success.

Create_FromClip synchronously validates the outer handles and the spec and reserves the action's receipt stream. A resident clip is compiled and bound before the call returns: the action comes back PendingAdmission, or already Finished with `Rejected` (`ClipBindingsRejected`) or `InvalidContent` (`InvalidClipContent`). An unloaded clip starts PendingLoad and loads asynchronously. Claims and admission are always deferred: a valid returned handle means a request instance exists, NOT that playback has started. Compilation copies immutable content and retains no strong UObject dependency, so edits or garbage collection after the call cannot change the active compiled revision; transient clips assembled from consumer tuners play the same way. Invalid outer handles, an invalid spec or exhausted receipt capacity return invalid with a contextual diagnostic and no partial children.

### Blueprint flow

Compose subject -> Add Action Player -> Create From Clip -> Is Valid -> bind OnFinished and inspect Get_State. Tooltips distinguish PendingLoad/PendingAdmission from Acquiring, Playing and Finished. Example BP must exercise rejection, cancellation, scope destruction and completed return, not just happy-path construction.

### AngelScript flow

```angelscript
auto Player = utils_procedural_action_player::Add(Subject, PlayerSpec);
auto Action = utils_procedural_action::Create_FromClip(StateScope, Player, PlaySpec);

// During the consumer's update:
const auto State = Action.Get_State();
if (State == ECk_ProceduralAction_State::Finished)
{
    const auto Result = Action.Get_Result();
    // Consumer handles Result; framework does not interpret gameplay.
}
```

The executable helpers in `Script/CkProceduralAnimation/Examples/CkProceduralAction_Noncombat.as` play a clip with Auto delivery and an OnFinished completion using these namespaces. A valid created handle means an instance exists; clip playback reaches Playing on its own.

## 3. Struct essentials and groups

| Type | Essential constructor values | Optional grouped values |
|---|---|---|
| FCk_ProceduralArticulation_Spec | Layout, Bindings | Presentation, ClearanceSettings |
| FCk_ProceduralActionPlayer_Spec | none | Tuning, Capacity, Locomotion (support and walking return policy) |
| FCk_ProceduralAction_PlaySpec | TSoftObjectPtr<UCk_ProceduralAction_Data> Clip | SlotBindings, Delivery (Explicit by default, or Auto) |
| FCk_ProceduralAction_MirrorSpec | SourceSlot, DestinationSlot, Plane | none; Plane defaults to XZ (normal Y); builder input |
| FCk_ProceduralAction_SlotBinding | Slot, LimbId | none |
| FCk_Request_ProceduralAction_Cancel | Reason FName | none; recovery policy belongs to subject/tuning |
| FCk_Request_ProceduralAction_Release | none | none; early teardown only |
| FCk_Request_ProceduralActionPlayer_ConsumeReceipts | ActionIdentity, ThroughStreamSequence | none; action-scoped cursor, inclusive acknowledgement |
| FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs | none | none; recovers every limb that is Unplanted when it drains, as one set |

Do not add raw booleans such as Force, IgnoreSupport, SkipValidation, NoReturn, ApplyImmediately or AllowPartial. If a new policy is legitimate, name it, define its safety contract and validate it.

No public UFUNCTION overloads or reflected TOptional defaults. Use explicit mode enums for optional groups and reject inconsistent inactive fields where they create ambiguity. Get-only runtime observations cannot be reused as mutation payloads.

## 4. Requests and outcomes

```cpp
UFUNCTION(BlueprintCallable,
          Category = "Ck|Utils|ProceduralAction",
          DisplayName = "[Ck][ProceduralAction] Request Cancel",
          meta = (AutoCreateRefTerm = "InDelegate"))
static FCk_Handle_ProceduralAction
Request_Cancel(
    UPARAM(ref) FCk_Handle_ProceduralAction& InAction,
    const FCk_Request_ProceduralAction_Cancel& InRequest,
    const FCk_Delegate_Request_OnCompleted& InDelegate);
```

Request_Release follows the same form; Request_RecoverUnplantedLimbs targets the articulation. Request completion uses normal Ck machinery. It is distinct from action completion and observed settlement; terminal outcomes remain pollable when AS callers omit delegates.

Read_Receipts(Player, ActionIdentity, AfterStreamSequence, MaxRecords) returns a copied bounded page. Request_ConsumeReceipts targets Player with the request type above; it never acknowledges another action's records, and on an Auto identity it ensures and completes `Failed_NotEnqueued` without moving the cursor. These are separate named UFUNCTIONs, not overloads.

A read cursor behind the gameplay-acknowledged cursor returns `ECk_ProceduralAction_ReadResult::Gap`, inclusive `FirstMissingSequence`/`LastMissingSequence`, and any retained records after that range. `LastStreamSequence` is resumable even when the page contains no records. Negative or future cursors remain `InvalidCursor`. Independent debugger/editor copies never consume gameplay records and retain explicit missing ranges instead of inventing events.

Cancellation never resets an existing return deadline. Duplicate cancellation during Returning/Cancelling or after Finished completes with the standard `ECk_Request_OperationResult::Failed`; query the action state/result for its existing outcome. Request_Release of a running action requests cancellation and removes the instance after recovery; of a finished action, it destroys the instance at once. The normal flow needs neither: an action releases itself once its terminal record is acknowledged (section 10). Scope destruction transfers reconciliation to the subject and retained receipt stream without relying on a living delegate target.

BindTo_OnStateChanged and BindTo_OnFinished use the existing signal binder/unbinder conventions. OnFinished fires once, when the state commits to Finished, with `FCk_ProceduralAction_Terminal`: a copied value with action identity/generation, result (outcome, recovery, reason) and failure context, never a reference into mutable storage. Emit only after state is committed. Reentrant calls enqueue for a later drain, never mutate the current iteration.

## 5. Queries

| Query | Result and contract |
|---|---|
| Get_State(Action) | PendingLoad, PendingAdmission, Acquiring, Playing, Returning, Cancelling, Finished |
| Get_Result(Action) | Outcome, Recovery and Reason; default Outcome=None; Finished or a Terminal record establishes completion |
| Get_LastReceipt(Action) | HasAcceptedPose plus copied receipt; before first publication false |
| Get_LimbControl(Subject,LimbId) | State, Owner, Generation, Locomotor, Participating, TrustedSupport and Reason in one copied observation |
| Get_Health(Subject), Get_Fault(Subject) | Operational/Faulted and typed fault; faulted subjects require recomposition |
| Get_SettledRevision(Subject) | subject-wide accepted publication revision; other actions/holds can advance it |
| Get_Identity(Action) | monotonic player-local stream identity; retain the Player with it; generation is a separate stamp field |
| Get_RetainedReceiptIdentities(Player) | bounded sorted snapshot of retained streams, including terminal streams |
| Read_Receipts(Player,Identity,AfterSequence,MaxRecords) | copied records, last sequence, terminal/pending state; bad cursor rejected, no consumption side effect |
| Validate_Data(Data) | structured validation report using the same compiler validation as cook/runtime |

Never use 'CanPlay' as a promise: support can change before deferred admission. Validate_Data checks content only; binding and admission are reported on the action.

## 6. Receipt schema

FCk_ProceduralAction_Receipt is an immutable reflected observation:
- HasAcceptedPose.
- Action identity + action generation + monotonically increasing SampleSequence.
- Subject pose revision and topology revision; content revision.
- FromTime, ToTime as FCk_Time; PhaseId.
- Root frame at evaluation, accepted body presentation, named limb results.
- Each limb result: requested endpoint, accepted endpoint, endpoint error, chain state, all accepted segment transforms and contact evidence. The accepted endpoint is the terminal observation; it is rigid even when presentation parts retain nonuniform visual scale.
- Named windows active throughout each returned half-open interval `[FromTime, ToTime)`. Clip publication stops at every window enter/exit; events at `ToTime` apply after that interval. Point markers may remain interior. Marker crossings have stable sequence IDs.
- Previous/current settled poses and optional evaluated intermediate samples.
- Continuity enum: Continuous, FirstSample, Teleported, ControllerChanged, SolverDiscontinuity, Incomplete.
- Bounded sampling counters.

The first sample supplies no previous continuous interval. A receipt is not authoritative damage, physics contact, or an observed intermediate rendered frame. Retrieval does not mutate or acknowledge the action; internal publication drives its clock. Diagnostic history may report ReceiptGap; it never fabricates missing events.

Reliable consumption uses player-owned streams keyed by ActionIdentity. Each record has a strictly increasing action-local StreamSequence, distinct from SampleSequence, and a typed kind: AcceptedPose, WindowsClosed, Terminal. Consumers deduplicate by ActionIdentity + StreamSequence. Request_ConsumeReceipts includes that identity and an inclusive ThroughStreamSequence. Reject future/out-of-stream cursors; repeated acknowledgement is idempotent. This is one designated acknowledging consumer per stream, the caller for an Explicit identity and the player for an Auto one; extra observers use signals/diagnostic copies. Signals do not erase the inbox.

Capacity is a construction group, not mutable live tuning. Reserve one complete stream and result slot at action creation, including two non-pose slots for an unconditional all-window closure summary and terminal record. RecordsPerInstance bounds ordinary records; overflow closes windows, stops new motion and finishes with ReceiptOverflow through the reserved slots. A closure carries all active window IDs and reason with HasAcceptedPose=false; it precedes Terminal, including cancellation/support loss without a new pose. Terminal includes recovery outcome. No event is silently overwritten.

Retained streams, live or finished, are capped at four per concurrent action (section 11). A finished action stays queryable until its terminal record is acknowledged; the player's next service pass then destroys it. Scope destruction may remove the entity earlier, but recovery completion still appends its terminal record without touching the dead entity. A stream is reclaimed after entity removal plus terminal acknowledgement. For a removed entity whose consumer never acknowledges, a 2 s orphan retention starts at terminal publication; expiry reclaims the stream and records a diagnosed abandoned result. Read/consume with its old identity returns StreamExpired, never empty-success. IDs are not reused within player lifetime. Consumers reconcile player/action identity and expiry before retaining window state. This is bounded orphan retention, not infinite delivery to a destroyed scope. Player/subject destruction ends all streams; no delivery claim crosses world teardown.

Polling LastReceipt is for presentation/diagnostics. Effects consumers drain after publication and process StreamSequence in order: an accepted interval before closure remains historical evidence, while closure bars intervals at/after its boundary. Closure does not retroactively erase accepted intervals. A consumer may explicitly reject late evidence under its own gameplay policy; the framework neither promises damage nor conceals that discard. Never execute a cached interval twice or keep using a window merely because Enter was once observed.

## 7. Diagnostics and defaults

`UCk_Utils_ProceduralAnimation_Debug_UE::Get_ActionSnapshot` and `Get_ControlSnapshot` return reflected value copies for native, Blueprint and AngelScript tools. Call on the game thread. Invalid, removed or wrong-feature handles return `Available=false`; inspecting a subject never reserves resources, loads content, queries the world or advances simulation. Copies are observations, not authority to submit poses or apply effects.

The action snapshot identifies the pinned content revision, accepted time, backlog, pending publication and currently active watchdogs. Deadline elapsed/remaining values use the runtime's serviced clocks. The subject snapshot exposes settled revision, controller ownership, original return deadlines, support leases, committed contacts/reservations and proposed landing destinations. Root-owned recovery remains inspectable after its action entity disappears.

`SupportFresh` validates the support/root frame; it does not assert that contacts were solved this frame. A lease's `ProjectedRegion` is the uninset convex hull projected onto the support plane through the root proxy. Interpret it together with `Policy.ProjectedRegionInsetCm`, `MinimumEdgeDistanceCm` and `SupportResult`; the drawn hull alone is not a permitted takeoff region. Proposed landing targets and actual trusted contact evidence are separate fields. Recorded receipt poses retain their own stamp and must not be relabeled with current live diagnostics.

Errors identify subject/action/limb, controller owner, phase, field path and requested versus accepted pose. Expected contention is returned, not logged as a framework invariant failure. Malformed input uses the house ensure plus explicit safe rejection. Get_State/Get_Result remain available after error.

Default tuning is safe but visible. Missing required slot mapping is an error, never 'use the first leg'. Duplicate bindings fail the entire instance. Two slots mapped to one limb fail unless the compiler has proven those are the same single resource by explicit aliasing; release 1 does not support aliases.

The integration acceptance set covers:
- standalone articulated inspection arm with no gait;
- walking carrier reaching with one limb and replanting;
- body-only mechanical lever/lean action;
- authored two-limb gesture with body counter-motion.
These examples use no game enemy classes, damage tags or game-project imports. Consult PROGRESS for each example's executed evidence; this list is not a claim that every example has passed its final runtime gate.

## 8. Outcomes, likely causes and corrective actions

`FCk_ProceduralAction_Result` carries `Outcome`, `Recovery` and `Reason`; the terminal record and the OnFinished payload (`FCk_ProceduralAction_Terminal`) also carry `FCk_ProceduralAction_FailureContext` (phase, claimed limb IDs, elapsed against limit for a missed deadline, observed against budget with a unit name). Only measured fields are filled: None, zero and empty mean "not measured". `UCk_Utils_ProceduralAction_UE::Get_ResultText(Result, Context)` renders both for a person; it formats on demand, renders an unknown reason as its identifier and is not meant for branching. Branch on `Outcome` and `Recovery`.

The reasons below are the ones the runtime writes (checked against the `Action/`, `ActionPlayer/` and `LimbControl/` sources on 2026-10-13). Five kinds of failure read differently: rejected input (`Rejected`, `InvalidContent`, `AssetFailed`), unreachable or unsettled motion (`SubjectPoseRejected`, `PublicationFailed`, `PublicationTimeout`, `SamplingBudgetExceeded`), support loss (`SupportLost`), a delivery gap on an Explicit stream (`ReadResult` `Gap` or `StreamExpired`), and a failed return (`Recovery` `Failed`, `RecoveryFailed`).

| Outcome | Reason(s) | Likely cause | Corrective action |
|---|---|---|---|
| `None` | none | The action has not finished yet. | Wait for `Finished`, `OnFinished` or the terminal record; never treat `None` as success. |
| `Completed` | none, or the reason the return reported | The clip reached its end and the limbs returned. | Nothing. Check `Recovery`: `Succeeded` means every claimed limb is back under its default control. |
| `Rejected` | `ClipBindingsRejected` | A slot without a binding, a binding to a limb the subject lacks, two slots on one limb, or more windows (32) or events (64) at one time than a receipt carries. With a resident clip the action is already `Finished` when `Create_FromClip` returns. | Fix the bindings; spread the clip's windows and events. `Validate_Data(Clip)` checks the content itself. |
| `Rejected` | `Busy`, `UnavailableLimb`, `InvalidChannels`, `InvalidSubject`, `InvalidOwner`, `Faulted`, `AcquireFailed` | Admission found a claimed channel held by another action, a limb that is not available (detached, unplanted, faulted), or a subject that cannot be controlled. | Wait for or cancel the other action; recover unplanted limbs; recompose a faulted subject. Do not retry blindly every frame. |
| `Rejected` | `SupportUnavailable` | A walking limb is bound while the player's support mode is `Disabled` (the default; Recovery `NotRequired`), or borrowing it would leave fewer trusted planted contacts than the support policy requires. | Author `Locomotion.Support` on the player spec; borrow when the walker stands on trusted ground, lower the requirement deliberately, or borrow a different limb. |
| `Rejected` | `EntryCaptureRejected` | The subject's settled entry pose could not seed the clip's initial values (for example a captured body pose on a subject with no body presentation). | Compose a presentation for body clips; check the clip's initial sources against the subject. |
| `InvalidContent` | `InvalidClipContent` | The clip fails the compiler; `Validate_Data(Clip)` lists every diagnostic with its field path. | Fix the clip; the builder's reports use the same field paths. |
| `AssetFailed` | `AssetLoadFailed`, `LoadTimeout`, `StaleLoadGeneration`, `MissingLoadingState` | The soft clip path does not resolve, or loading outlived `Watchdogs.LoadTimeout` (the context carries elapsed and limit). | Fix the soft path, or preload the clip so `Create_FromClip` compiles it before returning. |
| `Cancelled` | the caller's cancel reason, or `Released` | `Request_Cancel`, or `Request_Release` of a live action. | Expected when you asked for it; read `Recovery` for the return. |
| `OwnerLost` | `OwnerLost` | The action's lifetime scope was destroyed. The player still returns the limbs and appends the terminal. | Keep scopes alive while the action matters; drain orphaned explicit streams through `Get_RetainedReceiptIdentities`. |
| `SubjectLost`, `PartLost` | none | Reserved: no current runtime path writes these. Destroying the player ends its streams and destroys its live Auto actions; losing a bound part faults the subject. | Treat as a failure if seen; read `Get_Health` / `Get_Fault` on the subject. |
| `SupportLost` | `SupportLost`, `SupportHoldTimeout`, `SupportEmergencyRequested` | While a walking limb was borrowed, the other trusted contacts dropped below the policy, or the gait needed the limb back. | Expected on unstable ground. Require more contacts in `Locomotion.Support`, or keep the action shorter. |
| `AcquisitionTimeout` | `AcquisitionTimeout` | Claims or the first settled entry did not complete within `Watchdogs.AcquisitionTimeout` (context: elapsed, limit). | Look for a held channel or a subject that never settles; raise the watchdog only when acquisition is legitimately slow. |
| `PublicationTimeout` | `PublicationTimeout`, `ClipPublicationTimeout` | A staged pose was not settled and published in time (context: elapsed, limit). | Usually unreachable or blocked targets: check the endpoint errors in the last receipt and the limb's reach. |
| `PublicationFailed` | `SettlementFailed`, `HoldSettlementFailed`, `ReceiptRejected` | The solver did not settle the staged pose, or the receipt store refused a record. | Inspect `Get_LastReceipt` endpoint errors and chain state; check the player capacity. |
| `SubjectPoseRejected` | `SubjectPoseRejected`, `ClipHoldRejected`, `HoldCandidateInvalid`, `ReturnReattachmentDiscontinuity`, or the subject's interruption reason | A requested pose the subject cannot hold: non-rigid or unreachable targets under `RejectUnreachable`, a body offset outside admission, or a reattachment discontinuity. | Keep targets within reach or author `ProjectToReach`; keep body offsets rigid and bounded. |
| `SamplingBudgetExceeded` | `SamplingBudgetExceeded`, `SettledMotionExceededSamplingLimits` | One accepted step moved an endpoint or a segment farther than the tuned sampling budget (context: observed and budget in micrometres or millidegrees when measured). | Slow the clip (`PlayRate`, longer phases) or raise the `Tuning.Sampling` limits deliberately. |
| `ReceiptOverflow` | `ReceiptRejected` | Explicit delivery whose consumer did not acknowledge records fast enough (context: pending records against `RecordsPerInstance` at the per-action limit). | Drain and `Request_ConsumeReceipts` every frame, raise `Capacity.RecordsPerInstance`, or use Auto delivery. |
| `RecoveryFailed` | the return's reason: `ReturnTimeout`, `ReturnSettlementTimeout`, `ReturnSettlementFailed`, `ReturnProbeAttemptsExhausted`, `ReturnPreparationFailed`, `ReturnHandoffStale`, `ReturnReattachmentFailed`, `ReturnReattachmentEndpointChanged` | The motion completed but a limb could not return: no trusted ground within the return budget, or the return could not settle. The context carries elapsed and limit only when a deadline was missed. | The limb stays `Unplanted` (`Get_UnplantedLimbIds`). Restore usable ground, then `Request_RecoverUnplantedLimbs`. Any other outcome with `Recovery` `Failed` means the same for its limbs. |

## 9. Pose builder

`UCk_Utils_ProceduralActionBuilder_UE` (AngelScript `utils_procedural_action_builder`, header `ActionBuilder/CkProceduralActionBuilder_Utils.h`) edits a value-only `FCk_ProceduralAction_ClipDraft` (the clip's body channel, slots, phases, markers and windows). Every mutation takes the draft by reference, validates every input before writing and returns `FCk_ProceduralAction_ValidationReport`; an `Invalid` report leaves the draft unchanged and names the field with the compiler's field paths. Nothing is sorted, clamped or invented.

| Function | Use |
|---|---|
| `Make_FromRecipe(Recipe, OutDraft)` | Starting data: `Blank` (one Motion phase), `BodyLean` (Prepare, Execute and Recover body keys), `Reach` and `Sweep` (one slot). Limb recipes want `LimbRestNormalized`; the report warns otherwise, and the rest is then the slot frame's origin. |
| `Make_Draft_FromData(Clip, OutDraft)` | Copies an existing clip for editing. |
| `Add_Phase`, `Add_Slot`, `Set_BodyChannel`, `Set_SlotFrame(Draft, Slot, Space)` | Structure and the per-slot coordinate frame. A phase's progress is one preset easing on `FCk_ProceduralAction_Phase`. |
| `Add_BodyKey`, `Add_LimbKey`, `Add_PoseKey`, `Replace_PoseKey` | Keys. A pose key is atomic over the body and every listed limb; `Replace_PoseKey` requires existing keys at that time. |
| `Add_Marker`, `Add_Window` | Events on the elapsed phase timeline; windows are half-open. |
| `Mirror_Slot(Draft, MirrorSpec)` | Writes a reflected copy of a declared source slot's tracks into a declared destination slot (body-local and root-relative frames only; a rest-normalized frame needs a subject and is rejected here). Bind the destination slot to the opposite limb at play. |
| `Build_Into(Draft, Clip)` | Compiles the draft with the clip compiler and, only when it compiles, writes the five fields into a caller-owned clip. Not a play and not an editor transaction. |
| `Write_Into(Draft, Clip)` | Writes the five fields without compiling, for incremental authoring: a clip still missing keys can be saved. Rejects only a null clip or an unknown compatibility version. `Create_FromClip` and `Build_Into` still compile and refuse such a clip. |

The clip object is caller-owned and must stay referenced while it is built and played. Build per input revision, not per frame. `ReflectVector`, `ReflectRotation` (`q' = (w, -S v)`) and `ComputeCatmullRomTangents` in `ck::procedural_action` are the shared pure helpers.

## 10. Delivery and release

`FCk_ProceduralAction_PlaySpec::_Delivery` chooses who acknowledges the action's receipt stream. It is fixed when `Create_FromClip` reserves the identity.

- **Explicit** (the default). The caller drains with `Read_Receipts` and acknowledges with `Request_ConsumeReceipts`. Damage that must see every accepted interval uses it.
- **Auto.** The player acknowledges every record of the identity at the start of each of its service passes. `BindTo_OnFinished` is the completion: it fires once, when the state commits to `Finished`, with `FCk_ProceduralAction_Terminal`. Bind it right after `Create_FromClip`; with the default `FireIfPayloadInFlightThisFrame` policy it also reports a resident clip rejected inside the call. `Request_ConsumeReceipts` on the identity ensures and completes `Failed_NotEnqueued`. `Read_Receipts` stays an ordinary non-consuming read for every identity, so the gameplay debugger keeps recording Auto streams. Auto supports presentation and noncritical effects; it is not a damage channel.
- **Release.** Either way the action releases itself: once its state is `Finished` and its stream's terminal record is acknowledged, the player's next service pass destroys the action entity (destruction completes in the end-of-frame lifetime pass). For Explicit, acknowledging means `Request_ConsumeReceipts` through the terminal record. For Auto, a terminal written during frame N is acknowledged, and its action destroyed, in the service pass of frame N+1. `Request_Release` is for early teardown only: a finished action is destroyed at once, a running one returns its limbs first. A player's end of play destroys its live Auto actions.

## 11. Player spec

`FCk_ProceduralActionPlayer_Spec` has three groups, set directly and passed to `Add`.

- **Tuning** (`Set_Tuning`) is the only tuning the player's actions play with; each action captures it at creation. Its `Sampling` limits (`MaxEndpointTravelCm`, `MaxSegmentAngularStepDegrees`) also bound every limb return.
- **Capacity** has two knobs: `MaxConcurrentInstances` (1..64, default 4) and `RecordsPerInstance` (1..4096, default 64). The rest is derived (`Get_MaxRetainedInstances`, `Get_MaxRetainedRecords`, `Get_MaxRetainedPoseTransforms` in `Core/CkProceduralActionReceiptStore.h`): 4 retained streams per concurrent action, at most 256 (16 by default); `RecordsPerInstance` + 2 records per retained stream, at most 1049088 (1056 by default, and always at least 3 x `MaxConcurrentInstances`); 32 pose transforms per ordinary record, at most 1048576 (32768 by default). A receipt carries at most 64 samples, 64 events and 32 windows (`receipt_limits`), and an orphaned finished stream is retained 2 s.
- **Locomotion** (`Set_Locomotion`) carries `Support` (`FCk_ProceduralLimbControl_Support{Mode, MinimumTrustedContacts}`) and `Return`. Support mode `Disabled` is the default: an action that binds a walking limb finishes at admission with Outcome `Rejected`, Recovery `NotRequired`, reason `SupportUnavailable`, while dedicated limbs and the body channel play as usual. `CountOnly` counts trusted planted contacts, not visually standing legs.

No spec guarantees a reachable plant on unknown terrain.

## 12. Subject queries and recovery

On `UCk_Utils_ProceduralArticulation_UE`: `Get_IsAnyLimbControlled` (currently action-controlled; a returning limb is not controlled), `Get_IsAnyLimbReturning`, `Get_UnplantedLimbIds` (in `Get_Limbs` order), and `Request_RecoverUnplantedLimbs(Subject, FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs{}, OnCompleted)`, which gathers the limbs that are unplanted when the request is processed and retries them as one set under the player's support and return policy: an empty set completes `Succeeded` without claims, an incompatible state completes `Failed`. Recover only after usable ground is back. An admitted recovery is not a replanted limb: wait for the limb's control state to return to `Default` with trusted support.
