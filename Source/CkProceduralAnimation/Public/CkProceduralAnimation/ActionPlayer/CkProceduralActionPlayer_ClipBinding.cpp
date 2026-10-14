#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ServiceInternal.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include "CkCore/Validation/CkIsValid.h"

namespace ck_procedural_action_player_service
{
    auto
    IsFiniteDirection(
        const FVector& InValue)
        -> bool
    {
        return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
            && FMath::IsFinite(InValue.Z) && NOT InValue.IsNearlyZero();
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
        if (ck::Is_NOT_Valid(Limb))
        { return false; }
        auto Layout = FCk_ProceduralLimb_Layout{};
        if (NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout))
        { return false; }
        OutChainLength = 0.0f;
        for (const auto Length : Layout.Get_SegmentLengths())
        { OutChainLength += Length; }
        if (NOT FMath::IsFinite(OutChainLength) || OutChainLength <= 0.0f)
        { return false; }
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
        return ck::procedural_animation::IsRigid(OutFrame, RigidTolerance);
    }

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
                if (Kind == ck::procedural_action::EEventKind::WindowExit)
                { --ActiveWindows; }
                else if (Kind == ck::procedural_action::EEventKind::WindowEnter)
                { ++ActiveWindows; }
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
            if (NOT ck::procedural_animation::IsRigid(OutInitial.Get_Body(), RigidTolerance))
            { return false; }
        }
        for (auto Index = 0; Index < Clip.Get_Slots().Num(); ++Index)
        {
            const auto& Slot = Clip.Get_Slots()[Index];
            const auto LimbId = InCompiled.Get_BoundLimbIds()[Index];
            const auto* LimbPose = InEntry.Get_Limbs().FindByPredicate([LimbId](const auto& InLimb)
            { return InLimb.Get_Id() == LimbId; });
            if (LimbPose == nullptr || NOT ck::procedural_animation::IsRigid(LimbPose->Get_AcceptedEndpoint(), RigidTolerance))
            { return false; }
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
                if (NOT ck::procedural_animation::IsRigid(InitialWorld, RigidTolerance))
                { return false; }
            }
            auto Local = InitialWorld.GetRelativeTransform(Frame);
            if (Slot.Get_Space() == ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
            { Local.SetTranslation(Local.GetTranslation() / ChainLength); }
            if (NOT ck::procedural_animation::IsRigid(Local, RigidTolerance))
            { return false; }
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
}
