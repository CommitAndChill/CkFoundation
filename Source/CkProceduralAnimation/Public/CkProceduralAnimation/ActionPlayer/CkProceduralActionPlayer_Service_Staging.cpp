#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/Core/CkProceduralActionMotion.h"
#include "CkProceduralAnimation/Core/CkProceduralClipTimeMapping.h"
#include "CkProceduralAnimation/Core/CkProceduralReturnPrefix.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Validation/CkIsValid.h"

namespace ck_procedural_action_player_service_staging
{
    auto
    SortByInstance(
        TArray<FCk_ArticulationTokenCandidate>& InOutCandidates)
        -> void
    {
        InOutCandidates.StableSort([](const FCk_ArticulationTokenCandidate& InA, const FCk_ArticulationTokenCandidate& InB)
        { return InA.Get_Token().Get_InstanceId() < InB.Get_Token().Get_InstanceId(); });
    }

    // Appends the candidates that are not returns, keeping the actions' holds beside a new return proposal.
    auto
    AppendNonReturns(
        const ck::FContext_ProceduralActionPlayer_Service& InCtx,
        TArray<FCk_ArticulationTokenCandidate>& OutCandidates)
        -> void
    {
        for (const auto& Candidate : InCtx.Candidates)
        {
            if (NOT InCtx.ReturnTokens.Contains(Candidate.Get_Token().Get_InstanceId()))
            { OutCandidates.Add(Candidate); }
        }
    }

    auto
    ReattachmentEndpointsHold(
        const ck::FContext_ProceduralActionPlayer_Service& InCtx,
        const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
        const FCk_ArticulationBatchEvaluation& InEvaluation)
        -> bool
    {
        if (InEvaluation.Get_PredictedTokenPoses().Num() != InCandidates.Num())
        { return false; }
        for (auto Index = 0; Index < InCandidates.Num(); ++Index)
        {
            if (NOT InCtx.ReturnTokens.Contains(InCandidates[Index].Get_Token().Get_InstanceId()))
            { continue; }
            auto Actual = FCk_ProceduralArticulation_Pose{};
            if (NOT FCk_ProceduralLimbControl_Control::Get_ReturnActualPose(
                InCtx.Subject, InCandidates[Index].Get_Token(), Actual))
            { return false; }
            const auto& Predicted = InEvaluation.Get_PredictedTokenPoses()[Index];
            if (Actual.Get_Limbs().Num() != Predicted.Get_Limbs().Num()
                || Actual.Get_HasBodyPresentation() != Predicted.Get_HasBodyPresentation())
            { return false; }
            for (const auto& Limb : Actual.Get_Limbs())
            {
                const auto* Match = Predicted.Get_Limbs().FindByPredicate([&Limb](const auto& InLimb)
                { return InLimb.Get_Id() == Limb.Get_Id(); });
                if (Match == nullptr || FVector::Distance(Limb.Get_AcceptedEndpoint().GetLocation(),
                        Match->Get_AcceptedEndpoint().GetLocation()) > 0.02
                    || FMath::RadiansToDegrees(Limb.Get_AcceptedEndpoint().GetRotation().AngularDistance(
                        Match->Get_AcceptedEndpoint().GetRotation())) > 0.1)
                { return false; }
            }
            if (Actual.Get_HasBodyPresentation()
                && NOT Actual.Get_BodyPresentation().Equals(Predicted.Get_BodyPresentation(), 0.02))
            { return false; }
        }
        return true;
    }

