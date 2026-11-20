#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"

#include "CkCore/Math/Geometry/CkGeometry_Types.h"

#include <cmath>
#include <limits>

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    using EFailure = ECk_HexGeometryFailure;

    struct FWideHexCoord
    {
        int64 _Q;
        int64 _R;
    };

    constexpr FWideHexCoord Directions[] = {{1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}};
    constexpr int32 DirectionCount = UE_ARRAY_COUNT(Directions);
    constexpr auto SqrtThree = 1.7320508075688772935;
    using ck::hex_grid::DomainCellLimit;
    constexpr auto PlaneTolerance = 1e-7;
    constexpr auto ParallelTolerance = 1e-12;

    auto
        TryGet_NormalizedUncertainty(
            double InBound,
            double& OutUncertainty)
        -> bool
    {
        constexpr auto ErrorPerUnit = 128.0 * std::numeric_limits<double>::epsilon();
        constexpr auto MinimumUncertainty = 1e-12;
        constexpr auto MaximumUncertainty = 1e-5;

        if (NOT FMath::IsFinite(InBound))
        { return false; }

        OutUncertainty = FMath::Max(MinimumUncertainty, ErrorPerUnit * InBound);
        return OutUncertainty <= MaximumUncertainty;
    }

    auto
        IsRepresentable(
            int64 InValue)
        -> bool
    {
        return InValue >= MIN_int32 && InValue <= MAX_int32;
    }

    auto
        TryWrite_Coordinate(
            FWideHexCoord InCoordinate,
            FCk_HexCoord& OutCoordinate,
            EFailure& OutFailure)
        -> bool
    {
        if (NOT IsRepresentable(InCoordinate._Q) || NOT IsRepresentable(InCoordinate._R))
        {
            OutCoordinate = {};
            OutFailure = EFailure::Overflow;
            return false;
        }

        OutCoordinate = FCk_HexCoord{static_cast<int32>(InCoordinate._Q), static_cast<int32>(InCoordinate._R)};
        OutFailure = EFailure::None;
        return true;
    }

    auto
        ResolveDomain(
            const FCk_HexDomain& InDomain,
            TArray<FCk_HexCoord>& OutCoordinates)
        -> EFailure
    {
        const auto& Center = InDomain.Get_Center();
        const auto Radius = static_cast<int64>(InDomain.Get_RingRadius());
        const auto& Coordinates = InDomain.Get_Coordinates();

        switch (InDomain.Get_Kind())
        {
            case ECk_HexDomainKind::Disk:
            {
                if (Radius < 0 || NOT Coordinates.IsEmpty())
                { return EFailure::InvalidDomain; }

                // Divide the count limit first, so a malformed radius never reaches the count multiplication.
                if (Radius > (DomainCellLimit - 1) / 3 / (Radius + 1))
                { return EFailure::LimitExceeded; }

                const auto CenterQ = static_cast<int64>(Center.Get_Q());
                const auto CenterR = static_cast<int64>(Center.Get_R());
                if (NOT IsRepresentable(CenterQ - Radius) || NOT IsRepresentable(CenterQ + Radius) ||
                    NOT IsRepresentable(CenterR - Radius) || NOT IsRepresentable(CenterR + Radius))
                { return EFailure::Overflow; }

                OutCoordinates.Reserve(static_cast<int32>(1 + 3 * Radius * (Radius + 1)));
                for (auto R = -Radius; R <= Radius; ++R)
                {
                    const auto FirstQ = FMath::Max(-Radius, -Radius - R);
                    const auto LastQ = FMath::Min(Radius, Radius - R);
                    for (auto Q = FirstQ; Q <= LastQ; ++Q)
                    {
                        OutCoordinates.Emplace(static_cast<int32>(CenterQ + Q), static_cast<int32>(CenterR + R));
                    }
                }
                return EFailure::None;
            }

            case ECk_HexDomainKind::Explicit:
            {
                if (Center != FCk_HexCoord{} || Radius != 0 || Coordinates.IsEmpty())
                { return EFailure::InvalidDomain; }

                if (Coordinates.Num() > DomainCellLimit)
                { return EFailure::LimitExceeded; }

                OutCoordinates = Coordinates;
                OutCoordinates.Sort([](const FCk_HexCoord& InA, const FCk_HexCoord& InB)
                {
                    return InA.Get_R() < InB.Get_R() ||
                        (InA.Get_R() == InB.Get_R() && InA.Get_Q() < InB.Get_Q());
                });
                for (auto Index = 1; Index < OutCoordinates.Num(); ++Index)
                {
                    if (OutCoordinates[Index - 1] == OutCoordinates[Index])
                    { return EFailure::DuplicateCoordinate; }
                }
                return EFailure::None;
            }
        }

        return EFailure::InvalidDomain;
    }

    auto
        ValidateLayout(
            const FCk_HexLayout& InLayout)
        -> EFailure
    {
        const auto Orientation = InLayout.Get_Orientation();
        if (Orientation != ECk_HexOrientation::Pointy && Orientation != ECk_HexOrientation::Flat)
        { return EFailure::InvalidOrientation; }

        const auto Radius = InLayout.Get_OuterRadius();
        if (NOT FMath::IsFinite(Radius) || Radius <= 0.0)
        { return EFailure::InvalidLayout; }

        return EFailure::None;
    }

    auto
        Get_MaxAbs(
            const FVector& InVector)
        -> double
    {
        return FMath::Max3(FMath::Abs(InVector.X), FMath::Abs(InVector.Y), FMath::Abs(InVector.Z));
    }

    auto
        Get_NormalizedCenter(
            FWideHexCoord InCoordinate,
            ECk_HexOrientation InOrientation)
        -> FVector2D
    {
        const auto Q = static_cast<double>(InCoordinate._Q);
        const auto R = static_cast<double>(InCoordinate._R);
        if (InOrientation == ECk_HexOrientation::Pointy)
        { return FVector2D{SqrtThree * (Q + R * 0.5), R * 1.5}; }

        return FVector2D{Q * 1.5, SqrtThree * (R + Q * 0.5)};
    }

    auto
        ProjectCenter(
            const FCk_HexCoord& InCoordinate,
            const FCk_HexLayout& InLayout,
            FVector2D& OutCenter)
        -> EFailure
    {
        const auto Orientation = InLayout.Get_Orientation();
        const auto Radius = InLayout.Get_OuterRadius();
        const auto Q = static_cast<int64>(InCoordinate.Get_Q());
        const auto R = static_cast<int64>(InCoordinate.Get_R());
        const auto Center = Get_NormalizedCenter({Q, R}, Orientation) * Radius;
        if (Center.ContainsNaN())
        { return EFailure::Overflow; }

        for (const auto& Direction : Directions)
        {
            // An out-of-range neighbor still cannot round to the same finite center.
            const auto Neighbor = Get_NormalizedCenter({Q + Direction._Q, R + Direction._R}, Orientation) * Radius;
            if (Neighbor == Center)
            { return EFailure::PrecisionLoss; }
        }

        OutCenter = Center;
        return EFailure::None;
    }

    auto
        FindNearestCoordinate(
            const FVector2D& InNormalizedPoint,
            ECk_HexOrientation InOrientation,
            double InWorldBound,
            double InConditioning,
            FCk_HexCoord& OutCoordinate,
            EFailure& OutFailure)
        -> bool
    {
        // A finite source point can overflow during radius normalization.
        if (InNormalizedPoint.ContainsNaN())
        {
            OutFailure = EFailure::Overflow;
            return false;
        }

        const auto bPointy = InOrientation == ECk_HexOrientation::Pointy;
        const auto Q = bPointy
            ? InNormalizedPoint.X / SqrtThree - InNormalizedPoint.Y / 3.0
            : InNormalizedPoint.X / 1.5;
        const auto R = bPointy
            ? InNormalizedPoint.Y / 1.5
            : InNormalizedPoint.Y / SqrtThree - InNormalizedPoint.X / 3.0;
        const auto S = -Q - R;

        constexpr auto MinCandidate = static_cast<double>(MIN_int32) - 4.0;
        constexpr auto MaxCandidate = static_cast<double>(MAX_int32) + 4.0;
        if (Q < MinCandidate || Q > MaxCandidate || R < MinCandidate || R > MaxCandidate)
        {
            OutFailure = EFailure::Overflow;
            return false;
        }

        const auto ChartMagnitude = FMath::Max(FMath::Abs(InNormalizedPoint.X), FMath::Abs(InNormalizedPoint.Y));
        const auto CubeMagnitude = FMath::Max3(FMath::Abs(Q), FMath::Abs(R), FMath::Abs(S));
        const auto Bound = (1.0 + FMath::Max3(ChartMagnitude, CubeMagnitude, InWorldBound)) * InConditioning;
        auto Uncertainty = 0.0;
        if (NOT TryGet_NormalizedUncertainty(Bound, Uncertainty))
        {
            OutFailure = EFailure::PrecisionLoss;
            return false;
        }

        const auto RoundedQ = std::round(Q);
        const auto RoundedR = std::round(R);
        const auto RoundedS = std::round(S);
        const auto ErrorQ = FMath::Abs(Q - RoundedQ);
        const auto ErrorR = FMath::Abs(R - RoundedR);
        const auto ErrorS = FMath::Abs(S - RoundedS);

        auto Base = FWideHexCoord{static_cast<int64>(RoundedQ), static_cast<int64>(RoundedR)};
        if (ErrorQ >= ErrorR && ErrorQ >= ErrorS)
        { Base._Q = static_cast<int64>(-RoundedR - RoundedS); }
        else if (ErrorR >= ErrorS)
        { Base._R = static_cast<int64>(-RoundedQ - RoundedS); }

        // If S has the greatest residual, repairing S leaves Q and R intact.
        constexpr auto CandidateCount = DirectionCount + 1;
        FWideHexCoord Candidates[CandidateCount];
        Candidates[0] = Base;
        for (auto Index = 0; Index < DirectionCount; ++Index)
        {
            const auto& Direction = Directions[Index];
            Candidates[Index + 1] = {Base._Q + Direction._Q, Base._R + Direction._R};
        }

        double Distances[CandidateCount];
        auto MinimumDistance = std::numeric_limits<double>::infinity();
        for (auto Index = 0; Index < CandidateCount; ++Index)
        {
            const auto Delta = InNormalizedPoint - Get_NormalizedCenter(Candidates[Index], InOrientation);
            Distances[Index] = Delta.Size();
            MinimumDistance = FMath::Min(MinimumDistance, Distances[Index]);
        }

        const auto TieDistance = MinimumDistance + 4.0 * Uncertainty;
        auto Winner = FWideHexCoord{MAX_int64, MAX_int64};
        for (auto Index = 0; Index < CandidateCount; ++Index)
        {
            const auto& Candidate = Candidates[Index];
            if (Distances[Index] <= TieDistance &&
                (Candidate._Q < Winner._Q || (Candidate._Q == Winner._Q && Candidate._R < Winner._R)))
            { Winner = Candidate; }
        }

        return TryWrite_Coordinate(Winner, OutCoordinate, OutFailure);
    }

    auto
        MakeWorldFrame(
            const FTransform& InPivot,
            double InOuterRadius,
            FCk_Frame3D& OutFrame,
            double& OutWorldRadius)
        -> EFailure
    {
        if (InPivot.ContainsNaN())
        { return EFailure::UnsupportedTransform; }

        const auto Rotation = InPivot.GetRotation();
        const auto Scale = InPivot.GetScale3D();
        const auto MinScale = FMath::Min3(Scale.X, Scale.Y, Scale.Z);
        const auto MaxScale = FMath::Max3(Scale.X, Scale.Y, Scale.Z);
        if (FMath::Abs(Rotation.SizeSquared() - 1.0) > 1e-12 || MinScale <= 0.0 ||
            (MaxScale - MinScale) / MaxScale > 1e-9)
        { return EFailure::UnsupportedTransform; }

        const auto WorldRadius = InOuterRadius * Scale.X;
        if (NOT FMath::IsFinite(WorldRadius) || WorldRadius <= 0.0)
        { return EFailure::PrecisionLoss; }

        OutFrame = FCk_Frame3D{InPivot.GetTranslation(), Rotation.GetNormalized()};
        OutWorldRadius = WorldRadius;
        return EFailure::None;
    }

    auto
        Get_ScaledFrameDelta(
            const FVector& InPoint,
            const FVector& InOrigin,
            double& OutScale)
        -> FVector
    {
        // Leave headroom for subtraction and the three dot-product terms, even for finite inputs near double max.
        constexpr auto ExtremeScale = 8.0;
        constexpr auto ExtremeMagnitude = std::numeric_limits<double>::max() / ExtremeScale;
        OutScale = FMath::Max(Get_MaxAbs(InPoint), Get_MaxAbs(InOrigin)) > ExtremeMagnitude ? ExtremeScale : 1.0;
        return InPoint / OutScale - InOrigin / OutScale;
    }

    auto
        Get_ScaledFramePoint(
            const FVector& InPoint,
            const FCk_Frame3D& InFrame,
            double& OutScale)
        -> FVector
    {
        const auto Delta = Get_ScaledFrameDelta(InPoint, InFrame.Get_Origin(), OutScale);
        return FVector{FVector::DotProduct(Delta, InFrame.Get_X()), FVector::DotProduct(Delta, InFrame.Get_Y()),
            FVector::DotProduct(Delta, InFrame.Get_Z())};
    }

    auto
        Get_PlaneOffsetRoundoff(
            const FVector& InDelta,
            const FVector& InNormal,
            const FQuat& InRotation)
        -> double
    {
        constexpr auto RoundoffFactor = 16.0 * std::numeric_limits<double>::epsilon();
        const auto X = InRotation.X;
        const auto Y = InRotation.Y;
        const auto Z = InRotation.Z;
        const auto W = InRotation.W;
        // A near-zero normal component can come from cancellation in the quaternion-to-axis conversion.
        // Include the magnitudes of those terms as well as the contributions to the final dot product.
        const auto Weights = FVector{
            (FMath::Abs(InNormal.X) + 2.0 * (FMath::Abs(Z * X) + FMath::Abs(Y * W))) * RoundoffFactor,
            (FMath::Abs(InNormal.Y) + 2.0 * (FMath::Abs(Z * Y) + FMath::Abs(X * W))) * RoundoffFactor,
            (FMath::Abs(InNormal.Z) + 2.0 * (X * X + Y * Y)) * RoundoffFactor};
        return FVector::DotProduct(InDelta.GetAbs(), Weights);
    }

    auto
        Get_WorldBound(
            const FVector& InPoint,
            const FVector& InPivotTranslation,
            double InWorldRadius)
        -> double
    {
        // Divide first so even a sum above double's maximum can have a finite bound.
        return Get_MaxAbs(InPoint) / InWorldRadius + Get_MaxAbs(InPivotTranslation) / InWorldRadius;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_HexGeometry_UE::
    TryValidateWorldLayout(
        const FCk_HexLayout& InLayout,
        const FTransform& InPivot,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    OutFailure = ValidateLayout(InLayout);
    if (OutFailure != EFailure::None)
    { return false; }

    auto Frame = FCk_Frame3D{};
    auto Radius = 0.0;
    OutFailure = MakeWorldFrame(InPivot, InLayout.Get_OuterRadius(), Frame, Radius);
    return OutFailure == EFailure::None;
}

auto
    UCk_Utils_HexGeometry_UE::
    TryRotate(
        const FCk_HexCoord& InOffset,
        ECk_HexRotation InRotation,
        FCk_HexCoord& OutOffset,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    const auto Turns = static_cast<uint8>(InRotation);
    if (Turns > static_cast<uint8>(ECk_HexRotation::Turn5))
    {
        OutOffset = {};
        OutFailure = EFailure::InvalidRotation;
        return false;
    }

    auto Offset = FWideHexCoord{InOffset.Get_Q(), InOffset.Get_R()};
    for (auto Turn = 0; Turn < Turns; ++Turn)
    {
        Offset = {-Offset._R, Offset._Q + Offset._R};
    }

    return TryWrite_Coordinate(Offset, OutOffset, OutFailure);
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_Neighbor(
        const FCk_HexCoord& InCoordinate,
        ECk_HexDirection InDirection,
        FCk_HexCoord& OutNeighbor,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    const auto DirectionIndex = static_cast<uint8>(InDirection);
    if (DirectionIndex >= DirectionCount)
    {
        OutNeighbor = {};
        OutFailure = EFailure::InvalidDirection;
        return false;
    }

    const auto& Direction = Directions[DirectionIndex];
    return TryWrite_Coordinate({InCoordinate.Get_Q() + Direction._Q, InCoordinate.Get_R() + Direction._R},
        OutNeighbor, OutFailure);
}

auto
    UCk_Utils_HexGeometry_UE::
    TryTranslate(
        const FCk_HexCoord& InAnchor,
        const FCk_HexCoord& InOffset,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    const auto Q = static_cast<int64>(InAnchor.Get_Q()) + InOffset.Get_Q();
    const auto R = static_cast<int64>(InAnchor.Get_R()) + InOffset.Get_R();
    return TryWrite_Coordinate({Q, R}, OutCoordinate, OutFailure);
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_Distance(
        const FCk_HexCoord& InA,
        const FCk_HexCoord& InB,
        int64& OutDistance,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    const auto Q = static_cast<int64>(InA.Get_Q()) - InB.Get_Q();
    const auto R = static_cast<int64>(InA.Get_R()) - InB.Get_R();
    OutDistance = FMath::Max3(FMath::Abs(Q), FMath::Abs(R), FMath::Abs(Q + R));
    OutFailure = EFailure::None;
    return true;
}

auto
    UCk_Utils_HexGeometry_UE::
    TryResolve_Domain(
        const FCk_HexDomain& InDomain,
        TArray<FCk_HexCoord>& OutCoordinates,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    auto Coordinates = TArray<FCk_HexCoord>{};
    OutFailure = ResolveDomain(InDomain, Coordinates);
    if (OutFailure != EFailure::None)
    {
        OutCoordinates.Reset();
        return false;
    }

    OutCoordinates = MoveTemp(Coordinates);
    return true;
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_LocalCenter(
        const FCk_HexCoord& InCoordinate,
        const FCk_HexLayout& InLayout,
        FVector2D& OutCenter,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    OutCenter = FVector2D::ZeroVector;
    OutFailure = ValidateLayout(InLayout);
    if (OutFailure != EFailure::None)
    { return false; }

    OutFailure = ProjectCenter(InCoordinate, InLayout, OutCenter);
    return OutFailure == EFailure::None;
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_LocalCorners(
        const FCk_HexCoord& InCoordinate,
        const FCk_HexLayout& InLayout,
        TArray<FVector2D>& OutCorners,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    OutCorners.Reset();
    auto Center = FVector2D::ZeroVector;
    if (NOT TryGet_LocalCenter(InCoordinate, InLayout, Center, OutFailure))
    { return false; }

    constexpr auto HalfSqrtThree = SqrtThree * 0.5;
    const FVector2D PointyCorners[] =
        {{HalfSqrtThree, 0.5}, {0.0, 1.0}, {-HalfSqrtThree, 0.5},
         {-HalfSqrtThree, -0.5}, {0.0, -1.0}, {HalfSqrtThree, -0.5}};
    const FVector2D FlatCorners[] =
        {{1.0, 0.0}, {0.5, HalfSqrtThree}, {-0.5, HalfSqrtThree},
         {-1.0, 0.0}, {-0.5, -HalfSqrtThree}, {0.5, -HalfSqrtThree}};
    const auto& UnitCorners = InLayout.Get_Orientation() == ECk_HexOrientation::Pointy ? PointyCorners : FlatCorners;

    constexpr int32 CornerCount = UE_ARRAY_COUNT(UnitCorners);
    FVector2D Corners[CornerCount];
    for (auto Index = 0; Index < CornerCount; ++Index)
    {
        Corners[Index] = Center + UnitCorners[Index] * InLayout.Get_OuterRadius();
        if (Corners[Index].ContainsNaN())
        {
            OutFailure = EFailure::Overflow;
            return false;
        }

        if (Index > 0 && Corners[Index] == Corners[Index - 1])
        {
            OutFailure = EFailure::PrecisionLoss;
            return false;
        }
    }

    if (Corners[0] == Corners[CornerCount - 1])
    {
        OutFailure = EFailure::PrecisionLoss;
        return false;
    }

    OutCorners.Append(Corners, CornerCount);
    return true;
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_CoordFromLocal(
        const FVector2D& InPoint,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    OutCoordinate = {};
    OutFailure = ValidateLayout(InLayout);
    if (OutFailure != EFailure::None)
    { return false; }

    if (InPoint.ContainsNaN())
    {
        OutFailure = EFailure::InvalidCoordinate;
        return false;
    }

    const auto Radius = InLayout.Get_OuterRadius();
    return FindNearestCoordinate({InPoint.X / Radius, InPoint.Y / Radius}, InLayout.Get_Orientation(),
        0.0, 1.0, OutCoordinate, OutFailure);
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_CoordFromWorldPoint(
        const FVector& InPoint,
        const FTransform& InPivotWorld,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    OutCoordinate = {};
    OutFailure = ValidateLayout(InLayout);
    if (OutFailure != EFailure::None)
    { return false; }

    if (InPoint.ContainsNaN())
    {
        OutFailure = EFailure::InvalidCoordinate;
        return false;
    }

    auto Frame = FCk_Frame3D{};
    auto WorldRadius = 0.0;
    OutFailure = MakeWorldFrame(InPivotWorld, InLayout.Get_OuterRadius(), Frame, WorldRadius);
    if (OutFailure != EFailure::None)
    { return false; }

    auto FramePointScale = 1.0;
    const auto FramePoint = Get_ScaledFramePoint(InPoint, Frame, FramePointScale);
    if (FMath::Abs(FramePoint.Z) / WorldRadius * FramePointScale > PlaneTolerance)
    {
        OutFailure = EFailure::OffPlane;
        return false;
    }

    return FindNearestCoordinate({FramePoint.X / WorldRadius * FramePointScale,
        FramePoint.Y / WorldRadius * FramePointScale}, InLayout.Get_Orientation(),
        Get_WorldBound(InPoint, Frame.Get_Origin(), WorldRadius), 1.0, OutCoordinate, OutFailure);
}

auto
    UCk_Utils_HexGeometry_UE::
    TryGet_CoordFromWorldRay(
        const FVector& InOrigin,
        const FVector& InDirection,
        const FTransform& InPivotWorld,
        const FCk_HexLayout& InLayout,
        FCk_HexCoord& OutCoordinate,
        FVector& OutWorldHit,
        ECk_HexGeometryFailure& OutFailure)
    -> bool
{
    const auto Origin = InOrigin;
    const auto Direction = InDirection;
    OutCoordinate = {};
    OutWorldHit = FVector::ZeroVector;
    OutFailure = ValidateLayout(InLayout);
    if (OutFailure != EFailure::None)
    { return false; }

    if (Origin.ContainsNaN())
    {
        OutFailure = EFailure::InvalidCoordinate;
        return false;
    }

    if (Direction.ContainsNaN())
    {
        OutFailure = EFailure::InvalidDirection;
        return false;
    }

    const auto DirectionScale = Get_MaxAbs(Direction);
    if (DirectionScale == 0.0)
    {
        OutFailure = EFailure::InvalidDirection;
        return false;
    }

    auto Frame = FCk_Frame3D{};
    auto WorldRadius = 0.0;
    OutFailure = MakeWorldFrame(InPivotWorld, InLayout.Get_OuterRadius(), Frame, WorldRadius);
    if (OutFailure != EFailure::None)
    { return false; }

    const auto ScaledDirection = FVector{Direction.X / DirectionScale, Direction.Y / DirectionScale,
        Direction.Z / DirectionScale};
    const auto UnitDirection = ScaledDirection / ScaledDirection.Size();
    const auto Normal = Frame.Get_Z();
    const auto NormalDot = FVector::DotProduct(UnitDirection, Normal);
    const auto AbsNormalDot = FMath::Abs(NormalDot);
    if (AbsNormalDot <= ParallelTolerance)
    {
        OutFailure = EFailure::ParallelRay;
        return false;
    }

    const auto PivotTranslation = Frame.Get_Origin();
    auto FrameOriginScale = 1.0;
    const auto OriginDelta = Get_ScaledFrameDelta(Origin, PivotTranslation, FrameOriginScale);
    const auto PlaneOffset = FVector::DotProduct(OriginDelta, Normal);
    auto Hit = Origin;
    if (FMath::Abs(PlaneOffset) > Get_PlaneOffsetRoundoff(OriginDelta, Normal, Frame.Get_Rotation()))
    {
        if ((PlaneOffset > 0.0) == (NormalDot > 0.0))
        {
            OutFailure = EFailure::BehindRay;
            return false;
        }

        // The distance can exceed double max even when the hit itself is representable.
        constexpr auto DistanceHeadroom = 8.0;
        constexpr auto MaxDistanceWithHeadroom = std::numeric_limits<double>::max() / DistanceHeadroom;
        const auto DistanceScale = FMath::Abs(PlaneOffset) > MaxDistanceWithHeadroom * AbsNormalDot
            ? DistanceHeadroom : 1.0;
        const auto ScaledDistance = -(PlaneOffset / DistanceScale) / NormalDot;
        if (NOT FMath::IsFinite(ScaledDistance))
        {
            OutFailure = EFailure::PrecisionLoss;
            return false;
        }

        const auto HitScale = FrameOriginScale * DistanceScale;
        const auto HitX = std::fma(UnitDirection.X, ScaledDistance, Origin.X / HitScale) * HitScale;
        const auto HitY = std::fma(UnitDirection.Y, ScaledDistance, Origin.Y / HitScale) * HitScale;
        const auto HitZ = std::fma(UnitDirection.Z, ScaledDistance, Origin.Z / HitScale) * HitScale;
        if (NOT FMath::IsFinite(HitX) || NOT FMath::IsFinite(HitY) || NOT FMath::IsFinite(HitZ))
        {
            OutFailure = EFailure::PrecisionLoss;
            return false;
        }

        Hit = FVector{HitX, HitY, HitZ};
    }

    auto FrameHitScale = 1.0;
    const auto FrameHit = Get_ScaledFramePoint(Hit, Frame, FrameHitScale);
    const auto WorldBound = FMath::Max(Get_WorldBound(Origin, PivotTranslation, WorldRadius),
        Get_WorldBound(Hit, PivotTranslation, WorldRadius));
    if (NOT FindNearestCoordinate({FrameHit.X / WorldRadius * FrameHitScale,
            FrameHit.Y / WorldRadius * FrameHitScale}, InLayout.Get_Orientation(), WorldBound,
            FMath::Max(1.0, 1.0 / AbsNormalDot), OutCoordinate, OutFailure))
    { return false; }

    OutWorldHit = Hit;
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
