#pragma once

#include "CkPmg_Fragment_Data_Polyline.h"

#include <ProceduralMeshComponent.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::pmg::polyline
{
    struct CKPMG_API FTubeBuffers
    {
        TArray<FVector> _Positions;       // relative to the first input point
        TArray<FVector> _Normals;
        TArray<FVector2D> _UV0;           // X = distance along the path in cm, Y = 0..1 around the tube / across the ribbon
        TArray<FLinearColor> _Colors;     // R = distance along the path / total length, G = B = 0, A = 1
        TArray<FProcMeshTangent> _Tangents;
        int32 _NumDrawnPoints = 0;        // points that survived (0 when nothing is drawn)
        double _Length = 0.0;
    };

    // Everything the mesh's topology and vertices depend on besides the points.
    struct CKPMG_API FShape
    {
        ECk_Pmg_Polyline_Profile _Profile = ECk_Pmg_Polyline_Profile::Tube;
        float _Radius = 1.0f;             // tube radius, or a ribbon's half width
        int32 _Sides = 6;                 // a tube's faces; a ribbon ignores it
        int32 _MaxPoints = 64;
        float _ArrowLength = 0.0f;        // 0: no arrowhead
        float _ArrowRadius = 0.0f;
    };

    // Vertex count is always rings * ring size: MaxPoints rings (two more for an arrowhead) of Sides + 1 vertices on a
    // tube, 2 on a ribbon. It never depends on the input.
    CKPMG_API auto Get_NumVertices(const FShape& InShape) -> int32;

    // One quad per face between every pair of consecutive rings, front faces as Unreal culls them (clockwise seen from
    // the front): a tube's face outward, a ribbon's face up when the path runs level. Fixed for the mesh's life.
    CKPMG_API auto Build_Triangles(const FShape& InShape) -> TArray<int32>;

    // Consecutive points closer than 0.01 cm are merged. More than MaxPoints points are resampled evenly by arc length,
    // keeping the first and last, so the end of the path is never cut off. Ring frames are parallel transported, so a
    // planar curve does not twist. Rings past the last drawn point collapse onto it. An arrowhead takes the path's last
    // _ArrowLength centimetres (all of a shorter path): the shaft stops at its base, and its last ring is the tip.
    CKPMG_API auto Build(
        TConstArrayView<FVector> InWorldPoints,
        const FShape& InShape) -> FTubeBuffers;

    // A tube with no arrowhead.
    CKPMG_API auto Get_NumVertices(int32 InMaxPoints, int32 InSides) -> int32;
    CKPMG_API auto Build_Triangles(int32 InMaxPoints, int32 InSides) -> TArray<int32>;
    CKPMG_API auto Build_Tube(
        TConstArrayView<FVector> InWorldPoints,
        float InRadius,
        int32 InSides,
        int32 InMaxPoints) -> FTubeBuffers;
}

// --------------------------------------------------------------------------------------------------------------------
