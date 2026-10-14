#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Processor.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralBodySupport.h"
#include "CkProceduralAnimation/Core/CkProceduralActionMotion.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReport.h"
#include "CkProceduralAnimation/Core/CkProceduralReturnPrefix.h"
#include "CkProceduralAnimation/Core/CkProceduralClipTimeMapping.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Log.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_Service);

namespace ck_procedural_action_player_service
{
    struct FClipServicePlan
    {
        FCk_Handle_ProceduralAction Action;
        FCk_Time From;
        FCk_Time Requested;
        bool EntryBlend = false;
        double MaximumFraction = 1.0;
        TArray<double> CriticalFractions;
        TArray<FCk_Time> CriticalTimes;
        FCk_Time MaximumTime;
    };

    auto
    IsRigid(
        const FTransform& InValue)
        -> bool;

    auto
    BindCompiledClip(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FProceduralActionClipParams& InParams,
        const ck::procedural_action::FCompiledClip& InClip,
        TArray<FName>& OutLimbIds)
        -> bool
    {
        OutLimbIds.Reset();
        if (InParams.Get_SlotBindings().Num() != InClip.Get_Slots().Num())
        { return false; }
        auto SeenSlots = TSet<FName>{};
        auto SeenLimbs = TSet<FName>{};
        for (const auto& Binding : InParams.Get_SlotBindings())
        {
            if (Binding.Get_Slot().IsNone() || Binding.Get_LimbId().IsNone()
                || SeenSlots.Contains(Binding.Get_Slot()) || SeenLimbs.Contains(Binding.Get_LimbId())
                || NOT InClip.Get_Slots().ContainsByPredicate([&Binding](const auto& InSlot)
                    { return InSlot.Get_Id() == Binding.Get_Slot(); })
                || ck::Is_NOT_Valid(UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, Binding.Get_LimbId())))
            { return false; }
            SeenSlots.Add(Binding.Get_Slot());
            SeenLimbs.Add(Binding.Get_LimbId());
        }
        for (const auto& Slot : InClip.Get_Slots())
        {
            const auto* Binding = InParams.Get_SlotBindings().FindByPredicate([&Slot](const auto& InBinding)
            { return InBinding.Get_Slot() == Slot.Get_Id(); });
            if (Binding == nullptr)
            { return false; }
            OutLimbIds.Add(Binding->Get_LimbId());
        }
        auto ActiveWindows = 0;
        for (auto Index = 0; Index < InClip.Get_Events().Num();)
        {
            const auto Time = InClip.Get_Events()[Index].Get_Time();
            auto AtTime = 0;
            do
            {
                const auto Kind = InClip.Get_Events()[Index].Get_Kind();
                if (Kind == ck::procedural_action::EEventKind::WindowExit) { --ActiveWindows; }
                else if (Kind == ck::procedural_action::EEventKind::WindowEnter) { ++ActiveWindows; }
                if (ActiveWindows < 0 || ActiveWindows > ck::procedural_action::receipt_limits::MaxWindowsPerReceipt)
                { return false; }
                ++Index;
                ++AtTime;
            }
            while (Index < InClip.Get_Events().Num() && InClip.Get_Events()[Index].Get_Time() == Time);
            if (AtTime > ck::procedural_action::receipt_limits::MaxEventsPerReceipt)
            { return false; }
        }
        return ActiveWindows == 0;
    }

    auto
    IsRigid(
        const FTransform& InValue)
        -> bool
    {
        return NOT InValue.ContainsNaN() && InValue.GetScale3D().Equals(FVector::OneVector, 1.0e-6)
            && InValue.GetRotation().IsNormalized();
    }

    auto
    IsFiniteDirection(
        const FVector& InValue)
        -> bool
    {
        return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
            && FMath::IsFinite(InValue.Z) && NOT InValue.IsNearlyZero();
    }

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
            if (Limb == nullptr || NOT IsRigid(Limb->Get_AcceptedEndpoint()))
            { return false; }
            Limbs.Add(FCk_ArticulationLimbTarget{Id, Limb->Get_AcceptedEndpoint(), {},
                ECk_ArticulationReachPolicy::RejectUnreachable});
        }
        if (InBodyClaim && (NOT InEntry.Get_HasBodyPresentation()
            || NOT IsRigid(InEntry.Get_BodyPresentation())))
        { return false; }
        const auto Body = InBodyClaim ? TOptional<FTransform>{InEntry.Get_BodyPresentation()}
            : TOptional<FTransform>{};
        OutCandidate = FCk_ArticulationTokenCandidate{InToken, 0, Limbs, Body};
        return true;
    }

    auto
    ResolveClipFrame(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::procedural_action::FCompiledSlot& InSlot,
        FName InLimbId,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        const FTransform& InBodyWorld,
        FTransform& OutFrame,
        float& OutChainLength,
        FVector& OutHipWorld)
        -> bool
    {
        const auto Limb = UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, InLimbId);
        if (ck::Is_NOT_Valid(Limb)) { return false; }
        auto Layout = FCk_ProceduralLimb_Layout{};
        if (NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout)) { return false; }
        OutChainLength = 0.0f;
        for (const auto Length : Layout.Get_SegmentLengths()) { OutChainLength += Length; }
        if (NOT FMath::IsFinite(OutChainLength) || OutChainLength <= 0.0f) { return false; }
        OutHipWorld = (Layout.Get_AttachmentLocal() * InBodyWorld).GetTranslation();
        switch (InSlot.Get_Space())
        {
            case ECk_ProceduralAction_TrackSpace::BodyLocal:
                OutFrame = InDefault.Get_RootFrame();
                break;
            case ECk_ProceduralAction_TrackSpace::RootAtStart:
                OutFrame = InEntry.Get_RootFrame();
                break;
            case ECk_ProceduralAction_TrackSpace::LimbRestNormalized:
                OutFrame = Layout.Get_RestEndpointLocal() * Layout.Get_AttachmentLocal() * InBodyWorld;
                break;
            default:
                return false;
        }
        return IsRigid(OutFrame);
    }

    auto
    CaptureClipInitial(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FFragment_ProceduralAction_Compiled& InCompiled,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        ck::procedural_action::FInitialPose& OutInitial)
        -> bool
    {
        OutInitial = {};
        const auto& Clip = *InCompiled.Get_Clip();
        const auto UseCapturedBody = Clip.Get_HasBodyChannel()
            && Clip.Get_BodyInitialSource() == ECk_ProceduralAction_InitialSource::CapturedAcceptedPose;
        const auto BodyWorld = UseCapturedBody && InEntry.Get_HasBodyPresentation()
            ? InEntry.Get_BodyPresentation()
            : (InDefault.Get_HasBodyPresentation() ? InDefault.Get_BodyPresentation()
                : InDefault.Get_RootFrame());
        if (Clip.Get_HasBodyChannel()
            && Clip.Get_BodyInitialSource() == ECk_ProceduralAction_InitialSource::CapturedAcceptedPose)
        {
            if (NOT InEntry.Get_HasBodyPresentation() || NOT InDefault.Get_HasBodyPresentation())
            { return false; }
            const auto EntryLocal = InEntry.Get_BodyPresentation().GetRelativeTransform(InEntry.Get_RootFrame());
            const auto BaseLocal = InDefault.Get_BodyPresentation().GetRelativeTransform(InDefault.Get_RootFrame());
            OutInitial.Get_Body() = FTransform{
                (BaseLocal.GetRotation().Inverse() * EntryLocal.GetRotation()).GetNormalized(),
                EntryLocal.GetTranslation() - BaseLocal.GetTranslation()};
            if (NOT IsRigid(OutInitial.Get_Body())) { return false; }
        }
        for (auto Index = 0; Index < Clip.Get_Slots().Num(); ++Index)
        {
            const auto& Slot = Clip.Get_Slots()[Index];
            const auto LimbId = InCompiled.Get_BoundLimbIds()[Index];
            const auto* LimbPose = InEntry.Get_Limbs().FindByPredicate([LimbId](const auto& InLimb)
            { return InLimb.Get_Id() == LimbId; });
            if (LimbPose == nullptr || NOT IsRigid(LimbPose->Get_AcceptedEndpoint())) { return false; }
            auto Frame = FTransform::Identity;
            auto ChainLength = 0.0f;
            auto Hip = FVector::ZeroVector;
            if (NOT ResolveClipFrame(InSubject, Slot, LimbId, InEntry, InDefault,
                BodyWorld, Frame, ChainLength, Hip))
            { return false; }
            auto InitialWorld = LimbPose->Get_AcceptedEndpoint();
            if (Slot.Get_InitialSource() == ECk_ProceduralAction_InitialSource::AuthoredRest)
            {
                const auto Limb = UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, LimbId);
                auto Layout = FCk_ProceduralLimb_Layout{};
                if (ck::Is_NOT_Valid(Limb)
                    || NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout))
                { return false; }
                InitialWorld = Layout.Get_RestEndpointLocal()
                    * Layout.Get_AttachmentLocal() * BodyWorld;
                if (NOT IsRigid(InitialWorld)) { return false; }
            }
            auto Local = InitialWorld.GetRelativeTransform(Frame);
            if (Slot.Get_Space() == ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
            { Local.SetTranslation(Local.GetTranslation() / ChainLength); }
            if (NOT IsRigid(Local)) { return false; }
            OutInitial.Get_Limbs().Add(Slot.Get_Id(), Local);
            if (Slot.Get_InitialSource() == ECk_ProceduralAction_InitialSource::CapturedAcceptedPose
                && LimbPose->Get_Segments().Num() > 0)
            {
                const auto Axis = (LimbPose->Get_AcceptedEndpoint().GetTranslation() - Hip).GetSafeNormal();
                const auto Upper = LimbPose->Get_Segments()[0].GetTranslation() - Hip;
                const auto BendWorld = Upper - FVector::DotProduct(Upper, Axis) * Axis;
                if (IsFiniteDirection(BendWorld))
                {
                    const auto BendLocal = Frame.InverseTransformVectorNoScale(BendWorld.GetSafeNormal());
                    if (IsFiniteDirection(BendLocal))
                    { OutInitial.Get_BendDirections().Add(Slot.Get_Id(), BendLocal.GetSafeNormal()); }
                }
            }
        }
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
            || NOT IsRigid(InDefault.Get_RootFrame()))
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
            if (NOT Composed.IsSet()) { return false; }
            BodyWorld = Composed.GetValue() * InDefault.Get_RootFrame();
            BodyTarget = BodyWorld;
        }
        auto Limbs = TArray<FCk_ArticulationLimbTarget>{};
        Limbs.Reserve(Clip.Get_Slots().Num());
        for (auto Index = 0; Index < Clip.Get_Slots().Num(); ++Index)
        {
            const auto& Slot = Clip.Get_Slots()[Index];
            const auto& EvaluatedLimb = InEvaluated.Get_Limbs()[Index];
            if (EvaluatedLimb.Get_Slot() != Slot.Get_Id()) { return false; }
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
            if (NOT IsRigid(Endpoint)) { return false; }
            auto Pole = TOptional<FVector>{};
            if (EvaluatedLimb.Get_HasBend())
            {
                const auto Direction = Frame.TransformVectorNoScale(EvaluatedLimb.Get_BendDirection())
                    .GetSafeNormal();
                if (NOT IsFiniteDirection(Direction)) { return false; }
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
            if (Initial == nullptr) { return false; }
            Limb.Get_Position() = FMath::Lerp(Initial->GetTranslation(), Limb.Get_Position(), InFraction);
            Limb.Get_Orientation() = FQuat::Slerp(Initial->GetRotation(),
                Limb.Get_Orientation(), InFraction).GetNormalized();
            if (Limb.Get_HasBend())
            {
                if (const auto* InitialBend = InInitial.Get_BendDirections().Find(Limb.Get_Slot()))
                {
                    const auto Blended = FMath::Lerp(*InitialBend, Limb.Get_BendDirection(), InFraction);
                    if (NOT IsFiniteDirection(Blended)) { return false; }
                    Limb.Get_BendDirection() = Blended.GetSafeNormal();
                }
            }
        }
        return IsRigid(OutBlended.Get_Body());
    }

    auto
    AcquireFailureReason(
        ECk_ProceduralLimbControl_AcquireResult InResult)
        -> FName
    {
        switch (InResult)
        {
            case ECk_ProceduralLimbControl_AcquireResult::Busy: return FName{TEXT("Busy")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidSubject: return FName{TEXT("InvalidSubject")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidOwner: return FName{TEXT("InvalidOwner")};
            case ECk_ProceduralLimbControl_AcquireResult::InvalidChannels: return FName{TEXT("InvalidChannels")};
            case ECk_ProceduralLimbControl_AcquireResult::UnavailableLimb: return FName{TEXT("UnavailableLimb")};
            case ECk_ProceduralLimbControl_AcquireResult::SupportUnavailable: return FName{TEXT("SupportUnavailable")};
            case ECk_ProceduralLimbControl_AcquireResult::Faulted: return FName{TEXT("Faulted")};
            default: return FName{TEXT("AcquireFailed")};
        }
    }


}

namespace ck
{
    auto
    FProcessor_ProceduralActionPlayer_Service::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer)
        -> void
    {
        auto Subject = UCk_Utils_ProceduralArticulation_UE::CastChecked(InHandle);
        const auto TimeValid = FMath::IsFinite(InDeltaT.Get_Seconds())
            && InDeltaT.Get_Seconds() >= 0.0;
        if (NOT TimeValid) { return; }
        InPlayer._Receipts.AdvanceService(InDeltaT);
        for (auto Index = InPlayer._PendingRecoveries.Num() - 1; Index >= 0; --Index)
        {
            const auto& Pending = InPlayer._PendingRecoveries[Index];
            const auto State = FCk_ProceduralLimbControl_Control::Get_ReturnState(Subject, Pending.Get_Token());
            if (State != ECk_ProceduralLimbControl_ReturnState::Returned
                && State != ECk_ProceduralLimbControl_ReturnState::Failed)
            { continue; }
            const auto Recovery = State == ECk_ProceduralLimbControl_ReturnState::Returned
                ? ECk_ProceduralAction_Recovery::Succeeded : ECk_ProceduralAction_Recovery::Failed;
            const auto Outcome = Recovery == ECk_ProceduralAction_Recovery::Failed
                && Pending.Get_Outcome() == ECk_ProceduralAction_Outcome::Completed
                ? ECk_ProceduralAction_Outcome::RecoveryFailed : Pending.Get_Outcome();
            const auto Reason = Recovery == ECk_ProceduralAction_Recovery::Failed
                ? FCk_ProceduralLimbControl_Control::Get_ReturnReason(Subject, Pending.Get_Token())
                : Pending.Get_Reason();
            InPlayer._Receipts.Finish(Pending.Get_Identity(), FCk_ProceduralAction_Result{
                Outcome, Recovery, Reason});
            FCk_ProceduralLimbControl_Control::ForgetTerminal(Subject, Pending.Get_Token());
            InPlayer._PendingRecoveries.RemoveAt(Index);
        }

        InPlayer._Actions.RemoveAll([](const auto& InAction)
        { return ck::Is_NOT_Valid(InAction) || NOT InAction.template Has<FFragment_ProceduralAction>(); });
        InPlayer._Receipts.AcknowledgeAutoDelivered();
        for (auto ActionHandle : InPlayer._Actions)
        {
            if (ActionHandle.Has<FTag_DestroyEntity_Initiate>()
                || ActionHandle.Get<FFragment_ProceduralAction>()._State != ECk_ProceduralAction_State::Finished
                || NOT InPlayer._Receipts.Get_IsTerminalAcknowledged(
                    ActionHandle.Get<FFragment_ProceduralAction_Params>().Get_Identity()))
            { continue; }
            auto Handle = ActionHandle.ConvertToHandle();
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
        }
        const auto ActionSnapshot = InPlayer._Actions;
        for (auto ActionHandle : ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            if (Action._State != ECk_ProceduralAction_State::PendingLoad) { continue; }
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            auto Failed = false;
            auto FailureOutcome = ECk_ProceduralAction_Outcome::AssetFailed;
            auto FailureReason = FName{TEXT("AssetLoadFailed")};
            if (NOT ActionHandle.Has<FFragment_ProceduralAction_Loading>())
            { Failed = true; FailureReason = FName{TEXT("MissingLoadingState")}; }
            else
            {
                auto& Loading = ActionHandle.Get<FFragment_ProceduralAction_Loading>();
                if (Loading._Generation != Action._Generation)
                { Failed = true; FailureReason = FName{TEXT("StaleLoadGeneration")}; }
                else
                {
                    Loading._Elapsed += InDeltaT;
                    if (Loading._Elapsed >= Params.Get_Tuning().Get_Watchdogs().Get_LoadTimeout())
                    {
                        Failed = true;
                        FailureReason = FName{TEXT("LoadTimeout")};
                        Action.BeginFailureContext(ActionHandle, Params)
                            .Set_Elapsed(Loading._Elapsed)
                            .Set_Limit(Params.Get_Tuning().Get_Watchdogs().Get_LoadTimeout());
                    }
                    else
                    {
                        if (NOT Loading._Assets.Get_IsRequested())
                        {
                            Loading._Assets = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
                                FName{TEXT("ProceduralAction.Clip")},
                                TArray<FSoftObjectPath>{Params.Get_Clip().Get_Clip().ToSoftObjectPath()});
                        }
                        if (Loading._Assets.Get_HasFailed())
                        { Failed = true; FailureReason = FName{TEXT("AssetLoadFailed")}; }
                        else if (Loading._Assets.Get_IsReady())
                        {
                            auto* Asset = Cast<UCk_ProceduralAction_Data>(Loading._Assets.Get_ResolvedObject(
                                Params.Get_Clip().Get_Clip().ToSoftObjectPath()));
                            auto Compiled = procedural_action::FCompiledClip{};
                            auto Diagnostics = TArray<procedural_action::FDiagnostic>{};
                            if (Asset == nullptr || NOT procedural_action::Compile(*Asset, Compiled, Diagnostics))
                            {
                                UE_LOG(CkProceduralAnimation, Display,
                                    TEXT("ProceduralAction clip compile failed: path=%s class=%s diagnostics=%d"),
                                    *Params.Get_Clip().Get_Clip().ToSoftObjectPath().ToString(),
                                    Asset == nullptr ? TEXT("<null>") : *Asset->GetClass()->GetPathName(),
                                    Diagnostics.Num());
                                for (auto DiagnosticIndex = 0; DiagnosticIndex < FMath::Min(Diagnostics.Num(), 4);
                                    ++DiagnosticIndex)
                                {
                                    const auto& Diagnostic = Diagnostics[DiagnosticIndex];
                                    UE_LOG(CkProceduralAnimation, Display,
                                        TEXT("ProceduralAction clip diagnostic: code=%s field=%s expected=%s actual=%s"),
                                        *Diagnostic.Get_Code().ToString(), *Diagnostic.Get_FieldPath(),
                                        *Diagnostic.Get_Expected(), *Diagnostic.Get_Actual());
                                }
                                Failed = true;
                                FailureOutcome = ECk_ProceduralAction_Outcome::InvalidContent;
                                FailureReason = FName{TEXT("InvalidClipContent")};
                            }
                            else
                            {
                                auto BoundIds = TArray<FName>{};
                                if (NOT ck_procedural_action_player_service::BindCompiledClip(Subject,
                                        Params.Get_Clip(), Compiled, BoundIds)
                                    || InPlayer._Receipts.BindChannels(Params.Get_Identity(), BoundIds,
                                        Compiled.Get_HasBodyChannel())
                                        != procedural_action::EReceiptStoreWrite::Accepted)
                                {
                                    Failed = true;
                                    FailureOutcome = ECk_ProceduralAction_Outcome::Rejected;
                                    FailureReason = FName{TEXT("ClipBindingsRejected")};
                                }
                                else
                                {
                                    TSharedPtr<const procedural_action::FCompiledClip> SharedClip =
                                        MakeShared<procedural_action::FCompiledClip>(MoveTemp(Compiled));
                                    ActionHandle.Add<FFragment_ProceduralAction_Compiled>(
                                        MoveTemp(SharedClip), MoveTemp(BoundIds));
                                    ActionHandle.Remove<FFragment_ProceduralAction_Loading>();
                                    Action.CommitState(ActionHandle, Params,
                                        ECk_ProceduralAction_State::PendingAdmission);
                                }
                            }
                        }
                    }
                }
            }
            if (Failed)
            {
                if (ActionHandle.Has<FFragment_ProceduralAction_Loading>())
                { ActionHandle.Remove<FFragment_ProceduralAction_Loading>(); }
                Action._Result = FCk_ProceduralAction_Result{FailureOutcome,
                    ECk_ProceduralAction_Recovery::NotRequired, FailureReason};
                InPlayer._Receipts.Finish(Params.Get_Identity(), Action._Result, Action._Context);
                Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
            }
        }
        auto HasActiveWork = NOT InPlayer._PendingRecoveries.IsEmpty()
            || FCk_ProceduralLimbControl_Control::HasReturnWork(Subject);
        for (const auto& ActionHandle : ActionSnapshot)
        {
            if (ck::IsValid(ActionHandle) && ActionHandle.Has<FFragment_ProceduralAction>()
                && NOT ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            {
                const auto State = ActionHandle.Get<FFragment_ProceduralAction>().Get_State();
                if (State != ECk_ProceduralAction_State::Finished
                    && State != ECk_ProceduralAction_State::PendingLoad)
                { HasActiveWork = true; break; }
            }
        }
        if (NOT HasActiveWork) { return; }

        const auto ReattachmentBarrier = FCk_ProceduralLimbControl_Control::HasReattachmentPending(Subject);
        if (ReattachmentBarrier)
        {
            const auto OutstandingActionStage = ActionSnapshot.ContainsByPredicate([](const auto& InAction)
            {
                return ck::IsValid(InAction) && InAction.Has<FFragment_ProceduralAction>()
                    && (InAction.Get<FFragment_ProceduralAction>()._PendingStageKey.IsSet()
                        || InAction.Get<FFragment_ProceduralAction>()._PendingHoldStageKey.IsSet());
            });
            auto ClosedActiveWindow = false;
            if (NOT OutstandingActionStage)
            {
                for (auto ActionHandle : ActionSnapshot)
                {
                    if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                        || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
                    { continue; }
                    auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
                    if (NOT Action._HasClaim || (Action._State != ECk_ProceduralAction_State::Acquiring
                        && Action._State != ECk_ProceduralAction_State::Playing))
                    { continue; }
                    const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
                    const auto Identity = Params.Get_Identity();
                    const auto Token = FCk_ArticulationControlToken{static_cast<uint64>(Identity.Get_Value()),
                        static_cast<uint64>(Action._Generation)};
                    const auto Reason = FName{TEXT("ReturnReattachmentDiscontinuity")};
                    InPlayer._Receipts.CloseWindows(Identity, Reason);
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, Reason);
                    Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::SubjectPoseRejected,
                        ECk_ProceduralAction_Recovery::Pending, Reason};
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                    ClosedActiveWindow = true;
                }
            }
            if (ClosedActiveWindow) { return; }
        }

        auto DefaultPose = FCk_ProceduralArticulation_Pose{};
        const auto DefaultValid = FCk_ProceduralArticulation_Control::GetDefaultPose(Subject, DefaultPose)
            == ECk_ArticulationControlResult::Accepted;
        auto Candidates = TArray<FCk_ArticulationTokenCandidate>{};
        if (DefaultValid || ReattachmentBarrier)
        { FCk_ProceduralLimbControl_Control::AppendReturnCandidates(Subject, DefaultPose, Candidates); }
        const auto ReturnCandidateCount = Candidates.Num();
        auto ReturnTokens = TSet<uint64>{};
        for (auto Index = 0; Index < ReturnCandidateCount; ++Index)
        { ReturnTokens.Add(Candidates[Index].Get_Token().Get_InstanceId()); }
        auto ClipPlans = TArray<ck_procedural_action_player_service::FClipServicePlan>{};
        auto HasUnsettledActionStage = false;
        // Identities are appended in enqueue order; snapshot traversal also isolates reentrant creation.
        for (auto ActionHandle : ActionSnapshot)
        {
            if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }
            auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
            const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
            const auto Identity = Params.Get_Identity();
            const auto Token = FCk_ArticulationControlToken{static_cast<uint64>(Identity.Get_Value()),
                static_cast<uint64>(Action._Generation)};
            if (ReattachmentBarrier && Action._State == ECk_ProceduralAction_State::PendingAdmission)
            { continue; }
            Action._ServiceElapsed += InDeltaT;
            HasUnsettledActionStage |= Action._PendingStageKey.IsSet()
                || Action._PendingHoldStageKey.IsSet();

            if (Action._HasClaim && (Action._State == ECk_ProceduralAction_State::Acquiring
                    || Action._State == ECk_ProceduralAction_State::Playing))
            {
                const auto Interruption = FCk_ProceduralLimbControl_Control::Get_InterruptionReason(
                    Subject, Token);
                if (NOT Interruption.IsNone())
                {
                    InPlayer._Receipts.CloseWindows(Identity, Interruption);
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, Interruption);
                    Action._Result = FCk_ProceduralAction_Result{
                        (Interruption == FName{TEXT("SupportLost")}
                            || Interruption == FName{TEXT("SupportHoldTimeout")}
                            || Interruption == FName{TEXT("SupportEmergencyRequested")})
                            ? ECk_ProceduralAction_Outcome::SupportLost
                            : ECk_ProceduralAction_Outcome::SubjectPoseRejected,
                        ECk_ProceduralAction_Recovery::Pending, Interruption};
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                }
            }

            if (Action._State == ECk_ProceduralAction_State::PendingAdmission
                && ActionHandle.Has<FFragment_ProceduralAction_Compiled>())
            {
                const auto& Compiled = ActionHandle.Get<FFragment_ProceduralAction_Compiled>();
                auto Return = FCk_ProceduralLimbControl_Return{};
                Return.Set_Duration(Params.Get_Tuning().Get_Transition().Get_DedicatedReturn());
                Return.Set_Timeout(Params.Get_Tuning().Get_Transition().Get_ReturnTimeout());
                auto Entry = FCk_ProceduralArticulation_Pose{};
                const auto Acquired = FCk_ProceduralLimbControl_Control::TryAcquire(Subject, Token,
                    ActionHandle, FCk_ArticulationChannelSet{Compiled.Get_BoundLimbIds(),
                        Compiled.Get_Clip()->Get_HasBodyChannel()}, Return, Entry);
                if (Acquired == ECk_ProceduralLimbControl_AcquireResult::Accepted)
                {
                    Action._EntryPose = Entry;
                    Action._MostRecentObservedPose = Entry;
                    Action._HasClaim = true;
                    if (DefaultValid && ck_procedural_action_player_service::CaptureClipInitial(Subject,
                        Compiled, Entry, DefaultPose, Action._InitialPose))
                    {
                        Action._EntryBlendAccepted = FCk_Time{};
                        Action.CommitState(ActionHandle, Params,
                            Params.Get_Tuning().Get_Transition().Get_EntryBlend() > FCk_Time{}
                                ? ECk_ProceduralAction_State::Acquiring
                                : ECk_ProceduralAction_State::Playing);
                    }
                    else
                    {
                        Action._Result = FCk_ProceduralAction_Result{
                            ECk_ProceduralAction_Outcome::Rejected,
                            ECk_ProceduralAction_Recovery::Pending, FName{TEXT("EntryCaptureRejected")}};
                        InPlayer._Receipts.CloseWindows(Identity, FName{TEXT("EntryCaptureRejected")});
                        FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                            FName{TEXT("EntryCaptureRejected")});
                        Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                    }
                }
                else if (Acquired == ECk_ProceduralLimbControl_AcquireResult::NotReady)
                {
                    Action._AdmissionElapsed += InDeltaT;
                    if (Action._AdmissionElapsed >= Params.Get_Tuning().Get_Watchdogs().Get_AcquisitionTimeout())
                    {
                        Action._Result = FCk_ProceduralAction_Result{
                            ECk_ProceduralAction_Outcome::AcquisitionTimeout,
                            ECk_ProceduralAction_Recovery::NotRequired, FName{TEXT("AcquisitionTimeout")}};
                        Action.BeginFailureContext(ActionHandle, Params)
                            .Set_Elapsed(Action._AdmissionElapsed)
                            .Set_Limit(Params.Get_Tuning().Get_Watchdogs().Get_AcquisitionTimeout());
                        InPlayer._Receipts.Finish(Identity, Action._Result, Action._Context);
                        Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
                    }
                }
                else
                {
                    Action._Result = FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::Rejected,
                        ECk_ProceduralAction_Recovery::NotRequired,
                        ck_procedural_action_player_service::AcquireFailureReason(Acquired)};
                    InPlayer._Receipts.Finish(Identity, Action._Result);
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
                }
            }

            if (Action._State == ECk_ProceduralAction_State::Returning
                || Action._State == ECk_ProceduralAction_State::Cancelling)
            {
                const auto ReturnState = FCk_ProceduralLimbControl_Control::Get_ReturnState(Subject, Token);
                if (ReturnState == ECk_ProceduralLimbControl_ReturnState::Returned
                    || ReturnState == ECk_ProceduralLimbControl_ReturnState::Failed)
                {
                    const auto Recovery = ReturnState == ECk_ProceduralLimbControl_ReturnState::Returned
                        ? ECk_ProceduralAction_Recovery::Succeeded : ECk_ProceduralAction_Recovery::Failed;
                    const auto Outcome = Recovery == ECk_ProceduralAction_Recovery::Failed
                        && Action._Result.Get_Outcome() == ECk_ProceduralAction_Outcome::Completed
                        ? ECk_ProceduralAction_Outcome::RecoveryFailed : Action._Result.Get_Outcome();
                    const auto Reason = Recovery == ECk_ProceduralAction_Recovery::Failed
                        ? FCk_ProceduralLimbControl_Control::Get_ReturnReason(Subject, Token)
                        : Action._Result.Get_Reason();
                    Action._Result = FCk_ProceduralAction_Result{Outcome, Recovery, Reason};
                    if (Outcome == ECk_ProceduralAction_Outcome::RecoveryFailed)
                    {
                        auto& Context = Action.BeginFailureContext(ActionHandle, Params);
                        auto Elapsed = FCk_Time{};
                        auto Deadline = FCk_Time{};
                        if (FCk_ProceduralLimbControl_Control::Get_MissedReturnDeadline(Subject, Token, Elapsed, Deadline))
                        { Context.Set_Elapsed(Elapsed).Set_Limit(Deadline); }
                    }
                    InPlayer._Receipts.Finish(Identity, Action._Result, Action._Context);
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Finished);
                    Action._HasClaim = false;
                    FCk_ProceduralLimbControl_Control::ForgetTerminal(Subject, Token);
                }
            }

            if (Action._State == ECk_ProceduralAction_State::Acquiring
                || Action._State == ECk_ProceduralAction_State::Playing)
            {
                Action._PublicationElapsed += InDeltaT;
                if (Action._PublicationElapsed >= Params.Get_Tuning().Get_Watchdogs().Get_PublicationTimeout())
                {
                    const auto Reason = FName{TEXT("ClipPublicationTimeout")};
                    InPlayer._Receipts.CloseWindows(Identity, Reason);
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, Reason);
                    Action._PendingStageKey.Reset();
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::PublicationTimeout,
                        ECk_ProceduralAction_Recovery::Pending, Reason};
                    Action.BeginFailureContext(ActionHandle, Params)
                        .Set_Elapsed(Action._PublicationElapsed)
                        .Set_Limit(Params.Get_Tuning().Get_Watchdogs().Get_PublicationTimeout());
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                }
            }

            if (DefaultValid && InDeltaT > FCk_Time{}
                && (Action._State == ECk_ProceduralAction_State::Acquiring
                    || Action._State == ECk_ProceduralAction_State::Playing)
                && NOT Action._PendingStageKey.IsSet()
                && ActionHandle.Has<FFragment_ProceduralAction_Compiled>())
            {
                const auto& Compiled = ActionHandle.Get<FFragment_ProceduralAction_Compiled>();
                const auto& Clip = *Compiled.Get_Clip();
                const auto& Sampling = Params.Get_Tuning().Get_Sampling();
                auto Plan = ck_procedural_action_player_service::FClipServicePlan{};
                Plan.Action = ActionHandle;
                Plan.EntryBlend = Action._State == ECk_ProceduralAction_State::Acquiring;
                Plan.From = Plan.EntryBlend ? Action._EntryBlendAccepted : Action._AcceptedTime;
                if (Plan.EntryBlend)
                {
                    const auto End = Params.Get_Tuning().Get_Transition().Get_EntryBlend();
                    Plan.Requested = FCk_Time{FMath::Min(End.Get_Seconds(),
                        Plan.From.Get_Seconds() + InDeltaT.Get_Seconds())};
                    auto At = Plan.From.Get_Seconds();
                    while (At < Plan.Requested.Get_Seconds()
                        && Plan.CriticalFractions.Num() < Sampling.Get_MaxSamples())
                    {
                        const auto Next = FMath::Min(Plan.Requested.Get_Seconds(),
                            At + Sampling.Get_MaxAdvance().Get_Seconds());
                        if (Next <= At) { break; }
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
                            + InDeltaT.Get_Seconds() * Params.Get_Tuning().Get_Playback().Get_PlayRate()
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
                    InPlayer._Receipts.CloseWindows(Identity, Reason);
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token, Reason);
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::SamplingBudgetExceeded,
                        ECk_ProceduralAction_Recovery::Pending, Reason};
                    Action.BeginFailureContext(ActionHandle, Params);
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                }
                if (NOT Plan.CriticalFractions.IsEmpty())
                { ClipPlans.Add(MoveTemp(Plan)); }
            }

            if (DefaultValid && InDeltaT > FCk_Time{}
                && (Action._State == ECk_ProceduralAction_State::Acquiring
                    || Action._State == ECk_ProceduralAction_State::Playing)
                && NOT Action._PendingStageKey.IsSet() && NOT Action._PendingHoldStageKey.IsSet()
                && ActionHandle.Has<FFragment_ProceduralAction_Compiled>()
                && NOT ClipPlans.ContainsByPredicate([ActionHandle](const auto& InPlan)
                    { return InPlan.Action == ActionHandle; }))
            {
                const auto& Compiled = ActionHandle.Get<FFragment_ProceduralAction_Compiled>();
                auto Hold = FCk_ArticulationTokenCandidate{};
                auto Valid = false;
                if (Action._State == ECk_ProceduralAction_State::Acquiring)
                {
                    Valid = ck_procedural_action_player_service::BuildEntryHoldCandidate(
                        Compiled.Get_BoundLimbIds(), Compiled.Get_Clip()->Get_HasBodyChannel(),
                        Action._EntryPose, Token, Hold);
                }
                else
                {
                    auto Evaluated = procedural_action::FEvaluatedPose{};
                    Valid = procedural_action::Evaluate(*Compiled.Get_Clip(), Action._InitialPose,
                        Action._AcceptedTime, Evaluated)
                        && ck_procedural_action_player_service::BuildClipCandidate(Subject,
                            Compiled, Evaluated, Action._EntryPose, DefaultPose, Token, 0, Hold);
                }
                if (Valid) { Candidates.Add(MoveTemp(Hold)); }
                else
                {
                    InPlayer._Receipts.CloseWindows(Identity, FName{TEXT("ClipHoldRejected")});
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                        FName{TEXT("ClipHoldRejected")});
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::SubjectPoseRejected,
                        ECk_ProceduralAction_Recovery::Pending, FName{TEXT("ClipHoldRejected")}};
                    Action.CommitState(ActionHandle, Params, ECk_ProceduralAction_State::Cancelling);
                }
            }

            if (Action._State == ECk_ProceduralAction_State::Finished && Action._ReleaseWhenFinished)
            {
                auto Handle = ActionHandle.ConvertToHandle();
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
            }
        }

        if ((Candidates.IsEmpty() && ClipPlans.IsEmpty()) || InDeltaT <= FCk_Time{}
            || HasUnsettledActionStage)
        { return; }
        struct FOtherStepLimit
        {
            FCk_ProceduralArticulation_Pose Previous;
            float MaxTravel = 0.0f;
            float MaxAngular = 0.0f;
        };
        auto OtherLimits = TMap<uint64, FOtherStepLimit>{};
        for (auto Index = 0; Index < Candidates.Num(); ++Index)
        {
            const auto InstanceId = Candidates[Index].Get_Token().Get_InstanceId();
            auto Limit = FOtherStepLimit{};
            if (Index < ReturnCandidateCount)
            {
                auto Budget = FCk_ProceduralAction_Sampling{};
                if (NOT FCk_ProceduralLimbControl_Control::Get_ReturnMotionBudget(
                        Subject, Candidates[Index].Get_Token(), Budget)
                    || NOT FCk_ProceduralLimbControl_Control::Get_ReturnActualPose(
                        Subject, Candidates[Index].Get_Token(), Limit.Previous))
                { return; }
                Limit.MaxTravel = Budget.Get_MaxEndpointTravelCm();
                Limit.MaxAngular = Budget.Get_MaxSegmentAngularStepDegrees();
            }
            else
            {
                const auto* Owner = ActionSnapshot.FindByPredicate([InstanceId](const auto& InAction)
                {
                    return ck::IsValid(InAction) && InAction.Has<FFragment_ProceduralAction_Params>()
                        && static_cast<uint64>(InAction.Get<FFragment_ProceduralAction_Params>()
                            .Get_Identity().Get_Value()) == InstanceId;
                });
                if (Owner == nullptr || NOT Owner->Has<FFragment_ProceduralAction>()) { return; }
                const auto& Action = Owner->Get<FFragment_ProceduralAction>();
                const auto& Sampling = Owner->Get<FFragment_ProceduralAction_Params>()
                    .Get_Tuning().Get_Sampling();
                Limit.Previous = Action._MostRecentObservedPose;
                Limit.MaxTravel = Sampling.Get_MaxEndpointTravelCm();
                Limit.MaxAngular = Sampling.Get_MaxSegmentAngularStepDegrees();
            }
            OtherLimits.Add(InstanceId, MoveTemp(Limit));
        }
        const auto QualifiesOther = [&OtherLimits](
            const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
            const FCk_ArticulationBatchEvaluation& InEvaluation,
            int32 InFirst,
            int32 InLast) -> bool
        {
            if (InEvaluation.Get_PredictedTokenPoses().Num() != InCandidates.Num()) { return false; }
            for (auto Index = InFirst; Index < InLast; ++Index)
            {
                const auto* Limit = OtherLimits.Find(InCandidates[Index].Get_Token().Get_InstanceId());
                if (Limit == nullptr) { return false; }
                auto Travel = 0.0f;
                auto Angular = 0.0f;
                procedural_action::MeasureMotion(Limit->Previous,
                    InEvaluation.Get_PredictedTokenPoses()[Index], Travel, Angular);
                if (NOT FMath::IsFinite(Travel) || NOT FMath::IsFinite(Angular)
                    || Travel > Limit->MaxTravel || Angular > Limit->MaxAngular)
                { return false; }
            }
            return true;
        };
        auto Evaluation = FCk_ArticulationBatchEvaluation{};
        if (ClipPlans.IsEmpty())
        {
            const auto ReattachmentEndpointsHold = [&](const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
                const FCk_ArticulationBatchEvaluation& InEvaluation) -> bool
            {
                if (InEvaluation.Get_PredictedTokenPoses().Num() != InCandidates.Num()) { return false; }
                for (auto Index = 0; Index < InCandidates.Num(); ++Index)
                {
                    if (NOT ReturnTokens.Contains(InCandidates[Index].Get_Token().Get_InstanceId())) { continue; }
                    auto Actual = FCk_ProceduralArticulation_Pose{};
                    if (NOT FCk_ProceduralLimbControl_Control::Get_ReturnActualPose(
                        Subject, InCandidates[Index].Get_Token(), Actual))
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
            };
            Candidates.StableSort([](const FCk_ArticulationTokenCandidate& InA,
                const FCk_ArticulationTokenCandidate& InB)
            { return InA.Get_Token().Get_InstanceId() < InB.Get_Token().Get_InstanceId(); });
            const auto FullBatchResult = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
                Subject, Candidates, Evaluation);
            const auto BatchQualified = [&QualifiesOther](
                const TArray<FCk_ArticulationTokenCandidate>& InCandidates,
                const FCk_ArticulationBatchEvaluation& InEvaluation) -> bool
            {
                return QualifiesOther(InCandidates, InEvaluation, 0, InCandidates.Num());
            };
            if (FullBatchResult == ECk_ArticulationControlResult::Busy
                || FullBatchResult == ECk_ArticulationControlResult::StaleInput)
            { return; }
            if (ReattachmentBarrier && (FullBatchResult != ECk_ArticulationControlResult::Accepted
                || NOT ReattachmentEndpointsHold(Candidates, Evaluation)))
            { return; }
            if (FullBatchResult != ECk_ArticulationControlResult::Accepted
                || (NOT ReattachmentBarrier && NOT BatchQualified(Candidates, Evaluation)))
            {
                const auto ProbeReturnFraction = [&](double InFraction,
                    TArray<FCk_ArticulationTokenCandidate>& OutCandidates,
                    FCk_ArticulationBatchEvaluation& OutEvaluation)
                    -> procedural_action::EReturnPrefixProbe
                {
                    FCk_ProceduralLimbControl_Control::AppendReturnCandidates(Subject, DefaultPose,
                        OutCandidates, InFraction);
                    if (OutCandidates.Num() != ReturnCandidateCount)
                    { return procedural_action::EReturnPrefixProbe::Interrupted; }
                    for (const auto& Candidate : Candidates)
                    {
                        if (NOT ReturnTokens.Contains(Candidate.Get_Token().Get_InstanceId()))
                        { OutCandidates.Add(Candidate); }
                    }
                    OutCandidates.StableSort([](const auto& InA, const auto& InB)
                    { return InA.Get_Token().Get_InstanceId() < InB.Get_Token().Get_InstanceId(); });
                    const auto Result = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
                        Subject, OutCandidates, OutEvaluation);
                    if (Result == ECk_ArticulationControlResult::Busy
                        || Result == ECk_ArticulationControlResult::StaleInput)
                    { return procedural_action::EReturnPrefixProbe::Interrupted; }
                    return Result == ECk_ArticulationControlResult::Accepted
                        && BatchQualified(OutCandidates, OutEvaluation)
                            ? procedural_action::EReturnPrefixProbe::Qualified
                            : procedural_action::EReturnPrefixProbe::Rejected;
                };
                auto AcceptedReturnPrefix = false;
                auto SearchInterrupted = false;
                auto ProbeCount = 0;
                if (ReturnCandidateCount > 0)
                {
                    const auto BestFraction = procedural_action::FindReturnPrefix(
                        [&](double InFraction) -> procedural_action::EReturnPrefixProbe
                        {
                            auto StepCandidates = TArray<FCk_ArticulationTokenCandidate>{};
                            auto StepEvaluation = FCk_ArticulationBatchEvaluation{};
                            const auto Probe = ProbeReturnFraction(
                                InFraction, StepCandidates, StepEvaluation);
                            if (Probe == procedural_action::EReturnPrefixProbe::Qualified)
                            {
                                Candidates = MoveTemp(StepCandidates);
                                Evaluation = MoveTemp(StepEvaluation);
                            }
                            return Probe;
                        }, ProbeCount, SearchInterrupted);
                    if (SearchInterrupted) { return; }
                    if (BestFraction.IsSet())
                    {
                        AcceptedReturnPrefix = true;
                    }
                }
                if (NOT AcceptedReturnPrefix)
                {
                    if (ReturnCandidateCount > 0 && NOT ReattachmentBarrier)
                    {
                        auto Held = TArray<FCk_ArticulationTokenCandidate>{};
                        if (FCk_ProceduralLimbControl_Control::AppendHeldReturnCandidates(Subject, Held)
                            && Held.Num() == ReturnCandidateCount)
                        {
                            for (const auto& Candidate : Candidates)
                            {
                                if (NOT ReturnTokens.Contains(Candidate.Get_Token().Get_InstanceId()))
                                { Held.Add(Candidate); }
                            }
                            Held.StableSort([](const auto& InA, const auto& InB)
                            { return InA.Get_Token().Get_InstanceId() < InB.Get_Token().Get_InstanceId(); });
                            auto HeldEvaluation = FCk_ArticulationBatchEvaluation{};
                            if (FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
                                    Subject, Held, HeldEvaluation) == ECk_ArticulationControlResult::Accepted
                                && ReattachmentEndpointsHold(Held, HeldEvaluation))
                            {
                                auto Discontinuous = false;
                                for (auto Index = 0; Index < Held.Num(); ++Index)
                                {
                                    if (NOT ReturnTokens.Contains(Held[Index].Get_Token().Get_InstanceId()))
                                    { continue; }
                                    const auto* Limit = OtherLimits.Find(Held[Index].Get_Token().Get_InstanceId());
                                    if (Limit == nullptr) { continue; }
                                    auto Travel = 0.0f;
                                    auto Angular = 0.0f;
                                    procedural_action::MeasureMotion(Limit->Previous,
                                        HeldEvaluation.Get_PredictedTokenPoses()[Index], Travel, Angular);
                                    Discontinuous |= FMath::IsFinite(Travel) && FMath::IsFinite(Angular)
                                        && (Travel > Limit->MaxTravel || Angular > Limit->MaxAngular);
                                }
                                if (Discontinuous
                                    && FCk_ProceduralLimbControl_Control::RequestReattachment(Subject))
                                { return; }
                            }
                        }
                    }
                    return;
                }
            }
        }
        else
        {
            auto Fractions = TArray<double>{};
            auto MaximumFraction = 1.0;
            auto MaximumSteps = procedural_action::receipt_limits::MaxSamplesPerReceipt;
            for (const auto& Plan : ClipPlans)
            {
                MaximumFraction = FMath::Min(MaximumFraction, Plan.MaximumFraction);
                const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                MaximumSteps = FMath::Min(MaximumSteps,
                    Params.Get_Tuning().Get_Sampling().Get_MaxSamples());
            }
            for (const auto& Plan : ClipPlans)
            {
                for (const auto Fraction : Plan.CriticalFractions)
                {
                    if (Fraction > 0.0 && Fraction <= MaximumFraction)
                    { Fractions.AddUnique(Fraction); }
                }
            }
            if (MaximumFraction > 0.0) { Fractions.AddUnique(MaximumFraction); }
            Fractions.Sort();
            if (Fractions.Num() > MaximumSteps) { Fractions.SetNum(MaximumSteps); }
            auto PredictedByPlan = TArray<FCk_ProceduralArticulation_Pose>{};
            auto QualifiedSamples = TArray<TArray<FCk_ProceduralAction_Sample>>{};
            auto MaxTravels = TArray<float>{};
            auto MaxAngularSteps = TArray<float>{};
            for (const auto& Plan : ClipPlans)
            {
                PredictedByPlan.Add(Plan.Action.Get<FFragment_ProceduralAction>()._MostRecentObservedPose);
                QualifiedSamples.Emplace();
                MaxTravels.Add(0.0f);
                MaxAngularSteps.Add(0.0f);
            }
            auto FinalFraction = 0.0;
            auto FinalCandidates = TArray<FCk_ArticulationTokenCandidate>{};
            auto FinalEvaluation = FCk_ArticulationBatchEvaluation{};
            auto FinalStepTimes = TArray<FCk_Time>{};
            for (const auto ProposedFraction : Fractions)
            {
                auto Fraction = ProposedFraction;
                auto StepAccepted = false;
                for (auto Refinement = 0; Refinement <= 32; ++Refinement)
                {
                    if (Fraction <= FinalFraction || NOT FMath::IsFinite(Fraction)) { break; }
                    auto StepCandidates = TArray<FCk_ArticulationTokenCandidate>{};
                    FCk_ProceduralLimbControl_Control::AppendReturnCandidates(Subject, DefaultPose,
                        StepCandidates, Fraction);
                    for (auto Index = ReturnCandidateCount; Index < Candidates.Num(); ++Index)
                    { StepCandidates.Add(Candidates[Index]); }
                    auto StepTimes = TArray<FCk_Time>{};
                    auto StepPolicies = TArray<ck::procedural_action::FEvaluatedPose>{};
                    auto StepMaxTravels = MaxTravels;
                    auto StepMaxAngularSteps = MaxAngularSteps;
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
                        if (NOT CanEvaluate) { break; }
                        const auto Seconds = Time.Get_Seconds();
                        auto Pose = procedural_action::FEvaluatedPose{};
                        if (Plan.EntryBlend)
                        {
                            auto Target = procedural_action::FEvaluatedPose{};
                            CanEvaluate = procedural_action::Evaluate(*Compiled.Get_Clip(),
                                Action._InitialPose, FCk_Time{}, Target)
                                && ck_procedural_action_player_service::BlendClipEntry(
                                    Action._InitialPose, Target,
                                    Seconds / Params.Get_Tuning().Get_Transition().Get_EntryBlend()
                                        .Get_Seconds(), Pose);
                        }
                        else
                        { CanEvaluate = procedural_action::Evaluate(*Compiled.Get_Clip(),
                            Action._InitialPose, Time, Pose); }
                        if (NOT CanEvaluate) { break; }
                        const auto Identity = Params.Get_Identity();
                        const auto Token = FCk_ArticulationControlToken{
                            static_cast<uint64>(Identity.Get_Value()),
                            static_cast<uint64>(Action._Generation)};
                        auto Candidate = FCk_ArticulationTokenCandidate{};
                        CanEvaluate = ck_procedural_action_player_service::BuildClipCandidate(Subject,
                            Compiled, Pose, Action._EntryPose, DefaultPose,
                            Token, static_cast<uint64>(Action._NextSampleSequence), Candidate);
                        if (NOT CanEvaluate) { break; }
                        StepCandidates.Add(MoveTemp(Candidate));
                        StepTimes.Add(Time);
                        StepPolicies.Add(MoveTemp(Pose));
                    }
                    if (CanEvaluate)
                    {
                        StepCandidates.StableSort([](const FCk_ArticulationTokenCandidate& InA,
                            const FCk_ArticulationTokenCandidate& InB)
                        { return InA.Get_Token().Get_InstanceId() < InB.Get_Token().Get_InstanceId(); });
                        auto StepEvaluation = FCk_ArticulationBatchEvaluation{};
                        const auto StepResult = FCk_ProceduralArticulation_Control::TryEvaluateCandidateBatch(
                            Subject, StepCandidates, StepEvaluation);
                        CanEvaluate = StepResult == ECk_ArticulationControlResult::Accepted;
                        if (CanEvaluate && StepEvaluation.Get_PredictedTokenPoses().Num()
                            == StepCandidates.Num())
                        {
                            for (auto Index = 0; Index < StepCandidates.Num(); ++Index)
                            {
                                if (OtherLimits.Contains(StepCandidates[Index].Get_Token().Get_InstanceId())
                                    && NOT QualifiesOther(StepCandidates, StepEvaluation, Index, Index + 1))
                                { CanEvaluate = false; break; }
                            }
                            if (NOT CanEvaluate) { Fraction = (FinalFraction + Fraction) * 0.5; continue; }
                            for (auto PlanIndex = 0; PlanIndex < ClipPlans.Num(); ++PlanIndex)
                            {
                                const auto& Plan = ClipPlans[PlanIndex];
                                const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                                const auto& Sampling = Params.Get_Tuning().Get_Sampling();
                                const auto CandidateIndex = StepCandidates.IndexOfByPredicate(
                                    [&Params](const auto& InCandidate)
                                    { return InCandidate.Get_Token().Get_InstanceId()
                                        == static_cast<uint64>(Params.Get_Identity().Get_Value()); });
                                if (CandidateIndex == INDEX_NONE) { CanEvaluate = false; break; }
                                const auto& Predicted = StepEvaluation.Get_PredictedTokenPoses()[CandidateIndex];
                                auto Travel = 0.0f;
                                auto Angular = 0.0f;
                                procedural_action::MeasureMotion(PredictedByPlan[PlanIndex], Predicted,
                                    Travel, Angular);
                                if (NOT FMath::IsFinite(Travel) || NOT FMath::IsFinite(Angular)
                                    || Travel > Sampling.Get_MaxEndpointTravelCm()
                                    || Angular > Sampling.Get_MaxSegmentAngularStepDegrees())
                                {
                                    CanEvaluate = false;
                                    break;
                                }
                                StepMaxTravels[PlanIndex] = FMath::Max(StepMaxTravels[PlanIndex], Travel);
                                StepMaxAngularSteps[PlanIndex] = FMath::Max(StepMaxAngularSteps[PlanIndex], Angular);
                                for (auto LimbIndex = 0; LimbIndex < StepPolicies[PlanIndex].Get_Limbs().Num();
                                    ++LimbIndex)
                                {
                                    const auto Id = Plan.Action.Get<FFragment_ProceduralAction_Compiled>()
                                        .Get_BoundLimbIds()[LimbIndex];
                                    const auto* Limb = Predicted.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
                                    { return InLimb.Get_Id() == Id; });
                                    if (Limb == nullptr || Limb->Get_ChainState() == ECk_ProceduralRig_ChainState::Crossing)
                                    { CanEvaluate = false; break; }
                                }
                                if (NOT CanEvaluate) { break; }
                                if (NOT Plan.EntryBlend)
                                {
                                    auto Events = 0;
                                    for (const auto& Event : Plan.Action.Get<FFragment_ProceduralAction_Compiled>()
                                        .Get_Clip()->Get_Events())
                                    {
                                        if ((Event.Get_Time() > Plan.From
                                            || (Plan.From == FCk_Time{}
                                                && NOT Plan.Action.Get<FFragment_ProceduralAction>()
                                                    ._HasAuthoredReceipt
                                                && Event.Get_Time() == FCk_Time{}))
                                            && Event.Get_Time() <= StepTimes[PlanIndex])
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
                                if (auto* Limit = OtherLimits.Find(StepCandidates[Index].Get_Token().Get_InstanceId()))
                                { Limit->Previous = StepEvaluation.Get_PredictedTokenPoses()[Index]; }
                            }
                            for (auto PlanIndex = 0; PlanIndex < ClipPlans.Num(); ++PlanIndex)
                            {
                                const auto& Params = ClipPlans[PlanIndex].Action
                                    .Get<FFragment_ProceduralAction_Params>();
                                const auto CandidateIndex = StepCandidates.IndexOfByPredicate(
                                    [&Params](const auto& InCandidate)
                                    { return InCandidate.Get_Token().Get_InstanceId()
                                        == static_cast<uint64>(Params.Get_Identity().Get_Value()); });
                                const auto Pose = StepEvaluation.Get_PredictedTokenPoses()[CandidateIndex];
                                PredictedByPlan[PlanIndex] = Pose;
                                QualifiedSamples[PlanIndex].Add(FCk_ProceduralAction_Sample{
                                    ClipPlans[PlanIndex].EntryBlend ? FCk_Time{} : StepTimes[PlanIndex],
                                    Pose, ECk_ProceduralAction_SampleEvidence::Evaluated});
                            }
                            FinalFraction = Fraction;
                            MaxTravels = MoveTemp(StepMaxTravels);
                            MaxAngularSteps = MoveTemp(StepMaxAngularSteps);
                            FinalCandidates = MoveTemp(StepCandidates);
                            FinalEvaluation = MoveTemp(StepEvaluation);
                            FinalStepTimes = MoveTemp(StepTimes);
                            StepAccepted = true;
                            break;
                        }
                    }
                    Fraction = (FinalFraction + Fraction) * 0.5;
                }
                if (NOT StepAccepted || FinalFraction < ProposedFraction) { break; }
            }
            if (FinalFraction <= 0.0)
            {
                for (auto& Plan : ClipPlans)
                {
                    if (ck::Is_NOT_Valid(Plan.Action)
                        || NOT Plan.Action.Has<FFragment_ProceduralAction>()
                        || Plan.Action.Has<FTag_DestroyEntity_Initiate>())
                    { continue; }
                    auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
                    const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                    const auto Identity = Params.Get_Identity();
                    const auto Token = FCk_ArticulationControlToken{
                        static_cast<uint64>(Identity.Get_Value()), static_cast<uint64>(Action._Generation)};
                    InPlayer._Receipts.CloseWindows(Identity, FName{TEXT("SubjectPoseRejected")});
                    FCk_ProceduralLimbControl_Control::BeginReturn(Subject, Token,
                        FName{TEXT("SubjectPoseRejected")});
                    Action._Result = FCk_ProceduralAction_Result{
                        ECk_ProceduralAction_Outcome::SubjectPoseRejected,
                        ECk_ProceduralAction_Recovery::Pending, FName{TEXT("SubjectPoseRejected")}};
                    Action.CommitState(Plan.Action, Params, ECk_ProceduralAction_State::Cancelling);
                }
                return;
            }
            Candidates = MoveTemp(FinalCandidates);
            Evaluation = MoveTemp(FinalEvaluation);
            // Refinement may have last probed a rejected fraction. Return settlement must use
            // the proposal for the exact accepted fraction that is staged below.
            auto FinalReturnScratch = TArray<FCk_ArticulationTokenCandidate>{};
            FCk_ProceduralLimbControl_Control::AppendReturnCandidates(Subject, DefaultPose,
                FinalReturnScratch, FinalFraction);
            for (auto PlanIndex = 0; PlanIndex < ClipPlans.Num(); ++PlanIndex)
            {
                auto& Plan = ClipPlans[PlanIndex];
                auto& Action = Plan.Action.Get<FFragment_ProceduralAction>();
                const auto& Params = Plan.Action.Get<FFragment_ProceduralAction_Params>();
                const auto& Clip = *Plan.Action.Get<FFragment_ProceduralAction_Compiled>().Get_Clip();
                const auto To = FinalStepTimes[PlanIndex];
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
                        if ((Event.Get_Time() > Plan.From
                            || (Plan.From == FCk_Time{} && NOT Action._HasAuthoredReceipt
                                && Event.Get_Time() == FCk_Time{}))
                            && Event.Get_Time() <= To)
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
                Action._PendingClipInterval = FCk_ProceduralAction_Interval{
                    Action._AcceptedTime, Action._PendingToTime,
                    Plan.EntryBlend || NOT Clip.Get_Phases().IsValidIndex(EvaluatedFinal.Get_PhaseIndex())
                        ? NAME_None : Clip.Get_Phases()[EvaluatedFinal.Get_PhaseIndex()].Get_Id(),
                    InPlayer._Receipts.HasAccepted(Params.Get_Identity())
                        ? (Action._HasHoldSinceLastReceipt
                            ? ECk_ProceduralAction_Continuity::ControllerChanged
                            : ECk_ProceduralAction_Continuity::Continuous)
                        : ECk_ProceduralAction_Continuity::FirstSample,
                    Windows, Events};
                const auto QualifiedCount = QualifiedSamples[PlanIndex].Num();
                Action._PendingIntermediateSamples = MoveTemp(QualifiedSamples[PlanIndex]);
                if (NOT Action._PendingIntermediateSamples.IsEmpty())
                { Action._PendingIntermediateSamples.Pop(); }
                Action._PendingPredictedPose = PredictedByPlan[PlanIndex];
                Action._PendingClipSampling = FCk_ProceduralAction_SamplingReport{
                    QualifiedCount, 1, MaxTravels[PlanIndex], MaxAngularSteps[PlanIndex],
                    Action._PendingBacklog};
            }
        }
        if (Evaluation.Get_PredictedTokenPoses().Num() != Candidates.Num()) { return; }
        auto Keys = TArray<FCk_ArticulationStageKey>{};
        const auto Staged = FCk_ProceduralArticulation_Control::TryStageCandidateBatch(
            Subject, MoveTemp(Evaluation), Keys);
        if (Staged != ECk_ArticulationControlResult::Accepted) { return; }
        FCk_ProceduralLimbControl_Control::Notify_StagedBatch(Subject, Keys, ReattachmentBarrier);
        for (const auto& Key : Keys)
        {
            for (auto ActionHandle : ActionSnapshot)
            {
                if (ck::Is_NOT_Valid(ActionHandle) || NOT ActionHandle.Has<FFragment_ProceduralAction>()
                    || ActionHandle.Has<FTag_DestroyEntity_Initiate>())
                { continue; }
                auto& Action = ActionHandle.Get<FFragment_ProceduralAction>();
                const auto& Params = ActionHandle.Get<FFragment_ProceduralAction_Params>();
                if (static_cast<uint64>(Params.Get_Identity().Get_Value()) != Key.Get_Token().Get_InstanceId()
                    || static_cast<uint64>(Action._Generation) != Key.Get_Token().Get_Generation())
                { continue; }
                if (Key.Get_SampleSequence() == 0)
                {
                    if (Action._State == ECk_ProceduralAction_State::Acquiring
                        || Action._State == ECk_ProceduralAction_State::Playing)
                    { Action._PendingHoldStageKey = Key; }
                    break;
                }
                Action._PendingStageKey = Key;
                ++Action._NextSampleSequence;
                break;
            }
        }
    }
}
