# Public API and misuse resistance

For the short workflow (compose once, `Play_Loaded` with a completion, the pose builder, explicit receipts for damage), see [GETTING_STARTED.md](GETTING_STARTED.md). Sections 9 to 13 cover the outcome table, the builder, managed playback, player presets and subject queries.

The public surface is implemented in `Source/CkProceduralAnimation/Public/CkProceduralAnimation/{Action,ActionBuilder,ActionPlayer,Articulation,Limb,LimbControl}`. Native, Blueprint VM and AngelScript focused tests exercise it; [PROGRESS.md](PROGRESS.md) records the current gate and outstanding release evidence. This reference describes callable names, not a certification of every acceptance row.

Reflected values use private UPROPERTY fields and CK accessors. Typesafe handles contain no runtime state. Native callers use the `UCk_Utils_*_UE` classes; generated AngelScript namespaces and handle mixins expose the same reflected operations.

## 1. Features and handles

| Public feature | Typesafe handle | Factory |
|---|---|---|
| Articulation on subject root | FCk_Handle_ProceduralArticulation | Add |
| Generic limb, direct lifetime child of root | FCk_Handle_ProceduralLimb | Created atomically by articulation composition |
| Player on the articulation root | FCk_Handle_ProceduralActionPlayer | Add |
| Scope-owned action instance | FCk_Handle_ProceduralAction | Play / Play_Loaded (managed delivery), Create_FromClip / Create_FromLoadedClip / Create_Manual (explicit delivery) |

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

Create_FromClip synchronously validates outer handles and allocates a Pending action entity only; load/compile/admission is deferred. It does not synchronously load assets or claim limbs. A valid returned handle means a request instance exists, NOT that playback has started. Expected load/admission failure is observable on that instance. Invalid outer handles return invalid with a contextual diagnostic and no partial children.

`Create_FromLoadedClip` accepts `FCk_ProceduralAction_LoadedPlaySpec` with a hard `UCk_ProceduralAction_Data*`. It pins the already-resident source for the call, synchronously compiles and copies immutable content, then enters the same deferred claim/publication/return pipeline. It performs no asset IO and retains no strong UObject dependency after compilation. Edits or garbage collection after the call cannot change the active compiled revision. Invalid content/bindings produce a reserved identity with a typed terminal result; invalid outer handles/configuration return invalid. This supports transient clips assembled from consumer tuners. Loaded-clip native/AS and Blueprint VM execution have focused passing evidence in PROGRESS.

### Blueprint flow

Compose subject -> Add Action Player -> Create From Clip -> Is Valid -> bind OnFinished and inspect Get_State. Tooltips distinguish PendingLoad/PendingAdmission from Acquiring, Playing and Finished. Manual actions expose Ready after acquisition; only Ready accepts a pose batch. Example BP must exercise rejection, cancellation, scope destruction and completed return, not just happy-path construction.

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

The executable helpers in `Script/CkProceduralAnimation/Examples/CkProceduralAction_Noncombat.as` demonstrate manual control using these namespaces and generated request defaults. A valid created handle means an instance exists; wait for Ready before manual submission, or Playing for managed clip playback.

## 3. Struct essentials and groups

| Type | Essential constructor values | Optional grouped values |
|---|---|---|
| FCk_ProceduralArticulation_Spec | Layout, Bindings | Presentation, ClearanceSettings |
| FCk_ProceduralActionPlayer_Spec | none | Tuning, Capacity, Locomotion (support and walking return policy) |
| FCk_ProceduralAction_PlaySpec | TSoftObjectPtr<UCk_ProceduralAction_Data> Clip | SlotBindings, AnchorBindings, Mirrors, explicit TuningOverride mode/value |
| FCk_ProceduralAction_LoadedPlaySpec | UCk_ProceduralAction_Data* Clip | Same PlayOptions bindings/mirrors/tuning group as PlaySpec |
| FCk_ProceduralAction_MirrorSpec | SourceSlot, DestinationSlot, Plane | none; Plane defaults to XZ (normal Y) |
| FCk_ProceduralAction_SlotBinding | Slot, LimbId | none |
| FCk_ProceduralAction_AnchorBinding | Id, FTransform Frame | none; snapshot frame, not live entity tracking |
| FCk_Request_ProceduralAction_Cancel | Reason FName | none; recovery policy belongs to subject/tuning |
| FCk_Request_ProceduralAction_Release | none | none |
| FCk_Request_ProceduralAction_End | none | none; manual control only, returns resources then completes |
| FCk_Request_ProceduralActionPlayer_ConsumeReceipts | ActionIdentity, ThroughStreamSequence | none; action-scoped cursor, inclusive acknowledgement |
| FCk_Request_ProceduralActionPlayer_UpdateTuning | Tuning | none; active instances unchanged |
| FCk_Request_ProceduralArticulation_RecoverLimbs | LimbIds | none; retries use configured finite return budget |

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

