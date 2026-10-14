# Phase 2 — API shape, surface cuts, structure, docs, host adaptation

One executor session across three repos (CkFoundation, CkTests, host). Commit per repo with explicit
pathspecs; the host commit bumps the two submodule gitlinks.

## Entry criteria

Phase 1 committed in CkFoundation and CkTests; PROGRESS.md names both hashes. Baseline run:
`--test-pattern ProceduralAnimation` (expect the Phase-1 baseline names) and `--test-pattern Crawler`
(host; expect all green except known reds). Paste counts.

## Step 1 — The walker returns what it made, and takes a Spec

`CkProceduralAnimation_Utils.h`:

```cpp
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralWalker_Spec
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralWalker_Spec);
private:
    // Construction inputs the caller has already resolved, consumed inside Add_Walker and not retained
    // (same contract as today's raw-pointer parameters). A retained Spec on a data asset must hold
    // these as soft refs on ITS side and resolve before calling; never load synchronously here.
    UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralRig_Data> _Rig;

    UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralGait_Data> _Gait;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralWalker_LegChain> _Chains;

    // Optional composition: present when set, so one call stands up a walker that can pose and act.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _BodyPose = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, EditCondition = "_BodyPose == ECk_EnableDisable::Enable"))
    FCk_ProceduralBodyPose_Spec _BodyPoseSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _ActionPlayer = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, EditCondition = "_ActionPlayer == ECk_EnableDisable::Enable"))
    FCk_ProceduralActionPlayer_Spec _ActionPlayerSpec;
public:
    CK_PROPERTY_GET(_Rig);
    CK_PROPERTY_GET(_Gait);
    CK_PROPERTY(_Chains);
    CK_PROPERTY(_BodyPose);
    CK_PROPERTY(_BodyPoseSpec);
    CK_PROPERTY(_ActionPlayer);
    CK_PROPERTY(_ActionPlayerSpec);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralWalker_Spec, _Rig, _Gait);
};
```

(Assets are hard refs here because they are construction inputs consumed inside `Add_Walker`, not
retained; the existing signature already takes raw pointers.) Enum-mode + value, not `TOptional`,
per the reflected-optionality rule.

`FCk_ProceduralWalker` gains `_Articulation` (`FCk_Handle_ProceduralArticulation`), `_Rigs`
(`TArray<FCk_Handle_ProceduralRig>`), `_BodyPose` (`FCk_Handle_ProceduralBodyPose`, invalid when not
requested) and `_ActionPlayer` (`FCk_Handle_ProceduralActionPlayer`, same). `CK_DEFINE_CONSTRUCTORS`
over `_Gait, _Legs, _Articulation`.

`Add_Walker(UPARAM(ref) FCk_Handle_Transform& InBody, const FCk_ProceduralWalker_Spec& InSpec) -> FCk_ProceduralWalker`
replaces the four-positional overload (no overloads in UFUNCTIONs). Body: existing admission, then
legs, gait, rigs, articulation (existing :175-183), then `BodyPose::Add(Gait, Spec)` and
`ActionPlayer::Add(Articulation, Spec)` when enabled. Admission is atomic: validate the body-pose and
player specs before creating anything; one rejection returns `{}` with the ensure.

## Step 2 — `Gait::Add` takes its legs

`Gait/CkProceduralGait_Utils.h`:

```cpp
static FCk_Handle_ProceduralGait
Add(
    const TArray<FCk_Handle_ProceduralLeg>& InLegs,
    const UCk_ProceduralGait_Data* InData);
```

Body derived as the legs' common lifetime owner (`UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner`);
ensure all legs share one owner, 2..64, attached, rests within reach (existing checks at :24-39).
Update the 14 call sites (`rg -n "ProceduralGait_UE::Add\(|utils_procedural_gait::Add\("` across
CkFoundation, CkTests Source+Script). `Add_Walker` passes the legs it just created.

## Step 3 — `Private/` goes public; tests stop reaching in

Move the three headers to `Public/CkProceduralAnimation/{Articulation,Gait,LimbControl}/` (same
names), fix includes, delete `PrivateIncludePaths` in `CkProceduralAnimation.Build.cs:8` and the
`PrivateIncludePaths` block in `CkTests/Source/CkTests/CkTests.Build.cs:13-19`. Classes keep
`CKPROCEDURALANIMATION_API`. Add one line to the module `Claude.md`: these are the control-plane
API used by the player and by tests; game code uses the Utils.

Fence: do not rename the classes in this phase.

## Step 4 — Cut the unused surface

