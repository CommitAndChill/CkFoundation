#include "CkProceduralAnimation/Core/CkProceduralContactStaging.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSettings.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_contact_staging
{
    constexpr auto MaximumCount = 64;
    constexpr auto PreferredTopInset = 1.0;
    constexpr auto OwnSideQueryMargin = 0.1;

    enum class EPlanFailure : uint8
    {
        Other,
        DirectQuerySpan,
        CommonHoldEmpty
    };

    auto
        Get_PhysicalReach(float InReach)
        -> double
    {
        return static_cast<double>(InReach) + 1.0e-3;
    }

    struct FInterval
    {
        double _Lower = 0.0;
        double _Upper = 0.0;
    };

    auto
        Get_IsVectorBounded(const FVector& InValue)
        -> bool
    {
        const auto Limit = static_cast<double>(TNumericLimits<float>::Max());
        return NOT InValue.ContainsNaN() && FMath::Abs(InValue.X) <= Limit
            && FMath::Abs(InValue.Y) <= Limit && FMath::Abs(InValue.Z) <= Limit;
    }

    auto
        Get_IsRadius(float InValue)
        -> bool
    {
        return FMath::IsFinite(InValue) && InValue > 0.0f;
    }

    auto
        Get_IsCardinal(const FVector& InValue)
        -> bool
    {
        return (InValue.X == 0.0 || FMath::Abs(InValue.X) == 1.0)
            && (InValue.Y == 0.0 || FMath::Abs(InValue.Y) == 1.0)
            && (InValue.Z == 0.0 || FMath::Abs(InValue.Z) == 1.0)
            && FMath::Abs(InValue.X) + FMath::Abs(InValue.Y) + FMath::Abs(InValue.Z) == 1.0;
    }

    auto
        Get_IsBodySupported(const FTransform& InBody, bool InRequireCardinal = true)
        -> bool
    {
        if (InBody.ContainsNaN())
        { return false; }
        const auto& Rotation = InBody.GetRotation();
        if (NOT Get_IsVectorBounded(InBody.GetLocation()) || InBody.GetScale3D() != FVector::OneVector
            || FMath::Abs(Rotation.X) > 1.0 || FMath::Abs(Rotation.Y) > 1.0
            || FMath::Abs(Rotation.Z) > 1.0 || FMath::Abs(Rotation.W) > 1.0)
        { return false; }
        if (NOT Rotation.IsNormalized())
        { return false; }

        return NOT InRequireCardinal
            || (Get_IsCardinal(InBody.GetRotation().RotateVector(FVector::ForwardVector))
                && Get_IsCardinal(InBody.GetRotation().RotateVector(FVector::RightVector))
                && Get_IsCardinal(InBody.GetRotation().RotateVector(FVector::UpVector)));
    }

    auto
        Get_Extent(const FTransform& InBody, const FVector& InExtents, const FVector& InForward)
        -> double
    {
        const auto Local = InBody.GetRotation().UnrotateVector(InForward);
        return FMath::Abs(Local.X) * InExtents.X + FMath::Abs(Local.Y) * InExtents.Y
            + FMath::Abs(Local.Z) * InExtents.Z;
    }

    auto
        Get_TranslatedBody(const FTransform& InBody, const FVector& InDelta)
        -> FTransform
    {
        auto Result = InBody;
        Result.SetLocation(InBody.GetLocation() + InDelta);
        return Result;
    }

    auto
        Get_FitsSphere(const FVector& InPoint, const FVector& InHip, double InRadius)
        -> bool
    {
        if (NOT Get_IsVectorBounded(InPoint) || NOT Get_IsVectorBounded(InHip)
            || NOT FMath::IsFinite(InRadius) || InRadius <= 0.0)
        { return false; }

        const auto DistanceSquared = FVector::DistSquared(InPoint, InHip);
        return FMath::IsFinite(DistanceSquared) && DistanceSquared <= InRadius * InRadius
            && FVector::Dist(InPoint, InHip) <= InRadius;
    }

    auto
        DoIntersectSphere(FInterval& InOutInterval, const FVector& InPoint, const FVector& InHip,
            double InRadius, double InCurrentCoordinate, const FVector& InForward)
        -> bool
    {
        // Every input component is bounded by FLT_MAX before this arithmetic. Differences, squares and
        // the three-term sums therefore remain far below DBL_MAX; no overflow can masquerade as an interval.
        if (NOT Get_IsVectorBounded(InPoint) || NOT Get_IsVectorBounded(InHip)
            || NOT FMath::IsFinite(InRadius) || InRadius <= 0.0
            || InRadius > static_cast<double>(TNumericLimits<float>::Max()))
        { return false; }

        const auto Difference = InHip - InPoint;
        const auto Along = FVector::DotProduct(Difference, InForward);
        const auto Perpendicular = Difference - InForward * Along;
        const auto Radicand = InRadius * InRadius - Perpendicular.SizeSquared();
        if (NOT FMath::IsFinite(Radicand) || Radicand < 0.0)
        { return false; }

        const auto HalfWidth = FMath::Sqrt(Radicand);
        const auto Center = InCurrentCoordinate - Along;
        const auto Lower = Center - HalfWidth;
        const auto Upper = Center + HalfWidth;
        if (NOT FMath::IsFinite(Lower) || NOT FMath::IsFinite(Upper))
        { return false; }

        InOutInterval._Lower = FMath::Max(InOutInterval._Lower, Lower);
        InOutInterval._Upper = FMath::Min(InOutInterval._Upper, Upper);
        return InOutInterval._Lower < InOutInterval._Upper;
    }

    auto
        DoIntersectOwnSide(FInterval& InOutInterval, const FVector& InPoint, const FVector& InHip,
            const FVector& InOutboard, float InTolerance, double InCurrentCoordinate, const FVector& InForward)
        -> bool
    {
        const auto Outboard = InOutboard.GetSafeNormal();
        const auto Minimum = -static_cast<double>(InTolerance) + OwnSideQueryMargin;
        const auto Current = FVector::DotProduct(InPoint - InHip, Outboard);
        const auto Coefficient = FVector::DotProduct(InForward, Outboard);
        if (NOT FMath::IsFinite(Current) || NOT FMath::IsFinite(Coefficient))
        { return false; }

        if (Coefficient == 0.0)
        { return Current >= Minimum; }

        const auto Bound = InCurrentCoordinate + (Current - Minimum) / Coefficient;
        if (NOT FMath::IsFinite(Bound))
        { return false; }

        if (Coefficient > 0.0)
        { InOutInterval._Upper = FMath::Min(InOutInterval._Upper, Bound); }
        else
        { InOutInterval._Lower = FMath::Max(InOutInterval._Lower, Bound); }
        return InOutInterval._Lower < InOutInterval._Upper;
    }

    auto
        TryGet_RepresentedMidpoint(const FInterval& InInterval)
        -> TOptional<double>
    {
        if (NOT FMath::IsFinite(InInterval._Lower) || NOT FMath::IsFinite(InInterval._Upper)
            || InInterval._Lower >= InInterval._Upper)
        { return {}; }

        const auto Midpoint = InInterval._Lower + (InInterval._Upper - InInterval._Lower) * 0.5;
        if (NOT FMath::IsFinite(Midpoint)
            || FMath::Abs(Midpoint) > static_cast<double>(TNumericLimits<float>::Max()))
        { return {}; }

        const auto Represented = static_cast<double>(static_cast<float>(Midpoint));
        if (NOT FMath::IsFinite(Represented)
            || Represented <= InInterval._Lower || Represented >= InInterval._Upper)
        { return {}; }
        return Represented;
    }

    auto
        Get_BodyAtCoordinate(const FTransform& InBody, const FVector& InForward, double InCoordinate)
        -> FTransform
    {
        auto Location = InBody.GetLocation();
        if (InForward.X != 0.0)
        { Location.X = InForward.X * InCoordinate; }
        else if (InForward.Y != 0.0)
        { Location.Y = InForward.Y * InCoordinate; }
        else
        { Location.Z = InForward.Z * InCoordinate; }
        auto Result = InBody;
        Result.SetLocation(Location);
        return Result;
    }

    auto
        Get_FitsPhysical(const ck::FProceduralContactStagingInput& InInput, const FTransform& InBody,
            const FVector& InPoint, const FVector& InHipLocal, float InReach, double InSimulationReach)
        -> bool
    {
        if (NOT Get_FitsSphere(InPoint, InBody.TransformPosition(InHipLocal), InSimulationReach))
        { return false; }
        if (NOT InInput._PosedBody.IsSet())
        { return true; }
        const auto Posed = Get_TranslatedBody(InInput._PosedBody.GetValue(),
            InBody.GetLocation() - InInput._Body.GetLocation());
        return Get_FitsSphere(InPoint, Posed.TransformPosition(InHipLocal), Get_PhysicalReach(InReach));
    }

    auto
        DoIntersectPhysical(FInterval& InOutInterval, const ck::FProceduralContactStagingInput& InInput,
            const FVector& InPoint, const FVector& InHipLocal, float InReach, double InSimulationReach,
            double InCurrentCoordinate)
        -> bool
    {
        if (NOT DoIntersectSphere(InOutInterval, InPoint, InInput._Body.TransformPosition(InHipLocal),
            InSimulationReach, InCurrentCoordinate, InInput._Forward))
        { return false; }
        return NOT InInput._PosedBody.IsSet() || DoIntersectSphere(InOutInterval, InPoint,
            InInput._PosedBody->TransformPosition(InHipLocal), Get_PhysicalReach(InReach), InCurrentCoordinate, InInput._Forward);
    }

    auto
        TryGet_Query(const ck::FProceduralContactStagingInput& InInput,
            const ck::FProceduralContactStagingLeg& InLeg, double InFaceCoordinate, bool& OutDirectSphereFailure)
        -> TOptional<FVector>
    {
        if (NOT InLeg._NativeTop.IsSet() || NOT Get_IsVectorBounded(InLeg._NativeTop->_Point)
            || InLeg._NativeTop->_Normal != InInput._Up)
        { return {}; }
        const auto& Plant = InLeg._Anchor.Get_FootWorld();
        const auto Coordinate = InFaceCoordinate - PreferredTopInset;
        const auto Query = Get_BodyAtCoordinate(FTransform{FQuat::Identity, Plant},
            InInput._Forward, Coordinate).GetLocation();
        if (NOT Get_IsVectorBounded(Query))
        { return {}; }
        const auto Represented = FVector{static_cast<float>(Query.X), static_cast<float>(Query.Y),
            static_cast<float>(Query.Z)};
        const auto Tangent = FVector::CrossProduct(InInput._Up, InInput._Forward);
        const auto Displacement = Represented - Plant;
        if (FVector::DotProduct(Represented, InInput._Forward) != Coordinate
            || FVector::DotProduct(Displacement, Tangent) != 0.0
            || FVector::DotProduct(Displacement, InInput._Up) != 0.0
            || FVector::DotProduct(Displacement, InInput._Forward) <= 0.0
            || FVector::DotProduct(Represented - InLeg._NativeTop->_Point, InInput._Up) != 0.0)
        { return {}; }
        OutDirectSphereFailure = NOT Get_FitsSphere(Represented, Plant, InLeg._SearchRadius);
        return Represented;
    }

    auto
        TryGet_Intermediate(const ck::FProceduralContactStagingInput& InInput,
            const ck::FProceduralContactStagingLeg& InLeg, const FVector& InFinal)
        -> TOptional<FVector>
    {
        const auto& Plant = InLeg._Anchor.Get_FootWorld();
        const auto Tangent = FVector::CrossProduct(InInput._Up, InInput._Forward);
        if (NOT InLeg._NativeTop.IsSet()
            || Plant != FVector(static_cast<float>(Plant.X), static_cast<float>(Plant.Y), static_cast<float>(Plant.Z))
            || FVector::DotProduct(Plant - InLeg._NativeTop->_Point, InInput._Up) != 0.0
            || FVector::DotProduct(InFinal - InLeg._NativeTop->_Point, InInput._Up) != 0.0
            || FVector::DotProduct(InFinal - Plant, Tangent) != 0.0)
        { return {}; }
        const auto Start = FVector::DotProduct(Plant, InInput._Forward);
        const auto Difference = FVector::DotProduct(InFinal, InInput._Forward) - Start;
        if (NOT FMath::IsFinite(Difference) || Difference <= 0.0)
        { return {}; }
        const auto Half = Difference * 0.5;
        const auto Coordinate = Start + Half;
        if (NOT FMath::IsFinite(Half) || NOT FMath::IsFinite(Coordinate)
            || FMath::Abs(Coordinate) > static_cast<double>(TNumericLimits<float>::Max()))
        { return {}; }
        const auto Represented = static_cast<double>(static_cast<float>(Coordinate));
        const auto Midpoint = Get_BodyAtCoordinate(FTransform{FQuat::Identity, Plant},
            InInput._Forward, Represented).GetLocation();
        if (NOT Get_IsVectorBounded(Midpoint)
            || FVector::DotProduct(Midpoint - Plant, InInput._Up) != 0.0
            || FVector::DotProduct(Midpoint - Plant, Tangent) != 0.0
            || FVector::DotProduct(Midpoint - InLeg._NativeTop->_Point, InInput._Up) != 0.0
            || NOT ck::Get_IsProceduralContactStagingIntermediateTargetValid(
                Midpoint, Plant, InFinal, InInput._Forward, InLeg._SearchRadius))
        { return {}; }
        return Midpoint;
    }

    auto
        DoIntersectMilestone(FInterval& InOutInterval, const ck::FProceduralContactStagingInput& InInput,
            const ck::FProceduralContactStagingLeg& InLeg, const FVector& InPoint, double InCoordinate)
        -> bool
    {
        const auto IntersectHip = [&](const FVector& InHip, const FVector& InOutboard)
        {
            return DoIntersectSphere(InOutInterval, InPoint, InHip, InLeg._ForceReach,
                InCoordinate, InInput._Forward)
                && DoIntersectOwnSide(InOutInterval, InPoint, InHip, InOutboard,
                    InLeg._OwnSideTolerance, InCoordinate, InInput._Forward);
        };
        return IntersectHip(InInput._Body.TransformPosition(InLeg._Anchor.Get_HipLocal()), InLeg._OutboardWorld)
            && (NOT InInput._PosedBody.IsSet()
                || IntersectHip(InInput._PosedBody->TransformPosition(InLeg._Anchor.Get_HipLocal()),
                    InInput._PosedBody->TransformVectorNoScale(InInput._Body.InverseTransformVectorNoScale(InLeg._OutboardWorld))));
    }

    auto
        Get_FitsMilestone(const ck::FProceduralContactStagingInput& InInput, const FTransform& InBody,
            const ck::FProceduralContactStagingLeg& InLeg, const FVector& InPoint)
        -> bool
    {
        const auto FitsHip = [&](const FVector& InHip, const FVector& InOutboard)
        {
            return Get_FitsSphere(InPoint, InHip, InLeg._ForceReach)
                && FVector::DotProduct(InPoint - InHip, InOutboard.GetSafeNormal())
                    >= -static_cast<double>(InLeg._OwnSideTolerance) + OwnSideQueryMargin;
        };
        if (NOT FitsHip(InBody.TransformPosition(InLeg._Anchor.Get_HipLocal()), InLeg._OutboardWorld))
        { return false; }
        return NOT InInput._PosedBody.IsSet() || FitsHip(Get_TranslatedBody(InInput._PosedBody.GetValue(),
            InBody.GetLocation() - InInput._Body.GetLocation()).TransformPosition(InLeg._Anchor.Get_HipLocal()),
                InInput._PosedBody->TransformVectorNoScale(InInput._Body.InverseTransformVectorNoScale(InLeg._OutboardWorld)));
    }

    auto
        TryGet_RepresentedPoint(const FVector& InPoint)
        -> TOptional<FVector>
    {
        if (NOT Get_IsVectorBounded(InPoint))
        { return {}; }
        const auto Point = FVector{static_cast<float>(InPoint.X), static_cast<float>(InPoint.Y),
            static_cast<float>(InPoint.Z)};
        return Get_IsVectorBounded(Point) ? TOptional<FVector>{Point} : TOptional<FVector>{};
    }

    auto
        Get_FitsPreparationTarget(const ck::FProceduralContactStagingInput& InInput,
            const FTransform& InBody, const ck::FProceduralContactStagingLeg& InLeg,
            const FVector& InPoint, double InOwnSideMargin)
        -> bool
    {
        const auto Fits = [&](const FVector& InHip, const FVector& InOutboard)
        {
            return Get_FitsSphere(InPoint, InHip, InLeg._ForceReach)
                && FVector::DotProduct(InPoint - InHip, InOutboard.GetSafeNormal())
                    >= -static_cast<double>(InLeg._OwnSideTolerance) + InOwnSideMargin;
        };
        if (NOT Fits(InBody.TransformPosition(InLeg._Anchor.Get_HipLocal()), InLeg._OutboardWorld))
        { return false; }
        return NOT InInput._PosedBody.IsSet() || Fits(Get_TranslatedBody(InInput._PosedBody.GetValue(),
            InBody.GetLocation() - InInput._Body.GetLocation()).TransformPosition(InLeg._Anchor.Get_HipLocal()),
                InInput._PosedBody->TransformVectorNoScale(InInput._Body.InverseTransformVectorNoScale(InLeg._OutboardWorld)));
    }

    auto
        Get_IsPreparationStanceValid(const ck::FProceduralContactStagingInput& InInput,
            const FTransform& InBody, const std::array<FVector, 4>& InPoints)
        -> bool
    {
        for (auto Index = 0; Index < 4; ++Index)
        {
            const auto& Leg = InInput._Legs[Index];
            if (NOT Get_FitsPhysical(InInput, InBody, InPoints[Index], Leg._Anchor.Get_HipLocal(),
                Leg._Anchor.Get_Reach(), Get_PhysicalReach(Leg._Anchor.Get_Reach())))
            { return false; }
            for (auto Other = 0; Other < Index; ++Other)
            {
                const auto Radius = static_cast<double>(Leg._FootContactRadius)
                    + InInput._Legs[Other]._FootContactRadius;
                const auto Distance = FVector::DistSquared(InPoints[Index], InPoints[Other]);
                if (NOT FMath::IsFinite(Distance) || Distance < Radius * Radius)
                { return false; }
            }
        }
        return true;
    }

    auto
        TryGet_PreparationPoint(const ck::FProceduralContactStagingInput& InInput,
            const ck::FProceduralContactStagingLeg& InLeg, const FTransform& InProspectiveHold,
            const FVector& InSeed)
        -> TOptional<FVector>
    {
        if (NOT Get_IsVectorBounded(InSeed))
        { return {}; }
        const auto Outboard = InLeg._OutboardWorld.GetSafeNormal();
        const auto Tangent = Outboard - InInput._Up * FVector::DotProduct(Outboard, InInput._Up);
        const auto Denominator = FVector::DotProduct(Tangent, Outboard);
        const auto Own = FVector::DotProduct(InSeed - InProspectiveHold.TransformPosition(
            InLeg._Anchor.Get_HipLocal()), Outboard);
        if (NOT FMath::IsFinite(Own) || NOT FMath::IsFinite(Denominator) || Denominator <= 0.0)
        { return {}; }
        auto Point = InSeed;
        if (Own < -static_cast<double>(InLeg._OwnSideTolerance))
        {
            const auto Scale = (-static_cast<double>(InLeg._OwnSideTolerance) + OwnSideQueryMargin - Own)
                / Denominator;
            if (NOT FMath::IsFinite(Scale))
            { return {}; }
            const auto Correction = Tangent * Scale;
            if (NOT Get_IsVectorBounded(Correction))
            { return {}; }
            Point += Correction;
        }
        if (NOT Get_FitsSphere(Point, InLeg._Anchor.Get_FootWorld(), InLeg._SearchRadius)
            || NOT Get_FitsPreparationTarget(InInput, InProspectiveHold, InLeg, Point, 0.0))
        { return {}; }
        const auto Represented = TryGet_RepresentedPoint(Point);
        if (NOT Represented.IsSet()
            || FVector::DotProduct(Represented.GetValue() - InLeg._Anchor.Get_FootWorld(), InInput._Up) != 0.0
            || NOT Get_FitsSphere(Represented.GetValue(), InLeg._Anchor.Get_FootWorld(), InLeg._SearchRadius)
            || NOT Get_FitsPreparationTarget(InInput, InProspectiveHold, InLeg, Represented.GetValue(), 0.0))
        { return {}; }
        return Represented;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        Get_IsProceduralContactStagingFuturePlantReachValid(
            const FTransform& InExit,
            const TOptional<FTransform>& InPoseOffset,
            const FProceduralSurfaceLandingConstraint& InConstraint)
        -> bool
    {
        using namespace ck_procedural_contact_staging;
        if (NOT Get_IsBodySupported(InExit, false)
            || (InPoseOffset.IsSet() && NOT Get_IsBodySupported(InPoseOffset.GetValue(), false))
            || NOT Get_IsVectorBounded(InConstraint.Get_PointWorld())
            || NOT Get_IsVectorBounded(InConstraint.Get_HipLocal())
            || NOT Get_IsRadius(InConstraint.Get_Reach()) || NOT Get_IsRadius(InConstraint.Get_AdmissionReach())
            || InConstraint.Get_AdmissionReach() > InConstraint.Get_Reach())
        { return false; }

        const auto Hip = InExit.TransformPosition(InConstraint.Get_HipLocal());
        if (NOT Get_IsVectorBounded(Hip))
        { return false; }
        auto PosedHip = Hip;
        if (InPoseOffset.IsSet())
        {
            const auto Posed = InPoseOffset.GetValue() * InExit;
            if (NOT Get_IsBodySupported(Posed, false))
            { return false; }
            PosedHip = Posed.TransformPosition(InConstraint.Get_HipLocal());
            if (NOT Get_IsVectorBounded(PosedHip))
            { return false; }
        }
        const auto PhysicalReach = Get_PhysicalReach(InConstraint.Get_Reach());
        return Get_FitsSphere(InConstraint.Get_PointWorld(), Hip, PhysicalReach)
            && Get_FitsSphere(InConstraint.Get_PointWorld(), PosedHip, PhysicalReach);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        DoGet_ProceduralContactStagingPlan(
            const FProceduralContactStagingInput& InInput, bool InAllowTwoPlacement, bool& OutDirectSphereFailure,
            ck_procedural_contact_staging::EPlanFailure* OutFailure = nullptr)
        -> TOptional<FProceduralContactStagingPlan>
    {
        using namespace ck_procedural_contact_staging;
        OutDirectSphereFailure = false;
        if (OutFailure != nullptr)
        { *OutFailure = EPlanFailure::Other; }
        if (InInput._Legs.Num() <= 0 || InInput._Legs.Num() > MaximumCount
            || InInput._Landings.Num() < 0 || InInput._Landings.Num() > MaximumCount
            || InInput._Legs.GetData() == nullptr
            || (InInput._Landings.Num() > 0 && InInput._Landings.GetData() == nullptr)
            || NOT Get_IsBodySupported(InInput._Body)
            || (InInput._PosedBody.IsSet() && NOT Get_IsBodySupported(InInput._PosedBody.GetValue(), false))
            || NOT Get_IsCardinal(InInput._Up) || NOT Get_IsCardinal(InInput._Forward)
            || FVector::DotProduct(InInput._Up, InInput._Forward) != 0.0
            || NOT Get_IsVectorBounded(InInput._Face._Point) || InInput._Face._Normal != InInput._Forward
            || NOT Get_IsVectorBounded(InInput._BodyHalfExtents)
            || InInput._BodyHalfExtents.X <= 0.0 || InInput._BodyHalfExtents.Y <= 0.0
            || InInput._BodyHalfExtents.Z <= 0.0 || NOT Get_IsRadius(InInput._CollisionMargin))
        { return {}; }

        const auto PaddingCheck = static_cast<double>(InInput._CollisionMargin)
            + static_cast<double>(JPH::cDefaultConvexRadius);
        const auto Limit = static_cast<double>(TNumericLimits<float>::Max());
        if (InInput._BodyHalfExtents.X + PaddingCheck > Limit
            || InInput._BodyHalfExtents.Y + PaddingCheck > Limit
            || InInput._BodyHalfExtents.Z + PaddingCheck > Limit)
        { return {}; }
        const auto PaddedExtents = InInput._BodyHalfExtents
            + FVector{InInput._CollisionMargin + JPH::cDefaultConvexRadius};
        if (NOT Get_IsVectorBounded(PaddedExtents))
        { return {}; }

        const auto BodyCoordinate = FVector::DotProduct(InInput._Body.GetLocation(), InInput._Forward);
        const auto FaceCoordinate = FVector::DotProduct(InInput._Face._Point, InInput._Forward);
        const auto HalfExtent = Get_Extent(InInput._Body, InInput._BodyHalfExtents, InInput._Forward);
        auto Clearance = Get_Extent(InInput._Body, PaddedExtents, InInput._Forward);
        if (InInput._PosedBody.IsSet())
        {
            const auto Offset = FVector::DotProduct(
                InInput._PosedBody->GetLocation() - InInput._Body.GetLocation(), InInput._Forward);
            Clearance = FMath::Max(Clearance, Get_Extent(InInput._PosedBody.GetValue(), PaddedExtents,
                InInput._Forward) - Offset);
        }
        const auto ExitLower = FaceCoordinate + Clearance;
        if (NOT FMath::IsFinite(ExitLower) || BodyCoordinate <= FaceCoordinate)
        { return {}; }
        const auto ClearanceBody = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, ExitLower);
        FProceduralContactStagingPlan Result;
        std::array<int32, 2> RequiredInputIndices{};
        auto Seen = uint64{0};
        int32 LastRosterIndex = INDEX_NONE;
        for (auto Index = 0; Index < InInput._Legs.Num(); ++Index)
        {
            const auto& Leg = InInput._Legs[Index];
            const auto& Anchor = Leg._Anchor;
            if (Leg._RosterIndex < 0 || Leg._RosterIndex >= MaximumCount || Leg._RosterIndex <= LastRosterIndex
                || NOT Get_IsVectorBounded(Anchor.Get_FootWorld()) || NOT Get_IsVectorBounded(Anchor.Get_HipLocal())
                || NOT Get_IsRadius(Anchor.Get_Reach()) || NOT Get_IsRadius(Leg._ForceReach)
                || Leg._ForceReach > Anchor.Get_Reach() || NOT Get_IsRadius(Leg._SearchRadius)
                || NOT Get_IsVectorBounded(Leg._OutboardWorld) || NOT FMath::IsFinite(Leg._OwnSideTolerance)
                || Leg._OwnSideTolerance < 0.0f)
            { return {}; }
            const auto Bit = uint64{1} << Leg._RosterIndex;
            if ((Seen & Bit) != 0)
            { return {}; }
            Seen |= Bit;
            LastRosterIndex = Leg._RosterIndex;
            if (Get_FitsPhysical(InInput, ClearanceBody, Anchor.Get_FootWorld(), Anchor.Get_HipLocal(),
                Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach())))
            { continue; }
            if (Result._RequiredCount == static_cast<int32>(Result._Required.size()))
            { return {}; }
            auto DirectSphereFailure = false;
            const auto Query = TryGet_Query(InInput, Leg, FaceCoordinate, DirectSphereFailure);
            if (NOT Query.IsSet())
            { return {}; }
            if (DirectSphereFailure && NOT InAllowTwoPlacement)
            {
                OutDirectSphereFailure = true;
                if (OutFailure != nullptr)
                { *OutFailure = EPlanFailure::DirectQuerySpan; }
                return {};
            }
            TOptional<FVector> Intermediate;
            if (DirectSphereFailure)
            {
                Intermediate = TryGet_Intermediate(InInput, Leg, Query.GetValue());
                if (NOT Intermediate.IsSet())
                { return {}; }
                Result._TwoPlacement = true;
            }
            if (Result._RequiredCount > 0 && FVector::DotProduct(Query.GetValue()
                - Result._Required[0]._ProposedQuery, InInput._Up) != 0.0)
            { return {}; }
            RequiredInputIndices[Result._RequiredCount] = Index;
            Result._Required[Result._RequiredCount++] =
                FProceduralContactStagingRequiredLeg{Leg._RosterIndex, Anchor.Get_FootWorld(), Query.GetValue(), Intermediate};
        }
        if (Result._RequiredCount == 0)
        { return {}; }
        for (const auto& Landing : InInput._Landings)
        {
            if (NOT Get_IsVectorBounded(Landing.Get_PointWorld()) || NOT Get_IsVectorBounded(Landing.Get_HipLocal())
                || NOT Get_IsRadius(Landing.Get_AdmissionReach()) || NOT Get_IsRadius(Landing.Get_Reach())
                || Landing.Get_AdmissionReach() > Landing.Get_Reach())
            { return {}; }
        }

        auto Retreat = FInterval{FMath::Max(FaceCoordinate, BodyCoordinate - HalfExtent), BodyCoordinate};
        const auto RejectHold = [&]() -> TOptional<FProceduralContactStagingPlan>
        {
            if (OutFailure != nullptr && Result._TwoPlacement && Retreat._Lower >= Retreat._Upper)
            { *OutFailure = EPlanFailure::CommonHoldEmpty; }
            return {};
        };
        for (const auto& Leg : InInput._Legs)
        {
            const auto& Anchor = Leg._Anchor;
            if (NOT DoIntersectPhysical(Retreat, InInput, Anchor.Get_FootWorld(), Anchor.Get_HipLocal(),
                Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach()), BodyCoordinate))
            { return RejectHold(); }
        }
        for (const auto& Landing : InInput._Landings)
        {
            if (NOT DoIntersectPhysical(Retreat, InInput, Landing.Get_PointWorld(), Landing.Get_HipLocal(),
                Landing.Get_Reach(), Landing.Get_AdmissionReach(), BodyCoordinate))
            { return RejectHold(); }
        }
        for (auto Index = 0; Index < Result._RequiredCount; ++Index)
        {
            const auto& Leg = InInput._Legs[RequiredInputIndices[Index]];
            const auto& Point = Result._Required[Index]._ProposedQuery;
            if (NOT DoIntersectPhysical(Retreat, InInput, Point, Leg._Anchor.Get_HipLocal(),
                Leg._Anchor.Get_Reach(), Leg._ForceReach, BodyCoordinate)
                || NOT DoIntersectOwnSide(Retreat, Point, InInput._Body.TransformPosition(Leg._Anchor.Get_HipLocal()),
                    Leg._OutboardWorld, Leg._OwnSideTolerance, BodyCoordinate, InInput._Forward))
            { return RejectHold(); }
        }
        if (Result._TwoPlacement)
        {
            for (const auto& Leg : InInput._Legs)
            {
                if (NOT DoIntersectMilestone(Retreat, InInput, Leg, Leg._Anchor.Get_FootWorld(), BodyCoordinate))
                { return RejectHold(); }
            }
            for (auto Index = 0; Index < Result._RequiredCount; ++Index)
            {
                const auto& Required = Result._Required[Index];
                const auto& Leg = InInput._Legs[RequiredInputIndices[Index]];
                if (NOT DoIntersectMilestone(Retreat, InInput, Leg, Required._ProposedQuery, BodyCoordinate)
                    || (Required._IntermediateQuery.IsSet() && NOT DoIntersectMilestone(Retreat, InInput,
                        Leg, Required._IntermediateQuery.GetValue(), BodyCoordinate)))
                { return RejectHold(); }
            }
        }
        const auto RetreatCoordinate = TryGet_RepresentedMidpoint(Retreat);
        if (NOT RetreatCoordinate.IsSet())
        { return {}; }
        Result._RetreatBody = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, RetreatCoordinate.GetValue());
        auto Exit = FInterval{FMath::Max(ExitLower, BodyCoordinate), RetreatCoordinate.GetValue() + HalfExtent};
        for (auto Index = 0; Index < InInput._Legs.Num(); ++Index)
        {
            const auto& Anchor = InInput._Legs[Index]._Anchor;
            auto Point = Anchor.Get_FootWorld();
            for (auto Required = 0; Required < Result._RequiredCount; ++Required)
            {
                if (RequiredInputIndices[Required] == Index)
                { Point = Result._Required[Required]._ProposedQuery; }
            }
            if (NOT DoIntersectPhysical(Exit, InInput, Point, Anchor.Get_HipLocal(),
                Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach()), BodyCoordinate))
            { return {}; }
        }
        for (const auto& Landing : InInput._Landings)
        {
            if (NOT DoIntersectPhysical(Exit, InInput, Landing.Get_PointWorld(), Landing.Get_HipLocal(),
                Landing.Get_Reach(), Landing.Get_AdmissionReach(), BodyCoordinate))
            { return {}; }
        }
        const auto ExitCoordinate = TryGet_RepresentedMidpoint(Exit);
        if (NOT ExitCoordinate.IsSet())
        { return {}; }
        Result._ExitBody = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, ExitCoordinate.GetValue());
        if (NOT Get_IsVectorBounded(Result._RetreatBody.GetLocation())
            || NOT Get_IsVectorBounded(Result._ExitBody.GetLocation())
            || RetreatCoordinate.GetValue() <= FaceCoordinate || RetreatCoordinate.GetValue() >= BodyCoordinate
            || BodyCoordinate - RetreatCoordinate.GetValue() > HalfExtent
            || ExitCoordinate.GetValue() <= ExitLower || ExitCoordinate.GetValue() <= BodyCoordinate
            || ExitCoordinate.GetValue() - RetreatCoordinate.GetValue() > HalfExtent)
        { return {}; }

        for (auto Index = 0; Index < InInput._Legs.Num(); ++Index)
        {
            const auto& Leg = InInput._Legs[Index];
            const auto& Anchor = Leg._Anchor;
            auto ExitPoint = Anchor.Get_FootWorld();
            for (auto Required = 0; Required < Result._RequiredCount; ++Required)
            {
                if (RequiredInputIndices[Required] != Index)
                { continue; }
                ExitPoint = Result._Required[Required]._ProposedQuery;
                const auto Hip = Result._RetreatBody.TransformPosition(Anchor.Get_HipLocal());
                if (NOT Get_FitsPhysical(InInput, Result._RetreatBody, ExitPoint, Anchor.Get_HipLocal(),
                    Anchor.Get_Reach(), Leg._ForceReach)
                    || FVector::DotProduct(ExitPoint - Hip, Leg._OutboardWorld.GetSafeNormal())
                        < -static_cast<double>(Leg._OwnSideTolerance) + OwnSideQueryMargin
                    || Get_FitsPhysical(InInput, Result._ExitBody, Anchor.Get_FootWorld(), Anchor.Get_HipLocal(),
                        Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach())))
                { return {}; }
            }
            if (NOT Get_FitsPhysical(InInput, Result._RetreatBody, Anchor.Get_FootWorld(), Anchor.Get_HipLocal(),
                Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach()))
                || NOT Get_FitsPhysical(InInput, Result._ExitBody, ExitPoint, Anchor.Get_HipLocal(),
                    Anchor.Get_Reach(), Get_PhysicalReach(Anchor.Get_Reach())))
            { return {}; }
        }
        for (const auto& Landing : InInput._Landings)
        {
            if (NOT Get_FitsPhysical(InInput, Result._RetreatBody, Landing.Get_PointWorld(), Landing.Get_HipLocal(),
                Landing.Get_Reach(), Landing.Get_AdmissionReach())
                || NOT Get_FitsPhysical(InInput, Result._ExitBody, Landing.Get_PointWorld(), Landing.Get_HipLocal(),
                    Landing.Get_Reach(), Landing.Get_AdmissionReach()))
            { return {}; }
        }
        if (Result._TwoPlacement && NOT Get_IsProceduralContactStagingHoldValid(InInput, Result))
        { return {}; }
        return Result;
    }

    auto
        Get_ProceduralContactStagingPlan(
            const FProceduralContactStagingInput& InInput)
        -> TOptional<FProceduralContactStagingPlan>
    {
        auto DirectSphereFailure = false;
        return DoGet_ProceduralContactStagingPlan(InInput, false, DirectSphereFailure);
    }

    auto
        Get_ProceduralContactStagingTwoPlacementPlan(
            const FProceduralContactStagingInput& InInput)
        -> TOptional<FProceduralContactStagingPlan>
    {
        auto DirectSphereFailure = false;
        const auto Existing = DoGet_ProceduralContactStagingPlan(InInput, false, DirectSphereFailure);
        if (Existing.IsSet() || NOT DirectSphereFailure)
        { return Existing; }
        return DoGet_ProceduralContactStagingPlan(InInput, true, DirectSphereFailure);
    }

    auto
        Get_ProceduralContactStagingPreparationPlan(
            const FProceduralContactStagingInput& InInput)
        -> TOptional<FProceduralContactStagingPreparationPlan>
    {
        using namespace ck_procedural_contact_staging;
        if (InInput._Legs.Num() != 4 || InInput._Legs.GetData() == nullptr || InInput._Landings.Num() != 0)
        { return {}; }
        auto DirectFailure = false;
        auto Failure = EPlanFailure::Other;
        if (DoGet_ProceduralContactStagingPlan(InInput, false, DirectFailure, &Failure).IsSet()
            || Failure != EPlanFailure::DirectQuerySpan)
        { return {}; }
        if (DoGet_ProceduralContactStagingPlan(InInput, true, DirectFailure, &Failure).IsSet()
            || Failure != EPlanFailure::CommonHoldEmpty)
        { return {}; }

        for (const auto& Leg : InInput._Legs)
        {
            if (NOT Leg._RetainedPlantNormal.IsSet() || Leg._RetainedPlantNormal.GetValue() != InInput._Up
                || NOT Get_IsRadius(Leg._FootContactRadius)
                || Leg._OutboardWorld.GetSafeNormal().IsNearlyZero())
            { return {}; }
        }
        const auto A = FVector::DotProduct(InInput._Body.GetLocation(), InInput._Forward);
        const auto B = FVector::DotProduct(InInput._Face._Point, InInput._Forward);
        const auto HalfExtent = Get_Extent(InInput._Body, InInput._BodyHalfExtents, InInput._Forward);
        const auto C = TryGet_RepresentedMidpoint(FInterval{B, A});
        if (NOT C.IsSet() || A - B > HalfExtent)
        { return {}; }
        const auto Hold = TryGet_RepresentedMidpoint(FInterval{B, C.GetValue()});
        if (NOT Hold.IsSet())
        { return {}; }
        const auto BodyB = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, B);
        const auto BodyC = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, C.GetValue());
        const auto BodyH = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, Hold.GetValue());
        const auto Padded = InInput._BodyHalfExtents + FVector{InInput._CollisionMargin + JPH::cDefaultConvexRadius};
        auto Clearance = Get_Extent(InInput._Body, Padded, InInput._Forward);
        if (InInput._PosedBody.IsSet())
        {
            Clearance = FMath::Max(Clearance, Get_Extent(InInput._PosedBody.GetValue(), Padded, InInput._Forward)
                - FVector::DotProduct(InInput._PosedBody->GetLocation() - InInput._Body.GetLocation(), InInput._Forward));
        }
        const auto ClearanceBody = Get_BodyAtCoordinate(InInput._Body, InInput._Forward, B + Clearance);
        auto RequiredCount = 0;
        auto SpanIndex = int32{INDEX_NONE};
        auto OtherIndex = int32{INDEX_NONE};
        std::array<FVector, 4> Points;
        std::array<FVector, 4> Queries;
        std::array<bool, 4> Required{};
        for (auto Index = 0; Index < 4; ++Index)
        {
            const auto& Leg = InInput._Legs[Index];
            Points[Index] = Leg._Anchor.Get_FootWorld();
            if (Get_FitsPhysical(InInput, ClearanceBody, Points[Index], Leg._Anchor.Get_HipLocal(),
                Leg._Anchor.Get_Reach(), Get_PhysicalReach(Leg._Anchor.Get_Reach())))
            { continue; }
            Required[Index] = true;
            ++RequiredCount;
            auto SpanFailure = false;
            const auto Query = TryGet_Query(InInput, Leg, B, SpanFailure);
            if (NOT Query.IsSet())
            { return {}; }
            Queries[Index] = Query.GetValue();
            auto& Slot = SpanFailure ? SpanIndex : OtherIndex;
            if (Slot != INDEX_NONE)
            { return {}; }
            Slot = Index;
        }
        if (RequiredCount != 2 || SpanIndex == INDEX_NONE || OtherIndex == INDEX_NONE
            || NOT Get_IsPreparationStanceValid(InInput, InInput._Body, Points))
        { return {}; }
        const auto& SpanLeg = InInput._Legs[SpanIndex];
        const auto Step = 0.95f * SpanLeg._SearchRadius;
        const auto M = TryGet_PreparationPoint(InInput, SpanLeg, BodyH,
            Points[SpanIndex] + InInput._Forward * static_cast<double>(Step));
        const auto F = TryGet_PreparationPoint(InInput, InInput._Legs[OtherIndex], BodyH, Queries[OtherIndex]);
        if (NOT M.IsSet() || NOT F.IsSet()
            || NOT Get_FitsPreparationTarget(InInput, BodyB, SpanLeg, M.GetValue(), OwnSideQueryMargin)
            || NOT Get_FitsPreparationTarget(InInput, BodyB, InInput._Legs[OtherIndex], F.GetValue(), OwnSideQueryMargin))
        { return {}; }

        auto AuxiliaryIndex = int32{INDEX_NONE};
        for (auto Index = 0; Index < 4; ++Index)
        {
            if (Index == SpanIndex)
            { continue; }
            const auto Radius = static_cast<double>(SpanLeg._FootContactRadius)
                + InInput._Legs[Index]._FootContactRadius;
            if (FVector::DistSquared(M.GetValue(), Points[Index]) >= Radius * Radius)
            { continue; }
            if (AuxiliaryIndex != INDEX_NONE || Required[Index])
            { return {}; }
            AuxiliaryIndex = Index;
        }
        if (AuxiliaryIndex == INDEX_NONE)
        { return {}; }
        auto LowerIndex = int32{INDEX_NONE};
        for (auto Index = 0; Index < 4; ++Index)
        {
            if (FVector::DotProduct(Points[Index] - Points[AuxiliaryIndex], InInput._Up) >= 0.0)
            { continue; }
            if (LowerIndex != INDEX_NONE)
            { return {}; }
            LowerIndex = Index;
        }
        if (LowerIndex == INDEX_NONE)
        { return {}; }
        const auto& Auxiliary = InInput._Legs[AuxiliaryIndex];
        const auto Horizontal = Auxiliary._OutboardWorld
            - InInput._Up * FVector::DotProduct(Auxiliary._OutboardWorld, InInput._Up);
        const auto AuxiliaryOutboard = Horizontal.GetSafeNormal();
        if (AuxiliaryOutboard.IsNearlyZero())
        { return {}; }
        auto AuxiliarySeed = InInput._Body.TransformPosition(Auxiliary._Anchor.Get_HipLocal())
            + AuxiliaryOutboard * static_cast<double>(Auxiliary._SearchRadius);
        AuxiliarySeed += InInput._Up * FVector::DotProduct(Points[LowerIndex] - AuxiliarySeed, InInput._Up);
        // Radius bounds this one seed construction before world-float conversion, not a new returned-hit sphere.
        const auto R = TryGet_RepresentedPoint(AuxiliarySeed);
        if (NOT R.IsSet() || FVector::DotProduct(R.GetValue() - Points[LowerIndex], InInput._Up) != 0.0
            || NOT Get_FitsPreparationTarget(InInput, InInput._Body, Auxiliary, R.GetValue(), OwnSideQueryMargin))
        { return {}; }
        Points[AuxiliaryIndex] = R.GetValue();
        if (NOT Get_IsPreparationStanceValid(InInput, InInput._Body, Points)
            || NOT Get_IsPreparationStanceValid(InInput, BodyB, Points))
        { return {}; }
        Points[SpanIndex] = M.GetValue();
        if (NOT Get_IsPreparationStanceValid(InInput, BodyB, Points))
        { return {}; }
        Points[OtherIndex] = F.GetValue();
        if (NOT Get_IsPreparationStanceValid(InInput, BodyB, Points)
            || NOT Get_IsPreparationStanceValid(InInput, BodyC, Points))
        { return {}; }

        std::array<FProceduralContactStagingLeg, 4> FutureLegs;
        for (auto Index = 0; Index < 4; ++Index)
        {
            FutureLegs[Index] = InInput._Legs[Index];
            const auto& Anchor = InInput._Legs[Index]._Anchor;
            FutureLegs[Index]._Anchor = FProceduralSurfaceReachPaceAnchor{
                Points[Index], Anchor.Get_HipLocal(), Anchor.Get_Reach()};
        }
        auto FutureInput = InInput;
        FutureInput._Body = BodyC;
        FutureInput._Legs = TArrayView<const FProceduralContactStagingLeg>{FutureLegs.data(), 4};
        if (InInput._PosedBody.IsSet())
        { FutureInput._PosedBody = Get_TranslatedBody(InInput._PosedBody.GetValue(), BodyC.GetLocation() - InInput._Body.GetLocation()); }
        const auto Legacy = Get_ProceduralContactStagingPlan(FutureInput);
        if (NOT Legacy.IsSet() || Legacy->_TwoPlacement || Legacy->_RequiredCount != 1
            || Legacy->_Required[0]._RosterIndex != SpanLeg._RosterIndex
            || Legacy->_RetreatBody.GetLocation() != BodyH.GetLocation()
            || NOT Get_IsPreparationStanceValid(InInput, BodyH, Points)
            || NOT Get_FitsPreparationTarget(InInput, BodyH, SpanLeg, Legacy->_Required[0]._ProposedQuery, OwnSideQueryMargin))
        { return {}; }
        Points[SpanIndex] = Legacy->_Required[0]._ProposedQuery;
        if (NOT Get_IsPreparationStanceValid(InInput, BodyH, Points)
            || NOT Get_IsPreparationStanceValid(InInput, Legacy->_ExitBody, Points))
        { return {}; }
        auto Result = FProceduralContactStagingPreparationPlan{};
        Result._AuxiliaryRosterIndex = Auxiliary._RosterIndex;
        Result._SpanRosterIndex = SpanLeg._RosterIndex;
        Result._OtherRosterIndex = InInput._Legs[OtherIndex]._RosterIndex;
        Result._LowerPlaneRosterIndex = InInput._Legs[LowerIndex]._RosterIndex;
        Result._LowerPlane = FProceduralContactStagingPlane{InInput._Legs[LowerIndex]._Anchor.Get_FootWorld(),
            InInput._Legs[LowerIndex]._RetainedPlantNormal.GetValue()};
        Result._AuxiliaryQuery = R.GetValue();
        Result._SpanPreparationQuery = M.GetValue();
        Result._OtherPreparationQuery = F.GetValue();
        Result._BodyB = BodyB;
        Result._BodyC = BodyC;
        Result._LegacyPlan = Legacy.GetValue();
        return Result;
    }

    auto
        Get_IsProceduralContactStagingIntermediateTargetValid(
            const FVector& InPoint,
            const FVector& InActualPlant,
            const FVector& InFinalQuery,
            const FVector& InForward,
            float InSearchRadius)
        -> bool
    {
        using namespace ck_procedural_contact_staging;
        if (NOT Get_IsVectorBounded(InPoint) || NOT Get_IsVectorBounded(InActualPlant)
            || NOT Get_IsVectorBounded(InFinalQuery) || NOT Get_IsCardinal(InForward)
            || NOT Get_IsRadius(InSearchRadius))
        { return false; }
        const auto Displacement = InPoint - InActualPlant;
        const auto FinalDisplacement = InFinalQuery - InActualPlant;
        const auto Along = FVector::DotProduct(Displacement, InForward);
        const auto FinalAlong = FVector::DotProduct(FinalDisplacement, InForward);
        return FMath::IsFinite(Along) && FMath::IsFinite(FinalAlong) && Along > 0.0 && Along < FinalAlong
            && Displacement == InForward * Along && FinalDisplacement == InForward * FinalAlong
            && Get_FitsSphere(InPoint, InActualPlant, InSearchRadius)
            && Get_FitsSphere(InFinalQuery, InPoint, InSearchRadius);
    }

    auto
        Get_IsProceduralContactStagingHoldValid(
            const FProceduralContactStagingInput& InInput,
            const FProceduralContactStagingPlan& InPlan)
        -> bool
    {
        using namespace ck_procedural_contact_staging;
        if (NOT InPlan._TwoPlacement || InPlan._RequiredCount < 1 || InPlan._RequiredCount > 2
            || InInput._Legs.Num() < 1 || InInput._Legs.Num() > MaximumCount || InInput._Legs.GetData() == nullptr
            || InInput._Landings.Num() < 0 || InInput._Landings.Num() > MaximumCount
            || (InInput._Landings.Num() > 0 && InInput._Landings.GetData() == nullptr)
            || NOT Get_IsCardinal(InInput._Up) || NOT Get_IsCardinal(InInput._Forward)
            || FVector::DotProduct(InInput._Up, InInput._Forward) != 0.0
            || NOT Get_IsBodySupported(InInput._Body) || NOT Get_IsBodySupported(InPlan._RetreatBody)
            || (InInput._PosedBody.IsSet() && NOT Get_IsBodySupported(InInput._PosedBody.GetValue(), false)))
        { return false; }
        auto Seen = uint64{0};
        auto Found = uint64{0};
        for (const auto& Leg : InInput._Legs)
        {
            if (Leg._RosterIndex < 0 || Leg._RosterIndex >= MaximumCount
                || NOT Get_IsVectorBounded(Leg._Anchor.Get_FootWorld())
                || NOT Get_IsVectorBounded(Leg._Anchor.Get_HipLocal())
                || NOT Get_IsRadius(Leg._Anchor.Get_Reach()) || NOT Get_IsRadius(Leg._ForceReach)
                || Leg._ForceReach > Leg._Anchor.Get_Reach() || NOT Get_IsRadius(Leg._SearchRadius)
                || NOT Get_IsVectorBounded(Leg._OutboardWorld)
                || NOT FMath::IsFinite(Leg._OwnSideTolerance) || Leg._OwnSideTolerance < 0.0f)
            { return false; }
            const auto Bit = uint64{1} << Leg._RosterIndex;
            if ((Seen & Bit) != 0 || NOT Get_FitsMilestone(InInput, InPlan._RetreatBody, Leg, Leg._Anchor.Get_FootWorld()))
            { return false; }
            Seen |= Bit;
            for (auto Required = 0; Required < InPlan._RequiredCount; ++Required)
            {
                const auto& Milestones = InPlan._Required[Required];
                if (Milestones._RosterIndex != Leg._RosterIndex)
                { continue; }
                if ((Found & Bit) != 0
                    || NOT Get_FitsMilestone(InInput, InPlan._RetreatBody, Leg, Milestones._OriginalPlant)
                    || NOT Get_IsVectorBounded(Milestones._ProposedQuery)
                    || (Milestones._IntermediateQuery.IsSet() && NOT Get_IsVectorBounded(Milestones._IntermediateQuery.GetValue())))
                { return false; }
                Found |= Bit;
            }
        }
        auto RequiredMask = uint64{0};
        for (auto Required = 0; Required < InPlan._RequiredCount; ++Required)
        {
            const auto RosterIndex = InPlan._Required[Required]._RosterIndex;
            if (RosterIndex < 0 || RosterIndex >= MaximumCount)
            { return false; }
            const auto Bit = uint64{1} << RosterIndex;
            if ((RequiredMask & Bit) != 0)
            { return false; }
            RequiredMask |= Bit;
        }
        if (Found != RequiredMask)
        { return false; }
        for (const auto& Landing : InInput._Landings)
        {
            if (NOT Get_IsVectorBounded(Landing.Get_PointWorld()) || NOT Get_IsVectorBounded(Landing.Get_HipLocal())
                || NOT Get_IsRadius(Landing.Get_Reach()) || NOT Get_IsRadius(Landing.Get_AdmissionReach())
                || Landing.Get_AdmissionReach() > Landing.Get_Reach()
                || NOT Get_FitsPhysical(InInput, InPlan._RetreatBody, Landing.Get_PointWorld(),
                    Landing.Get_HipLocal(), Landing.Get_Reach(), Landing.Get_AdmissionReach()))
            { return false; }
        }
        const auto Combinations = InPlan._RequiredCount == 2 ? 9 : 3;
        for (auto Combination = 0; Combination < Combinations; ++Combination)
        {
            for (auto Required = 0; Required < InPlan._RequiredCount; ++Required)
            {
                const auto& Milestones = InPlan._Required[Required];
                const auto State = Required == 0 ? Combination % 3 : Combination / 3;
                const auto Point = State == 2 ? Milestones._ProposedQuery
                    : State == 1 && Milestones._IntermediateQuery.IsSet() ? Milestones._IntermediateQuery.GetValue()
                    : Milestones._OriginalPlant;
                for (const auto& Leg : InInput._Legs)
                {
                    if (Leg._RosterIndex == Milestones._RosterIndex
                        && NOT Get_FitsMilestone(InInput, InPlan._RetreatBody, Leg, Point))
                    { return false; }
                }
            }
        }
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