Request_Release follows the same form. Request_UpdateTuning targets the player; Request_RecoverLimbs targets the articulation. Request completion uses normal Ck machinery. It is distinct from action completion and observed settlement; terminal outcomes remain pollable when AS callers omit delegates.

Create_Manual has the same scope/player arguments and handle return as Create_FromClip, substituting FCk_ProceduralAction_ControlSpec. Request_End and Request_SubmitPoseBatch follow the action request form above. Read_Receipts(Player, ActionIdentity, AfterStreamSequence, MaxRecords) returns a copied bounded page. Request_ConsumeReceipts targets Player with the request type above; it never acknowledges another action's records. These are separate named UFUNCTIONs, not overloads.

A read cursor behind the gameplay-acknowledged cursor returns `ECk_ProceduralAction_ReadResult::Gap`, inclusive `FirstMissingSequence`/`LastMissingSequence`, and any retained records after that range. `LastStreamSequence` is resumable even when the page contains no records. Negative or future cursors remain `InvalidCursor`. Independent debugger/editor copies never consume gameplay records and retain explicit missing ranges instead of inventing events.

Cancellation never resets an existing return deadline. Duplicate cancellation during Returning/Cancelling or after Finished completes with the standard `ECk_Request_OperationResult::Failed`; query the action state/result for its existing outcome. Request_Release of a running action requests cancellation and removes the instance after recovery. Scope destruction transfers reconciliation to the subject and retained receipt stream without relying on a living delegate target.

Provide BindTo_OnStateChanged and BindTo_OnFinished using the existing signal binder/unbinder conventions. Terminal payload is a copied value with action identity/generation, result and recovery outcome; never a reference into mutable storage. Emit only after state is committed. Reentrant calls enqueue for a later drain, never mutate the current iteration.

## 5. Queries

| Query | Result and contract |
|---|---|
| Get_State(Action) | PendingLoad, PendingAdmission, Acquiring, Ready (manual only), Playing (clip only), Returning, Cancelling, Finished |
| Get_Result(Action) | Outcome, Recovery and Reason; default Outcome=None; Finished or a Terminal record establishes completion |
| Get_LastReceipt(Action) | HasAcceptedPose plus copied receipt; before first publication false |
| Get_AdmissionReport(Player, PlaySpec) | static/resident-content diagnostics only; Advisory=true, never reserves or guarantees later admission |
| Get_LimbControl(Subject,LimbId) | State, Owner, Generation, Locomotor, Participating, TrustedSupport and Reason in one copied observation |
| Get_Health(Subject), Get_Fault(Subject) | Operational/Faulted and typed fault; faulted subjects require recomposition |
| Get_SettledRevision(Subject) | subject-wide accepted publication revision; other actions/holds can advance it |
| Get_Identity(Action) | monotonic player-local stream identity; retain the Player with it; generation is a separate stamp field |
| Get_RetainedReceiptIdentities(Player) | bounded sorted snapshot of retained streams, including terminal streams |
| Read_Receipts(Player,Identity,AfterSequence,MaxRecords) | copied records, last sequence, terminal/pending state; bad cursor rejected, no consumption side effect |
| Validate_Data(Data) | structured validation report using the same compiler validation as cook/runtime |

Never use 'CanPlay' as a promise: support can change before deferred admission. Get_AdmissionReport explicitly distinguishes AssetNotResident/NotEvaluated from Invalid.

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
- Tuning revision and bounded sampling counters.

The first sample supplies no previous continuous interval. A receipt is not authoritative damage, physics contact, or an observed intermediate rendered frame. Retrieval does not mutate or acknowledge the action; internal publication drives its clock. Diagnostic history may report ReceiptGap; it never fabricates missing events.

