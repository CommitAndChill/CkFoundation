#include "CkProceduralAnimation/Core/CkProceduralHeightPlanning.h"

namespace ck_procedural_height_planning
{
#if !ENABLE_VECTORIZED_TRANSFORM
    auto
        Get_FiniteVector(
            double InX,
            double InY,
            double InZ)
        -> TOptional<FVector>
    {
        if (NOT FMath::IsFinite(InX) || NOT FMath::IsFinite(InY) || NOT FMath::IsFinite(InZ))
        { return {}; }
        return FVector{InX, InY, InZ};
    }
#endif

    // Frames and local points are validated by the caller. Preserve the selected backend's arithmetic.
    auto
        Get_CheckedPosition(
            const FTransform& InFrame,
            const FVector& InLocal)
        -> TOptional<FVector>
    {
#if ENABLE_VECTORIZED_TRANSFORM
        const auto Result = InFrame.TransformPosition(InLocal);
        if (Result.ContainsNaN())
        { return {}; }
        return Result;
#else
        const auto Scale = InFrame.GetScale3D();
        const auto Rotation = InFrame.GetRotation();
        const auto Translation = InFrame.GetTranslation();
        const auto Scaled = Get_FiniteVector(Scale.X * InLocal.X, Scale.Y * InLocal.Y, Scale.Z * InLocal.Z);
        if (NOT Scaled.IsSet())
        { return {}; }
        const auto Cross = Get_FiniteVector(Rotation.Y * Scaled->Z - Rotation.Z * Scaled->Y,
            Rotation.Z * Scaled->X - Rotation.X * Scaled->Z, Rotation.X * Scaled->Y - Rotation.Y * Scaled->X);
        if (NOT Cross.IsSet())
        { return {}; }
        const auto TwiceCross = Get_FiniteVector(2.0 * Cross->X, 2.0 * Cross->Y, 2.0 * Cross->Z);
        if (NOT TwiceCross.IsSet())
        { return {}; }
        const auto Weighted = Get_FiniteVector(Rotation.W * TwiceCross->X,
            Rotation.W * TwiceCross->Y, Rotation.W * TwiceCross->Z);
        if (NOT Weighted.IsSet())
        { return {}; }
        const auto Base = Get_FiniteVector(Scaled->X + Weighted->X, Scaled->Y + Weighted->Y, Scaled->Z + Weighted->Z);
        if (NOT Base.IsSet())
        { return {}; }
        const auto SecondCross = Get_FiniteVector(Rotation.Y * TwiceCross->Z - Rotation.Z * TwiceCross->Y,
            Rotation.Z * TwiceCross->X - Rotation.X * TwiceCross->Z,
            Rotation.X * TwiceCross->Y - Rotation.Y * TwiceCross->X);
        if (NOT SecondCross.IsSet())
        { return {}; }
        const auto Rotated = Get_FiniteVector(Base->X + SecondCross->X,
            Base->Y + SecondCross->Y, Base->Z + SecondCross->Z);
        if (NOT Rotated.IsSet())
        { return {}; }
        return Get_FiniteVector(Rotated->X + Translation.X, Rotated->Y + Translation.Y, Rotated->Z + Translation.Z);
#endif
    }

    auto
        Get_CheckedPose(
            const FTransform& InBody,
            const FTransform& InPose)
        -> TOptional<FTransform>
    {
#if ENABLE_VECTORIZED_TRANSFORM
        const auto Result = InPose * InBody;
#else
        const auto Translation = Get_CheckedPosition(InBody, InPose.GetTranslation());
        if (NOT Translation.IsSet())
        { return {}; }
        const auto Result = FTransform{InBody.GetRotation() * InPose.GetRotation(), Translation.GetValue(),
            InPose.GetScale3D() * InBody.GetScale3D()};
#endif
        if (Result.ContainsNaN())
        { return {}; }
        return Result;
    }

    auto
        Get_IsNativeVector(
            const FVector& InVector)
        -> bool
    {
        const auto Limit = static_cast<double>(TNumericLimits<float>::Max());
        return NOT InVector.ContainsNaN() && FMath::Abs(InVector.X) <= Limit
            && FMath::Abs(InVector.Y) <= Limit && FMath::Abs(InVector.Z) <= Limit;
    }