Delete (each verified zero consumers outside module+tests during planning; re-verify with
`rg` across CkFoundation/CkTests/CkGameplayDebugger/host `Script` before deleting):
- `FCk_ProceduralArticulation_Control::TryTransferClaims`, `::GetControlState` + `FCk_ArticulationControlState`;
  `FCk_ProceduralGaitParticipation_Control::RequestInterrupt`; `ECk_ProceduralGaitParticipation_State`
  (written, never read); `Outcome::PartLost` (keep `SubjectLost`, now used);
  `ReportStatus::ContentNotResident`; `Watchdogs._MaxTimeBacklog`; `ValidationReport._Advisory`;
  `FProceduralLimbControl_Controller::Get_ReattachmentSettledRevision` (keep if a test asserts it —
  then it stays, note in PROGRESS.md).
- Builder: `Add_BodyKey`, `Add_LimbKey`, `Add_Marker` (tests only → delete rows with them, or keep
  `Add_Marker` if the Phase-4 editor needs it: it does not, the editor uses `AddMarker` commands);
  drop the export on `ComputeCatmullRomTangents`.
- Receipts: delete `_IntermediateSamples`, `ECk_ProceduralAction_SampleEvidence`,
  `receipt_limits::MaxSamplesPerReceipt` and the per-receipt sample budget accounting (8 files; the
  store's memory accounting shrinks accordingly). A receipt is: previous pose, current pose,
  interval (windows + events), stamp. The host reads only `_Windows`/`_Events`.
- `Action_Utils`: `ResolveClipSampleTime`, `FindReturnPrefix` become private to the service file.
- `_RecordedStore.h:18`: stock `check()` → `CK_ENSURE_IF_NOT`.
- Domain types: `_MaxEndpointTravelCm`/`_MaxSegmentAngularStepDegrees`/`_MinimumEdgeDistanceCm` stay
  (no house type for distance/angle; note the ruling), but `FailureContext` sentinels (0/None =
  "not measured") become enum-mode + value per ADJUDICATIONS A1.

## Step 5 — Docs

- `Source/CLAUDE.md:194` tier row: deps `Core,Ecs,EcsExt,Jolt,Label,Log,Record,ResourceLoader,Tween`.
- `Source/EDITOR_MODULES.md`: add `CkProceduralAnimationEditor` in the existing row format (read two
  neighbouring rows first); `Source/CLAUDE.md:269` T5 count +1.
- `Source/CkEcs/Claude.md`: a short section "Production preview worlds" documenting
  `Mark_ProductionPreviewWorld`/`Is_ProductionPreviewWorld` and that the ECS/Jolt subsystems opt in
  only for marked `GamePreview` worlds (purely additive to the Game/PIE policy).
- `Source/CkProceduralAnimation/Claude.md`: fold the content of the gitignored
  `PROCEDURAL_ACTIONS.md` (on the planner's disk; the executor finds it at
  `Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md` if present, else rewrites from the code) into
  the tracked doc; delete the untracked file and the `PROGRESS.md` campaign link at :3.

## Step 6 — Host adaptation (Orion)

`Script/ECS/Crawler/Mars_Crawler_Utils.as:150-200`: replace the five-call composition with
`Make_WalkerSpec(InSpec)` → `utils_procedural_animation::Add_Walker(InRoot, WalkerSpec)` with
`BodyPose` and `ActionPlayer` enabled in the spec; read `Walker.Get_ActionPlayer()` and
`Walker.Get_Articulation()` (delete the `DoCastChecked` at :190 and in `Mars_Crawler_ActionAdapter.as:30,42`
if the handle can be stored on the crawler fragment instead; the Spec→Params rule applies).
`utils_surface_motion::Add` stays separate (independent feature). Regenerate `Script/Generated` by
opening the editor once (or let the toolbox boot do it); never hand-edit.

## Exit criteria

- `rg -n "Private" Source/CkProceduralAnimation/CkProceduralAnimation.Build.cs` → 0; `ls Source/CkProceduralAnimation/Private` → does not exist.
- `rg -n "Add_Walker\(" Source Script ../CkTests/Source ../CkTests/Script` → every call passes a Spec (no 4-arg form).
- `rg -n "_IntermediateSamples|SampleEvidence" Source ../CkTests ../CkGameplayDebugger` → 0.
- Gates: `ProceduralAnimation` (Phase-1 baseline names), `CrawlerMelee` 38/38, `Crawler` all green,
  `Jolt.Query` unchanged.
- Three environments: `Add_Walker` with a Spec callable from AS (the host script compiles) and BP
  (node exists; `[EDITOR-VERIFY]` in VALIDATION.md).