Reliable consumption uses player-owned streams keyed by ActionIdentity. Each record has a strictly increasing action-local StreamSequence, distinct from SampleSequence, and a typed kind: AcceptedPose, WindowsClosed, Terminal. Consumers deduplicate by ActionIdentity + StreamSequence. Request_ConsumeReceipts includes that identity and an inclusive ThroughStreamSequence. Reject future/out-of-stream cursors; repeated acknowledgement is idempotent. This is one designated acknowledging consumer per stream; extra observers use signals/diagnostic copies. Signals do not erase the inbox.

Capacity is a construction group, not mutable live tuning. Reserve one complete stream and result slot at action creation, including two non-pose slots for an unconditional all-window closure summary and terminal record. RecordsPerInstance bounds ordinary records; overflow closes windows, stops new motion and finishes with ReceiptOverflow through the reserved slots. A closure carries all active window IDs and reason with HasAcceptedPose=false; it precedes Terminal, including cancellation/support loss without a new pose. Terminal includes recovery outcome. No event is silently overwritten.

MaxRetainedInstances counts all reserved streams/results, including live actions; MaxConcurrentInstances cannot exceed it. Action Release/scope destruction may remove the entity, but recovery completion still appends its terminal record without touching the dead entity. A finished live entity remains queryable until Release/scope death even after acknowledgement. Reclaim a reservation after entity removal plus terminal acknowledgement. For a removed entity whose consumer never acknowledges, finite OrphanRetentionTimeout starts at terminal publication; expiry reclaims the stream and records a diagnosed abandoned result. Read/consume with its old identity returns StreamExpired, never empty-success. IDs are not reused within player lifetime. Consumers reconcile player/action identity and expiry before retaining window state. This is bounded orphan retention, not infinite delivery to a destroyed scope. Player/subject destruction ends all streams; no delivery claim crosses world teardown.

Polling LastReceipt is for presentation/diagnostics. Effects consumers drain after publication and process StreamSequence in order: an accepted interval before closure remains historical evidence, while closure bars intervals at/after its boundary. Closure does not retroactively erase accepted intervals. A consumer may explicitly reject late evidence under its own gameplay policy; the framework neither promises damage nor conceals that discard. Never execute a cached interval twice or keep using a window merely because Enter was once observed.

## 7. Low-level procedural callers

Create_Manual takes FCk_ProceduralAction_ControlSpec containing LimbIds, optional BodyChannel, captured AnchorBindings and InputTimeout. Return policy comes from admitted player tuning/locomotion settings. Manual control uses the same claims, support, solve and return path as clips; it does not start a clip clock.

Request_SubmitPoseBatch takes one complete batch for all claimed channels, an optional opaque correlation token and the standard completion delegate. The framework assigns sequence/generation; caller tokens never authorize writes. Before Ready, or while an earlier batch is unsettled, completion returns Failed with no coalescing. A partial batch rejects atomically. Positive service time without new accepted input beyond InputTimeout initiates return. Manual control cannot submit gameplay markers/windows; callers own those semantics.

`FCk_ProceduralAction_LimbTarget` leaves bend selection to the limb's authored solver pole by default (`HasBend=false`). `Set_BendDirection` explicitly enables a per-target bend override; `Clear_BendDirection` (or `Set_HasBend(false)` in AngelScript) restores the authored pole. The inactive direction value is retained for authoring but ignored during candidate validation and solve. An active override must be finite and nonzero. Blueprint Details exposes `HasBend` and enables `BendDirection` editing only while it is true.

Ready is retained while a manual batch is pending; Get_LastReceipt and the operation outcome expose completion without a transient state race. Request_End stops accepting batches, closes control, returns resources, then reports Completed. It is valid only for a manual instance; clips end by their timeline or Request_Cancel. InputTimeout is a failure result, not normal completion.

This supports code-driven pointing, reaching and procedural inspection without making callers reverse-engineer clip files. It must not create a second unguarded SetFootTarget escape hatch.

## 8. Diagnostics and defaults

`UCk_Utils_ProceduralAnimation_Debug_UE::Get_ActionSnapshot` and `Get_ControlSnapshot` return reflected value copies for native, Blueprint and AngelScript tools. Call on the game thread. Invalid, removed or wrong-feature handles return `Available=false`; inspecting a subject never reserves resources, loads content, queries the world or advances simulation. Copies are observations, not authority to submit poses or apply effects.

