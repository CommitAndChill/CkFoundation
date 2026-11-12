#include "CkPmg_Polyline_Geometry.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_pmg_polyline_geometry
{
    constexpr auto MergeDistance = 0.01;

    // The first frame's side vector is Tangent x WorldUp; within this angle of vertical that cross product
    // degenerates, so WorldForward stands in for WorldUp.
    constexpr auto VerticalFallbackDegrees = 1.0;

    auto
        Get_MergedPoints(
            TConstArrayView<FVector> InWorldPoints)
        -> TArray<FVector>
    {
        auto Merged = TArray<FVector>{};
        Merged.Reserve(InWorldPoints.Num());

        for (const auto& Point : InWorldPoints)
        {
            if (Merged.Num() > 0 && FVector::Dist(Merged.Last(), Point) < MergeDistance)
            { continue; }

            Merged.Add(Point);
        }

        return Merged;
    }

    auto
        Get_CumulativeLengths(
            const TArray<FVector>& InPoints)
        -> TArray<double>
    {
        auto Lengths = TArray<double>{};
        Lengths.Reserve(InPoints.Num());

        auto Accumulated = 0.0;
        for (auto Index = 0; Index < InPoints.Num(); ++Index)
        {
            if (Index > 0)
            { Accumulated += FVector::Dist(InPoints[Index - 1], InPoints[Index]); }

            Lengths.Add(Accumulated);
        }

        return Lengths;
    }

    // Keeps the first and last point and spaces the interior evenly by arc length, so a long path keeps its end.
    auto
        Get_ResampledPoints(
            const TArray<FVector>& InPoints,
            int32 InNumPoints)
        -> TArray<FVector>
    {
        const auto Lengths = Get_CumulativeLengths(InPoints);
        const auto TotalLength = Lengths.Last();

        auto Resampled = TArray<FVector>{};
        Resampled.Reserve(InNumPoints);
        Resampled.Add(InPoints[0]);

        auto SegmentIndex = 0;
        for (auto SampleIndex = 1; SampleIndex < InNumPoints - 1; ++SampleIndex)
        {
            const auto Target = TotalLength * SampleIndex / (InNumPoints - 1);

            while (SegmentIndex < InPoints.Num() - 2 && Lengths[SegmentIndex + 1] < Target)
            { ++SegmentIndex; }

            const auto SegmentStart = Lengths[SegmentIndex];
            const auto SegmentLength = Lengths[SegmentIndex + 1] - SegmentStart;
            const auto Alpha = FMath::Clamp((Target - SegmentStart) / SegmentLength, 0.0, 1.0);

            Resampled.Add(FMath::Lerp(InPoints[SegmentIndex], InPoints[SegmentIndex + 1], Alpha));
        }

        Resampled.Add(InPoints.Last());
        return Resampled;
    }

    auto
        Get_PointTangents(
            const TArray<FVector>& InPoints)
        -> TArray<FVector>
    {
        const auto NumPoints = InPoints.Num();

        auto SegmentDirections = TArray<FVector>{};
        SegmentDirections.Reserve(NumPoints - 1);
        for (auto Index = 0; Index < NumPoints - 1; ++Index)
        {
            SegmentDirections.Add((InPoints[Index + 1] - InPoints[Index]).GetSafeNormal());
        }

        auto Tangents = TArray<FVector>{};
        Tangents.Reserve(NumPoints);
        for (auto Index = 0; Index < NumPoints; ++Index)
        {
            if (Index == 0)
            {
                Tangents.Add(SegmentDirections[0]);
                continue;
            }

            if (Index == NumPoints - 1)
            {
                Tangents.Add(SegmentDirections.Last());
                continue;
            }

            const auto Sum = SegmentDirections[Index - 1] + SegmentDirections[Index];

            // A path that doubles back on itself has no bisector; the outgoing direction keeps the ring well formed.
            if (Sum.IsNearlyZero())
            {
                Tangents.Add(SegmentDirections[Index]);
                continue;
            }

            Tangents.Add(Sum.GetSafeNormal());
        }

        return Tangents;
    }

    auto
        Get_InitialSide(
            const FVector& InTangent)
        -> FVector
    {
        const auto VerticalCosine = FMath::Cos(FMath::DegreesToRadians(VerticalFallbackDegrees));
        const auto IsNearlyVertical = FMath::Abs(FVector::DotProduct(InTangent, FVector::UpVector)) > VerticalCosine;

        const auto Reference = IsNearlyVertical ? FVector::ForwardVector : FVector::UpVector;
        return FVector::CrossProduct(InTangent, Reference).GetSafeNormal();
    }

    // Parallel transport: the side vector turns with the tangent and with nothing else, so it does not twist.
    auto
        Get_TransportedSide(
            const FVector& InPreviousSide,
            const FVector& InPreviousTangent,
            const FVector& InTangent)
        -> FVector
    {
        const auto Rotation = FQuat::FindBetweenNormals(InPreviousTangent, InTangent);
        const auto Rotated = Rotation.RotateVector(InPreviousSide);
        const auto Orthogonal = Rotated - FVector::DotProduct(Rotated, InTangent) * InTangent;

        if (Orthogonal.IsNearlyZero())
        { return Get_InitialSide(InTangent); }

        return Orthogonal.GetSafeNormal();
    }

    // The path cut InDistance along it: every point before the cut, then the cut point.
    auto
        Get_PointsUpTo(
            const TArray<FVector>& InPoints,
            const TArray<double>& InLengths,
            double InDistance)
        -> TArray<FVector>
    {
        auto Cut = TArray<FVector>{};
        Cut.Reserve(InPoints.Num());
        Cut.Add(InPoints[0]);

        for (auto Index = 1; Index < InPoints.Num(); ++Index)
        {
            if (InLengths[Index] < InDistance - MergeDistance)
            {
                Cut.Add(InPoints[Index]);
                continue;
            }

            const auto SegmentStart = InLengths[Index - 1];
            const auto SegmentLength = InLengths[Index] - SegmentStart;
            const auto Alpha = SegmentLength > 0.0 ? FMath::Clamp((InDistance - SegmentStart) / SegmentLength, 0.0, 1.0) : 1.0;
            const auto CutPoint = FMath::Lerp(InPoints[Index - 1], InPoints[Index], Alpha);

            if (FVector::Dist(Cut.Last(), CutPoint) >= MergeDistance)
            { Cut.Add(CutPoint); }

            break;
        }

        return Cut;
    }

    auto
        Get_IsRibbon(
            const ck::pmg::polyline::FShape& InShape)
        -> bool
    {
        return InShape._Profile == ECk_Pmg_Polyline_Profile::Ribbon;
    }

    auto
        Get_RingSize(
            const ck::pmg::polyline::FShape& InShape)
        -> int32
    {
        return Get_IsRibbon(InShape) ? 2 : FMath::Max(InShape._Sides, 0) + 1;
    }

    // Quads between two consecutive rings
    auto
        Get_FacesPerGap(
            const ck::pmg::polyline::FShape& InShape)
        -> int32
    {
        return Get_IsRibbon(InShape) ? 1 : FMath::Max(InShape._Sides, 0);
    }

    auto
        Get_HasArrow(
            const ck::pmg::polyline::FShape& InShape)
        -> bool
    {
        return InShape._ArrowLength > 0.0f;
    }

    // MaxPoints rings for the path, then the arrowhead's base ring and its tip ring
    auto
        Get_NumRings(
            const ck::pmg::polyline::FShape& InShape)
        -> int32
    {
        return FMath::Max(InShape._MaxPoints, 0) + (Get_HasArrow(InShape) ? 2 : 0);
    }

    struct FRing
    {
        FVector _Centre = FVector::ZeroVector;
        FVector _Tangent = FVector::ForwardVector;
        FVector _Side = FVector::RightVector;
        double _Distance = 0.0;
        float _Radius = 0.0f;
    };

    auto
        Write_Ring(
            ck::pmg::polyline::FTubeBuffers& InOutBuffers,
            const ck::pmg::polyline::FShape& InShape,
            int32 InRingIndex,
            const FRing& InRing,
            double InTotalLength)
        -> void
    {
        const auto RingSize = Get_RingSize(InShape);
        const auto DistanceFraction = InTotalLength > 0.0 ? static_cast<float>(InRing._Distance / InTotalLength) : 0.0f;
        const auto Binormal = FVector::CrossProduct(InRing._Tangent, InRing._Side);

        // A ribbon faces Side x Tangent: up for a level path, because the side vector is Tangent x WorldUp.
        const auto RibbonNormal = FVector::CrossProduct(InRing._Side, InRing._Tangent);

        for (auto Around = 0; Around < RingSize; ++Around)
        {
            const auto VertexIndex = InRingIndex * RingSize + Around;

            auto Offset = FVector::ZeroVector;
            auto Normal = FVector::ZeroVector;
            auto AroundFraction = 0.0;

            if (Get_IsRibbon(InShape))
            {
                Offset = Around == 0 ? InRing._Side : -InRing._Side;
                Normal = RibbonNormal;
                AroundFraction = static_cast<double>(Around);
            }
            else
            {
                AroundFraction = static_cast<double>(Around) / InShape._Sides;
                const auto Angle = UE_DOUBLE_TWO_PI * AroundFraction;
                Offset = FMath::Cos(Angle) * InRing._Side + FMath::Sin(Angle) * Binormal;
                Normal = Offset;
            }

            InOutBuffers._Positions[VertexIndex] = InRing._Centre + Offset * InRing._Radius;
            InOutBuffers._Normals[VertexIndex] = Normal;
            InOutBuffers._UV0[VertexIndex] = FVector2D{InRing._Distance, AroundFraction};
            InOutBuffers._Colors[VertexIndex] = FLinearColor{DistanceFraction, 0.0f, 0.0f, 1.0f};
            InOutBuffers._Tangents[VertexIndex] = FProcMeshTangent{InRing._Tangent, false};
        }
    }

    auto
        Copy_Ring(
            ck::pmg::polyline::FTubeBuffers& InOutBuffers,
            int32 InRingSize,
            int32 InSourceRing,
            int32 InTargetRing)
        -> void
    {
        for (auto Around = 0; Around < InRingSize; ++Around)
        {
            const auto Source = InSourceRing * InRingSize + Around;
            const auto Target = InTargetRing * InRingSize + Around;

            InOutBuffers._Positions[Target] = InOutBuffers._Positions[Source];
            InOutBuffers._Normals[Target] = InOutBuffers._Normals[Source];
            InOutBuffers._UV0[Target] = InOutBuffers._UV0[Source];
            InOutBuffers._Colors[Target] = InOutBuffers._Colors[Source];
            InOutBuffers._Tangents[Target] = InOutBuffers._Tangents[Source];
        }
    }

    auto
        Make_TubeShape(
            int32 InMaxPoints,
            int32 InSides,
            float InRadius)
        -> ck::pmg::polyline::FShape
    {
        auto Shape = ck::pmg::polyline::FShape{};
        Shape._MaxPoints = InMaxPoints;
        Shape._Sides = InSides;
        Shape._Radius = InRadius;
        return Shape;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::pmg::polyline
{
    auto
        Get_NumVertices(
            const FShape& InShape)
        -> int32
    {
        return ck_pmg_polyline_geometry::Get_NumRings(InShape) * ck_pmg_polyline_geometry::Get_RingSize(InShape);
    }

    auto
        Build_Triangles(
            const FShape& InShape)
        -> TArray<int32>
    {
        using namespace ck_pmg_polyline_geometry;

        auto Triangles = TArray<int32>{};
        const auto Faces = Get_FacesPerGap(InShape);
        if (InShape._MaxPoints < 2 || Faces < 1)
        { return Triangles; }

        const auto RingSize = Get_RingSize(InShape);
        const auto NumRings = Get_NumRings(InShape);
        const auto IsRibbon = Get_IsRibbon(InShape);
        Triangles.Reserve((NumRings - 1) * Faces * 6);

        // Unreal's front face is clockwise seen from the front: its normal is (C - A) x (B - A), the opposite of the
        // plain cross product (KismetProceduralMeshLibrary::GenerateBoxMesh winds its +Z face this way). A tube's
        // (A, B, ANext) faces outward; a ribbon's (A, ANext, B) faces Side x Tangent.
        for (auto Ring = 0; Ring < NumRings - 1; ++Ring)
        {
            for (auto Around = 0; Around < Faces; ++Around)
            {
                const auto A = Ring * RingSize + Around;
                const auto ANext = A + 1;
                const auto B = A + RingSize;
                const auto BNext = B + 1;

                if (IsRibbon)
                {
                    Triangles.Append({A, ANext, B});
                    Triangles.Append({ANext, BNext, B});
                    continue;
                }

                Triangles.Append({A, B, ANext});
                Triangles.Append({ANext, B, BNext});
            }
        }

        return Triangles;
    }

    auto
        Build(
            TConstArrayView<FVector> InWorldPoints,
            const FShape& InShape)
        -> FTubeBuffers
    {
        using namespace ck_pmg_polyline_geometry;

        auto Buffers = FTubeBuffers{};

        const auto NumVertices = Get_NumVertices(InShape);
        Buffers._Positions.SetNumZeroed(NumVertices);
        Buffers._Normals.SetNumZeroed(NumVertices);
        Buffers._UV0.SetNumZeroed(NumVertices);
        Buffers._Colors.Init(FLinearColor{0.0f, 0.0f, 0.0f, 1.0f}, NumVertices);
        Buffers._Tangents.SetNum(NumVertices);

        const auto MaxPoints = InShape._MaxPoints;
        if (MaxPoints < 2 || Get_FacesPerGap(InShape) < 1)
        { return Buffers; }

        auto Points = Get_MergedPoints(InWorldPoints);
        if (Points.Num() < 2)
        { return Buffers; }

        if (Points.Num() > MaxPoints)
        { Points = Get_ResampledPoints(Points, MaxPoints); }

        const auto Lengths = Get_CumulativeLengths(Points);
        const auto TotalLength = Lengths.Last();
        const auto Origin = Points[0];
        const auto RingSize = Get_RingSize(InShape);
        const auto HasArrow = Get_HasArrow(InShape);
        const auto HeadLength = HasArrow ? FMath::Min(static_cast<double>(InShape._ArrowLength), TotalLength) : 0.0;

        auto Shaft = HasArrow ? Get_PointsUpTo(Points, Lengths, TotalLength - HeadLength) : Points;
        if (Shaft.Num() > MaxPoints)
        { Shaft = Get_ResampledPoints(Shaft, MaxPoints); }

        const auto HeadDirection = HasArrow ? (Points.Last() - Shaft.Last()).GetSafeNormal() : FVector::ZeroVector;

        auto Side = FVector::ZeroVector;
        auto Tangent = FVector::ZeroVector;

        if (Shaft.Num() >= 2)
        {
            const auto ShaftLengths = Get_CumulativeLengths(Shaft);
            const auto Tangents = Get_PointTangents(Shaft);
            Side = Get_InitialSide(Tangents[0]);

            for (auto Ring = 0; Ring < Shaft.Num(); ++Ring)
            {
                if (Ring > 0)
                { Side = Get_TransportedSide(Side, Tangents[Ring - 1], Tangents[Ring]); }

                Write_Ring(Buffers, InShape, Ring,
                    FRing{Shaft[Ring] - Origin, Tangents[Ring], Side, ShaftLengths[Ring], InShape._Radius}, TotalLength);
            }

            Tangent = Tangents.Last();
        }
        else
        {
            // The whole path is the head: the shaft is a single ring at the head's base, enclosing nothing.
            Tangent = HeadDirection;
            Side = Get_InitialSide(Tangent);
            Write_Ring(Buffers, InShape, 0, FRing{Shaft[0] - Origin, Tangent, Side, 0.0, InShape._Radius}, TotalLength);
        }

        for (auto Ring = Shaft.Num(); Ring < MaxPoints; ++Ring)
        { Copy_Ring(Buffers, RingSize, Shaft.Num() - 1, Ring); }

        if (HasArrow)
        {
            const auto HeadSide = Get_TransportedSide(Side, Tangent, HeadDirection);
            const auto BaseDistance = TotalLength - HeadLength;

            Write_Ring(Buffers, InShape, MaxPoints,
                FRing{Shaft.Last() - Origin, HeadDirection, HeadSide, BaseDistance, InShape._ArrowRadius}, TotalLength);
            Write_Ring(Buffers, InShape, MaxPoints + 1,
                FRing{Points.Last() - Origin, HeadDirection, HeadSide, TotalLength, 0.0f}, TotalLength);
        }

        Buffers._NumDrawnPoints = Points.Num();
        Buffers._Length = TotalLength;

        return Buffers;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        Get_NumVertices(
            int32 InMaxPoints,
            int32 InSides)
        -> int32
    {
        return Get_NumVertices(ck_pmg_polyline_geometry::Make_TubeShape(InMaxPoints, InSides, 1.0f));
    }

    auto
        Build_Triangles(
            int32 InMaxPoints,
            int32 InSides)
        -> TArray<int32>
    {
        return Build_Triangles(ck_pmg_polyline_geometry::Make_TubeShape(InMaxPoints, InSides, 1.0f));
    }

    auto
        Build_Tube(
            TConstArrayView<FVector> InWorldPoints,
            float InRadius,
            int32 InSides,
            int32 InMaxPoints)
        -> FTubeBuffers
    {
        return Build(InWorldPoints, ck_pmg_polyline_geometry::Make_TubeShape(InMaxPoints, InSides, InRadius));
    }
}

// --------------------------------------------------------------------------------------------------------------------