    auto
        Get_AreNativeInputsValid(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            TArrayView<const ck::FProceduralSurfaceReachPaceAnchor> InAnchors,
            const FVector& InCandidateHipLocal,
            const FVector& InCandidatePoint,
            float InCandidateReach)
        -> bool
    {
        // This proposal feeds native float queries. Reject their unrepresentable inputs before any vector
        // subtraction, transform composition or squared distance can overflow in the stance calculation.
        if (InAnchors.Num() <= 0 || InAnchors.Num() > 64 || InAnchors.GetData() == nullptr
            || NOT Get_IsNativeVector(InBody.GetLocation()) || NOT Get_IsNativeVector(InCandidateHipLocal)
            || NOT Get_IsNativeVector(InCandidatePoint) || NOT FMath::IsFinite(InCandidateReach) || InCandidateReach <= 0.0f
            || (InPoseOffset.IsSet() && NOT Get_IsNativeVector(InPoseOffset->GetLocation())))
        { return false; }
        for (const auto& Anchor : InAnchors)
        {
            if (NOT Get_IsNativeVector(Anchor.Get_HipLocal()) || NOT Get_IsNativeVector(Anchor.Get_FootWorld()))
            { return false; }
        }
        return true;
    }

    // Called only after the rigid frames, support normal and anchor data have been validated.
    auto
        Get_RetainedPlaneLower(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const ck::FProceduralSurfaceReachPaceAnchor> InAnchors)
        -> TOptional<double>
    {
        const auto Posed = InPoseOffset.IsSet() ? InPoseOffset.GetValue() * InBody : InBody;
        auto Lower = -TNumericLimits<double>::Max();
        for (const auto& Anchor : InAnchors)
        {
            const auto Hip = InBody.TransformPosition(Anchor.Get_HipLocal());
            const auto SimLower = FVector::DotProduct(Anchor.Get_FootWorld() - Hip, InUp);
            if (NOT FMath::IsFinite(SimLower))
            { return {}; }
            Lower = FMath::Max(Lower, SimLower);
            if (InPoseOffset.IsSet())
            {
                const auto PosedHip = Posed.TransformPosition(Anchor.Get_HipLocal());
                const auto PosedLower = FVector::DotProduct(Anchor.Get_FootWorld() - PosedHip, InUp);
                if (NOT FMath::IsFinite(PosedLower))
                { return {}; }
                Lower = FMath::Max(Lower, PosedLower);
            }
        }
        return FMath::IsFinite(Lower) ? TOptional<double>{Lower} : TOptional<double>{};
    }

    auto
        DoIntersect(
            const FVector& InHip,
            const FVector& InFoot,
            const FVector& InUp,
            double InReach,
            FVector2D& InOutInterval)
        -> bool
    {
        if (InHip.ContainsNaN() || InFoot.ContainsNaN() || NOT FMath::IsFinite(InReach) || InReach <= 0.0)
        { return false; }
        const auto DeltaX = InHip.X - InFoot.X;
        const auto DeltaY = InHip.Y - InFoot.Y;
        const auto DeltaZ = InHip.Z - InFoot.Z;
        if (NOT FMath::IsFinite(DeltaX) || NOT FMath::IsFinite(DeltaY) || NOT FMath::IsFinite(DeltaZ))
        { return false; }
        const auto Delta = FVector{DeltaX, DeltaY, DeltaZ};
        const auto Along = FVector::DotProduct(Delta, InUp);
        const auto DistanceSquared = Delta.SizeSquared();
        const auto AlongSquared = Along * Along;
        if (NOT FMath::IsFinite(Along) || NOT FMath::IsFinite(DistanceSquared) || NOT FMath::IsFinite(AlongSquared))
        { return false; }
        const auto PerpendicularSquared = DistanceSquared - AlongSquared;
        if (NOT FMath::IsFinite(PerpendicularSquared))
        { return false; }
        const auto RadiusSquared = InReach * InReach - FMath::Max(0.0, PerpendicularSquared);
        if (NOT FMath::IsFinite(RadiusSquared) || RadiusSquared < 0.0)
        { return false; }
        const auto HalfSpan = FMath::Sqrt(RadiusSquared);
        const auto Lower = -Along - HalfSpan;
        const auto Upper = -Along + HalfSpan;
        if (NOT FMath::IsFinite(Lower) || NOT FMath::IsFinite(Upper))
        { return false; }
        InOutInterval.X = FMath::Max(InOutInterval.X, Lower);
        InOutInterval.Y = FMath::Min(InOutInterval.Y, Upper);
        return InOutInterval.X <= InOutInterval.Y;
    }
}

