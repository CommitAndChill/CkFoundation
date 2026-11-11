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
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::pmg::polyline
{
    auto
        Get_NumVertices(
            int32 InMaxPoints,
            int32 InSides)
        -> int32
    {
        return FMath::Max(InMaxPoints, 0) * (FMath::Max(InSides, 0) + 1);
    }

    auto
        Build_Triangles(
            int32 InMaxPoints,
            int32 InSides)
        -> TArray<int32>
    {
        auto Triangles = TArray<int32>{};
        if (InMaxPoints < 2 || InSides < 1)
        { return Triangles; }

        const auto RingSize = InSides + 1;
        Triangles.Reserve((InMaxPoints - 1) * InSides * 6);

        for (auto Ring = 0; Ring < InMaxPoints - 1; ++Ring)
        {
            for (auto Around = 0; Around < InSides; ++Around)
            {
                const auto A = Ring * RingSize + Around;
                const auto ANext = A + 1;
                const auto B = A + RingSize;
                const auto BNext = B + 1;

                Triangles.Add(A);
                Triangles.Add(ANext);
                Triangles.Add(B);

                Triangles.Add(ANext);
                Triangles.Add(BNext);
                Triangles.Add(B);
            }
        }

        return Triangles;
    }

    auto
        Build_Tube(
            TConstArrayView<FVector> InWorldPoints,
            float InRadius,
            int32 InSides,
            int32 InMaxPoints)
        -> FTubeBuffers
    {
        auto Buffers = FTubeBuffers{};

        const auto NumVertices = Get_NumVertices(InMaxPoints, InSides);
        Buffers._Positions.SetNumZeroed(NumVertices);
        Buffers._Normals.SetNumZeroed(NumVertices);
        Buffers._UV0.SetNumZeroed(NumVertices);
        Buffers._Colors.Init(FLinearColor{0.0f, 0.0f, 0.0f, 1.0f}, NumVertices);
        Buffers._Tangents.SetNum(NumVertices);

        if (InMaxPoints < 2 || InSides < 1)
        { return Buffers; }

        auto Points = ck_pmg_polyline_geometry::Get_MergedPoints(InWorldPoints);
        if (Points.Num() < 2)
        { return Buffers; }

        if (Points.Num() > InMaxPoints)
        { Points = ck_pmg_polyline_geometry::Get_ResampledPoints(Points, InMaxPoints); }

        const auto NumPoints = Points.Num();
        const auto Lengths = ck_pmg_polyline_geometry::Get_CumulativeLengths(Points);
        const auto TotalLength = Lengths.Last();
        const auto Tangents = ck_pmg_polyline_geometry::Get_PointTangents(Points);
        const auto RingSize = InSides + 1;
        const auto Origin = Points[0];

        auto Side = ck_pmg_polyline_geometry::Get_InitialSide(Tangents[0]);

        for (auto Ring = 0; Ring < NumPoints; ++Ring)
        {
            const auto& Tangent = Tangents[Ring];

            if (Ring > 0)
            { Side = ck_pmg_polyline_geometry::Get_TransportedSide(Side, Tangents[Ring - 1], Tangent); }

            const auto Binormal = FVector::CrossProduct(Tangent, Side);
            const auto Centre = Points[Ring] - Origin;
            const auto Distance = Lengths[Ring];
            const auto DistanceFraction = static_cast<float>(Distance / TotalLength);

            for (auto Around = 0; Around < RingSize; ++Around)
            {
                const auto AroundFraction = static_cast<double>(Around) / InSides;
                const auto Angle = UE_DOUBLE_TWO_PI * AroundFraction;
                const auto Offset = FMath::Cos(Angle) * Side + FMath::Sin(Angle) * Binormal;
                const auto VertexIndex = Ring * RingSize + Around;

                Buffers._Positions[VertexIndex] = Centre + Offset * InRadius;
                Buffers._Normals[VertexIndex] = Offset;
                Buffers._UV0[VertexIndex] = FVector2D{Distance, AroundFraction};
                Buffers._Colors[VertexIndex] = FLinearColor{DistanceFraction, 0.0f, 0.0f, 1.0f};
                Buffers._Tangents[VertexIndex] = FProcMeshTangent{Tangent, false};
            }
        }

        const auto LastRingStart = (NumPoints - 1) * RingSize;
        for (auto Ring = NumPoints; Ring < InMaxPoints; ++Ring)
        {
            for (auto Around = 0; Around < RingSize; ++Around)
            {
                const auto Source = LastRingStart + Around;
                const auto Target = Ring * RingSize + Around;

                Buffers._Positions[Target] = Buffers._Positions[Source];
                Buffers._Normals[Target] = Buffers._Normals[Source];
                Buffers._UV0[Target] = Buffers._UV0[Source];
                Buffers._Colors[Target] = Buffers._Colors[Source];
                Buffers._Tangents[Target] = Buffers._Tangents[Source];
            }
        }

        Buffers._NumDrawnPoints = NumPoints;
        Buffers._Length = TotalLength;

        return Buffers;
    }
}

// --------------------------------------------------------------------------------------------------------------------
