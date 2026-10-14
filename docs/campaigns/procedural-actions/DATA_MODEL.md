# Authoring data, tunables, visuals and runtime storage

This document records current authoring and runtime ownership contracts. Concrete public types are defined in `Source/CkProceduralAnimation/Public/CkProceduralAnimation/{Articulation,Limb,LimbControl,Action,ActionPlayer}`; consult those headers for callable fields. Persistence and future adapter notes below remain design boundaries, not additional shipped types.

## 1. Asset boundaries

| Asset/type | Contains | Must not contain |
|---|---|---|
| FCk_ProceduralArticulation_Layout / FCk_ProceduralLimb_Layout | Named limb topology, attachment/rest frames, segment lengths, conservative clearance geometry and limb role | Live handles, meshes/materials, attack rules, mutable clocks |
| UCk_ProceduralGait_Data (existing) | Walking timing, steps, probes and foothold tuning | Action clips, borrowing support/return policy, rendered materials |
| UCk_ProceduralAction_Data | Ordered phases, body/limb tracks, named point markers and windows, required symbolic slots, track reach policy | Entity references, damage, target search, sound assets, montage state |
| FCk_ProceduralAction_Tuning | Playback rate, blend/return timing, finite watchdogs, sampling budgets (which also bound limb returns) | Geometry changes, mesh offsets, arbitrary phase overrides |
| Editor preview appearance/content | Meshes/materials and mesh-local correction per part, test surface fixture and scene defaults | Shipping execution state or mandatory runtime dependency |
| Consumer presentation definition | Render mesh/material choices, optional audio/VFX reaction to named markers | Framework-owned solver state |
| FCk_ProceduralArticulation_Spec | Construction inputs: layout values, live bindings, clearance query settings, presentation handle optional until a body action is requested | Duplicated runtime clocks or durable asset handle graph |
| FCk_ProceduralActionPlayer_Spec | Tuning (the only tuning its actions play with), Capacity and Locomotion groups; subject supplied once as Add argument | Whole game behavior configuration |
| FCk_ProceduralAction_PlaySpec | Clip soft reference (resident or not), named slot bindings, delivery (Explicit or Auto) | Tuning, externally writable sample ID or forced claim stealing |

A Data asset is reusable authoring content. A Spec is a one-time composition payload. Settings are cohesive value groups inside these payloads. None is a catch-all 'config'.

Visual mesh extent is not chain length. Collision/clearance radii are authored simulation geometry; visuals may display mismatches and validation warnings, but the runtime does not infer kinematic constraints from a mesh's render bounds.

## 2. Articulation schema

`FCk_ProceduralLimb_Layout` is topology, not clip content:
- Id: nonempty FName, unique within subject; stable after composition.
- Attachment: FTransform in root/body-local cm, normalized rotation, unit scale.
- Geometry: ordered TArray<float> segment lengths, 1–8, finite positive cm.
- Rest: `_RestEndpointLocal` and `_BendLocal` in the attachment frame. Nondegenerate bend when required. A clip slot starts from the limb's captured accepted pose or from this rest (`FCk_ProceduralAction_Slot::_InitialSource`).
- Solver: existing ECk_ProceduralRig_ChainSolver.
- Clearance: `_Clearance` and `_SegmentClearanceRadii` on each limb; `_BodyEnvelopeMin`/`_BodyEnvelopeMax` on the subject layout; `_ClearanceSettings` (query mode and optional Jolt filter) on the articulation spec.
- Role: `_Role` is Dedicated or Locomotor on the limb layout, not a property of an action track. Existing leg/gait fragments remain authoritative for locomotor phase, contact and foot placement.
- Capabilities: immutable supported target channels. Current solvers accept endpoint position and bend guidance; terminal orientation is presentation capability, not a guarantee of arbitrary joint torsion.

FCk_ProceduralArticulation_Layout:
- Limbs: ordered values, max 64. No topology reorder at runtime.
- BodyEnvelope: explicit body-local conservative bounds for generic clearance. Legacy facade computes the equivalent old envelope; no dummy gait needed.
- CompatibilityVersion: serialized schema version, separate from content hash.

FCk_ProceduralLimb_Binding:
- LimbId, ordered segment transform handles, optional terminal transform.
- Each handle unique across entire subject; every part a direct root lifetime child; no other writer.
- Root and kinematic frames require unit scale. Bound visual part transforms may retain finite nonzero scale; it does not change chain lengths or accepted endpoint kinematic frames.

