# Phase 1 — Runtime lifecycle blockers

One executor session. Runtime module only (`Source/CkProceduralAnimation`) plus CkTests rows.

## Entry criteria

- CkFoundation working tree clean (`git status --short` under `Plugins/CkFoundation` prints nothing);
  Phase 0 (maintainer commit) done. If dirty → STOP, record in PROGRESS.md.
- `git log -1 --format=%h` is the Phase-0 commit recorded in PROGRESS.md.
- Baseline recorded in PROGRESS.md: run
  `./CkAuto/UnrealToolbox.exe --build --target=Editor --test --discover-fresh --test-pattern ProceduralAnimation.Action --project="D:/Repositories/CkRepos/Orion"`
  and paste `Total/Passed/Failed` and the failing names. Expected: every failure is on the known-reds
  list (the toolbox prints `NEW FAILURES` otherwise). If `NEW FAILURES` is non-empty → STOP.

## Step 1 — Write the red tests first

Location: `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/` (C++ PIE rows,
pattern in `Gait/Test_ProceduralGait_RecoveryPie.cpp`: `FCk_Latent_StartPIEMultiClient(1, "/Engine/Maps/Entry")`,
`DoAddFloor`, `DoAddWalker`, `DoPlayLift`). New file `Test_ProceduralAction_Lifecycle.cpp`, rows:

| Row | Arrange | Assert (must be RED on entry) |
|---|---|---|
| `Ck.ProceduralAnimation.Action.Lifecycle.DetachedClaimedLegReleasesItsClaim` | walker, player with `CountOnly` support 2, `DoPlayLift(3.0 s)`; wait `Playing && Get_LastReceipt().Get_HasAcceptedPose()`; `Request_Detach` the claimed leg (`LegId`); then **destroy the leg entity** on the next frame (`Request_DestroyEntity`) | within 60 frames: `Get_Health == Operational`; `GetClaimant(Subject, LegId).Get_InstanceId() == 0`; action reaches `Finished`; settled revision advanced ≥3 past the revision at detach |
| `...Lifecycle.TransientFaultRearms` | walker + player; after a settle, write a bound segment's transform from the test (`UCk_Utils_Transform_UE::Request_SetTransform` on `Bindings[0].Get_Segments()[0]`) for one frame | next Update reports `SettlementMismatch` once (`Get_Fault`), and within 5 frames `Get_Health == Operational` again |
| `...Lifecycle.ClearFaultRequestRecovers` | force a structural fault (`InvalidBinding`: destroy a bound segment entity while idle) | `Get_Health == Faulted`; `Request_ClearFault` completes `Succeeded`; health `Operational`; next publish does not re-fault (the dead binding is now skipped) |
| `...Lifecycle.CapacityExhaustionEnsures` | player `Capacity.RecordsPerInstance` minimal; create actions with `Delivery::Explicit` and never acknowledge until `Create_FromClip` returns invalid | the invalid return is accompanied by exactly one ensure (use the `EnsuresBefore` counting pattern from `CkAutoTest_ProceduralAnimation_LegAndRigAdmissionRejects.as` or `AddExpectedError`) |
| `...Lifecycle.ExplicitTerminalExpiresUnacknowledged` | `Delivery::Explicit`, let the action finish, never consume | after `receipt_limits::UnacknowledgedTerminalRetention` the stream is released and a new `Create_FromClip` succeeds |
| `...Lifecycle.PlayerEndPlayFinishesActions` | create the player on a child entity of the scope; play; destroy the player's entity | the action reaches `Finished` with `Outcome::SubjectLost`; `OnFinished` fired once |
| `...Lifecycle.CancelWhileCancellingSucceeds` | play; `Request_Cancel` twice in one frame | both completions `Succeeded`; one terminal record |
| `Ck.ProceduralAnimation.Action.Compile.PhaseBoundaryDiscontinuityRejected` (unit, no PIE; put in `Action/Test_ProceduralAction_Clip.cpp`) | two phases; phase 2 body key at 0 differs from phase 1's key at 1 | `Compile` fails with code `PhaseBoundary` naming phase 2 and channel `Body`; the same clip with equal boundary keys compiles |

