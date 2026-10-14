#include "CkProceduralAnimationEditor/CkProceduralActionPreviewSpace.h"

#include "CkProceduralAnimation/Core/CkProceduralBodySupport.h"

namespace ck::procedural_action_editor
{
    auto
        ResolveBodyWorld(
            const UCk_ProceduralActionPreview_Config& InConfig,
            const procedural_action::FCompiledClip& InClip,
            const procedural_action::FEvaluatedPose& InPose,
            FTransform& OutBodyWorld)
        -> bool
    {
        if (InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (InClip.Get_HasBodyChannel())
            { return false; }
            OutBodyWorld = InConfig.Get_RootFrame();
            return true;
        }
        if (NOT InClip.Get_HasBodyChannel())
        {
            OutBodyWorld = InConfig.Get_PresentationFrame();
            return true;
        }
        const auto BaseLocal = InConfig.Get_PresentationFrame()
            .GetRelativeTransform(InConfig.Get_RootFrame());
        const auto Composed = ck::ComposeProceduralBodyPoseAction(BaseLocal, InPose.Get_Body());
        if (NOT Composed.IsSet())
        { return false; }
        OutBodyWorld = Composed.GetValue() * InConfig.Get_RootFrame();
        return NOT OutBodyWorld.ContainsNaN();
    }