Binding contains no authored lengths; topology is their sole home. A binding mismatch reports the exact limb, expected count and supplied count.

Presentation is conditionally required at admission for any body-channel action, even if the subject has zero limbs. A bodyless layout may omit it for limb-only actions. `FCk_ProceduralArticulation_Spec` carries layout values, live bindings, optional presentation and clearance query settings directly; no `UCk_ProceduralArticulation_Data` type is currently declared.

## 3. Clip schema

UCk_ProceduralAction_Data contains Phases, required Slots, Markers and Windows once. Each Slot declares its Id, its InitialSource (CapturedAcceptedPose, or AuthoredRest: the limb's rest endpoint from its topology) and its Space, the frame every track of the slot is keyed in for the whole clip. The optional BodyChannel group declares InitialSource (CapturedAcceptedPose or IdentityOffset). InitialSource governs entry/missing-first-channel resolution, never an implicit world-origin key. Rest is authored in topology; body identity means neutral offset on the accepted environmental base.

FCk_ProceduralAction_Phase:
- Id: unique FName.
- Duration: FCk_Time > 0.
- BodyKeys: unit-scale rigid transform keys when the asset's `BodyChannel` is enabled.
- LimbTracks: array keyed by symbolic Slot (FName), unique per phase.
- Markers and Windows are clip-global tables, not duplicated inside phases.

`FCk_ProceduralAction_LimbTrack` is clip motion, not topology:
- Slot: symbolic required resource, mapped once at admission to LimbId.
- Frame: the slot's `_Space`, BodyLocal / RootAtStart / LimbRestNormalized.
- PositionKeys: typed normalized time and FVector values; values' units set by the slot's Space.
- OrientationKeys: normalized quaternion keys for optional terminal orientation.
- BendKeys: nonzero normalized bend directions; no silent zero-vector axis invention.
- ReachPolicy: RejectUnreachable or ProjectToReach; default RejectUnreachable.
- Clearance: a sample whose chain crosses a solid is always rejected. Clearance is never implied to be exact CCD.

`FCk_ProceduralAction_TransformKey` supplies body keys; limb positions/bends use `FCk_ProceduralAction_VectorKey` and optional terminal orientation uses `FCk_ProceduralAction_OrientationKey`:
- Time: FCk_FloatRange_0to1; keys strictly increasing, the last at 1. A body, position or orientation track may leave out its key at 0: the phase then moves that channel from its value when the phase starts (the slot's or body's initial pose in the first phase it moves, else where the previous phase left it). A bend track keeps its key at 0, since a limb can enter a phase with no bend direction to move from.
- Translation (cm), Rotation (normalized quaternion); scale fixed to one by type/surface.
- Interpolation: Linear or Smooth (bounded cubic Hermite translation with explicit tangents). Quaternion shortest-arc slerp; sign canonicalization at compile.
- No Euler XYZ curve ambiguities or implicit extrapolation.
- Phase progress is one preset easing (`ECk_ProceduralAction_Easing`), a monotonic set: Linear, InQuad, OutQuad, InOutQuad, InCubic, OutCubic, InOutCubic. Back/Elastic/Bounce are not offered, because overshoot invalidates simple reach/sampling assumptions.

Sparse channels hold the previous authored target inside a phase; a channel with no key at the start of the first phase that moves it starts from its initial source. No implicit world origin. Phases missing a previously claimed slot retain that slot and hold its endpoint until return. The compiled resource union is fixed for the complete action.

The slot holds its tracks' frame, so a slot's Space is constant for the whole clip and survives phases with no track; held coordinates are never reinterpreted in a new frame. A later explicit frame-transition feature must transform the held pose/tangents and define continuity; it cannot be inferred from changing an enum.

Markers: unique Id, phase Id, normalized time. Windows: unique Id, start and end references, strictly positive interval. No numeric 'damage' field. Half-open [start,end); end-of-clip closes all windows. At equal timestamps process exits, point markers, then enters in stable authored order. Compile detects invalid phase IDs, out-of-order boundaries, duplicate IDs and invalid overlaps of a window with itself.

No opaque per-marker callbacks or arbitrary UObject payloads. Consumers map FName IDs to their own typed behavior. Future marker payloads require a separate compatibility and binding design.

## 4. Tuning groups

| Group | Fields / behavior |
|---|---|
| Playback | positive finite PlayRate; complete clip consumes one clock |
| Transition | EntryBlend FCk_Time; DedicatedReturn FCk_Time; ReturnTimeout FCk_Time |
| Watchdogs | LoadTimeout, AcquisitionTimeout, PublicationTimeout, MaxTimeBacklog; all finite positive |
| Sampling | MaxAdvance FCk_Time, MaxEndpointTravel cm, MaxSegmentAngularStep degrees, MaxSamples positive int; travel and angular step also bound every limb return |
| Locomotion.Support (`FCk_ProceduralLimbControl_Locomotion` in Player Spec) | Mode, MinimumTrustedContacts, ProjectedRegionInsetCm, MaxContactNormalSpreadDegrees, SupportHoldTimeout |
| Locomotion.Return (same construction group) | Duration, Timeout, MaxProbeAttempts, RetryInterval, ContactToleranceCm; gait retains foothold probing/reservation authority |
| Capacity (player Spec/Params, immutable) | MaxConcurrentInstances, RecordsPerInstance; the rest of the receipt budget is derived |

Current defaults are bounded starting values, not measured optimal values: PlayRate 1; EntryBlend 0.1 s; DedicatedReturn 0.2 s; ReturnTimeout 1.0 s; LoadTimeout 5 s; AcquisitionTimeout 1 s; PublicationTimeout 1 s; MaxTimeBacklog 0.5 s; MaxAdvance 1/120 s; MaxEndpointTravel 2 cm; MaxSegmentAngularStep 5 degrees; MaxSamples 16. The Support group defaults to Disabled, so borrowing a walking limb requires explicit policy authoring.

Capacity has two knobs, MaxConcurrentInstances (1..64, default 4) and RecordsPerInstance (1..4096, default 64); live unread instances are never silently evicted. These are visible bounded defaults, not measured optimal values. SupportHoldTimeout is required with the borrowing support policy. Service-clock deadlines and original return policy are captured once when their state begins; duplicate cancel/release cannot restart them.

The store derives the rest (`Get_MaxRetainedInstances`, `Get_MaxRetainedRecords`, `Get_MaxRetainedPoseTransforms` in `Core/CkProceduralActionReceiptStore.h`): 4 retained streams per concurrent action, at most 256 (16 by default); RecordsPerInstance + 2 records per retained stream, at most 1049088 (1056 by default); 32 pose transforms per ordinary record, at most 1048576 (32768 by default). A receipt carries at most 64 samples, 64 events and 32 windows, and a finished stream whose action is gone is retained 2 s after terminal publication. These are exact-count limits, not estimated byte budgets. The record budget is always at least three times MaxConcurrentInstances: two reserved closure/terminal slots and room for the first ordinary pose per concurrently reserved action. Further poses and retained terminal streams share the global budget, so consumers must drain their streams. Count root/body/requested endpoint/accepted endpoint/segment transforms across previous, current and intermediate poses before copying, using checked wide arithmetic. Get_LastReceipt reads the newest queued accepted record, or one retained consumed snapshot only when no newer queued receipt exists; that snapshot counts against the same transform budget.

`FCk_ProceduralLimbControl_Locomotion` is the construction group carrying Support and Return. Player Add copies it into immutable root `FFragment_ProceduralLimbControl_Params`; Gait keeps the policy copy belonging to each admitted protection lease. This avoids mixing action support settings into ordinary gait presets or motion clips. Dedicated return uses the action's pinned transition tuning; locomotor recovery uses the adapter's captured return policy, its motion held to the player's Tuning.Sampling. Disabled support is the default: an action that binds a walking limb is rejected at admission (SupportUnavailable) until borrowing is explicitly authored.

Units stay typed: FCk_Time in reflected/runtime fields, ranges for normalized time. Integers for counts; explicitly suffixed cm/degrees for geometric tolerances where no existing domain wrapper is suitable. Sampling values express budgets/tolerances, not performance claims.

## 5. Compile and validation pipeline

1. Asset editor/cook validation: syntax, numeric finiteness, key ordering, identifiers, phase/window graph, scalar units.
2. Runtime load: root complete dependencies; verify schema and content again in packaged builds.
3. Compile immutable value data: resolved phase offsets, resource union, channel spans, conservative authored curve extrema, stable marker order, content hash.
4. Bind: map symbolic slots to limbs; validate capabilities, topology version and lengths; static reach checks where meaningful.
5. Admit: world-dependent liveness, support, ownership, reservations.
6. Solve/publish: validate actual outputs; requested validity never substitutes for accepted pose evidence.

Report includes Severity, Code, AssetPath, FieldPath, PhaseId, Slot, KeyIndex, expected condition and actual value. Editor validation returns the full list; runtime malformed admission diagnoses and rejects atomically. Expected Busy/SupportUnavailable are typed runtime results, not assertion spam.

No normalization that silently repairs malformed authoring. Explicit editor 'Normalize quaternion'/'Fix endpoint key' operations are undoable. Version mismatch never falls back to old semantics. A failed recompile does not replace the last valid compiled revision; the asset is shown invalid and new play refuses the invalid source.

## 6. Runtime homes: one datum, one owner

| Fragment | Stored values | Why retained |
|---|---|---|
| FFragment_ProceduralArticulation_Params | layout value and clearance query settings; legacy walker geometry remains in its existing Leg/Rig fragments | every solve |
| FFragment_ProceduralArticulation_Bindings | live part handles and identity mapping | validation/publication |
| FFragment_ProceduralArticulation_Claims | controller token per resource, claim revision and bounded publication tickets | sole ownership authority |
| FFragment_ProceduralArticulation_Pose | staged and last-settled subject poses, expected/observed part transforms and issue/settlement revisions | one coherent solve/publication |
| FFragment_ProceduralArticulation_Health | Operational/Faulted, actual observed partial pose and offending bindings | fail-stop publication integrity |
| FFragment_ProceduralLimbControl_Params | immutable installed support and return policy | admission and recovery defaults |
| FFragment_ProceduralLimbControl | per-token controllers, captured return poses/policies, pending stage key, clocks, retries, reattachment and terminal reason | subject-owned transition authority surviving action entity |
| FFragment_ProceduralActionPlayer_Params | immutable Capacity and Tuning values | admission, bounded storage and every action's tuning |
| FFragment_ProceduralActionPlayer | bounded receipt store, action handles and pending recovery identities | survives action entity while player lives |
| FFragment_ProceduralAction_Params | captured player, identity, clip source, slot bindings and tuning | immutable per action |
| FFragment_ProceduralAction_Compiled | immutable compiled clip pointer and bound limb IDs | repeated evaluation after load |
| FFragment_ProceduralAction | state/result, entry/observed pose, generation, clocks, pending publication and accepted samples | primary mutable state |
| FFragment_ProceduralAction_Loading | rooted asset batch and generation | exists only during load/compile |
| FCk_ProceduralAction_Result inside FFragment_ProceduralAction | terminal outcome/reason and recovery status | level-triggered result |
| FFragment_*_Requests / _Signals / _Debug | conventional mailbox, subscriptions, diagnostics | standard purpose |

This is a semantic split, not a demand for one fragment per scalar. Consolidate fields with identical lifetime/access only; never duplicate clocks or asset tuning inside mutable state and Params. Each fragment constructor accepts its own values, never a Spec. Factory owns the one mapping. Read-only compiled data can be shared as immutable native value ownership within the subject/player; no unbounded process-global cache.

Claims owns resource owner/generation and publication tickets; LimbControl owns subject-local transition state. ActionPlayer's receipt store owns copied output. Detached action recovery stores an identity/token and does not require a live action handle.

## 7. Retention and persistence

Compiled clips are immutable value objects with explicit shared ownership; gameplay entities hold native references, not mutable DataAsset pointers. New compilation creates a new revision. No cross-registry live handle capture.

All controller claims, contacts, compiled binding caches, receipts and active action state are transient. If the framework's posture mechanism requires marking new fragments, declare transient through existing policy and test it; do not quietly omit required registration. Asset authoring content persists normally. Loading a game reconstructs subject topology and neutral control; active action resume requires a separately designed host save adapter.

Networking must never replicate local FCk_Handle values. A future adapter may transmit stable subject/slot IDs, clip identity/revision, action generation and timing, while authority validates contacts/effects. That seam does not make this release multiplayer-ready.