The action snapshot identifies the pinned content/tuning revisions, accepted time, backlog, pending publication (including a queued manual batch), and currently active watchdogs. Deadline elapsed/remaining values use the runtime's serviced clocks. The subject snapshot exposes settled revision, controller ownership, original return deadlines, support leases, committed contacts/reservations and proposed landing destinations. Root-owned recovery remains inspectable after its action entity disappears.

`SupportFresh` validates the support/root frame; it does not assert that contacts were solved this frame. A lease's `ProjectedRegion` is the uninset convex hull projected onto the support plane through the root proxy. Interpret it together with `Policy.ProjectedRegionInsetCm`, `MinimumEdgeDistanceCm` and `SupportResult`; the drawn hull alone is not a permitted takeoff region. Proposed landing targets and actual trusted contact evidence are separate fields. Recorded receipt poses retain their own stamp and must not be relabeled with current live diagnostics.

Errors identify subject/action/limb, controller owner, phase, field path and requested versus accepted pose. Expected contention is returned, not logged as a framework invariant failure. Malformed input uses the house ensure plus explicit safe rejection. Get_State/Get_Result remain available after error.

Default tuning is safe but visible. Missing required slot mapping is an error, never 'use the first leg'. Duplicate bindings fail the entire instance. Two slots mapped to one limb fail unless the compiler has proven those are the same single resource by explicit aliasing; release 1 does not support aliases.

The integration acceptance set covers:
- standalone articulated inspection arm with no gait;
- walking carrier reaching with one limb and replanting;
- body-only mechanical lever/lean action;
- authored two-limb gesture with body counter-motion.
These examples use no game enemy classes, damage tags or game-project imports. Consult PROGRESS for each example's executed evidence; this list is not a claim that every example has passed its final runtime gate.

## 9. Outcomes, likely causes and corrective actions

`FCk_ProceduralAction_Result` carries `Outcome`, `Recovery` and `Reason`; the terminal record and the managed completion also carry `FCk_ProceduralAction_FailureContext` (phase, claimed limb IDs, elapsed against limit for a missed deadline, observed against budget with a unit name). Only measured fields are filled: None, zero and empty mean "not measured". `UCk_Utils_ProceduralAction_UE::Get_ResultText(Result, Context)` renders both for a person; it formats on demand, renders an unknown reason as its identifier and is not meant for branching. Branch on `Outcome`, `Recovery` and, for managed delivery, `ECk_ProceduralAction_DeliveryStatus`.

The reasons below are the ones the runtime writes (checked against the `Action/`, `ActionPlayer/` and `LimbControl/` sources on 2026-10-12). Five kinds of failure read differently: rejected input (`Rejected`, `InvalidContent`, `AssetFailed`), unreachable or unsettled motion (`SubjectPoseRejected`, `PublicationFailed`, `PublicationTimeout`, `SamplingBudgetExceeded`), support loss (`SupportLost`), a delivery gap (managed `Delivery` `Gap` or `Expired`, explicit `ReadResult` `Gap` or `StreamExpired`), and a failed return (`Recovery` `Failed`, `RecoveryFailed`).

