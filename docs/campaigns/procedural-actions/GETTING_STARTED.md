# Procedural action: first consumer

The shortest path is four steps: compose a subject once, play a clip with `Play_Loaded` and a completion, build clips in code with the pose builder when an asset is not the right tool, and drain explicit receipts only where gameplay needs every accepted interval (damage). The public declarations are `Articulation/CkProceduralArticulation_Utils.h`, `ActionPlayer/CkProceduralActionPlayer_Utils.h`, `Action/CkProceduralAction_Utils.h` and `ActionBuilder/CkProceduralActionBuilder_Utils.h` under `Source/CkProceduralAnimation/Public/CkProceduralAnimation`. AngelScript reaches them through `utils_procedural_articulation`, `utils_procedural_action_player`, `utils_procedural_action`, `utils_procedural_action_builder` and the handle mixins. The runnable helpers in `Script/CkProceduralAnimation/Examples/` (`CkProceduralAction_Noncombat.as`, `CkProceduralAction_Builder.as`, `CkProceduralAction_Sweep.as`) are the code the snippets below come from.

## 1. Compose once

Create direct-child transform parts under a subject root, add an Articulation, then one ActionPlayer. A body-only subject needs a presentation transform and no limb layout. Composition is per subject, not per clip: every clip the subject plays reuses it.

```angelscript
auto RootOwner = InHandle; // A live lifetime owner supplied by your entity script.
auto RootEntity = utils_entity_lifetime::Request_CreateEntity(RootOwner);
auto Root = utils_transform::Add(RootEntity, FTransform::Identity, ECk_Replication::DoesNotReplicate);
auto PresentationEntity = utils_entity_lifetime::Request_CreateEntity(RootEntity);
auto Presentation = utils_transform::Add(PresentationEntity, FTransform::Identity,
    ECk_Replication::DoesNotReplicate);

auto SubjectSpec = FCk_ProceduralArticulation_Spec();
SubjectSpec.Set_Presentation(Presentation);
auto Subject = utils_procedural_articulation::Add(Root, SubjectSpec);
// Wait for Subject.Get_SettledRevision() > 0 before playing.
auto Player = utils_procedural_action_player::Add(Subject,
    utils_procedural_action_player::Make_DedicatedPlayerSpec(FCk_ProceduralAction_Tuning()));
```

Pick the player spec by what the limbs are. `Make_DedicatedPlayerSpec(Tuning)` is for appendages with no gait. `Make_BorrowingPlayerSpec(FCk_ProceduralActionPlayer_BorrowingSpec(Support, Return))` lets an action borrow a walking leg: `Make_CountOnlySupport(N)` keeps N other trusted planted contacts (not visually standing legs) and the Return group bounds how the leg gets back to a plant. Neither preset guarantees ground to return to. A limb clip needs a layout per limb and one bound transform part per segment; see `CkAutoTest_ProceduralAnimation_NoncombatGesture.as` and `..._BuilderSweep.as` in `Plugins/CkTests/Script/CkProceduralAnimation/`.

## 2. Play a clip and wait for its completion

`Play_Loaded` plays resident content with managed delivery: the framework drains and acknowledges the action's receipt stream, releases the action when it is done, and calls your completion once, on a later frame, never inside the call.

```angelscript
auto Clip = Cast<UCk_ProceduralAction_Data>(LoadObject(this,
    "/CkFoundation/ProceduralAnimation/Examples/Ck_ProceduralAction_BodyPulse_DA.Ck_ProceduralAction_BodyPulse_DA"));
auto Start = ck_procedural_action_noncombat::PlayManagedGesture(InHandle, Player, Clip,
    TArray<FCk_ProceduralAction_SlotBinding>(),
    FCk_Delegate_ProceduralAction_OnManagedCompleted(this, n"OnGestureCompleted"));
if (Start.Get_Report().Get_Status() != ECk_ProceduralAction_ReportStatus::Valid)
    return; // Nothing was created; no completion will follow.

UFUNCTION()
void OnGestureCompleted(FCk_Handle_ProceduralActionPlayer InPlayer, FCk_ProceduralAction_ManagedCompletion InCompletion)
{
    // InCompletion.Get_Result(): the producer outcome, recovery and reason.
    // InCompletion.Get_Delivery(): Complete, Expired or PlayerLost.
    // utils_procedural_action::Get_ResultText(Result, Context) explains a failure for a log or a debug panel.
}
```

