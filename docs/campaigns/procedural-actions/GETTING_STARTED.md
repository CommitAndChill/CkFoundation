# Procedural action: first consumer

The shortest path is four steps: compose a subject once, play a clip with `Create_FromClip` and Auto delivery, build clips in code with the pose builder when an asset is not the right tool, and use Explicit delivery to drain receipts only where gameplay needs every accepted interval (damage). The public declarations are `Articulation/CkProceduralArticulation_Utils.h`, `ActionPlayer/CkProceduralActionPlayer_Utils.h`, `Action/CkProceduralAction_Utils.h` and `ActionBuilder/CkProceduralActionBuilder_Utils.h` under `Source/CkProceduralAnimation/Public/CkProceduralAnimation`. AngelScript reaches them through `utils_procedural_articulation`, `utils_procedural_action_player`, `utils_procedural_action`, `utils_procedural_action_builder` and the handle mixins. The runnable helpers in `Script/CkProceduralAnimation/Examples/` (`CkProceduralAction_Noncombat.as`, `CkProceduralAction_Builder.as`, `CkProceduralAction_Sweep.as`) are the code the snippets below come from.

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
auto Player = utils_procedural_action_player::Add(Subject, FCk_ProceduralActionPlayer_Spec());
```

Pick the player spec by what the limbs are. The default `FCk_ProceduralActionPlayer_Spec` serves the body channel and appendages with no gait; its `Tuning` is the tuning every action of the player plays with. To let an action borrow a walking leg, set its `Locomotion`: a `CountOnly` support keeps N other trusted planted contacts (not visually standing legs) and the `Return` bounds how the leg gets back to a plant (`ck_procedural_action_sweep::AddBorrowingSweepPlayer`). With the default `Disabled` support, an action that binds a walking leg is rejected at admission with reason `SupportUnavailable`. Neither policy guarantees ground to return to. A limb clip needs a layout per limb and one bound transform part per segment; see `CkAutoTest_ProceduralAnimation_NoncombatManaged.as` and `..._BuilderSweep.as` in `Plugins/CkTests/Script/CkProceduralAnimation/`.

## 2. Play a clip and wait for OnFinished

With `Delivery` `Auto` the player acknowledges the action's receipt stream, the action releases itself once it is done, and `OnFinished` fires once with the outcome.

```angelscript
auto Clip = Cast<UCk_ProceduralAction_Data>(LoadObject(this,
    "/CkFoundation/ProceduralAnimation/Examples/Ck_ProceduralAction_BodyPulse_DA.Ck_ProceduralAction_BodyPulse_DA"));
auto Action = ck_procedural_action_noncombat::PlayGesture(InHandle, Player, Clip,
    TArray<FCk_ProceduralAction_SlotBinding>(),
    FCk_Delegate_ProceduralAction_OnFinished(this, n"OnGestureFinished"));
if (ck::Is_NOT_Valid(Action))
    return; // Nothing was created; nothing will fire.

