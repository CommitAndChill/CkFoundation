// Language=angelscript

// Builds a clip in code instead of authoring an asset: a recipe gives editable phases and keys, a pose key shapes the
// motion, Build_Into compiles the draft and writes it into a clip object the caller owns, and Play_Loaded plays it.
// The clip object must stay referenced (a UPROPERTY, a member of a live script object) while it is built and played.
namespace ck_procedural_action_builder
{
    // A one-slot sweep in the slot's rest-normalized frame: from InDirection, through its quarter turn about body up, to
    // its reverse, at InAmplitude chain lengths from the rest endpoint. An extra pose key a quarter of the way through
    // bows the first half of the arc outward. The report names the field to fix when it is Invalid; InDestination is
    // then unchanged.
    FCk_ProceduralAction_ValidationReport BuildSweep(
        FName InSlot,
        FVector InDirection,
        float32 InAmplitude,
        FCk_Time InDuration,
        UCk_ProceduralAction_Data InDestination)
    {
        auto Slots = TArray<FName>();
        Slots.Add(InSlot);
        auto Recipe = FCk_ProceduralAction_RecipeSpec(ECk_ProceduralAction_Recipe::Sweep, Slots);
        Recipe.Set_Space(ECk_ProceduralAction_TrackSpace::LimbRestNormalized);
        Recipe.Set_Direction(InDirection);
        Recipe.Set_AmplitudeCm(InAmplitude);
        Recipe.Set_Execute(InDuration);
        auto Draft = FCk_ProceduralAction_ClipDraft();
        const auto RecipeReport = utils_procedural_action_builder::Make_FromRecipe(Recipe, Draft);
        if (RecipeReport.Get_Status() != ECk_ProceduralAction_ReportStatus::Valid)
        { return RecipeReport; }

        const auto Apex = FVector(-InDirection.Y, InDirection.X, InDirection.Z);
        auto Limbs = TArray<FCk_ProceduralAction_LimbPoseTarget>();
        Limbs.Add(FCk_ProceduralAction_LimbPoseTarget(InSlot, (InDirection + Apex).GetSafeNormal() * InAmplitude));
        auto Pose = FCk_ProceduralAction_PoseKey(FCk_FloatRange_0to1(0.25f));
        Pose.Set_Limbs(Limbs);
        const auto PoseReport = utils_procedural_action_builder::Add_PoseKey(Draft, n"Execute", Pose);
        if (PoseReport.Get_Status() != ECk_ProceduralAction_ReportStatus::Valid)
        { return PoseReport; }

        return utils_procedural_action_builder::Build_Into(Draft, InDestination);
    }

    // Plays a built clip with managed delivery, binding its one slot to a limb of the player's subject.
    FCk_ProceduralAction_ManagedStart PlayBuilt(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        UCk_ProceduralAction_Data InClip,
        FCk_ProceduralAction_SlotBinding InBinding,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    {
        auto Bindings = TArray<FCk_ProceduralAction_SlotBinding>();
        Bindings.Add(InBinding);
        auto Spec = FCk_ProceduralAction_LoadedPlaySpec(InClip);
        Spec.Set_SlotBindings(Bindings);
        return utils_procedural_action::Play_Loaded(InScope, InPlayer, Spec, InOnCompleted);
    }
}
