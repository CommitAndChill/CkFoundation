#include "CkProceduralAnimation/Core/CkProceduralFoothold.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_foothold
{
    // A foothold shallower than this share of the leg's rest drop below its hip stands beside the body, not under it.
    constexpr auto UnderBodyDepthShareOfRestDrop = 0.25;

    // A face exactly perpendicular to the support up (a vertical wall under a 90 degree limit) must pass the level gate
    // whatever the rounding of its normal.
    constexpr auto LevelCosineTolerance = 1.0e-4;

    auto
        Get_IsCandidateFinite(
            const ck::FProceduralFootholdCandidate& InCandidate)
        -> bool
    {
        return NOT InCandidate.Get_Position().ContainsNaN() && NOT InCandidate.Get_Normal().ContainsNaN();
    }

    auto
        Get_AreSettingsWellFormed(
            const ck::FProceduralFootholdSettings& InSettings)
        -> bool
    {
        return FMath::IsFinite(InSettings.Get_SlopeWeight())
            && FMath::IsFinite(InSettings.Get_ContinuityWeight())
            && FMath::IsFinite(InSettings.Get_MaxAngleDegrees())
            && NOT InSettings.Get_SlopeUp().ContainsNaN() && NOT InSettings.Get_SlopeUp().GetSafeNormal().IsNearlyZero();
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        Get_IsFootholdLevelEnough(
            const FVector& InNormal,
            float InMaxAngleDegrees)
        -> bool
    {
        if (InNormal.ContainsNaN() || NOT FMath::IsFinite(InMaxAngleDegrees))
        { return false; }

        const auto Normal = InNormal.GetSafeNormal();
        if (Normal.IsNearlyZero())
        { return false; }

        const auto MinCosine = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(InMaxAngleDegrees)));
        return Normal.Z >= MinCosine - ck_procedural_foothold::LevelCosineTolerance;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        Get_IsFootholdOnOwnSide(
            const FVector& InHipToPoint,
            const FVector& InOutboard,
            float InTolerance)
        -> bool
    {
        if (InHipToPoint.ContainsNaN() || NOT FMath::IsFinite(InTolerance))
        { return false; }

        const auto Outboard = InOutboard.GetSafeNormal();
        if (Outboard.IsNearlyZero())
        { return true; }

        return FVector::DotProduct(InHipToPoint, Outboard) >= -static_cast<double>(InTolerance);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        Get_IsFootholdUnderBody(
            float InLateralOffset,
            float InDepthBelowHip,
            float InMaxHipLateral,
            float InRestDrop,
            float InMargin)
        -> bool
    {
        const auto InputIsFinite = FMath::IsFinite(InLateralOffset) && FMath::IsFinite(InDepthBelowHip)
            && FMath::IsFinite(InMaxHipLateral) && FMath::IsFinite(InRestDrop) && FMath::IsFinite(InMargin);
        if (NOT InputIsFinite || InMaxHipLateral <= 0.0f)
        { return false; }

        return FMath::Abs(InLateralOffset) < InMaxHipLateral - InMargin
            && InDepthBelowHip > ck_procedural_foothold::UnderBodyDepthShareOfRestDrop * InRestDrop;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        Get_ProceduralFootholdLineSphereOffsets(
            const FVector& InLinePoint,
            const FVector& InUnitDirection,
            const FVector& InSphereCenter,
            double InRadius)
        -> TOptional<FVector2D>
    {
        if (InLinePoint.ContainsNaN() || InUnitDirection.ContainsNaN() || InSphereCenter.ContainsNaN()
            || NOT FMath::IsFinite(InRadius) || InRadius <= 0.0)
        { return {}; }
        const auto DirectionSquared = InUnitDirection.X * InUnitDirection.X
            + InUnitDirection.Y * InUnitDirection.Y + InUnitDirection.Z * InUnitDirection.Z;
        if (NOT FMath::IsFinite(DirectionSquared) || FMath::Abs(DirectionSquared - 1.0) > UE_KINDA_SMALL_NUMBER)
        { return {}; }
        const auto X = InLinePoint.X - InSphereCenter.X;
        const auto Y = InLinePoint.Y - InSphereCenter.Y;
        const auto Z = InLinePoint.Z - InSphereCenter.Z;
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return {}; }
        const auto DistanceSquared = X * X + Y * Y + Z * Z;
        const auto Along = X * InUnitDirection.X + Y * InUnitDirection.Y + Z * InUnitDirection.Z;
        const auto RadiusSquared = InRadius * InRadius;
        if (NOT FMath::IsFinite(DistanceSquared) || NOT FMath::IsFinite(Along) || NOT FMath::IsFinite(RadiusSquared))
        { return {}; }
        const auto Discriminant = Along * Along - DirectionSquared * (DistanceSquared - RadiusSquared);
        if (NOT FMath::IsFinite(Discriminant) || Discriminant < 0.0)
        { return {}; }
        const auto HalfSpan = FMath::Sqrt(Discriminant);
        const auto First = (-Along - HalfSpan) / DirectionSquared;
        const auto Second = (-Along + HalfSpan) / DirectionSquared;
        if (NOT FMath::IsFinite(First) || NOT FMath::IsFinite(Second))
        { return {}; }
        return FVector2D{First, Second};
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        Get_ProceduralFootholdFailedFaceQueryPoint(
            const FVector& InFacePoint,
            const FVector& InFaceNormal,
            const FVector& InUp,
            const FVector& InBody,
            const FVector& InHip,
            const FVector& InLateral,
            const FVector& InOutboard,
            double InBodyBoundary,
            double InOwnSideMinimum,
            float InSearchRadius)
        -> TOptional<FVector>
    {
        if (InFacePoint.ContainsNaN() || InBody.ContainsNaN() || InHip.ContainsNaN()
            || InFaceNormal.ContainsNaN() || NOT InFaceNormal.IsNormalized()
            || InUp.ContainsNaN() || NOT InUp.IsNormalized()
            || InLateral.ContainsNaN() || NOT InLateral.IsNormalized()
            || InOutboard.ContainsNaN() || NOT InOutboard.IsNormalized()
            || NOT FMath::IsFinite(InBodyBoundary) || NOT FMath::IsFinite(InOwnSideMinimum)
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f
            || FMath::Abs(FVector::DotProduct(InLateral, InUp)) > KINDA_SMALL_NUMBER
            || FMath::Abs(FVector::DotProduct(InOutboard, InUp)) > KINDA_SMALL_NUMBER)
        { return {}; }
        const auto SideDot = FVector::DotProduct(InOutboard, InLateral);
        if (NOT FMath::IsFinite(SideDot) || FMath::Abs(SideDot) <= UE_DOUBLE_SMALL_NUMBER)
        { return {}; }
        const auto Side = FMath::Sign(SideDot);
        const auto Normal = FVector::VectorPlaneProject(InFaceNormal, InUp).GetSafeNormal();
        if (NOT Normal.IsNormalized())
        { return {}; }
        const auto Direction = FVector::CrossProduct(InUp, Normal).GetSafeNormal();
        if (NOT Direction.IsNormalized())
        { return {}; }
        const auto Radius = static_cast<double>(InSearchRadius);
        const auto Inset = static_cast<double>(FMath::Max(1.0f, InSearchRadius * 0.15f));
        if (Inset > Radius)
        { return {}; }
        const auto X = InFacePoint.X - Inset * Normal.X;
        const auto Y = InFacePoint.Y - Inset * Normal.Y;
        const auto Z = InFacePoint.Z - Inset * Normal.Z;
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return {}; }
        const auto Base = FVector{X, Y, Z};
        const auto DifferenceDot = [](const FVector& InPoint, const FVector& InOrigin, const FVector& InAxis)
            -> TOptional<double>
        {
            const auto DX = InPoint.X - InOrigin.X;
            const auto DY = InPoint.Y - InOrigin.Y;
            const auto DZ = InPoint.Z - InOrigin.Z;
            if (NOT FMath::IsFinite(DX) || NOT FMath::IsFinite(DY) || NOT FMath::IsFinite(DZ))
            { return {}; }
            const auto Value = DX * InAxis.X + DY * InAxis.Y + DZ * InAxis.Z;
            return FMath::IsFinite(Value) ? TOptional<double>{Value} : TOptional<double>{};
        };
        const auto BodyDot = DifferenceDot(Base, InBody, InLateral);
        const auto OwnSideDot = DifferenceDot(Base, InHip, InOutboard);
        if (NOT BodyDot.IsSet() || NOT OwnSideDot.IsSet())
        { return {}; }
        const auto HalfSpan = FMath::Sqrt(Radius * Radius - Inset * Inset);
        auto Low = -HalfSpan;
        auto High = HalfSpan;
        auto InvalidInterval = false;
        const auto Intersect = [&](double InCoefficient, double InValue, double InMinimum) -> bool
        {
            if (NOT FMath::IsFinite(InCoefficient) || NOT FMath::IsFinite(InValue))
            { InvalidInterval = true; return false; }
            if (FMath::Abs(InCoefficient) <= UE_DOUBLE_SMALL_NUMBER)
            { return InValue >= InMinimum; }
            const auto Bound = (InMinimum - InValue) / InCoefficient;
            if (NOT FMath::IsFinite(Bound))
            { InvalidInterval = true; return false; }
            if (InCoefficient > 0.0)
            { Low = FMath::Max(Low, Bound); }
            else
            { High = FMath::Min(High, Bound); }
            return Low <= High;
        };
        if (NOT Intersect(Side * FVector::DotProduct(Direction, InLateral), Side * BodyDot.GetValue(), InBodyBoundary)
            || NOT Intersect(FVector::DotProduct(Direction, InOutboard), OwnSideDot.GetValue(), InOwnSideMinimum))
        {
            if (InvalidInterval)
            { return {}; }

            // The original short circuit may not have evaluated the second preferred constraint.
            const auto HasValidBoundArithmetic = [](double InCoefficient, double InValue, double InMinimum) -> bool
            {
                if (NOT FMath::IsFinite(InCoefficient) || NOT FMath::IsFinite(InValue))
                { return false; }
                if (FMath::Abs(InCoefficient) <= UE_DOUBLE_SMALL_NUMBER)
                { return true; }
                return FMath::IsFinite((InMinimum - InValue) / InCoefficient);
            };
            if (NOT HasValidBoundArithmetic(Side * FVector::DotProduct(Direction, InLateral),
                    Side * BodyDot.GetValue(), InBodyBoundary)
                || NOT HasValidBoundArithmetic(FVector::DotProduct(Direction, InOutboard),
                    OwnSideDot.GetValue(), InOwnSideMinimum))
            { return {}; }

            // Only a valid empty preferred slice reaches this bounded construction.
            const auto FaceBodyDot = DifferenceDot(InFacePoint, InBody, InLateral);
            const auto FaceOwnSideDot = DifferenceDot(InFacePoint, InHip, InOutboard);
            if (NOT FaceBodyDot.IsSet() || NOT FaceOwnSideDot.IsSet())
            { return {}; }
            struct FBoundary
            {
                double InsetCoefficient;
                double TangentCoefficient;
                double Value;
            };
            // Stable order: minimum inset, preferred inset, body side, own side.
            const FBoundary Boundaries[4] = {
                {1.0, 0.0, 1.0}, {1.0, 0.0, Inset},
                {-Side * FVector::DotProduct(Normal, InLateral),
                    Side * FVector::DotProduct(Direction, InLateral), InBodyBoundary - Side * FaceBodyDot.GetValue()},
                {-FVector::DotProduct(Normal, InOutboard),
                    FVector::DotProduct(Direction, InOutboard), InOwnSideMinimum - FaceOwnSideDot.GetValue()}};
            for (const auto& Boundary : Boundaries)
            {
                if (NOT FMath::IsFinite(Boundary.InsetCoefficient)
                    || NOT FMath::IsFinite(Boundary.TangentCoefficient) || NOT FMath::IsFinite(Boundary.Value))
                { return {}; }
            }
            auto Best = TOptional<FVector>{};
            auto BestDistanceSquared = TNumericLimits<double>::Max();
            const auto TryInset = [&](double InCandidateInset) -> void
            {
                if (NOT FMath::IsFinite(InCandidateInset) || InCandidateInset < 1.0 || InCandidateInset > Inset)
                { return; }
                const auto CandidateX = InFacePoint.X - InCandidateInset * Normal.X;
                const auto CandidateY = InFacePoint.Y - InCandidateInset * Normal.Y;
                const auto CandidateZ = InFacePoint.Z - InCandidateInset * Normal.Z;
                if (NOT FMath::IsFinite(CandidateX) || NOT FMath::IsFinite(CandidateY) || NOT FMath::IsFinite(CandidateZ))
                { return; }
                const auto CandidateBase = FVector{CandidateX, CandidateY, CandidateZ};
                const auto CandidateBodyDot = DifferenceDot(CandidateBase, InBody, InLateral);
                const auto CandidateOwnSideDot = DifferenceDot(CandidateBase, InHip, InOutboard);
                if (NOT CandidateBodyDot.IsSet() || NOT CandidateOwnSideDot.IsSet())
                { return; }
                const auto CandidateHalfSpan = FMath::Sqrt(Radius * Radius - InCandidateInset * InCandidateInset);
                if (NOT FMath::IsFinite(CandidateHalfSpan))
                { return; }
                auto CandidateLow = -CandidateHalfSpan;
                auto CandidateHigh = CandidateHalfSpan;
                const auto Clip = [&](double InCoefficient, double InValue, double InMinimum) -> bool
                {
                    if (NOT FMath::IsFinite(InCoefficient) || NOT FMath::IsFinite(InValue))
                    { return false; }
                    if (FMath::Abs(InCoefficient) <= UE_DOUBLE_SMALL_NUMBER)
                    { return InValue >= InMinimum; }
                    const auto Bound = (InMinimum - InValue) / InCoefficient;
                    if (NOT FMath::IsFinite(Bound))
                    { return false; }
                    if (InCoefficient > 0.0)
                    { CandidateLow = FMath::Max(CandidateLow, Bound); }
                    else
                    { CandidateHigh = FMath::Min(CandidateHigh, Bound); }
                    return CandidateLow <= CandidateHigh;
                };
                if (NOT Clip(Boundaries[2].TangentCoefficient, Side * CandidateBodyDot.GetValue(), InBodyBoundary)
                    || NOT Clip(Boundaries[3].TangentCoefficient, CandidateOwnSideDot.GetValue(), InOwnSideMinimum)
                    || NOT (CandidateLow < CandidateHigh))
                { return; }
                const auto CandidateOffset = CandidateLow + (CandidateHigh - CandidateLow) / 2.0;
                if (NOT FMath::IsFinite(CandidateOffset)
                    || NOT (CandidateLow < CandidateOffset && CandidateOffset < CandidateHigh))
                { return; }
                const auto PointX = CandidateX + CandidateOffset * Direction.X;
                const auto PointY = CandidateY + CandidateOffset * Direction.Y;
                const auto PointZ = CandidateZ + CandidateOffset * Direction.Z;
                const auto NativeLimit = static_cast<double>(TNumericLimits<float>::Max());
                if (NOT FMath::IsFinite(PointX) || NOT FMath::IsFinite(PointY) || NOT FMath::IsFinite(PointZ)
                    || FMath::Abs(PointX) > NativeLimit || FMath::Abs(PointY) > NativeLimit || FMath::Abs(PointZ) > NativeLimit)
                { return; }
                const auto Point = FVector{PointX, PointY, PointZ};
                const auto PointBodyDot = DifferenceDot(Point, InBody, InLateral);
                const auto PointOwnSideDot = DifferenceDot(Point, InHip, InOutboard);
                const auto PointNormalDot = DifferenceDot(Point, InFacePoint, Normal);
                const auto PointUpDot = DifferenceDot(Point, InFacePoint, InUp);
                if (NOT PointBodyDot.IsSet() || NOT PointOwnSideDot.IsSet()
                    || NOT PointNormalDot.IsSet() || NOT PointUpDot.IsSet()
                    || Side * PointBodyDot.GetValue() < InBodyBoundary || PointOwnSideDot.GetValue() < InOwnSideMinimum
                    || -PointNormalDot.GetValue() < 1.0 || -PointNormalDot.GetValue() > Inset
                    || PointUpDot.GetValue() != 0.0)
                { return; }
                const auto DX = PointX - InFacePoint.X;
                const auto DY = PointY - InFacePoint.Y;
                const auto DZ = PointZ - InFacePoint.Z;
                const auto DistanceSquared = DX * DX + DY * DY + DZ * DZ;
                if (NOT FMath::IsFinite(DistanceSquared) || DistanceSquared > Radius * Radius)
                { return; }
                // Equal distances retain the earlier projection/pair in the fixed enumeration.
                if (NOT Best.IsSet() || DistanceSquared < BestDistanceSquared)
                {
                    Best = Point;
                    BestDistanceSquared = DistanceSquared;
                }
            };
            // Four origin-to-line projections include the exact minimum-inset candidate.
            for (const auto& Boundary : Boundaries)
            {
                const auto Denominator = Boundary.InsetCoefficient * Boundary.InsetCoefficient
                    + Boundary.TangentCoefficient * Boundary.TangentCoefficient;
                const auto Numerator = Boundary.InsetCoefficient * Boundary.Value;
                if (FMath::IsFinite(Denominator) && Denominator > 0.0 && FMath::IsFinite(Numerator))
                { TryInset(Numerator / Denominator); }
            }
            // Six lexicographically ordered nonparallel boundary-pair intersections.
            for (auto First = 0; First < 4; ++First)
            {
                for (auto Second = First + 1; Second < 4; ++Second)
                {
                    const auto& A = Boundaries[First];
                    const auto& B = Boundaries[Second];
                    const auto Determinant = A.InsetCoefficient * B.TangentCoefficient
                        - B.InsetCoefficient * A.TangentCoefficient;
                    const auto Numerator = A.Value * B.TangentCoefficient - B.Value * A.TangentCoefficient;
                    if (FMath::IsFinite(Determinant) && Determinant != 0.0 && FMath::IsFinite(Numerator))
                    { TryInset(Numerator / Determinant); }
                }
            }
            return Best;
        }
        const auto Offset = FMath::Clamp(0.0, Low, High);
        const auto QX = X + Offset * Direction.X;
        const auto QY = Y + Offset * Direction.Y;
        const auto QZ = Z + Offset * Direction.Z;
        const auto NativeLimit = static_cast<double>(TNumericLimits<float>::Max());
        if (NOT FMath::IsFinite(QX) || NOT FMath::IsFinite(QY) || NOT FMath::IsFinite(QZ)
            || FMath::Abs(QX) > NativeLimit || FMath::Abs(QY) > NativeLimit || FMath::Abs(QZ) > NativeLimit)
        { return {}; }
        const auto DX = QX - InFacePoint.X;
        const auto DY = QY - InFacePoint.Y;
        const auto DZ = QZ - InFacePoint.Z;
        if (NOT FMath::IsFinite(DX) || NOT FMath::IsFinite(DY) || NOT FMath::IsFinite(DZ))
        { return {}; }
        const auto DistanceSquared = DX * DX + DY * DY + DZ * DZ;
        if (NOT FMath::IsFinite(DistanceSquared) || DistanceSquared > Radius * Radius)
        { return {}; }
        return FVector{QX, QY, QZ};
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        ComputeProceduralFootholdCost(
            const FProceduralFootholdCandidate& InCandidate,
            const FVector& InIdeal,
            const FVector& InPlant,
            bool InPlanted,
            float InReach,
            const FProceduralFootholdSettings& InSettings)
        -> double
    {
        if (NOT FMath::IsFinite(InReach) || InReach <= 0.0f || InIdeal.ContainsNaN() || InPlant.ContainsNaN()
            || NOT ck_procedural_foothold::Get_IsCandidateFinite(InCandidate)
            || NOT ck_procedural_foothold::Get_AreSettingsWellFormed(InSettings))
        { return TNumericLimits<double>::Max(); }

        const auto Reach = static_cast<double>(InReach);
        const auto Distance = FVector::Dist(InCandidate.Get_Position(), InIdeal) / Reach;
        const auto Slope = InSettings.Get_SlopeWeight() * (1.0 - FVector::DotProduct(
            InCandidate.Get_Normal().GetSafeNormal(), InSettings.Get_SlopeUp().GetSafeNormal()));
        const auto Continuity = InPlanted
            ? InSettings.Get_ContinuityWeight() * FMath::Abs(InCandidate.Get_Position().Z - InPlant.Z) / Reach
            : 0.0;

        return Distance + Slope + Continuity;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        SelectProceduralFoothold(
            TArrayView<const FProceduralFootholdCandidate> InCandidates,
            const FVector& InIdeal,
            const FVector& InPlant,
            bool InPlanted,
            float InReach,
            const FProceduralFootholdSettings& InSettings)
        -> int32
    {
        const auto InputIsWellFormed = NOT InCandidates.IsEmpty()
            && FMath::IsFinite(InReach) && InReach > 0.0f
            && NOT InIdeal.ContainsNaN() && NOT InPlant.ContainsNaN()
            && ck_procedural_foothold::Get_AreSettingsWellFormed(InSettings)
            && algo::AllOf(InCandidates, &ck_procedural_foothold::Get_IsCandidateFinite);
        if (NOT InputIsWellFormed)
        { return INDEX_NONE; }

        auto Chosen = int32{INDEX_NONE};
        auto ChosenCost = TNumericLimits<double>::Max();
        for (auto Index = 0; Index < InCandidates.Num(); ++Index)
        {
            const auto& Candidate = InCandidates[Index];
            if (Candidate.Get_Verdict() != EProceduralFootholdVerdict::Usable)
            { continue; }

            const auto Cost = ComputeProceduralFootholdCost(Candidate, InIdeal, InPlant, InPlanted, InReach, InSettings);
            if (Chosen == INDEX_NONE || Cost < ChosenCost)
            {
                Chosen = Index;
                ChosenCost = Cost;
            }
        }
        return Chosen;
    }
}

// --------------------------------------------------------------------------------------------------------------------