The start report is `Valid` whenever an action was created, including a clip that will be rejected at binding: that rejection arrives as the completion's `Rejected` outcome. It is `Invalid`, with a `StartRejected` diagnostic and no identity, only when nothing could be created (invalid scope or player, malformed spec, no receipt capacity). `Play` takes a soft `FCk_ProceduralAction_PlaySpec` and loads first. From the moment `Play` returns, and for as long as its stream is retained, `Get_IsManagedDelivery(Player, Identity)` is true and `Request_ConsumeReceipts` on that identity is refused: one identity has one delivery owner, chosen at creation. `Read_Receipts` still works as a non-consuming observation. To watch the stream without owning it, bind `BindTo_OnManagedRecords` (copied pages, records in order) or `BindTo_OnManagedAcceptedPose` (accepted poses only), at most 8 listeners of each kind per identity, with no replay of pages delivered before the bind.

A named limb clip maps each authored slot to a limb ID with `FCk_ProceduralAction_SlotBinding`. The play options can also mirror a slot onto another limb for this play only: `Spec.Set_Mirrors(...)` with `FCk_ProceduralAction_MirrorSpec(SourceSlot, DestinationSlot, Plane)`, plus a binding for the destination slot. The mirror is applied to the play's compiled copy; the clip asset is never changed.

Tuning comes from the player's defaults; `Spec.Set_Tuning(Tuning)` pins an override for one action (it selects `TuningMode::Override`). In Blueprint's Make/Set Members nodes set `TuningMode` to `Override` yourself when supplying custom `Tuning`; a nondefault `Tuning` with `PlayerDefault` is rejected rather than ignored.

## 3. Build a clip in code

When the motion comes from data the game computes, build the clip instead of authoring an asset. A recipe (`Blank`, `BodyLean`, `Reach`, `Sweep`) produces an editable draft with named phases; pose keys add whole poses; `Build_Into` compiles the draft with the one clip compiler and writes it into a clip object you own.

```angelscript
auto Sweep = Cast<UCk_ProceduralAction_Data>(NewObject(this, UCk_ProceduralAction_Data)); // Keep it referenced.
auto Report = ck_procedural_action_builder::BuildSweep(n"Sweep", FVector(0.6, 0.0, 0.8), 0.15f, FCk_Time(0.4), Sweep);
if (Report.Get_Status() == ECk_ProceduralAction_ReportStatus::Valid)
{
    ck_procedural_action_builder::PlayBuilt(InHandle, Player, Sweep,
        FCk_ProceduralAction_SlotBinding(n"Sweep", n"Arm"),
        FCk_Delegate_ProceduralAction_OnManagedCompleted(this, n"OnSweepCompleted"));
}
```

Every builder call takes the draft by reference, validates all of its inputs first and returns a report; an `Invalid` report leaves the draft unchanged and its diagnostics name the field to fix. Build once per input revision, not per play: compilation happens at build and at play, never per frame. The clip object is caller-owned; an Outer alone does not keep it alive. `CkProceduralAction_Sweep.as` adds cancellation (`CancelSweep`, completion `Cancelled`), the walking-leg variant through the borrowing preset, and the recovery of a leg whose return found no ground (`Get_IsWaitingForRecovery`, `RecoverUnplanted`).

## 4. Explicit receipts, for damage

Managed delivery reports how the motion ended; it is not a damage channel and promises no exactly-once effect. When gameplay must see every accepted interval (hit tests along the swept chain, window-scoped damage with deduplication), create the action with `Create_FromLoadedClip` or `Create_FromClip` and drain its stream yourself:

