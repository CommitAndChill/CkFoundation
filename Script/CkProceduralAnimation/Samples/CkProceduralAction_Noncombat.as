// Language=angelscript

// The caller owns the subject layout, visual bindings, action scope, and any game-specific object state.
// This helper only plays a noncombat gesture clip through the public ActionPlayer path.
namespace ck_procedural_action_noncombat
{
    // Plays a clip with Auto delivery: the player acknowledges the action's records and releases the action, and
    // InOnFinished fires once with the outcome. There is no receipt cursor to keep. A body-only clip takes no slot
    // bindings. An invalid handle means nothing was created and nothing will fire.
    FCk_Handle_ProceduralAction PlayGesture(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        UCk_ProceduralAction_Data InClip,
        const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
        const FCk_Delegate_ProceduralAction_OnFinished& InOnFinished)
    {
        auto Spec = FCk_ProceduralAction_PlaySpec(TSoftObjectPtr<UCk_ProceduralAction_Data>(InClip));
        Spec.Set_SlotBindings(InSlotBindings);
        Spec.Set_Delivery(ECk_ProceduralAction_Delivery::Auto);
        auto Action = utils_procedural_action::Create_FromClip(InScope, InPlayer, Spec);
        if (ck::IsValid(Action))
        { utils_procedural_action::BindTo_OnFinished(Action, InOnFinished); }
        return Action;
    }
}
