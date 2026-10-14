// Language=angelscript

// The caller owns the subject layout, visual bindings, action scope, and any game-specific object state.
// These helpers only author noncombat reach intents through the public ActionPlayer path.
namespace ck_procedural_action_noncombat
{
    // Plays a resident clip with managed delivery: the framework drains the action's records, releases the action and
    // calls InOnCompleted once, on a later frame, with the outcome. There is no receipt cursor to keep. A body-only clip
    // takes no slot bindings. An Invalid start report means nothing was created and no completion will follow.
    FCk_ProceduralAction_ManagedStart PlayManagedGesture(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        UCk_ProceduralAction_Data InClip,
        const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    {
        auto Spec = FCk_ProceduralAction_LoadedPlaySpec(InClip);
        Spec.Set_SlotBindings(InSlotBindings);
        return utils_procedural_action::Play_Loaded(InScope, InPlayer, Spec, InOnCompleted);
    }

    FCk_Handle_ProceduralAction BeginInspectionReach(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        FName InArmId)
    {
        auto Ids = TArray<FName>();
        Ids.Add(InArmId);
        auto Control = FCk_ProceduralAction_ControlSpec(Ids);
        return utils_procedural_action::Create_Manual(InScope, InPlayer, Control);
    }

    FCk_Handle_ProceduralAction BeginCarrierReach(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        FName InWalkingLimbId)
    {
        // The player's installed locomotion policy decides whether this walking limb can withdraw.
        auto Ids = TArray<FName>();
        Ids.Add(InWalkingLimbId);
        auto Control = FCk_ProceduralAction_ControlSpec(Ids);
        return utils_procedural_action::Create_Manual(InScope, InPlayer, Control);
    }

    FCk_Handle_ProceduralAction BeginCoordinatedGesture(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        FName InLeftId,
        FName InRightId)
    {
        // One action owns both limbs, so a single accepted sample coordinates the pair.
        auto Ids = TArray<FName>();
        Ids.Add(InLeftId);
        Ids.Add(InRightId);
        return utils_procedural_action::Create_Manual(InScope, InPlayer,
            FCk_ProceduralAction_ControlSpec(Ids));
    }

    FCk_Handle_ProceduralAction SubmitRestReach(
        FCk_Handle_ProceduralAction InAction,
        FName InLimbId,
        FTransform InRestNormalizedOffset,
        const FCk_Delegate_Request_OnCompleted& InCompletion = FCk_Delegate_Request_OnCompleted())
    {
        auto Target = FCk_ProceduralAction_LimbTarget(InLimbId, InRestNormalizedOffset);
        Target.Set_Space(ECk_ProceduralAction_TrackSpace::LimbRestNormalized);
        auto Batch = FCk_ProceduralAction_PoseBatch();
        auto Targets = TArray<FCk_ProceduralAction_LimbTarget>();
        Targets.Add(Target);
        Batch.Set_Limbs(Targets);
        return utils_procedural_action::Request_SubmitPoseBatch(InAction,
            FCk_Request_ProceduralAction_SubmitPoseBatch(Batch), InCompletion);
    }

    FCk_Handle_ProceduralAction SubmitCoordinatedGesture(
        FCk_Handle_ProceduralAction InAction,
        FName InLeftId,
        FTransform InLeftRestNormalizedOffset,
        FName InRightId,
        FTransform InRightRestNormalizedOffset,
        const FCk_Delegate_Request_OnCompleted& InCompletion = FCk_Delegate_Request_OnCompleted())
    {
        auto Left = FCk_ProceduralAction_LimbTarget(InLeftId, InLeftRestNormalizedOffset);
        Left.Set_Space(ECk_ProceduralAction_TrackSpace::LimbRestNormalized);
        auto Right = FCk_ProceduralAction_LimbTarget(InRightId, InRightRestNormalizedOffset);
        Right.Set_Space(ECk_ProceduralAction_TrackSpace::LimbRestNormalized);
        auto Targets = TArray<FCk_ProceduralAction_LimbTarget>();
        Targets.Add(Left);
        Targets.Add(Right);
        auto Batch = FCk_ProceduralAction_PoseBatch();
        Batch.Set_Limbs(Targets);
        return utils_procedural_action::Request_SubmitPoseBatch(InAction,
            FCk_Request_ProceduralAction_SubmitPoseBatch(Batch), InCompletion);
    }
}