UFUNCTION()
void OnGestureFinished(FCk_Handle_ProceduralAction InAction, FCk_ProceduralAction_Terminal InTerminal)
{
    // InTerminal.Get_Result(): the outcome, recovery and reason.
    // utils_procedural_action::Get_ResultText(Result, Context) explains a failure for a log or a debug panel.
}
```

A resident clip is compiled and bound inside `Create_FromClip`. A clip that cannot bind comes back already `Finished` with `Rejected`, and `OnFinished`, bound right after the call with its default `FireIfPayloadInFlightThisFrame` policy, still reports it. An unloaded clip loads first (`PendingLoad`). An invalid handle means nothing could be created (invalid scope or player, malformed spec, no receipt capacity). One identity has one acknowledging reader, chosen at creation: `Request_ConsumeReceipts` on an Auto identity is refused, while `Read_Receipts` still works as a non-consuming observation.

A named limb clip maps each authored slot to a distinct limb ID with `FCk_ProceduralAction_SlotBinding`. To play a slot's motion on the opposite limb, mirror it into its own slot with the builder's `Mirror_Slot` and bind that slot.

## 3. Build a clip in code

When the motion comes from data the game computes, build the clip instead of authoring an asset. A recipe (`Blank`, `BodyLean`, `Reach`, `Sweep`) produces an editable draft with named phases; pose keys add whole poses; `Build_Into` compiles the draft with the one clip compiler and writes it into a clip object you own.

```angelscript
auto Sweep = Cast<UCk_ProceduralAction_Data>(NewObject(this, UCk_ProceduralAction_Data)); // Keep it referenced.
auto Report = ck_procedural_action_builder::BuildSweep(n"Sweep", FVector(0.6, 0.0, 0.8), 0.15f, FCk_Time(0.4), Sweep);
if (Report.Get_Status() == ECk_ProceduralAction_ReportStatus::Valid)
{
    ck_procedural_action_builder::PlayBuilt(InHandle, Player, Sweep,
        FCk_ProceduralAction_SlotBinding(n"Sweep", n"Arm"),
        FCk_Delegate_ProceduralAction_OnFinished(this, n"OnSweepFinished"));
}
```

Every builder call takes the draft by reference, validates all of its inputs first and returns a report; an `Invalid` report leaves the draft unchanged and its diagnostics name the field to fix. Build once per input revision, not per play: compilation happens at build and at play, never per frame. The clip object is caller-owned; an Outer alone does not keep it alive. `CkProceduralAction_Sweep.as` adds cancellation (`CancelSweep`, outcome `Cancelled`), the walking-leg player (`AddBorrowingSweepPlayer`), and the recovery of a leg whose return found no ground (`Get_IsWaitingForRecovery`, `RecoverUnplanted`).

## 4. Explicit receipts, for damage

Auto delivery reports how the motion ended; it is not a damage channel and promises no exactly-once effect. When gameplay must see every accepted interval (hit tests along the swept chain, window-scoped damage with deduplication), create the action with the default `Explicit` delivery and drain its stream yourself:

```angelscript
auto Action = utils_procedural_action::Create_FromClip(ActionScope, Player,
    FCk_ProceduralAction_PlaySpec(TSoftObjectPtr<UCk_ProceduralAction_Data>(Clip)));
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

Use one designated consumer per `Identity` and persist `Cursor` across frames. `StreamSequence` is the consumption cursor, distinct from a pose `SampleSequence`. Only `AcceptedPose` carries accepted motion; `WindowsClosed` and `Terminal` are separate records. `Pending` means the producer has not finished; **it may already hold records to drain**. `Succeeded` means terminal was published. `StreamExpired`, `InvalidCursor` and `InvalidPlayer` are reader errors for that call. `Page.Get_Terminal()` means the producer finished, not that a truncated page is fully drained. An accepted interval is evidence of the settled pose; intermediate samples are evaluated candidates, not rendered or contact-certified frames.

Acknowledging the terminal record releases the action: the player destroys it on its next service pass, so read `Get_State`/`Get_Result` before that, or take the result from the `Terminal` record. `Action.Request_Release(FCk_Request_ProceduralAction_Release())` is only for tearing an action down early. If `ActionScope` dies while the player survives, keep the player handle and the copied `Identity`, find retained streams with `Player.Get_RetainedReceiptIdentities()` and drain them the same way; an unacknowledged orphan expires 2 s after its terminal and later reads report `StreamExpired`. Player or subject destruction ends all streams.

## Cancel and scope loss

Clips finish their own timeline. `Action.Request_Cancel(FCk_Request_ProceduralAction_Cancel(Reason))` interrupts a live clip; it closes active windows before the limbs return, and the terminal reports the outcome and recovery. An action whose scope dies reports `OwnerLost`, and the player still returns its limbs.

## Where data lives

Clip assets own phased body/limb keys and events. The player spec owns tuning, capacity and locomotion policy; a play spec owns the clip, its slot bindings and its delivery. The separate `_Preview_DA` asset owns editor-only subject topology, surface, visual meshes and Solver-only tuning (see [AUTHORING.md](AUTHORING.md#current-editor-quick-start)). Mesh appearance and scale are not motion parameters.

In C++ the same calls are on `UCk_Utils_ProceduralArticulation_UE`, `UCk_Utils_ProceduralActionPlayer_UE`, `UCk_Utils_ProceduralAction_UE` and `UCk_Utils_ProceduralActionBuilder_UE`, with the same reflected specs and handles. Blueprint nodes carry the same names (**Create From Clip**, **Bind To OnFinished**, **Make From Recipe**, **Add Pose Key**, **Build Into**, **Read Receipts**, **Request Consume Receipts**, **Request Cancel**, **Request Release**). The reasons behind every outcome, with their corrective actions, are in [API.md](API.md#8-outcomes-likely-causes-and-corrective-actions).
