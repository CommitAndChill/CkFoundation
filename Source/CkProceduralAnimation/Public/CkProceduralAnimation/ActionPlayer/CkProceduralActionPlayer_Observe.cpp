#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Processor.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/Core/CkProceduralActionMotion.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_Observe);

namespace ck_procedural_action_player_observe
{
    constexpr auto MicrometresPerCentimetre = 10000.0;
    constexpr auto MillidegreesPerDegree = 1000.0;

    // Integer units keep the measurement exact enough to compare against its budget without a float in the context.
    auto
    RecordSamplingExcess(
        FCk_ProceduralAction_FailureContext& InOutContext,
        float InTravelCm,
        float InAngularDegrees,
        const FCk_ProceduralAction_Sampling& InLimits)
        -> bool
    {
        if (FMath::IsFinite(InTravelCm) && InTravelCm > InLimits.Get_MaxEndpointTravelCm())
        {
            InOutContext.Set_BudgetExcess(FName{TEXT("EndpointTravelMicrometres")},
                FMath::RoundToInt64(InTravelCm * MicrometresPerCentimetre),
                FMath::RoundToInt64(InLimits.Get_MaxEndpointTravelCm() * MicrometresPerCentimetre));
            return true;
        }
        if (FMath::IsFinite(InAngularDegrees) && InAngularDegrees > InLimits.Get_MaxSegmentAngularStepDegrees())
        {
            InOutContext.Set_BudgetExcess(FName{TEXT("SegmentAngleMillidegrees")},
                FMath::RoundToInt64(InAngularDegrees * MillidegreesPerDegree),
                FMath::RoundToInt64(InLimits.Get_MaxSegmentAngularStepDegrees() * MillidegreesPerDegree));
            return true;
        }
        return false;
    }
}