    auto
    ProbeReturnFraction(
        const ck::FContext_ProceduralActionPlayer_Service& InCtx,
        double InFraction,
        TArray<FCk_ArticulationTokenCandidate>& OutCandidates,
        FCk_ArticulationBatchEvaluation& OutEvaluation)
        -> ck::procedural_action::EReturnPrefixProbe
    {
        FCk_ProceduralLimbControl_Control::AppendReturnCandidates(InCtx.Subject, InCtx.DefaultPose,
            OutCandidates, InFraction);
        if (OutCandidates.Num() != InCtx.ReturnCandidateCount)
        { return ck::procedural_action::EReturnPrefixProbe::Interrupted; }
        AppendNonReturns(InCtx, OutCandidates);
        SortByInstance(OutCandidates);
        const auto Result = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
            InCtx.Subject, OutCandidates, OutEvaluation);
        if (Result == ECk_ArticulationControlResult::Busy
            || Result == ECk_ArticulationControlResult::StaleInput)
        { return ck::procedural_action::EReturnPrefixProbe::Interrupted; }
        return Result == ECk_ArticulationControlResult::Accepted
            && ck::FTechnique_ProceduralActionPlayer_Service::DoQualifiesOther(
                InCtx, OutCandidates, OutEvaluation, 0, OutCandidates.Num())
                ? ck::procedural_action::EReturnPrefixProbe::Qualified
                : ck::procedural_action::EReturnPrefixProbe::Rejected;
    }

    // With no prefix of the returns qualifying, a held return whose next step is discontinuous asks the
    // articulation to reattach instead of staging a jump.
    auto
    RequestReattachmentIfDiscontinuous(
        ck::FContext_ProceduralActionPlayer_Service& InCtx)
        -> bool
    {
        auto Held = TArray<FCk_ArticulationTokenCandidate>{};
        if (NOT FCk_ProceduralLimbControl_Control::AppendHeldReturnCandidates(InCtx.Subject, Held)
            || Held.Num() != InCtx.ReturnCandidateCount)
        { return false; }
        AppendNonReturns(InCtx, Held);
        SortByInstance(Held);
        auto HeldEvaluation = FCk_ArticulationBatchEvaluation{};
        if (FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(InCtx.Subject, Held, HeldEvaluation)
                != ECk_ArticulationControlResult::Accepted
            || NOT ReattachmentEndpointsHold(InCtx, Held, HeldEvaluation))
        { return false; }
        auto Discontinuous = false;
        for (auto Index = 0; Index < Held.Num(); ++Index)
        {
            if (NOT InCtx.ReturnTokens.Contains(Held[Index].Get_Token().Get_InstanceId()))
            { continue; }
            const auto* Limit = InCtx.OtherLimits.Find(Held[Index].Get_Token().Get_InstanceId());
            if (Limit == nullptr)
            { continue; }
            auto Travel = 0.0f;
            auto Angular = 0.0f;
            ck::procedural_action::MeasureMotion(Limit->Previous,
                HeldEvaluation.Get_PredictedTokenPoses()[Index], Travel, Angular);
            Discontinuous |= FMath::IsFinite(Travel) && FMath::IsFinite(Angular)
                && (Travel > Limit->MaxTravel || Angular > Limit->MaxAngular);
        }
        return Discontinuous && FCk_ProceduralLimbControl_Control::RequestReattachment(InCtx.Subject);
    }
}

namespace ck
{
    auto
    FTechnique_ProceduralActionPlayer_Service::
    StageReturns(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        if (NOT InCtx.ClipPlans.IsEmpty())
        { return EStepResult::Continue; }

        ck_procedural_action_player_service_staging::SortByInstance(InCtx.Candidates);
        const auto FullBatchResult = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
            InCtx.Subject, InCtx.Candidates, InCtx.Evaluation);
        if (FullBatchResult == ECk_ArticulationControlResult::Busy
            || FullBatchResult == ECk_ArticulationControlResult::StaleInput)
        { return DoRecordStall(InCtx, FName{TEXT("ArticulationBusy")}); }
        if (InCtx.ReattachmentBarrier && (FullBatchResult != ECk_ArticulationControlResult::Accepted
            || NOT ck_procedural_action_player_service_staging::ReattachmentEndpointsHold(
                InCtx, InCtx.Candidates, InCtx.Evaluation)))
        { return DoRecordStall(InCtx, FName{TEXT("ReattachmentHoldRejected")}); }
        if (FullBatchResult == ECk_ArticulationControlResult::Accepted
            && (InCtx.ReattachmentBarrier
                || DoQualifiesOther(InCtx, InCtx.Candidates, InCtx.Evaluation, 0, InCtx.Candidates.Num())))
        { return EStepResult::Continue; }

