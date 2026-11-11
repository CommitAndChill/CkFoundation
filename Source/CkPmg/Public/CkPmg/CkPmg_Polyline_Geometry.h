#pragma once

#include <ProceduralMeshComponent.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::pmg::polyline
{
    struct CKPMG_API FTubeBuffers
    {
        TArray<FVector> _Positions;       // relative to the first input point
        TArray<FVector> _Normals;
        TArray<FVector2D> _UV0;           // X = distance along the path in cm, Y = 0..1 around the tube
        TArray<FLinearColor> _Colors;     // R = distance along the path / total length, G = B = 0, A = 1
        TArray<FProcMeshTangent> _Tangents;
        int32 _NumDrawnPoints = 0;        // points that survived (0 when nothing is drawn)
        double _Length = 0.0;
    };

    // Vertex count is always InMaxPoints * (InSides + 1); it never depends on the input.
    CKPMG_API auto Get_NumVertices(int32 InMaxPoints, int32 InSides) -> int32;

    // (InMaxPoints - 1) * InSides quads, outward-facing, fixed for the mesh's life.
    CKPMG_API auto Build_Triangles(int32 InMaxPoints, int32 InSides) -> TArray<int32>;

    // Consecutive points closer than 0.01 cm are merged. More than InMaxPoints points are resampled evenly by arc
    // length, keeping the first and last, so the end of the path is never cut off. Ring frames are parallel
    // transported, so a planar curve's tube does not twist. Rings past the last drawn point collapse onto it.
    CKPMG_API auto Build_Tube(
        TConstArrayView<FVector> InWorldPoints,
        float InRadius,
        int32 InSides,
        int32 InMaxPoints) -> FTubeBuffers;
}

// --------------------------------------------------------------------------------------------------------------------
