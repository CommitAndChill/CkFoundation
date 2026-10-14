// Language=angelscript

// A sweep on a dedicated appendage, and the same sweep on a walking leg borrowed from its gait. The clip is any
// one-slot sweep, for example ck_procedural_action_builder::BuildSweep. The game decides when to sweep, what it hits
// and what that does; these helpers only start, cancel and diagnose the motion.
namespace ck_procedural_action_sweep
{
    // A dedicated appendage has no gait to borrow from, so the default player spec is all it needs.
    FCk_Handle_ProceduralActionPlayer AddDedicatedSweepPlayer(FCk_Handle_ProceduralArticulation InSubject)
    {
        auto Subject = InSubject;
        return utils_procedural_action_player::Add(Subject, FCk_ProceduralActionPlayer_Spec());
    }

    // A walking leg leaves the gait only while InMinimumTrustedContacts other feet stay planted on trusted ground, and
    // returns to a plant through InReturn when the action ends. Neither guarantees ground to return to.
    FCk_Handle_ProceduralActionPlayer AddBorrowingSweepPlayer(
        FCk_Handle_ProceduralArticulation InSubject,
        int32 InMinimumTrustedContacts,
        FCk_ProceduralLimbControl_Return InReturn)
    {
        auto Subject = InSubject;
        auto Locomotion = FCk_ProceduralLimbControl_Locomotion();
        Locomotion.Set_Support(FCk_ProceduralLimbControl_Support(ECk_ProceduralLimbControl_SupportMode::CountOnly,
            InMinimumTrustedContacts));
        Locomotion.Set_Return(InReturn);
        auto Spec = FCk_ProceduralActionPlayer_Spec();
        Spec.Set_Locomotion(Locomotion);
        return utils_procedural_action_player::Add(Subject, Spec);
    }

    // Two ways to be rejected. An invalid handle is synchronous: no action exists and nothing fires. A sweep whose clip
    // cannot run (an unknown limb, a busy channel, missing support) finishes with Outcome Rejected through InOnFinished,
    // like any other outcome.
    FCk_Handle_ProceduralAction StartSweep(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        UCk_ProceduralAction_Data InSweep,
        FCk_ProceduralAction_SlotBinding InBinding,
        const FCk_Delegate_ProceduralAction_OnFinished& InOnFinished)
    {
        return ck_procedural_action_builder::PlayBuilt(InScope, InPlayer, InSweep, InBinding, InOnFinished);
    }

    // The limb returns through the player's return policy and the action finishes Cancelled. Cancelling an action that
    // already finished, or was released, changes nothing.
    void CancelSweep(FCk_Handle_ProceduralAction InAction)
    {
        if (ck::Is_NOT_Valid(InAction) || utils_procedural_action::Get_State(InAction) == ECk_ProceduralAction_State::Finished)
        { return; }
        auto Action = InAction;
        utils_procedural_action::Request_Cancel(Action, FCk_Request_ProceduralAction_Cancel(n"SweepCancelled"));
    }

    // For logs and debug text, never for branching: branch on the terminal's Result and Context fields.
    FString DescribeOutcome(FCk_ProceduralAction_Terminal InTerminal)
    {
        return utils_procedural_action::Get_ResultText(InTerminal.Get_Result(), InTerminal.Get_Context()).ToString();
    }

    // A walking leg whose return found no ground stays unplanted, and no action can borrow it, until it is recovered.
    bool Get_IsWaitingForRecovery(FCk_Handle_ProceduralArticulation InSubject, FName InLimbId)
    {
        return utils_procedural_articulation::Get_UnplantedLimbIds(InSubject).Contains(InLimbId);
    }

    // Recover once there is ground again. The request recovers the limbs that are unplanted when it is processed.
    void RecoverUnplanted(
        FCk_Handle_ProceduralArticulation InSubject,
        const FCk_Delegate_Request_OnCompleted& InOnCompleted)
    {
        utils_procedural_articulation::Request_RecoverUnplantedLimbs(InSubject,
            FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs(), InOnCompleted);
    }
}