| Outcome | Reason(s) | Likely cause | Corrective action |
|---|---|---|---|
| `None` | none | No terminal was observed: the producer never finished before delivery ended (a managed completion with `Delivery` `PlayerLost` or `Expired`), or the action has not finished yet. | Read `Delivery`; never treat `None` as success. Keep the player alive for as long as the action's result matters. |
| `Completed` | none, or the reason the return reported | The clip reached its end, or `Request_End` ended a manual action, and the limbs returned. | Nothing. Check `Recovery`: `Succeeded` means every claimed limb is back under its default control. |
| `Rejected` | `ClipBindingsRejected` | A slot without a binding, a binding to a limb the subject lacks, two slots on one limb, a missing anchor binding, or more simultaneous windows or events than the player capacity allows. | `Get_AdmissionReport(Player, PlaySpec)` names the field; fix the bindings or raise `Capacity.MaxWindowsPerReceipt` / `MaxEventsPerReceipt`. |
| `Rejected` | `MirrorRejected` | A play-option mirror whose destination slot the clip already declares, whose source slot it does not declare or never tracks, whose source track is anchor-relative, or whose destination has no binding to another limb of the subject. | `Get_AdmissionReport` names the mirror (`Mirrors[i]`) and the code (`MirrorDestination`, `MirrorSource`, `MirrorSpace`, `MirrorBinding`, `MirrorPlane`, `ContentLimit`); fix the mirror list or its bindings. |
| `Rejected` | `Busy`, `UnavailableLimb`, `InvalidChannels`, `InvalidSubject`, `InvalidOwner`, `Faulted`, `AcquireFailed` | Admission found a claimed channel held by another action, a limb that is not available (detached, unplanted, faulted), or a subject that cannot be controlled. | Wait for or cancel the other action; recover unplanted limbs; recompose a faulted subject. Do not retry blindly every frame. |
| `Rejected` | `SupportUnavailable` | A borrowed walking limb would leave fewer trusted planted contacts than the support policy requires. | Borrow when the walker stands on trusted ground, lower the requirement deliberately, or borrow a different limb. |
| `Rejected` | `EntryCaptureRejected` | The subject's settled entry pose could not seed the clip's initial values (for example a captured body pose on a subject with no body presentation). | Compose a presentation for body clips; check the slots' initial sources against the subject. |
| `InvalidContent` | `InvalidClipContent` | The clip fails the compiler; `Validate_Data(Clip)` lists every diagnostic with its field path. | Fix the clip; the builder's reports use the same field paths. |
| `AssetFailed` | `AssetLoadFailed`, `LoadTimeout`, `StaleLoadGeneration`, `MissingLoadingState` | The soft clip path does not resolve, or loading outlived `Watchdogs.LoadTimeout` (the context carries elapsed and limit). | Fix the soft path, or preload the clip and use `Play_Loaded` / `Create_FromLoadedClip`. |
| `Cancelled` | the caller's cancel reason, or `Released` | `Request_Cancel`, or `Request_Release` of a live action. | Expected when you asked for it; read `Recovery` for the return. |
| `OwnerLost` | `OwnerLost` | The action's lifetime scope was destroyed. The player still returns the limbs and appends the terminal. | Keep scopes alive while the action matters; drain orphaned explicit streams through `Get_RetainedReceiptIdentities`. |
| `SubjectLost`, `PartLost` | none | Reserved: no current runtime path writes these. Destroying the player or subject ends managed delivery with `PlayerLost`; losing a bound part faults the subject. | Treat as a failure if seen; read `Get_Health` / `Get_Fault` on the subject. |
| `SupportLost` | `SupportLost`, `SupportHoldTimeout`, `SupportEmergencyRequested` | While a walking limb was borrowed, the other trusted contacts dropped below the policy, or the gait needed the limb back. | Expected on unstable ground. Require more contacts with `Make_CountOnlySupport`, or keep the action shorter. |
| `AcquisitionTimeout` | `AcquisitionTimeout` | Claims or the first settled entry did not complete within `Watchdogs.AcquisitionTimeout` (context: elapsed, limit). | Look for a held channel or a subject that never settles; raise the watchdog only when acquisition is legitimately slow. |
| `PublicationTimeout` | `PublicationTimeout`, `ClipPublicationTimeout` | A staged pose was not settled and published in time (context: elapsed, limit). | Usually unreachable or blocked targets: check the endpoint errors in the last receipt and the limb's reach. |
| `PublicationFailed` | `SettlementFailed`, `HoldSettlementFailed`, `ReceiptRejected` | The solver did not settle the staged pose, or the receipt store refused a record. | Inspect `Get_LastReceipt` endpoint errors and chain state; check the player capacity. |
| `SubjectPoseRejected` | `SubjectPoseRejected`, `ClipHoldRejected`, `HoldCandidateInvalid`, `ReturnReattachmentDiscontinuity`, or the subject's interruption reason | A requested pose the subject cannot hold: non-rigid or unreachable targets under `RejectUnreachable`, a body offset outside admission, or a reattachment discontinuity. | Keep targets within reach or author `ProjectToReach`; keep body offsets rigid and bounded. |
| `SamplingBudgetExceeded` | `SamplingBudgetExceeded`, `SettledMotionExceededSamplingLimits` | One accepted step moved an endpoint or a segment farther than the tuned sampling budget (context: observed and budget in micrometres or millidegrees when measured). | Slow the clip (`PlayRate`, longer phases) or raise the `Tuning.Sampling` limits deliberately. |
| `ReceiptOverflow` | `ReceiptRejected` | Explicit delivery whose consumer did not acknowledge records fast enough (context: pending records against `RecordsPerInstance` at the per-action limit). | Drain and `Request_ConsumeReceipts` every frame, raise `Capacity.RecordsPerInstance`, or use managed delivery. |
| `InputTimeout` | `InputTimeout` | A manual action received no new accepted input within `InputTimeout` (context: elapsed, limit). | Submit pose batches continuously while the action should hold, or end it with `Request_End`. |
| `RecoveryFailed` | the return's reason: `ReturnTimeout`, `ReturnSettlementTimeout`, `ReturnSettlementFailed`, `ReturnProbeAttemptsExhausted`, `ReturnPreparationFailed`, `ReturnHandoffStale`, `ReturnReattachmentFailed`, `ReturnReattachmentEndpointChanged` | The motion completed but a limb could not return: no trusted ground within the return budget, or the return could not settle. The context carries elapsed and limit only when a deadline was missed. | The limb stays `Unplanted` (`Get_UnplantedLimbIds`). Restore usable ground, then `Request_RecoverUnplantedLimbs`. Any other outcome with `Recovery` `Failed` means the same for its limbs. |