Timing that makes the DetachedClaimedLeg row red (learned the hard way during planning): the fault
fires on the first articulation publish **while the claim is still held**, i.e. while the action is
`Playing`. A row that waits for `Finished` before asserting passes without the fix, because the
return releases the claim first and a released index is never held. Assert `Get_Health` 2–5 frames
after the detach, before any return begins; mirror the host sever: `Request_Detach` with parts
`TransferToLeg` and leg ownership `TransferToWorld` (the leg entity survives, `Get_IsLive` is false),
then a second variant that also destroys the leg entity.

Gate 1: build + run `--test-pattern ProceduralAnimation.Action.Lifecycle` and `.Compile.PhaseBoundary`.
Expected: all new rows **Failed**. If any new row passes on entry → it asserts nothing; fix the row
before continuing. If the DetachedClaimedLeg row cannot be made red after one revision → STOP,
record the exact observed health/claimant values per frame in PROGRESS.md.

## Step 2 — Claims reconcile against limb liveness

`Articulation/CkProceduralArticulation_Fragment.h`, `FFragment_ProceduralArticulation_Claims`: add

```cpp
// Tokens whose claimed limb died since their claims were last released; read by the controller's
// interruption query and cleared by ReleaseClaims.
TArray<FCk_ArticulationControlToken> _LostLimbOwners;
```

`Private/.../CkProceduralArticulation_Control.h` (moves to `Public/` in Phase 2; edit in place now):

```cpp
// Releases every claim whose limb is no longer live and records the owning tokens as lost.
// Runs at the top of FProcessor_ProceduralArticulation_Update::ForEachEntity, before any hold.
static auto
    ReleaseDeadClaims(
        FCk_Handle_ProceduralArticulation& InSubject)
    -> void;
```

Liveness per binding index `i`: `Bindings.Get_Limbs()[i]` cast with `UCk_Utils_ProceduralLeg_UE::Cast`;
live iff `procedural_leg::Get_IsLive(Leg)` (for a standalone limb with no leg feature: live iff
`ck::IsValid(limb) && NOT Has<FTag_DestroyEntity_Initiate>`). Dead → `_LimbOwners[i] = {}` and
`_LostLimbOwners.AddUnique(token)`. The body owner never dies this way.

`LimbControl/CkProceduralLimbControl_Control.cpp`, `Get_InterruptionReason`: return
`FName{TEXT("LimbUnavailable")}` (the existing reason at :925) when the token is in
`_LostLimbOwners`. The service's existing path (`Service.cpp:621-638`) then closes windows, begins the
return and commits `Cancelling`; no service change. `ReleaseClaims` removes the token from
`_LostLimbOwners`.

Delete the working-tree patch in `HoldClaimedChannels` (`Processor.cpp:114-122`, the block that
casts the leg and `continue`s); the `_LimbOwners[i] == {}` check at :112 already skips released
bindings.

Fence: do NOT make `HoldClaimedChannels` tolerate a missing settled limb for a *claimed* index; a
claimed index with no pose is a genuine `InvalidBinding`.

## Step 3 — Faulted is no longer a brick

`Articulation_Fragment_Data.h`: no enum changes. `Processor.cpp` Update (:91):

```cpp
// A transient fault is the other writer's frame, not a broken rig: re-arm and republish.
const auto TransientFault = InHealth._State == ECk_ProceduralArticulation_Health::Faulted
    && (InHealth._Fault == ECk_ProceduralArticulation_Fault::SettlementMismatch
        || InHealth._Fault == ECk_ProceduralArticulation_Fault::SettlementTimeout);
if (TransientFault)
{ InHealth.Clear(); }   // new member: _State = Operational, _Fault = None, offenders reset
if (InHealth._State == ECk_ProceduralArticulation_Health::Faulted)
{ return; }
```

Add to `Articulation/CkProceduralArticulation_Fragment_Data.h`:

```cpp
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralArticulation_ClearFault : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_ProceduralArticulation_ClearFault);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralArticulation_ClearFault);
};
```

`Articulation_Utils.h`, modelled on `Request_RecoverUnplantedLimbs` (:129) but an **immediate mutator**
(root doctrine "Immediate mutators"): validates the handle, calls `Health.Clear()`, fires
`InDelegate.ExecuteIfBound(InArticulation, Succeeded)`. Structural faults (`InvalidBinding`,
`PartLost`, …) stay until this request; the next Update republishes with dead bindings skipped
(Step 2 makes that true). Bind in AS via the generated `utils_procedural_articulation::Request_ClearFault`
(regenerate; do not hand-edit `Script/Generated`).

