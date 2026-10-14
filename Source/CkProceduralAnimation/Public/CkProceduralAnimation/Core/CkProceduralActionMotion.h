#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

#include <limits>

namespace ck::procedural_action
{
    // Solver lengths live in topology. Part scale is visual, but a scale change cannot be treated
    // as continuous sampled motion; the sampler below requires it to remain stable across an interval.
    inline auto
    IsSegmentPoseValid(
        const FTransform& InPose)
        -> bool
    {
        return NOT InPose.ContainsNaN()
            && FMath::Abs(InPose.GetRotation().SizeSquared() - 1.0) <= 1.0e-4
            && InPose.GetScale3D().GetAbsMin() > UE_SMALL_NUMBER;
    }

    inline auto
    FindLimb(
        const FCk_ProceduralArticulation_Pose& InPose,
        FName InId)
        -> const FCk_ProceduralArticulation_LimbPose*
    {
        return InPose.Get_Limbs().FindByPredicate([InId](const auto& InLimb)
        { return InLimb.Get_Id() == InId; });
    }

    inline auto
    IsMotionPoseValid(
        const FCk_ProceduralArticulation_Pose& InPose)
        -> bool
    {
        const auto IsRigid = [](const FTransform& InTransform)
        {
            return NOT InTransform.ContainsNaN() && InTransform.GetRotation().IsNormalized()
                && InTransform.GetScale3D().Equals(FVector::OneVector, 1.0e-6);
        };
        if (NOT IsRigid(InPose.Get_RootFrame())
            || (InPose.Get_HasBodyPresentation() && NOT IsRigid(InPose.Get_BodyPresentation())))
        { return false; }
        for (auto Index = 0; Index < InPose.Get_Limbs().Num(); ++Index)
        {
            const auto& Limb = InPose.Get_Limbs()[Index];
            if (Limb.Get_Id().IsNone() || Limb.Get_Segments().IsEmpty()
                || NOT IsRigid(Limb.Get_AcceptedEndpoint()))
            { return false; }
            for (auto Other = 0; Other < Index; ++Other)
            {
                if (InPose.Get_Limbs()[Other].Get_Id() == Limb.Get_Id())
                { return false; }
            }
            for (const auto& Segment : Limb.Get_Segments())
            {
                if (NOT IsSegmentPoseValid(Segment))
                { return false; }
            }
        }
        return true;
    }

    // Incomparable geometry is unbounded motion, never a zero-length accepted interval.
    inline auto
    MeasureMotion(
        const FCk_ProceduralArticulation_Pose& InPrevious,
        const FCk_ProceduralArticulation_Pose& InCurrent,
        float& OutEndpointTravelCm,
        float& OutSegmentAngularStepDegrees)
        -> void
    {
        OutEndpointTravelCm = std::numeric_limits<float>::infinity();
        OutSegmentAngularStepDegrees = std::numeric_limits<float>::infinity();
        if (NOT IsMotionPoseValid(InPrevious) || NOT IsMotionPoseValid(InCurrent)
            || InPrevious.Get_HasBodyPresentation() != InCurrent.Get_HasBodyPresentation()
            || InPrevious.Get_Limbs().Num() != InCurrent.Get_Limbs().Num())
        { return; }
        auto EndpointTravelCm = static_cast<float>(FVector::Distance(
            InPrevious.Get_RootFrame().GetTranslation(), InCurrent.Get_RootFrame().GetTranslation()));
        auto SegmentAngularStepDegrees = static_cast<float>(FMath::RadiansToDegrees(
            InPrevious.Get_RootFrame().GetRotation().AngularDistance(
                InCurrent.Get_RootFrame().GetRotation())));
        if (InPrevious.Get_HasBodyPresentation() && InCurrent.Get_HasBodyPresentation())
        {
            EndpointTravelCm = FMath::Max(EndpointTravelCm, static_cast<float>(FVector::Distance(
                InPrevious.Get_BodyPresentation().GetTranslation(),
                InCurrent.Get_BodyPresentation().GetTranslation())));
            SegmentAngularStepDegrees = FMath::Max(SegmentAngularStepDegrees,
                static_cast<float>(FMath::RadiansToDegrees(
                    InPrevious.Get_BodyPresentation().GetRotation().AngularDistance(
                        InCurrent.Get_BodyPresentation().GetRotation()))));
        }
        for (const auto& Limb : InCurrent.Get_Limbs())
        {
            const auto* Previous = FindLimb(InPrevious, Limb.Get_Id());
            if (Previous == nullptr || Previous->Get_Segments().Num() != Limb.Get_Segments().Num())
            { return; }
            EndpointTravelCm = FMath::Max(EndpointTravelCm,
                static_cast<float>(FVector::Distance(Previous->Get_AcceptedEndpoint().GetTranslation(),
                    Limb.Get_AcceptedEndpoint().GetTranslation())));
            const auto Count = Limb.Get_Segments().Num();
            for (auto Index = 0; Index < Count; ++Index)
            {
                if (NOT Previous->Get_Segments()[Index].GetScale3D().Equals(
                    Limb.Get_Segments()[Index].GetScale3D(), 1.0e-6))
                { return; }
                const auto Angle = FMath::RadiansToDegrees(Previous->Get_Segments()[Index].GetRotation()
                    .AngularDistance(Limb.Get_Segments()[Index].GetRotation()));
                SegmentAngularStepDegrees = FMath::Max(SegmentAngularStepDegrees,
                    static_cast<float>(Angle));
            }
        }
        OutEndpointTravelCm = EndpointTravelCm;
        OutSegmentAngularStepDegrees = SegmentAngularStepDegrees;
    }
}