namespace ck
{
    auto
    FProcessor_ProceduralActionPlayer_Observe::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer)
        -> void
    {
        const auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(InHandle);
        const auto ActionSnapshot = InPlayer._Actions;
        for (auto ActionHandle : ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            const auto Identity = Params.Get_Identity();
            const auto Token = Action.Get_Token(Params);
            if ((Action._State == ECk_ProceduralAction_State::Acquiring
                    || Action._State == ECk_ProceduralAction_State::Playing)
                && Action._PendingHoldStageKey.IsSet())
            {
                auto HoldRevision = uint64{0};
                auto HeldPose = FCk_ProceduralArticulation_Pose{};
                const auto HoldSettlement = FCk_ProceduralArticulation_Control::ObserveSettlement(Subject,
                    Action._PendingHoldStageKey.GetValue(), HoldRevision, HeldPose);
                if (HoldSettlement == ECk_ArticulationSettlement::Settled)
                {
                    auto Travel = 0.0f;
                    auto Angular = 0.0f;
                    procedural_action::MeasureMotion(Action._MostRecentObservedPose, HeldPose, Travel, Angular);
                    if (Travel > KINDA_SMALL_NUMBER || Angular > KINDA_SMALL_NUMBER)
                    { Action._HasHoldSinceLastReceipt = true; }
                    Action._MostRecentObservedPose = HeldPose;
                    Action._PendingHoldStageKey.Reset();
                }
                else if (HoldSettlement == ECk_ArticulationSettlement::Faulted
                    || HoldSettlement == ECk_ArticulationSettlement::ClaimLost)
                {
                    Action._PendingHoldStageKey.Reset();
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::PublicationFailed,
                        ECk_ProceduralAction_Recovery::Pending, TEXT("HoldSettlementFailed")};
                    InPlayer._Receipts.CloseWindows(Identity, TEXT("HoldSettlementFailed"));
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                        TEXT("HoldSettlementFailed"));
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                    continue;
                }
            }
            if (Action._PendingStageKey.IsSet())
            {
                auto Revision = uint64{0};
                auto Actual = FCk_ProceduralArticulation_Pose{};
                const auto Settlement = FCk_ProceduralArticulation_Control::ObserveSettlement(Subject,
                    Action._PendingStageKey.GetValue(), Revision, Actual);
                if (Settlement == ECk_ArticulationSettlement::Settled)
                {
                    const auto Previous = Action._MostRecentObservedPose;
                    auto Travel = 0.0f;
                    auto Angular = 0.0f;
                    const auto& LastAcceptedCandidate = Action._PendingLastIntermediatePose.IsSet()
                        ? Action._PendingLastIntermediatePose.GetValue() : Previous;
                    procedural_action::MeasureMotion(LastAcceptedCandidate, Actual, Travel, Angular);
                    auto PredictionTravel = 0.0f;
                    auto PredictionAngular = 0.0f;
                    procedural_action::MeasureMotion(Action._PendingPredictedPose, Actual,
                        PredictionTravel, PredictionAngular);
                    const auto Limits = Params.Get_Tuning().Get_Sampling();
                    if (NOT FMath::IsFinite(Travel) || NOT FMath::IsFinite(Angular)
                        || Travel > Limits.Get_MaxEndpointTravelCm()
                        || Angular > Limits.Get_MaxSegmentAngularStepDegrees()
                        || NOT FMath::IsFinite(PredictionTravel)
                        || NOT FMath::IsFinite(PredictionAngular)
                        || PredictionTravel > Limits.Get_MaxEndpointTravelCm()
                        || PredictionAngular > Limits.Get_MaxSegmentAngularStepDegrees())
                    {
                        Action._PendingStageKey.Reset();
                        Action._Result = FCk_ProceduralAction_Result{
                            ECk_ProceduralAction_Outcome::SamplingBudgetExceeded,
                            ECk_ProceduralAction_Recovery::Pending, TEXT("SettledMotionExceededSamplingLimits")};
                        auto& Context = Action.BeginFailureContext(ActionHandle, Params);
                        if (NOT ck_procedural_action_player_observe::RecordSamplingExcess(Context, Travel, Angular, Limits))
                        {
                            ck_procedural_action_player_observe::RecordSamplingExcess(Context, PredictionTravel,
                                PredictionAngular, Limits);
                        }
                        InPlayer._Receipts.CloseWindows(Identity,
                            TEXT("SettledMotionExceededSamplingLimits"));
                        FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                            TEXT("SettledMotionExceededSamplingLimits"));
                        Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                        continue;
                    }
                    auto ContentRevision = int64{0};
                    if (ActionHandle.Has<FFragment_ProceduralAction_Compiled>())
                    {
                        // Provenance fingerprint, not a monotonic asset edit revision.
                        const auto Hash = ActionHandle.Get<FFragment_ProceduralAction_Compiled>()
                            .Get_Clip()->Get_ContentHash();
                        const auto Masked = Hash & static_cast<uint64>(MAX_int64);
                        ContentRevision = static_cast<int64>(Masked == 0 ? 1 : Masked);
                    }
                    const auto Sampling = FCk_ProceduralAction_SamplingReport{
                        Action._PendingClipSampling.Get_EvaluatedSamples(), 1,
                        FMath::Max(Action._PendingClipSampling.Get_MaxEndpointTravelCm(), Travel),
                        FMath::Max(Action._PendingClipSampling.Get_MaxSegmentAngularStepDegrees(), Angular),
                        Action._PendingClipSampling.Get_Backlog()};
                    const auto Receipt = FCk_ProceduralAction_Receipt{true,
                        // Release-1 articulation layout is sealed after construction; its topology version is 1.
                        FCk_ProceduralAction_Stamp{Identity, Action._Generation,
                            static_cast<int64>(Action._PendingStageKey->Get_SampleSequence()),
                            static_cast<int64>(Revision), 1, ContentRevision},
                        Action._PendingClipInterval, Previous, Actual, Sampling};
                    const auto Published = InPlayer._Receipts.PublishAccepted(Identity, Receipt);
                    if (Published == procedural_action::EReceiptStoreWrite::Accepted)
                    {
                        Action._MostRecentObservedPose = Actual;
                        Action._EntryBlendAccepted = Action._PendingBlendTo;
                        Action._PlaybackBacklog = Action._PendingBacklog;
                        Action._HasAuthoredReceipt |= NOT Action._PendingIsEntryBlend;
                        Action._HasHoldSinceLastReceipt = false;
                        Action._AcceptedTime = Action._PendingToTime;
                        Action._PublicationElapsed = FCk_Time{};
                        Action._LastStall = NAME_None;
                        const auto EnterPlaying = Action._PendingIsEntryBlend
                            && Action._EntryBlendAccepted >= Params.Get_Tuning().Get_Transition().Get_EntryBlend();
                        const auto ClipCompleted = NOT Action._PendingIsEntryBlend
                            && Action._AcceptedTime >= ActionHandle.Get<FFragment_ProceduralAction_Compiled>()
                                .Get_Clip()->Get_Duration();
                        if (ClipCompleted)
                        {
                            Action._Result = FCk_ProceduralAction_Result{
                                ECk_ProceduralAction_Outcome::Completed,
                                ECk_ProceduralAction_Recovery::Pending, FName{TEXT("ClipCompleted")}};
                            InPlayer._Receipts.CloseWindows(Identity, FName{TEXT("ClipCompleted")});
                            FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                                FName{TEXT("ClipCompleted")});
                        }
                        Action._PendingStageKey.Reset();
                        Action._PendingLastIntermediatePose.Reset();
                        if (EnterPlaying)
                        { Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Playing); }
                        else if (ClipCompleted)
                        { Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Returning); }
                    }
                    else
                    {
                        Action._Result = FCk_ProceduralAction_Result{
                            Published == procedural_action::EReceiptStoreWrite::Overflow
                                ? ECk_ProceduralAction_Outcome::ReceiptOverflow
                                : ECk_ProceduralAction_Outcome::PublicationFailed,
                            ECk_ProceduralAction_Recovery::Pending, TEXT("ReceiptRejected")};
                        if (Published == procedural_action::EReceiptStoreWrite::Overflow)
                        {
                            auto& Context = Action.BeginFailureContext(ActionHandle, Params);
                            const auto RecordsPerInstance = InHandle.Get<FFragment_ProceduralActionPlayer_Params>()
                                .Get_Capacity().Get_RecordsPerInstance();
                            const auto PendingRecords = InPlayer._Receipts.Get_PendingRecordCount(Identity);
                            if (PendingRecords >= RecordsPerInstance)
                            {
                                Context.Set_BudgetExcess(FName{TEXT("PendingReceiptRecords")}, PendingRecords,
                                    RecordsPerInstance);
                            }
                        }
                        InPlayer._Receipts.CloseWindows(Identity, TEXT("ReceiptRejected"));
                        FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, TEXT("ReceiptRejected"));
                        Action._PendingStageKey.Reset();
                        Action._PendingLastIntermediatePose.Reset();
                        Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                    }
                }
                else if (Settlement == ECk_ArticulationSettlement::Faulted
                    || Settlement == ECk_ArticulationSettlement::ClaimLost)
                {
                    Action._PendingStageKey.Reset();
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::PublicationFailed,
                        ECk_ProceduralAction_Recovery::Pending, TEXT("SettlementFailed")};
                    InPlayer._Receipts.CloseWindows(Identity, TEXT("SettlementFailed"));
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, TEXT("SettlementFailed"));
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                }
            }

        }
    }
}