## Step 4 — Capacity ensures; `Auto` is the default; Explicit terminals expire

- `Action/CkProceduralAction_Utils.cpp:59` → `CK_ENSURE_IF_NOT(Reserved == Accepted, TEXT("ProceduralAction Create rejected: player [{}] has no receipt capacity left ([{}] retained instances)."), InPlayer, Count) { return {}; }`.
- `Action/CkProceduralAction_Fragment_Data.h:317` → `_Delivery = ECk_ProceduralAction_Delivery::Auto;`.
  Update the 18 `Delivery` sites in CkTests/CkFoundation scripts only where a row *relies* on the old
  default (a row that sets `Explicit` explicitly needs nothing). The host already sets `Explicit`
  (`Mars_Crawler_ActionAdapter.as:152`).
- `Core/CkProceduralActionReceiptStore.h`: `constexpr auto UnacknowledgedTerminalRetention = FCk_Time{5.0};`
  next to `OrphanRetention` (:25). In the expiry pass (:718) also expire a stream whose `Terminal`
  is set, is **not** `EntityRemoved`, and `_ServiceTime - TerminalServiceTime >= UnacknowledgedTerminalRetention`;
  log once per stream at `Warning` via `ck::procedural_animation::Warning` naming the identity.
- Fix the two forbidden ensure shapes in `Action_Utils.cpp` (:324-333, :350-359) and
  `Articulation_Utils.cpp:209-216`: the recovery goes inside the ensure body; delete the dead `if`.
- `Service.cpp:484,493`: replace `UE_LOG … %s` with the module log function and `{}`.

## Step 5 — Ordering, player teardown, result semantics, compiler

- `ActionPlayer/CkProceduralActionPlayer_Processor.h:84-87`: add `FProcessor_ProceduralRig_Update` to
  the Service `RunAfter` list. Both are `FGroup_Transform_Derived`; `Rig_Update` lists no dependency on
  the service, so no cycle. Gate: the editor boots (the scheduler asserts on cycles at startup).
- `EndPlay` (`:120`): for every handle in `InPlayer._Actions` not yet `Finished`, set
  `_Result = {Outcome::SubjectLost, Recovery::NotAttempted, NAME_None}` (use the existing `Recovery`
  value that means "not attempted"; read the enum) and `CommitState(Finished)`.
- `Action_Processor.cpp:53-61, 91-96`: a cancel on an action already `Cancelling`/`Returning`/`Finished`
  and a second `Release` complete `Succeeded` (intent holds).
- `Core/CkProceduralActionValidation.cpp`: in the per-phase loop, for phase `N+1` with a time-0 key on a
  channel, compare with phase `N`'s key at 1 on the same channel (body: translation within 1e-3 cm and
  rotation `|dot| > 1 - 1e-6`; limb position within 1e-3; orientation same rule; bend direction within
  1e-4). Differ → `AddError(TEXT("PhaseBoundary"), PhasePath, Phase.Get_Id(), Slot, INDEX_NONE,
  TEXT("the previous phase's end value"), TEXT("a different start value"))`.
- Sweep the module for same-line `if (…) { …; }` bodies (72 in `Action/` + `Core/`, 40 in `Service.cpp`,
  4 in `LimbControl_Control.cpp`, 3 in `ActionPlayer_Utils.cpp`): put the body on the next line.
  Mechanical; no behaviour change.

## Exit criteria

- Gate: `--test-pattern ProceduralAnimation.Action`, `ProceduralAnimation.Articulation`, and
  `CrawlerMelee` (host). Expected: the eight new rows green; every other row at the Phase-1 baseline
  (same failing names); `CrawlerMelee` 38/38 (+ the host row
  `PincerClawSeveredMidStrike` stays green).
- `rg -n "ReleaseDeadClaims" Source/CkProceduralAnimation` → ≥2 hits (declaration + processor call).
- `rg -n "Faulted;" Source/CkProceduralAnimation/Public/CkProceduralAnimation/Articulation/CkProceduralArticulation_Processor.cpp` unchanged count (10); `rg -n "Clear\(\)" …Fragment.h` → 1.
- `rg -n "Delivery::Explicit;" Action/CkProceduralAction_Fragment_Data.h` → 0.
- `rg -n "UE_LOG" Source/CkProceduralAnimation` → 0.
- Comment audit done (no `what` comments, no campaign names).

Anything else → STOP, PROGRESS.md blockers.