namespace ck
{
    auto
        Get_ProceduralHeightStanceInterval(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
            double InMaxTranslation)
        -> TOptional<FVector2D>
    {
        if (InBody.ContainsNaN() || NOT InBody.GetRotation().IsNormalized()
            || NOT InBody.GetScale3D().Equals(FVector::OneVector, 1.0e-4)
            || InUp.ContainsNaN() || NOT InUp.IsNormalized()
            || InAnchors.Num() < 0 || InAnchors.Num() > 64 || (InAnchors.Num() > 0 && InAnchors.GetData() == nullptr)
            || NOT FMath::IsFinite(InMaxTranslation) || InMaxTranslation <= 0.0
            || (InPoseOffset.IsSet() && (InPoseOffset->ContainsNaN() || NOT InPoseOffset->GetRotation().IsNormalized()
                || NOT InPoseOffset->GetScale3D().Equals(FVector::OneVector, 1.0e-4))))
        { return {}; }

        auto Interval = FVector2D{-InMaxTranslation, InMaxTranslation};
        if (InAnchors.IsEmpty())
        { return Interval; }
        auto Posed = TOptional<FTransform>{};
        if (InPoseOffset.IsSet())
        {
            Posed = ck_procedural_height_planning::Get_CheckedPose(InBody, InPoseOffset.GetValue());
            if (NOT Posed.IsSet())
            { return {}; }
        }
        for (const auto& Anchor : InAnchors)
        {
            if (Anchor.Get_HipLocal().ContainsNaN() || Anchor.Get_FootWorld().ContainsNaN()
                || NOT FMath::IsFinite(Anchor.Get_Reach()) || Anchor.Get_Reach() <= 0.0f)
            { return {}; }
            const auto Hip = ck_procedural_height_planning::Get_CheckedPosition(InBody, Anchor.Get_HipLocal());
            if (NOT Hip.IsSet() || NOT ck_procedural_height_planning::DoIntersect(Hip.GetValue(),
                    Anchor.Get_FootWorld(), InUp, Anchor.Get_Reach() + 1.0e-3, Interval))
            { return {}; }
            if (Posed.IsSet())
            {
                const auto PosedHip = ck_procedural_height_planning::Get_CheckedPosition(Posed.GetValue(), Anchor.Get_HipLocal());
                if (NOT PosedHip.IsSet() || NOT ck_procedural_height_planning::DoIntersect(PosedHip.GetValue(),
                        Anchor.Get_FootWorld(), InUp, Anchor.Get_Reach() + 1.0e-3, Interval))
                { return {}; }
            }
        }
        return Interval;
    }

    auto
        Get_ProceduralHeightRetainedPlaneLower(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors)
        -> TOptional<double>
    {
        if (InAnchors.Num() <= 0 || InAnchors.Num() > 64 || InAnchors.GetData() == nullptr
            || InBody.ContainsNaN() || NOT InBody.GetRotation().IsNormalized()
            || NOT InBody.GetScale3D().Equals(FVector::OneVector, 1.0e-4)
            || NOT ck_procedural_height_planning::Get_IsNativeVector(InBody.GetLocation())
            || InUp.ContainsNaN() || NOT InUp.IsNormalized()
            || (InPoseOffset.IsSet() && (InPoseOffset->ContainsNaN() || NOT InPoseOffset->GetRotation().IsNormalized()
                || NOT InPoseOffset->GetScale3D().Equals(FVector::OneVector, 1.0e-4)
                || NOT ck_procedural_height_planning::Get_IsNativeVector(InPoseOffset->GetLocation()))))
        { return {}; }
        for (const auto& Anchor : InAnchors)
        {
            if (NOT ck_procedural_height_planning::Get_IsNativeVector(Anchor.Get_HipLocal())
                || NOT ck_procedural_height_planning::Get_IsNativeVector(Anchor.Get_FootWorld())
                || NOT FMath::IsFinite(Anchor.Get_Reach()) || Anchor.Get_Reach() <= 0.0f)
            { return {}; }
        }
        return ck_procedural_height_planning::Get_RetainedPlaneLower(InBody, InPoseOffset, InUp, InAnchors);
    }

    auto
        Get_ProceduralHeightPlanningInterval(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
            const FVector& InCandidateHipLocal,
            const FVector& InCandidatePoint,
            float InCandidateReach,
            double InMaxTranslation)
        -> TOptional<FVector2D>
    {
        auto Interval = Get_ProceduralHeightStanceInterval(InBody, InPoseOffset, InUp, InAnchors, InMaxTranslation);
        if (NOT Interval.IsSet() || InCandidateHipLocal.ContainsNaN() || InCandidatePoint.ContainsNaN()
            || NOT FMath::IsFinite(InCandidateReach) || InCandidateReach <= 0.0f)
        { return {}; }
        auto Posed = TOptional<FTransform>{};
        if (InPoseOffset.IsSet())
        {
            Posed = ck_procedural_height_planning::Get_CheckedPose(InBody, InPoseOffset.GetValue());
            if (NOT Posed.IsSet())
            { return {}; }
        }
        const auto Hip = ck_procedural_height_planning::Get_CheckedPosition(InBody, InCandidateHipLocal);
        if (NOT Hip.IsSet() || NOT ck_procedural_height_planning::DoIntersect(Hip.GetValue(),
                InCandidatePoint, InUp, InCandidateReach, Interval.GetValue()))
        { return {}; }
        if (Posed.IsSet())
        {
            const auto PosedHip = ck_procedural_height_planning::Get_CheckedPosition(Posed.GetValue(), InCandidateHipLocal);
            if (NOT PosedHip.IsSet() || NOT ck_procedural_height_planning::DoIntersect(PosedHip.GetValue(),
                    InCandidatePoint, InUp, InCandidateReach, Interval.GetValue()))
            { return {}; }
        }
        return Interval;
    }