    auto
        ResolveSlotFrame(
            const UCk_ProceduralActionPreview_Config& InConfig,
            const procedural_action::FCompiledSlot& InSlot,
            const FTransform& InBodyWorld,
            FTransform& OutFrame,
            float& OutPositionScale)
        -> bool
    {
        OutPositionScale = 1.0f;
        switch (InSlot.Get_Space())
        {
            case ECk_ProceduralAction_TrackSpace::BodyLocal:
            case ECk_ProceduralAction_TrackSpace::RootAtStart:
                OutFrame = InConfig.Get_RootFrame();
                break;
            case ECk_ProceduralAction_TrackSpace::LimbRestNormalized:
            {
                OutPositionScale = 0.0f;
                const auto* Binding = InConfig.Get_SlotBindings().FindByPredicate([&InSlot](const auto& InBinding)
                    { return InBinding.Get_Slot() == InSlot.Get_Id(); });
                if (Binding == nullptr)
                { return false; }
                if (InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
                {
                    const auto* Rig = InConfig.Get_WalkingRig().Get();
                    if (Rig == nullptr)
                    { return false; }
                    const auto* Leg = Rig->Get_Legs().FindByPredicate([&Binding](const auto& InLeg)
                        { return InLeg.Get_Id() == Binding->Get_LimbId(); });
                    if (Leg == nullptr)
                    { return false; }
                    for (const auto Length : Leg->Get_Chain().Get_SegmentLengths())
                    { OutPositionScale += Length; }
                    OutFrame = FTransform{Leg->Get_Placement().Get_RestFootLocal()} * InBodyWorld;
                }
                else
                {
                    const auto* Layout = InConfig.Get_Layout().Get_Limbs().FindByPredicate(
                        [&Binding](const auto& InLayout)
                        { return InLayout.Get_Id() == Binding->Get_LimbId(); });
                    if (Layout == nullptr)
                    { return false; }
                    for (const auto Length : Layout->Get_SegmentLengths())
                    { OutPositionScale += Length; }
                    OutFrame = Layout->Get_RestEndpointLocal() * Layout->Get_AttachmentLocal() * InBodyWorld;
                }
                break;
            }
            default:
                return false;
        }
        return FMath::IsFinite(OutPositionScale) && OutPositionScale > 0.0f
            && NOT OutFrame.ContainsNaN() && OutFrame.GetRotation().IsNormalized();
    }

    auto
        ToWorldPosition(
            const FTransform& InFrame,
            float InPositionScale,
            const FVector& InTrackPosition)
        -> FVector
    {
        return InFrame.TransformPosition(InTrackPosition * InPositionScale);
    }

    auto
        ResolveRestInTrackSpace(
            const UCk_ProceduralActionPreview_Config& InConfig,
            const procedural_action::FCompiledSlot& InSlot,
            FTransform& OutRestInTrackSpace)
        -> bool
    {
        OutRestInTrackSpace = FTransform::Identity;

        const auto* Binding = InConfig.Get_SlotBindings().FindByPredicate([&InSlot](const auto& InBinding)
            { return InBinding.Get_Slot() == InSlot.Get_Id(); });
        if (Binding == nullptr)
        { return false; }

        const auto IsRigid = [](const FTransform& InTransform) -> bool
        {
            return NOT InTransform.ContainsNaN()
                && InTransform.GetRotation().IsNormalized()
                && InTransform.GetScale3D().Equals(FVector::OneVector);
        };
        const auto HasValidLengths = [](const TArray<float>& InLengths) -> bool
        {
            if (InLengths.IsEmpty())
            { return false; }
            for (const float Length : InLengths)
            {
                if (NOT FMath::IsFinite(Length) || Length <= 0.0f)
                { return false; }
            }
            return true;
        };
        const bool bWalking = InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
        const FTransform BodyWorld = bWalking ? InConfig.Get_RootFrame() : InConfig.Get_PresentationFrame();
        if (NOT IsRigid(BodyWorld))
        { return false; }
        auto RestWorld = FTransform::Identity;
        if (bWalking)
        {
            const auto* Rig = InConfig.Get_WalkingRig().Get();
            if (Rig == nullptr)
            { return false; }
            const auto* Leg = Rig->Get_Legs().FindByPredicate([&Binding](const auto& InLeg)
                { return InLeg.Get_Id() == Binding->Get_LimbId(); });
            if (Leg == nullptr)
            { return false; }
            if (NOT HasValidLengths(Leg->Get_Chain().Get_SegmentLengths()))
            { return false; }
            RestWorld = FTransform{Leg->Get_Placement().Get_RestFootLocal()} * BodyWorld;
        }
        else
        {
            const auto* Layout = InConfig.Get_Layout().Get_Limbs().FindByPredicate([&Binding](const auto& InLayout)
                { return InLayout.Get_Id() == Binding->Get_LimbId(); });
            if (Layout == nullptr)
            { return false; }
            if (NOT HasValidLengths(Layout->Get_SegmentLengths())
                || NOT IsRigid(Layout->Get_AttachmentLocal())
                || NOT IsRigid(Layout->Get_RestEndpointLocal()))
            { return false; }
            RestWorld = Layout->Get_RestEndpointLocal() * Layout->Get_AttachmentLocal() * BodyWorld;
        }

        auto Frame = FTransform::Identity;
        auto PositionScale = 0.0f;
        if (NOT ResolveSlotFrame(InConfig, InSlot, BodyWorld, Frame, PositionScale)
            || NOT IsRigid(RestWorld))
        { return false; }

        OutRestInTrackSpace = RestWorld.GetRelativeTransform(Frame);
        if (InSlot.Get_Space() == ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
        { OutRestInTrackSpace.SetTranslation(OutRestInTrackSpace.GetTranslation() / PositionScale); }
        return IsRigid(OutRestInTrackSpace);
    }

    auto
        ToTrackPosition(
            const FTransform& InFrame,
            float InPositionScale,
            const FVector& InWorldPosition)
        -> FVector
    {
        return InFrame.InverseTransformPosition(InWorldPosition) / InPositionScale;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    auto
        TryGetObservedHip(
            const FCk_ProceduralArticulation_LimbPose& InLimb,
            double InFirstSegmentLengthCm,
            FVector& OutHip)
        -> bool
    {
        if (InLimb.Get_Segments().IsEmpty() || NOT FMath::IsFinite(InFirstSegmentLengthCm) || InFirstSegmentLengthCm <= 0.0)
        { return false; }
        const auto& FirstSegment = InLimb.Get_Segments()[0];
        const auto Rotation = FirstSegment.GetRotation();
        if (FirstSegment.GetTranslation().ContainsNaN() || NOT Rotation.IsNormalized())
        { return false; }
        OutHip = FirstSegment.GetTranslation() - Rotation.RotateVector(FVector::XAxisVector) * (InFirstSegmentLengthCm * 0.5);
        return NOT OutHip.ContainsNaN();
    }

    auto
        Get_LimbReach(
            const UCk_ProceduralActionPreview_Config& InConfig,
            FName InLimbId,
            double& OutReachCm,
            double& OutFirstLengthCm)
        -> bool
    {
        const auto* Lengths = static_cast<const TArray<float>*>(nullptr);
        if (InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (const auto* Rig = InConfig.Get_WalkingRig().Get())
            {
                if (const auto* Leg = Rig->Get_Legs().FindByPredicate([InLimbId](const FCk_ProceduralLeg_Spec& InLeg)
                    { return InLeg.Get_Id() == InLimbId; }))
                { Lengths = &Leg->Get_Chain().Get_SegmentLengths(); }
            }
        }
        else if (const auto* Layout = InConfig.Get_Layout().Get_Limbs().FindByPredicate(
            [InLimbId](const FCk_ProceduralLimb_Layout& InLayout) { return InLayout.Get_Id() == InLimbId; }))
        { Lengths = &Layout->Get_SegmentLengths(); }
        if (Lengths == nullptr || Lengths->IsEmpty())
        { return false; }
        OutReachCm = 0.0;
        for (const auto Length : *Lengths)
        { OutReachCm += Length; }
        OutFirstLengthCm = (*Lengths)[0];
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
