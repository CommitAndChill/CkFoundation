#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h"

#include "CkProceduralAnimation/Core/CkProceduralActionMotion.h"
#include "CkProceduralAnimation/Core/CkProceduralBodySupport.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Validation/CkIsValid.h"

namespace ck_procedural_action_player_service
{
    auto
    BuildEntryHoldCandidate(
        const TArray<FName>& InLimbIds,
        bool InBodyClaim,
        const FCk_ProceduralArticulation_Pose& InEntry,
        FCk_ArticulationControlToken InToken,
        FCk_ArticulationTokenCandidate& OutCandidate)
        -> bool
    {
        auto Limbs = TArray<FCk_ArticulationLimbTarget>{};
        Limbs.Reserve(InLimbIds.Num());
        for (const auto Id : InLimbIds)
        {
            const auto* Limb = InEntry.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
            if (Limb == nullptr || NOT ck::procedural_animation::IsRigid(Limb->Get_AcceptedEndpoint(), RigidTolerance))
            { return false; }
            Limbs.Add(FCk_ArticulationLimbTarget{Id, Limb->Get_AcceptedEndpoint(), {},
                ECk_ArticulationReachPolicy::RejectUnreachable});
        }
        if (InBodyClaim && (NOT InEntry.Get_HasBodyPresentation()
            || NOT ck::procedural_animation::IsRigid(InEntry.Get_BodyPresentation(), RigidTolerance)))
        { return false; }
        const auto Body = InBodyClaim ? TOptional<FTransform>{InEntry.Get_BodyPresentation()}
            : TOptional<FTransform>{};
        OutCandidate = FCk_ArticulationTokenCandidate{InToken, 0, Limbs, Body};
        return true;
    }