```angelscript
auto Action = utils_procedural_action::Create_FromLoadedClip(ActionScope, Player,
    FCk_ProceduralAction_LoadedPlaySpec(Clip));
auto Identity = Action.Get_Identity(); // Keep it even if the action entity dies.

// Each frame:
auto Page = Player.Read_Receipts(Identity, Cursor, 16);
if (Page.Get_Result() == ECk_ProceduralAction_ReadResult::Pending ||
    Page.Get_Result() == ECk_ProceduralAction_ReadResult::Succeeded ||
    Page.Get_Result() == ECk_ProceduralAction_ReadResult::Gap)
{
    if (Page.Get_Result() == ECk_ProceduralAction_ReadResult::Gap)
    {
        // [FirstMissingSequence, LastMissingSequence] is irrecoverable here.
        // Clear dependent gameplay window state; do not invent accepted motion.
    }
    for (auto Record : Page.Get_Records())
    {
        if (Record.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
        {
            auto Receipt = Record.Get_Receipt();
            // Receipt.Get_CurrentPose() and Get_Interval() are observed evidence.
            // Gameplay owns collision/effects and its own (Identity, StreamSequence) deduplication.
        }
        // Handle WindowsClosed and Terminal with their own record payloads.
    }
    const auto NextCursor = Page.Get_LastStreamSequence();
    if (NextCursor > Cursor)
    {
        Cursor = NextCursor;
        Player.Request_ConsumeReceipts(
            FCk_Request_ProceduralActionPlayer_ConsumeReceipts(Identity, Cursor));
    }
    // If Page.Get_HasMore(), read the next page with the advanced Cursor.
}
```

Use one designated consumer per `Identity` and persist `Cursor` across frames. `StreamSequence` is the consumption cursor, distinct from a pose `SampleSequence`. Only `AcceptedPose` carries accepted motion; `WindowsClosed` and `Terminal` are separate records. `Pending` means the producer has not finished; **it may already hold records to drain**. `Succeeded` means terminal was published. `StreamExpired`, `InvalidCursor` and `InvalidPlayer` are reader errors for that call. `Page.Get_Terminal()` means the producer finished, not that a truncated page is fully drained. An accepted interval is evidence of the settled pose; intermediate samples are evaluated candidates, not rendered or contact-certified frames. The manual AS fixture `CkAutoTest_ProceduralAnimation_ActionManualPublicSurface.as` shows `Read_Receipts` with a completion-checked `Request_ConsumeReceipts`.

After reading and acknowledging closure plus terminal, call `Action.Request_Release(FCk_Request_ProceduralAction_Release())` if the live action still exists (managed delivery does this for you). If `ActionScope` dies while the player survives, keep the player handle and the copied `Identity`, find retained streams with `Player.Get_RetainedReceiptIdentities()` and drain them the same way; an unacknowledged orphan expires after the finite retention timeout and later reads report `StreamExpired`. Player or subject destruction ends all streams.

## End, cancel and scope loss

Clips finish their own timeline. `Action.Request_Cancel(FCk_Request_ProceduralAction_Cancel(Reason))` interrupts a live clip or manual action; `Action.Request_End(FCk_Request_ProceduralAction_End())` is the normal manual-control exit only. Both close active windows before the limbs return, and the terminal reports the outcome and recovery. A managed completion fires after the terminal record and every page before it were delivered; an action whose scope dies reports `OwnerLost`, and the player still returns its limbs.

## Where data lives

Clip assets own phased body/limb keys and events. The player spec owns default tuning, capacity and locomotion policy; a play spec owns bindings, mirrors and an optional tuning override. The separate `_Preview_DA` asset owns editor-only subject topology, surface, visual meshes and Solver-only tuning (see [AUTHORING.md](AUTHORING.md#current-editor-quick-start)). Mesh appearance and scale are not motion parameters.

In C++ the same calls are on `UCk_Utils_ProceduralArticulation_UE`, `UCk_Utils_ProceduralActionPlayer_UE`, `UCk_Utils_ProceduralAction_UE` and `UCk_Utils_ProceduralActionBuilder_UE`, with the same reflected specs and handles. Blueprint nodes carry the same names (**Play Loaded**, **Create From Loaded Clip**, **Make From Recipe**, **Add Pose Key**, **Build Into**, **Read Receipts**, **Request Consume Receipts**, **Request Cancel**, **Request Release**). The reasons behind every outcome, with their corrective actions, are in [API.md](API.md#9-outcomes-likely-causes-and-corrective-actions).