        if (InCtx.ReturnCandidateCount > 0)
        {
            auto SearchInterrupted = false;
            auto ProbeCount = 0;
            const auto BestFraction = procedural_action::FindReturnPrefix(
                [&InCtx](double InFraction) -> procedural_action::EReturnPrefixProbe
                {
                    auto StepCandidates = TArray<FCk_ArticulationTokenCandidate>{};
                    auto StepEvaluation = FCk_ArticulationBatchEvaluation{};
                    const auto Probe = ck_procedural_action_player_service_staging::ProbeReturnFraction(
                        InCtx, InFraction, StepCandidates, StepEvaluation);
                    if (Probe == procedural_action::EReturnPrefixProbe::Qualified)
                    {
                        InCtx.Candidates = MoveTemp(StepCandidates);
                        InCtx.Evaluation = MoveTemp(StepEvaluation);
                    }
                    return Probe;
                }, ProbeCount, SearchInterrupted);
            if (SearchInterrupted)
            { return DoRecordStall(InCtx, FName{TEXT("ReturnPrefixInterrupted")}); }
            if (BestFraction.IsSet())
            { return EStepResult::Continue; }
        }
        if (InCtx.ReturnCandidateCount > 0 && NOT InCtx.ReattachmentBarrier
            && ck_procedural_action_player_service_staging::RequestReattachmentIfDiscontinuous(InCtx))
        { return DoRecordStall(InCtx, FName{TEXT("ReattachmentRequested")}); }
        return DoRecordStall(InCtx, FName{TEXT("ReturnPrefixRejected")});
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    StageClips(
        ThisType& InSelf,
        ContextType& InCtx)
        -> EStepResult
    {
        if (InCtx.ClipPlans.IsEmpty())
        { return EStepResult::Continue; }

        const auto& ClipPlans = InCtx.ClipPlans;
        auto Fractions = TArray<double>{};
        auto MaximumFraction = 1.0;
        auto MaximumSteps = procedural_action::receipt_limits::MaxSamplingSteps;
        for (const auto& Plan : ClipPlans)
        {
            MaximumFraction = FMath::Min(MaximumFraction, Plan.MaximumFraction);
            MaximumSteps = FMath::Min(MaximumSteps,
                Plan.Action.Get<FFragment_ProceduralAction_Params>().Get_Tuning().Get_Sampling().Get_MaxSamples());
        }
        for (const auto& Plan : ClipPlans)
        {
            for (const auto Fraction : Plan.CriticalFractions)
            {
                if (Fraction > 0.0 && Fraction <= MaximumFraction)
                { Fractions.AddUnique(Fraction); }
            }
        }
        if (MaximumFraction > 0.0)
        { Fractions.AddUnique(MaximumFraction); }
        Fractions.Sort();
        if (Fractions.Num() > MaximumSteps)
        { Fractions.SetNum(MaximumSteps); }
        auto Accepted = ck_procedural_action_player_service::FClipStepResult{};
        for (const auto& Plan : ClipPlans)
        {
            Accepted.PredictedByPlan.Add(Plan.Action.Get<FFragment_ProceduralAction>()._MostRecentObservedPose);
            Accepted.QualifiedSamples.Emplace();
            Accepted.MaxTravels.Add(0.0f);
            Accepted.MaxAngularSteps.Add(0.0f);
        }
        auto FinalFraction = 0.0;
        auto FinalCandidates = TArray<FCk_ArticulationTokenCandidate>{};
        auto FinalEvaluation = FCk_ArticulationBatchEvaluation{};
        for (const auto ProposedFraction : Fractions)
        {
            auto Fraction = ProposedFraction;
            auto StepAccepted = false;
            for (auto Refinement = 0; Refinement <= 32; ++Refinement)
            {
                if (Fraction <= FinalFraction || NOT FMath::IsFinite(Fraction))
                { break; }
                auto StepCandidates = TArray<FCk_ArticulationTokenCandidate>{};
                FCk_ProceduralLimbControl_Control::AppendReturnCandidates(InCtx.Subject, InCtx.DefaultPose,
                    StepCandidates, Fraction);
                for (auto Index = InCtx.ReturnCandidateCount; Index < InCtx.Candidates.Num(); ++Index)
                { StepCandidates.Add(InCtx.Candidates[Index]); }
                auto StepTimes = TArray<FCk_Time>{};
                auto StepPolicies = TArray<procedural_action::FEvaluatedPose>{};
                auto StepMaxTravels = Accepted.MaxTravels;
                auto StepMaxAngularSteps = Accepted.MaxAngularSteps;
                auto CanEvaluate = true;
                for (const auto& Plan : ClipPlans)
                {
                    const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                    const auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
                    const auto& Compiled = Plan.Action.Get<FFragment_ProceduralAction_Compiled>();
                    auto Time = FCk_Time{};
                    CanEvaluate = procedural_action::ResolveClipSampleTime(Plan.From,
                        Plan.Requested, Plan.MaximumTime, Plan.CriticalFractions,
                        Plan.CriticalTimes, Fraction, Time);
                    if (NOT CanEvaluate)
                    { break; }
                    auto Pose = procedural_action::FEvaluatedPose{};
                    if (Plan.EntryBlend)
                    {
                        auto Target = procedural_action::FEvaluatedPose{};
                        CanEvaluate = procedural_action::Evaluate(*Compiled.Get_Clip(),
                            Action._InitialPose, FCk_Time{}, Target)
                            && ck_procedural_action_player_service::BlendClipEntry(
                                Action._InitialPose, Target,
                                Time.Get_Seconds() / Params.Get_Tuning().Get_Transition().Get_EntryBlend()
                                    .Get_Seconds(), Pose);
                    }
                    else
                    {
                        CanEvaluate = procedural_action::Evaluate(*Compiled.Get_Clip(),
                            Action._InitialPose, Time, Pose);
                    }
                    if (NOT CanEvaluate)
                    { break; }
                    auto Candidate = FCk_ArticulationTokenCandidate{};
                    CanEvaluate = ck_procedural_action_player_service::BuildClipCandidate(InCtx.Subject,
                        Compiled, Pose, Action._EntryPose, InCtx.DefaultPose,
                        Action.Get_Token(Params), static_cast<uint64>(Action._NextSampleSequence), Candidate);
                    if (NOT CanEvaluate)
                    { break; }
                    StepCandidates.Add(MoveTemp(Candidate));
                    StepTimes.Add(Time);
                    StepPolicies.Add(MoveTemp(Pose));
                }
                if (CanEvaluate)
                {
                    ck_procedural_action_player_service_staging::SortByInstance(StepCandidates);
                    auto StepEvaluation = FCk_ArticulationBatchEvaluation{};
                    const auto StepResult = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
                        InCtx.Subject, StepCandidates, StepEvaluation);
                    CanEvaluate = StepResult == ECk_ArticulationControlResult::Accepted;
                    if (CanEvaluate && StepEvaluation.Get_PredictedTokenPoses().Num() == StepCandidates.Num())
                    {
                        for (auto Index = 0; Index < StepCandidates.Num(); ++Index)
                        {
                            if (InCtx.OtherLimits.Contains(StepCandidates[Index].Get_Token().Get_InstanceId())
                                && NOT DoQualifiesOther(InCtx, StepCandidates, StepEvaluation, Index, Index + 1))
                            { CanEvaluate = false; break; }
                        }
                        if (NOT CanEvaluate)
                        { Fraction = (FinalFraction + Fraction) * 0.5; continue; }
                        for (auto PlanIndex = 0; PlanIndex < ClipPlans.Num(); ++PlanIndex)
                        {
                            const auto& Plan = ClipPlans[PlanIndex];
                            const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                            const auto& Sampling = Params.Get_Tuning().Get_Sampling();
                            const auto CandidateIndex = StepCandidates.IndexOfByPredicate(
                                [&Params](const auto& InCandidate)
                                { return InCandidate.Get_Token().Get_InstanceId()
                                    == static_cast<uint64>(Params.Get_Identity().Get_Value()); });
                            if (CandidateIndex == INDEX_NONE)
                            { CanEvaluate = false; break; }
                            const auto& Predicted = StepEvaluation.Get_PredictedTokenPoses()[CandidateIndex];
                            auto Travel = 0.0f;
                            auto Angular = 0.0f;
                            procedural_action::MeasureMotion(Accepted.PredictedByPlan[PlanIndex], Predicted, Travel, Angular);
                            if (NOT FMath::IsFinite(Travel) || NOT FMath::IsFinite(Angular)
                                || Travel > Sampling.Get_MaxEndpointTravelCm()
                                || Angular > Sampling.Get_MaxSegmentAngularStepDegrees())
                            {
                                CanEvaluate = false;
                                break;
                            }
                            StepMaxTravels[PlanIndex] = FMath::Max(StepMaxTravels[PlanIndex], Travel);
                            StepMaxAngularSteps[PlanIndex] = FMath::Max(StepMaxAngularSteps[PlanIndex], Angular);
                            const auto& BoundLimbIds = Plan.Action.Get<FFragment_ProceduralAction_Compiled>()
                                .Get_BoundLimbIds();
                            for (auto LimbIndex = 0; LimbIndex < StepPolicies[PlanIndex].Get_Limbs().Num(); ++LimbIndex)
                            {
                                const auto Id = BoundLimbIds[LimbIndex];
                                const auto* Limb = Predicted.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
                                { return InLimb.Get_Id() == Id; });
                                if (Limb == nullptr || Limb->Get_ChainState() == ECk_ProceduralRig_ChainState::Crossing)
                                { CanEvaluate = false; break; }
                            }
                            if (NOT CanEvaluate)
                            { break; }
                            if (NOT Plan.EntryBlend)
                            {
                                const auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
                                auto Events = 0;
                                for (const auto& Event : Plan.Action.Get<FFragment_ProceduralAction_Compiled>()
                                    .Get_Clip()->Get_Events())
                                {
                                    if (DoIsInInterval(Action, Plan.From, Event.Get_Time(), StepTimes[PlanIndex]))
                                    { ++Events; }
                                }
                                if (Events > procedural_action::receipt_limits::MaxEventsPerReceipt)
                                { CanEvaluate = false; break; }
                            }
                        }
                    }
                    else
                    { CanEvaluate = false; }
                    if (CanEvaluate)
                    {
                        for (auto Index = 0; Index < StepCandidates.Num(); ++Index)
                        {
                            if (auto* Limit = InCtx.OtherLimits.Find(StepCandidates[Index].Get_Token().Get_InstanceId()))
                            { Limit->Previous = StepEvaluation.Get_PredictedTokenPoses()[Index]; }
                        }
                        for (auto PlanIndex = 0; PlanIndex < ClipPlans.Num(); ++PlanIndex)
                        {
                            const auto& Params = ClipPlans[PlanIndex].Action.Get<FFragment_ProceduralAction_Params>();
                            const auto CandidateIndex = StepCandidates.IndexOfByPredicate(
                                [&Params](const auto& InCandidate)
                                { return InCandidate.Get_Token().Get_InstanceId()
                                    == static_cast<uint64>(Params.Get_Identity().Get_Value()); });
                            const auto Pose = StepEvaluation.Get_PredictedTokenPoses()[CandidateIndex];
                            Accepted.PredictedByPlan[PlanIndex] = Pose;
                            Accepted.QualifiedSamples[PlanIndex].Add(Pose);
                        }
                        FinalFraction = Fraction;
                        Accepted.MaxTravels = MoveTemp(StepMaxTravels);
                        Accepted.MaxAngularSteps = MoveTemp(StepMaxAngularSteps);
                        FinalCandidates = MoveTemp(StepCandidates);
                        FinalEvaluation = MoveTemp(StepEvaluation);
                        Accepted.StepTimes = MoveTemp(StepTimes);
                        StepAccepted = true;
                        break;
                    }
                }
                Fraction = (FinalFraction + Fraction) * 0.5;
            }
            if (NOT StepAccepted || FinalFraction < ProposedFraction)
            { break; }
        }
        if (FinalFraction <= 0.0)
        {
            DoRejectClipPlans(InCtx);
            return EStepResult::Abort;
        }
        InCtx.Candidates = MoveTemp(FinalCandidates);
        InCtx.Evaluation = MoveTemp(FinalEvaluation);
        Accepted.Fraction = FinalFraction;
        DoPreparePublications(InCtx, Accepted);
        return EStepResult::Continue;
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoRejectClipPlans(
        ContextType& InCtx)
        -> void
    {
        for (auto& Plan : InCtx.ClipPlans)
        {
            if (ck::Is_NOT_Valid(Plan.Action)
                || NOT Plan.Action.Has<FFragment_ProceduralAction>()
                || Plan.Action.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
            const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
            const auto Reason = FName{TEXT("SubjectPoseRejected")};
            InCtx.PlayerFragment->_Receipts.CloseWindows(Params.Get_Identity(), Reason);
            FCk_ProceduralLimbControl_Control::BeginReturn(InCtx.Subject, Action.Get_Token(Params), Reason);
            Action._Result = FCk_ProceduralAction_Result{
                ECk_ProceduralAction_Outcome::SubjectPoseRejected, ECk_ProceduralAction_Recovery::Pending, Reason};
            Action.CommitState(Plan.Action, Params, ECk_ProceduralAction_State::Cancelling);
        }
    }

    auto
    FTechnique_ProceduralActionPlayer_Service::
    DoIsInInterval(
        const FFragment_ProceduralAction& InAction,
        FCk_Time InFrom,
        FCk_Time InEventTime,
        FCk_Time InTo)
        -> bool
    {
        return (InEventTime > InFrom
                || (InFrom == FCk_Time{} && NOT InAction._HasAuthoredReceipt && InEventTime == FCk_Time{}))
            && InEventTime <= InTo;
    }
}