    auto
        Get_ProceduralHeightReprojectionDelta(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
            const FVector& InCandidateHipLocal,
            const FVector& InCandidatePoint,
            float InCandidateReach,
            double InMaxTranslation)
        -> TOptional<double>
    {
        if (NOT ck_procedural_height_planning::Get_AreNativeInputsValid(InBody, InPoseOffset, InAnchors,
                InCandidateHipLocal, InCandidatePoint, InCandidateReach))
        { return {}; }
        const auto Interval = Get_ProceduralHeightStanceInterval(InBody, InPoseOffset, InUp, InAnchors, InMaxTranslation);
        if (NOT Interval.IsSet())
        { return {}; }
        const auto Hip = InBody.TransformPosition(InCandidateHipLocal);
        const auto TowardSeed = (InCandidatePoint.X - Hip.X) * InUp.X
            + (InCandidatePoint.Y - Hip.Y) * InUp.Y + (InCandidatePoint.Z - Hip.Z) * InUp.Z;
        if (NOT FMath::IsFinite(TowardSeed))
        { return {}; }
        auto Delta = FMath::Clamp(TowardSeed, Interval->X, Interval->Y);
        if (Delta < 0.0)
        {
            const auto PlaneLower = ck_procedural_height_planning::Get_RetainedPlaneLower(InBody, InPoseOffset, InUp, InAnchors);
            if (NOT PlaneLower.IsSet())
            { return {}; }
            const auto Lower = FMath::Max(Interval->X, PlaneLower.GetValue());
            if (Lower > Interval->Y)
            { return {}; }
            Delta = FMath::Clamp(TowardSeed, Lower, Interval->Y);
            if (Delta >= 0.0)
            { return {}; }
        }
        if (NOT FMath::IsFinite(Delta) || FMath::Abs(Delta) <= 1.0e-3)
        { return {}; }
        return Delta;
    }

    auto
        Get_ProceduralHeightInteriorRetryDelta(
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FVector& InUp,
            TArrayView<const FProceduralSurfaceReachPaceAnchor> InAnchors,
            const FVector& InCandidateHipLocal,
            const FVector& InNativePoint,
            float InCandidateReach,
            double InMaxTranslation,
            double InFailedDelta)
        -> TOptional<double>
    {
        if (NOT FMath::IsFinite(InFailedDelta) || InFailedDelta >= 0.0 || FMath::Abs(InFailedDelta) <= 1.0e-3
            || NOT ck_procedural_height_planning::Get_AreNativeInputsValid(InBody, InPoseOffset, InAnchors,
                InCandidateHipLocal, InNativePoint, InCandidateReach))
        { return {}; }
        auto Interval = Get_ProceduralHeightPlanningInterval(InBody, InPoseOffset, InUp, InAnchors,
            InCandidateHipLocal, InNativePoint, InCandidateReach, InMaxTranslation);
        if (NOT Interval.IsSet() || InFailedDelta < -InMaxTranslation)
        { return {}; }
        const auto PlaneLower = ck_procedural_height_planning::Get_RetainedPlaneLower(InBody, InPoseOffset, InUp, InAnchors);
        if (NOT PlaneLower.IsSet())
        { return {}; }
        Interval->X = FMath::Max(FMath::Max(Interval->X, PlaneLower.GetValue()), InFailedDelta);
        if (NOT FMath::IsFinite(Interval->X) || NOT FMath::IsFinite(Interval->Y) || Interval->X >= Interval->Y)
        { return {}; }
        const auto Delta = Interval->X + (Interval->Y - Interval->X) * 0.5;
        if (NOT FMath::IsFinite(Delta) || Delta <= Interval->X || Delta >= Interval->Y
            || Delta <= InFailedDelta || Delta >= 0.0 || FMath::Abs(Delta) <= 1.0e-3)
        { return {}; }
        return Delta;
    }
}