    auto
    BuildClipCandidate(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FFragment_ProceduralAction_Compiled& InCompiled,
        const ck::procedural_action::FEvaluatedPose& InEvaluated,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        FCk_ArticulationControlToken InToken,
        uint64 InSequence,
        FCk_ArticulationTokenCandidate& OutCandidate)
        -> bool
    {
        const auto& Clip = *InCompiled.Get_Clip();
        if (InEvaluated.Get_Limbs().Num() != Clip.Get_Slots().Num()
            || NOT ck::procedural_animation::IsRigid(InDefault.Get_RootFrame(), RigidTolerance))
        { return false; }
        auto BodyWorld = InDefault.Get_HasBodyPresentation()
            ? InDefault.Get_BodyPresentation() : InDefault.Get_RootFrame();
        auto BodyTarget = TOptional<FTransform>{};
        if (Clip.Get_HasBodyChannel())
        {
            if (NOT InDefault.Get_HasBodyPresentation()
                || NOT ck::Get_IsProceduralBodyPoseActionAdmissible(InEvaluated.Get_Body()))
            { return false; }
            const auto BaseLocal = InDefault.Get_BodyPresentation()
                .GetRelativeTransform(InDefault.Get_RootFrame());
            const auto Composed = ck::ComposeProceduralBodyPoseAction(BaseLocal, InEvaluated.Get_Body());
            if (NOT Composed.IsSet())
            { return false; }
            BodyWorld = Composed.GetValue() * InDefault.Get_RootFrame();
            BodyTarget = BodyWorld;
        }
        auto Limbs = TArray<FCk_ArticulationLimbTarget>{};
        Limbs.Reserve(Clip.Get_Slots().Num());
        for (auto Index = 0; Index < Clip.Get_Slots().Num(); ++Index)
        {
            const auto& Slot = Clip.Get_Slots()[Index];
            const auto& EvaluatedLimb = InEvaluated.Get_Limbs()[Index];
            if (EvaluatedLimb.Get_Slot() != Slot.Get_Id())
            { return false; }
            const auto LimbId = InCompiled.Get_BoundLimbIds()[Index];
            auto Frame = FTransform::Identity;
            auto ChainLength = 0.0f;
            auto Hip = FVector::ZeroVector;
            if (NOT ResolveClipFrame(InSubject, Slot, LimbId, InEntry, InDefault,
                BodyWorld, Frame, ChainLength, Hip))
            { return false; }
            auto Position = EvaluatedLimb.Get_Position();
            if (Slot.Get_Space() == ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
            { Position *= ChainLength; }
            const auto Endpoint = FTransform{EvaluatedLimb.Get_Orientation(), Position} * Frame;
            if (NOT ck::procedural_animation::IsRigid(Endpoint, RigidTolerance))
            { return false; }
            auto Pole = TOptional<FVector>{};
            if (EvaluatedLimb.Get_HasBend())
            {
                const auto Direction = Frame.TransformVectorNoScale(EvaluatedLimb.Get_BendDirection())
                    .GetSafeNormal();
                if (NOT IsFiniteDirection(Direction))
                { return false; }
                Pole = Hip + Direction * ChainLength;
            }
            const auto Reach = EvaluatedLimb.Get_ReachPolicy()
                == ECk_ProceduralAction_ReachPolicy::ProjectToReach
                ? ECk_ArticulationReachPolicy::ProjectToReach
                : ECk_ArticulationReachPolicy::RejectUnreachable;
            auto Target = FCk_ArticulationLimbTarget{LimbId, Endpoint, Pole, Reach};
            Target.Set_Orientation(EvaluatedLimb.Get_HasOrientation()
                ? ECk_ArticulationTargetOrientation::Authored : ECk_ArticulationTargetOrientation::RigOwn);
            Limbs.Add(MoveTemp(Target));
        }
        OutCandidate = FCk_ArticulationTokenCandidate{InToken, InSequence, Limbs, BodyTarget};
        return true;
    }

    auto
    BlendClipEntry(
        const ck::procedural_action::FInitialPose& InInitial,
        const ck::procedural_action::FEvaluatedPose& InTarget,
        double InFraction,
        ck::procedural_action::FEvaluatedPose& OutBlended)
        -> bool
    {
        if (NOT FMath::IsFinite(InFraction) || InFraction < 0.0 || InFraction > 1.0)
        { return false; }
        OutBlended = InTarget;
        OutBlended.Get_Body().SetTranslation(FMath::Lerp(
            InInitial.Get_Body().GetTranslation(), InTarget.Get_Body().GetTranslation(), InFraction));
        OutBlended.Get_Body().SetRotation(FQuat::Slerp(
            InInitial.Get_Body().GetRotation(), InTarget.Get_Body().GetRotation(), InFraction).GetNormalized());
        for (auto& Limb : OutBlended.Get_Limbs())
        {
            const auto* Initial = InInitial.Get_Limbs().Find(Limb.Get_Slot());
            if (Initial == nullptr)
            { return false; }
            Limb.Get_Position() = FMath::Lerp(Initial->GetTranslation(), Limb.Get_Position(), InFraction);
            Limb.Get_Orientation() = FQuat::Slerp(Initial->GetRotation(),
                Limb.Get_Orientation(), InFraction).GetNormalized();
            if (Limb.Get_HasBend())
            {
                if (const auto* InitialBend = InInitial.Get_BendDirections().Find(Limb.Get_Slot()))
                {
                    const auto Blended = FMath::Lerp(*InitialBend, Limb.Get_BendDirection(), InFraction);
                    if (NOT IsFiniteDirection(Blended))
                    { return false; }
                    Limb.Get_BendDirection() = Blended.GetSafeNormal();
                }
            }
        }
        return ck::procedural_animation::IsRigid(OutBlended.Get_Body(), RigidTolerance);
    }
}

namespace ck
{
    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoPlanClip(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        const auto& Params = *InAction.Params;
        if (NOT InCtx.DefaultValid || InCtx.DeltaT <= FCk_Time{}
            || (Action._State != ECk_ProceduralAction_State::Acquiring
                && Action._State != ECk_ProceduralAction_State::Playing)
            || Action._PendingStageKey.IsSet()
            || NOT InAction.Handle.Has<FFragment_ProceduralAction_Compiled>())
        { return; }
        const auto& Clip = *InAction.Handle.Get<FFragment_ProceduralAction_Compiled>().Get_Clip();
        const auto& Sampling = Params.Get_Tuning().Get_Sampling();
        auto Plan = ck_procedural_action_player_service::FClipServicePlan{};
        Plan.Action = InAction.Handle;
        Plan.EntryBlend = Action._State == ECk_ProceduralAction_State::Acquiring;
        Plan.From = Plan.EntryBlend ? Action._EntryBlendAccepted : Action._AcceptedTime;
        if (Plan.EntryBlend)
        {
            const auto End = Params.Get_Tuning().Get_Transition().Get_EntryBlend();
            Plan.Requested = FCk_Time{FMath::Min(End.Get_Seconds(),
                Plan.From.Get_Seconds() + InCtx.DeltaT.Get_Seconds())};
            auto At = Plan.From.Get_Seconds();
            while (At < Plan.Requested.Get_Seconds()
                && Plan.CriticalFractions.Num() < Sampling.Get_MaxSamples())
            {
                const auto Next = FMath::Min(Plan.Requested.Get_Seconds(),
                    At + Sampling.Get_MaxAdvance().Get_Seconds());
                if (Next <= At)
                { break; }
                At = Next;
                Plan.CriticalFractions.Add((At - Plan.From.Get_Seconds())
                    / (Plan.Requested.Get_Seconds() - Plan.From.Get_Seconds()));
                Plan.CriticalTimes.Add(FCk_Time{At});
            }
            if (NOT Plan.CriticalFractions.IsEmpty())
            {
                Plan.MaximumFraction = Plan.CriticalFractions.Last();
                Plan.MaximumTime = Plan.CriticalTimes.Last();
            }
        }
        else
        {
            const auto RequestedSeconds = FMath::Min(Clip.Get_Duration().Get_Seconds(),
                Action._AcceptedTime.Get_Seconds()
                    + InCtx.DeltaT.Get_Seconds() * Params.Get_Tuning().Get_Playback().Get_PlayRate()
                    + Action._PlaybackBacklog.Get_Seconds());
            Plan.Requested = FCk_Time{RequestedSeconds};
            auto Sampled = procedural_action::FSampledInterval{};
            if (Plan.Requested > Plan.From
                && procedural_action::SampleInterval(Clip, Action._InitialPose,
                    Plan.From, Plan.Requested, Sampling, Sampled))
            {
                const auto Span = Plan.Requested.Get_Seconds() - Plan.From.Get_Seconds();
                for (const auto& Sample : Sampled.Get_Samples())
                {
                    Plan.CriticalFractions.Add((Sample.Get_Time().Get_Seconds()
                        - Plan.From.Get_Seconds()) / Span);
                    Plan.CriticalTimes.Add(Sample.Get_Time());
                }
                Plan.MaximumFraction = (Sampled.Get_AcceptedEnd().Get_Seconds()
                    - Plan.From.Get_Seconds()) / Span;
                Plan.MaximumTime = Sampled.Get_AcceptedEnd();
            }
        }
        if (Plan.Requested > Plan.From && Plan.CriticalFractions.IsEmpty())
        {
            const auto Reason = FName{TEXT("SamplingBudgetExceeded")};
            InCtx.PlayerFragment->_Receipts.CloseWindows(Params.Get_Identity(), Reason);
            FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, InAction.Token, Reason);
            Action._Result = FCk_ProceduralAction_Result{
                ECk_ProceduralAction_Outcome::SamplingBudgetExceeded, ECk_ProceduralAction_Recovery::Pending, Reason};
            Action.BeginFailureContext(InAction.Handle, Params);
            Action.CommitState(InAction.Handle, Params, ECk_ProceduralAction_State::Cancelling);
        }
        if (NOT Plan.CriticalFractions.IsEmpty())
        { InCtx.ClipPlans.Add(MoveTemp(Plan)); }
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoHoldClip(
        ContextType& InCtx,
        const ActionType& InAction)
        -> void
    {
        auto& Action = *InAction.Action;
        const auto ActionHandle = InAction.Handle;
        if (NOT InCtx.DefaultValid || InCtx.DeltaT <= FCk_Time{}
            || (Action._State != ECk_ProceduralAction_State::Acquiring
                && Action._State != ECk_ProceduralAction_State::Playing)
            || Action._PendingStageKey.IsSet() || Action._PendingHoldStageKey.IsSet()
            || NOT ActionHandle.Has<FFragment_ProceduralAction_Compiled>()
            || InCtx.ClipPlans.ContainsByPredicate([ActionHandle](const auto& InPlan)
                { return InPlan.Action == ActionHandle; }))
        { return; }
        const auto& Compiled = ActionHandle.Get<FFragment_ProceduralAction_Compiled>();
        auto Hold = FCk_ArticulationTokenCandidate{};
        auto Valid = false;
        if (Action._State == ECk_ProceduralAction_State::Acquiring)
        {
            Valid = ck_procedural_action_player_service::BuildEntryHoldCandidate(
                Compiled.Get_BoundLimbIds(), Compiled.Get_Clip()->Get_HasBodyChannel(),
                Action._EntryPose, InAction.Token, Hold);
        }
        else
        {
            auto Evaluated = procedural_action::FEvaluatedPose{};
            Valid = procedural_action::Evaluate(*Compiled.Get_Clip(), Action._InitialPose,
                Action._AcceptedTime, Evaluated)
                && ck_procedural_action_player_service::BuildClipCandidate(InCtx.Subject,
                    Compiled, Evaluated, Action._EntryPose, InCtx.DefaultPose, InAction.Token, 0, Hold);
        }
        if (Valid)
        {
            InCtx.Candidates.Add(MoveTemp(Hold));
            return;
        }
        const auto Reason = FName{TEXT("ClipHoldRejected")};
        InCtx.PlayerFragment->_Receipts.CloseWindows(InAction.Params->Get_Identity(), Reason);
        FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, InAction.Token, Reason);
        Action._Result = FCk_ProceduralAction_Result{
            ECk_ProceduralAction_Outcome::SubjectPoseRejected, ECk_ProceduralAction_Recovery::Pending, Reason};
        Action.CommitState(ActionHandle, *InAction.Params, ECk_ProceduralAction_State::Cancelling);
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoPreparePublications(
        ContextType& InCtx,
        const ck_procedural_action_player_service::FClipStepResult& InAccepted)
        -> void
    {
        // Refinement may have last probed a rejected fraction. Return settlement must use
        // the proposal for the exact accepted fraction that is staged.
        auto FinalReturnScratch = TArray<FCk_ArticulationTokenCandidate>{};
        FCk_ProceduralLimbControl_Control::AppendReturnCandidates(InCtx.Subject, InCtx.DefaultPose,
            FinalReturnScratch, InAccepted.Fraction);
        for (auto PlanIndex = 0; PlanIndex < InCtx.ClipPlans.Num(); ++PlanIndex)
        {
            auto& Plan = InCtx.ClipPlans[PlanIndex];
            auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
            const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
            const auto& Clip = *Plan.Action.Get<FFragment_ProceduralAction_Compiled>().Get_Clip();
            const auto To = InAccepted.StepTimes[PlanIndex];
            Action._PendingToTime = Plan.EntryBlend ? Action._AcceptedTime : To;
            Action._PendingBlendTo = Plan.EntryBlend ? To : Action._EntryBlendAccepted;
            Action._PendingIsEntryBlend = Plan.EntryBlend;
            Action._PendingBacklog = Plan.EntryBlend ? Action._PlaybackBacklog
                : FCk_Time{FMath::Max(0.0, Plan.Requested.Get_Seconds() - To.Get_Seconds())};
            auto Events = TArray<FCk_ProceduralAction_Event>{};
            auto Windows = TArray<FName>{};
            if (NOT Plan.EntryBlend)
            {
                for (const auto& Event : Clip.Get_Events())
                {
                    if (DoIsInInterval(Action, Plan.From, Event.Get_Time(), To))
                    {
                        const auto Kind = Event.Get_Kind() == procedural_action::EEventKind::WindowExit
                            ? ECk_ProceduralAction_EventKind::WindowExit
                            : (Event.Get_Kind() == procedural_action::EEventKind::WindowEnter
                                ? ECk_ProceduralAction_EventKind::WindowEnter
                                : ECk_ProceduralAction_EventKind::Point);
                        Events.Add(FCk_ProceduralAction_Event{Event.Get_Time(), Kind,
                            Event.Get_Id(), Event.Get_AuthoredIndex()});
                    }
                    // Window boundaries are publication barriers. The interval owns the
                    // state at From; events at To affect the following interval only.
                    if (Event.Get_Time() <= Plan.From)
                    {
                        if (Event.Get_Kind() == procedural_action::EEventKind::WindowEnter)
                        { Windows.AddUnique(Event.Get_Id()); }
                        else if (Event.Get_Kind() == procedural_action::EEventKind::WindowExit)
                        { Windows.Remove(Event.Get_Id()); }
                    }
                }
            }
            auto EvaluatedFinal = procedural_action::FEvaluatedPose{};
            if (NOT Plan.EntryBlend)
            { procedural_action::Evaluate(Clip, Action._InitialPose, To, EvaluatedFinal); }
            const auto Continuity = NOT InCtx.PlayerFragment->_Receipts.HasAccepted(Params.Get_Identity())
                ? ECk_ProceduralAction_Continuity::FirstSample
                : (Action._HasHoldSinceLastReceipt
                    ? ECk_ProceduralAction_Continuity::ControllerChanged
                    : ECk_ProceduralAction_Continuity::Continuous);
            Action._PendingClipInterval = FCk_ProceduralAction_Interval{
                Action._AcceptedTime, Action._PendingToTime,
                Plan.EntryBlend || NOT Clip.Get_Phases().IsValidIndex(EvaluatedFinal.Get_PhaseIndex())
                    ? NAME_None : Clip.Get_Phases()[EvaluatedFinal.Get_PhaseIndex()].Get_Id(),
                Continuity, Windows, Events};
            const auto& QualifiedSamples = InAccepted.QualifiedSamples[PlanIndex];
            const auto QualifiedCount = QualifiedSamples.Num();
            Action._PendingLastIntermediatePose.Reset();
            if (QualifiedCount > 1)
            { Action._PendingLastIntermediatePose = QualifiedSamples[QualifiedCount - 2]; }
            Action._PendingPredictedPose = InAccepted.PredictedByPlan[PlanIndex];
            Action._PendingClipSampling = FCk_ProceduralAction_SamplingReport{
                QualifiedCount, 1, InAccepted.MaxTravels[PlanIndex], InAccepted.MaxAngularSteps[PlanIndex],
                Action._PendingBacklog};
        }
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    MeasureOtherLimits(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        if ((InCtx.Candidates.IsEmpty() && InCtx.ClipPlans.IsEmpty()) || InCtx.DeltaT <= FCk_Time{}
            || InCtx.HasUnsettledActionStage)
        { return EStepResult::Abort; }
        for (auto Index = 0; Index < InCtx.Candidates.Num(); ++Index)
        {
            const auto& Token = InCtx.Candidates[Index].Get_Token();
            const auto InstanceId = Token.Get_InstanceId();
            auto Limit = ck_procedural_action_player_service::FOtherStepLimit{};
            if (Index < InCtx.ReturnCandidateCount)
            {
                auto Budget = FCk_ProceduralAction_Sampling{};
                if (NOT FCk_ProceduralLimbControl_Control::Get_ReturnMotionBudget(InCtx.Subject, Token, Budget)
                    || NOT FCk_ProceduralLimbControl_Control::Get_ReturnActualPose(InCtx.Subject, Token, Limit.Previous))
                { return DoRecordStall(InCtx, FName{TEXT("ReturnBudgetUnavailable")}); }
                Limit.MaxTravel = Budget.Get_MaxEndpointTravelCm();
                Limit.MaxAngular = Budget.Get_MaxSegmentAngularStepDegrees();
            }
            else
            {
                const auto* Owner = InCtx.ActionSnapshot.FindByPredicate([InstanceId](const auto& InAction)
                {
                    return ck::IsValid(InAction) && InAction.Has<FFragment_ProceduralAction_Params>()
                        && static_cast<uint64>(InAction.Get<FFragment_ProceduralAction_Params>()
                            .Get_Identity().Get_Value()) == InstanceId;
                });
                if (Owner == nullptr || NOT Owner->Has<FFragment_ProceduralAction>())
                { return DoRecordStall(InCtx, FName{TEXT("CandidateOwnerMissing")}); }
                const auto& Sampling = Owner->Get<FFragment_ProceduralAction_Params>().Get_Tuning().Get_Sampling();
                Limit.Previous = Owner->Get<FFragment_ProceduralAction>()._MostRecentObservedPose;
                Limit.MaxTravel = Sampling.Get_MaxEndpointTravelCm();
                Limit.MaxAngular = Sampling.Get_MaxSegmentAngularStepDegrees();
            }
            InCtx.OtherLimits.Add(InstanceId, MoveTemp(Limit));
        }
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoQualifiesOther(
        const ContextType& InCtx,
        const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
        const FCk_ArticulationBatchEvaluation& InEvaluation,
        int32 InFirst,
        int32 InLast)
        -> bool
    {
        if (InEvaluation.Get_PredictedTokenPoses().Num() != InCandidates.Num())
        { return false; }
        for (auto Index = InFirst; Index < InLast; ++Index)
        {
            const auto* Limit = InCtx.OtherLimits.Find(InCandidates[Index].Get_Token().Get_InstanceId());
            if (Limit == nullptr)
            { return false; }
            auto Travel = 0.0f;
            auto Angular = 0.0f;
            procedural_action::MeasureMotion(Limit->Previous,
                InEvaluation.Get_PredictedTokenPoses()[Index], Travel, Angular);
            if (NOT FMath::IsFinite(Travel) || NOT FMath::IsFinite(Angular)
                || Travel > Limit->MaxTravel || Angular > Limit->MaxAngular)
            { return false; }
        }
        return true;
    }
}