## 10. Pose builder

`UCk_Utils_ProceduralActionBuilder_UE` (AngelScript `utils_procedural_action_builder`, header `ActionBuilder/CkProceduralActionBuilder_Utils.h`) edits a value-only `FCk_ProceduralAction_ClipDraft` (the clip's body channel, slots, phases, markers and windows). Every mutation takes the draft by reference, validates every input before writing and returns `FCk_ProceduralAction_ValidationReport`; an `Invalid` report leaves the draft unchanged and names the field with the compiler's field paths. Nothing is sorted, clamped or invented.

| Function | Use |
|---|---|
| `Make_FromRecipe(Recipe, OutDraft)` | Starting data: `Blank` (one Motion phase), `BodyLean` (Prepare, Execute and Recover body keys), `Reach` and `Sweep` (one slot). Limb recipes want `LimbRestNormalized`, or an explicit rest target; the report warns otherwise. |
| `Make_Draft_FromData(Clip, OutDraft)` | Copies an existing clip for editing. |
| `Add_Phase`, `Add_Slot`, `Set_BodyChannel`, `Set_SlotFrame`, `Set_PhaseProgress` | Structure, the per-slot coordinate frame, and a phase's progress curve (`Preset` easing, or a monotone `CustomCurve` with `Linear` easing). |
| `Add_BodyKey`, `Add_LimbKey`, `Add_PoseKey`, `Replace_PoseKey` | Keys. A pose key is atomic over the body and every listed limb; `Replace_PoseKey` requires existing keys at that time. |
| `Add_Marker`, `Add_Window` | Events on the elapsed phase timeline; windows are half-open. |
| `Mirror_Slot(Draft, MirrorSpec)` | Writes a reflected copy of a declared source slot's tracks into a declared destination slot (body-local and root-relative frames only; rest-normalized and anchor-relative frames need a subject and are rejected here). |
| `Build_Into(Draft, Clip)` | Compiles the draft with the clip compiler and, only when it compiles, writes the five fields into a caller-owned clip. Not Play and not an editor transaction. |
| `Write_Into(Draft, Clip)` | Writes the five fields without compiling, for incremental authoring: a clip still missing keys can be saved. Rejects only a null clip or an unknown compatibility version. Play, the `Create_*` functions and `Build_Into` still compile and refuse such a clip. |

The clip object is caller-owned and must stay referenced while it is built and played. Build per input revision, not per frame. `ReflectVector`, `ReflectRotation` (`q' = (w, -S v)`) and `ComputeCatmullRomTangents` in `ck::procedural_action` are the shared pure helpers.

**Runtime mirrors.** `FCk_ProceduralAction_PlayOptions::_Mirrors` (on both `PlaySpec` and `LoadedPlaySpec`) mirrors at play time instead of in the draft. Each `FCk_ProceduralAction_MirrorSpec` adds its destination slot to the play's compiled copy before slots are bound, on the loaded path, the soft-load path and in `Get_AdmissionReport` alike. Positions, tangents and bend guides are reflected across the plane in the body frame, orientations as `(w, -S v)`. Rest-normalized tracks are converted through both bound limbs' rest frames and chain lengths, so the destination stays rest-normalized in its own limb's frame and its endpoints mirror the source's even when the two limbs differ. Body motion, markers and windows are never mirrored or duplicated. The destination slot needs its own binding to a limb other than the source's; a rejected mirror finishes the action `Rejected` with reason `MirrorRejected`. At most 64 mirrors, and at most 64 slots after expansion.

## 11. Managed playback

`Play(Scope, Player, PlaySpec, OnCompleted)` and `Play_Loaded(Scope, Player, LoadedPlaySpec, OnCompleted)` return `FCk_ProceduralAction_ManagedStart` (action handle, identity, start report). The framework drains and acknowledges the identity's receipt stream, fires `OnCompleted` once with `FCk_ProceduralAction_ManagedCompletion` (identity, producer `Result`, `Delivery`, failure `Context`), then releases the action. No callback fires inside the call; a clip rejected while `Play_Loaded` runs completes on a later frame.

- **Start report.** `Valid` whenever an action was created. `Invalid`, with a `StartRejected` diagnostic, an invalid action and no identity, when nothing was created; no completion follows.
- **Delivery status.** `Complete`: the terminal and every page before it were delivered. `Expired`: the stream was reclaimed before delivery finished. `PlayerLost`: the player was destroyed first; fired once per uncompleted identity from the player's end-of-play pass, with Outcome `None`, after that pass requested destruction of the identity's live action. Managed delivery is the stream's only acknowledging reader, so pages never carry a gap; records lost to capacity surface as the producer outcome `ReceiptOverflow`.
- **Sealed identity.** The seal is on the receipt stream, set when the identity is reserved. `Get_IsManagedDelivery(Player, Identity)` is true from the call's return for as long as the stream is retained, including inside and after the completion callback until the released stream is reclaimed. `Request_ConsumeReceipts` on the identity ensures and completes `Failed_NotEnqueued` without moving the cursor. `Read_Receipts` stays an ordinary non-consuming read for every identity, so the gameplay debugger keeps recording managed streams.
- **Listeners.** `BindTo_OnManagedRecords` (copied pages, contiguous and in order) and `BindTo_OnManagedAcceptedPose` (accepted receipts only), with matching `UnbindFrom_*`. At most 8 of each kind per identity; a duplicate bind is a no-op; binding to an unknown, explicit or completed identity ensures. A bind made after pages were delivered sees only later pages. A listener may call `Play`, cancel, release or destroy from inside its callback; the next dispatch pass sees the result.
- **Budget.** `Capacity.ManagedRecordsPerTick` (default 32) bounds the records dispatched per player per tick, shared fairly across identities.
- **When not to use it.** Managed notifications support presentation and noncritical effects. Damage that must see every accepted interval uses explicit delivery (`Create_*` with `Read_Receipts` and `Request_ConsumeReceipts`). `BindTo_OnFinished` remains the producer's completion and may precede the managed completion.

## 12. Player presets

On `UCk_Utils_ProceduralActionPlayer_UE`; each returns an ordinary `FCk_ProceduralActionPlayer_Spec` to pass to `Add`.

| Function | Result |
|---|---|
| `Make_DedicatedPlayerSpec(Tuning)` | Dedicated appendages; walking-limb borrowing stays disabled. |
| `Make_BorrowingPlayerSpec(FCk_ProceduralActionPlayer_BorrowingSpec{Support, Return, Tuning})` | Walking-limb borrowing with an explicit support policy and return. |
| `Make_CountOnlySupport(MinimumTrustedContacts)` | A `CountOnly` support that counts trusted planted contacts, not visually standing legs. A value outside 1..64 ensures and is returned unchanged, so `Add` rejects it. |

No preset guarantees a reachable plant on unknown terrain. Capacity stays a separate construction group.

## 13. Subject queries and recovery

On `UCk_Utils_ProceduralArticulation_UE`: `Get_IsAnyLimbControlled` (currently action-controlled; a returning limb is not controlled), `Get_IsAnyLimbReturning`, `Get_UnplantedLimbIds` (in `Get_Limbs` order), and `Request_RecoverUnplantedLimbs(Request, OnCompleted)`, which gathers the limbs that are unplanted when the request is processed: an empty set completes `Succeeded` without claims, an incompatible state completes `Failed`. Recover only after usable ground is back. An admitted recovery is not a replanted limb: wait for the limb's control state to return to `Default` with trusted support.
