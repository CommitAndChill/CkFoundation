// Language=angelscript

// A sweep on a dedicated appendage, and the same sweep on a walking leg borrowed from its gait. The clip is any
// one-slot sweep, for example ck_procedural_action_builder::BuildSweep. The game decides when to sweep, what it hits
// and what that does; these helpers only start, cancel and diagnose the motion.
namespace ck_procedural_action_sweep
{
    // A dedicated appendage has no gait to borrow from, so its player needs only playback tuning.
    FCk_Handle_ProceduralActionPlayer AddDedicatedSweepPlayer(FCk_Handle_ProceduralArticulation InSubject)
    {
        auto Subject = InSubject;
        return utils_procedural_action_player::Add(Subject,
            utils_procedural_action_player::Make_DedicatedPlayerSpec(FCk_ProceduralAction_Tuning()));
    }

    // A walking leg leaves the gait only while InMinimumTrustedContacts other feet stay planted on trusted ground, and
    // returns to a plant through InReturn when the action ends. Neither guarantees ground to return to.
    FCk_Handle_ProceduralActionPlayer AddBorrowingSweepPlayer(
        FCk_Handle_ProceduralArticulation InSubject,
        int32 InMinimumTrustedContacts,
        FCk_ProceduralLimbControl_Return InReturn)
    {
        auto Subject = InSubject;
        const auto Support = utils_procedural_action_player::Make_CountOnlySupport(InMinimumTrustedContacts);
        return utils_procedural_action_player::Add(Subject, utils_procedural_action_player::Make_BorrowingPlayerSpec(
            FCk_ProceduralActionPlayer_BorrowingSpec(Support, InReturn)));
    }

    // Two ways to be rejected. An Invalid start report is synchronous: no action exists and no completion follows. A
    // Valid start whose clip cannot bind (an unknown limb, a busy channel, missing support) completes with Outcome
    // Rejected on a later frame, like any other outcome.
    FCk_ProceduralAction_ManagedStart StartSweep(
        FCk_Handle InScope,
        FCk_Handle_ProceduralActionPlayer InPlayer,
        UCk_ProceduralAction_Data InSweep,
        FCk_ProceduralAction_SlotBinding InBinding,
        const FCk_Delegate_ProceduralAction_OnManagedCompleted& InOnCompleted)
    {
        return ck_procedural_action_builder::PlayBuilt(InScope, InPlayer, InSweep, InBinding, InOnCompleted);
    }

    // The limb returns through the player's return policy and the completion reports Cancelled. Cancelling an action
    // that already finished changes nothing.
    void CancelSweep(FCk_ProceduralAction_ManagedStart InStart)
    {
        if (ck::Is_NOT_Valid(InStart.Get_Action()))
        { return; }
        utils_procedural_action::Request_Cancel(InStart.Get_Action(), FCk_Request_ProceduralAction_Cancel(n"SweepCancelled"));
    }

    // For logs and debug text, never for branching: branch on the completion's Result, Delivery and Context fields.
    FString DescribeOutcome(FCk_ProceduralAction_ManagedCompletion InCompletion)
    {
        return utils_procedural_action::Get_ResultText(InCompletion.Get_Result(), InCompletion.Get_Context()).ToString();
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
