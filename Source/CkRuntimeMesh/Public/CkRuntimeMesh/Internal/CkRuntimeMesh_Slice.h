#pragma once

#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"
#include "CkCore/Format/CkFormat.h"

namespace ck::runtimemesh::slice
{
    struct FPlaneFrameLocal
    {
    public:
        CK_GENERATED_BODY(FPlaneFrameLocal);

    private:
        FVector3d _PositionCm = FVector3d::ZeroVector;
        FVector3d _Normal = FVector3d{0, 0, 1};
        FVector3d _Tangent = FVector3d{1, 0, 0};

    public:
        CK_PROPERTY(_PositionCm);
        CK_PROPERTY(_Normal);
        CK_PROPERTY(_Tangent);
    };

    // UV0 always carries the cap's planar projection; this decides what the layers after it carry.
    enum class ECapUVPolicy : uint8
    {
        PlanarOnAllLayers,
        PlanarOnLayer0_ZeroOthers,
        PlanarOnLayer0_KeepInterpolated
    };

    enum class EIslandPolicy : uint8
    {
        RejectMultiple,
        SplitIslands
    };

    struct FCapOptions
    {
    public:
        CK_GENERATED_BODY(FCapOptions);

    private:
        int32 _MaterialID = 0;
        FVector4f _Color = FVector4f{1, 1, 1, 1};
        double _CmPerUVUnit = 10.0;
        FVector2f _UVOffset = FVector2f::Zero();
        ECapUVPolicy _UVPolicy = ECapUVPolicy::PlanarOnAllLayers;

    public:
        CK_PROPERTY(_MaterialID);
        CK_PROPERTY(_Color);
        CK_PROPERTY(_CmPerUVUnit);
        CK_PROPERTY(_UVOffset);
        CK_PROPERTY(_UVPolicy);
    };

    // Provisional finite work ceilings, not measured frame-time guarantees.
    struct FCutLimits
    {
    public:
        CK_GENERATED_BODY(FCutLimits);

    private:
        double _MinimumOutputVolumeCm3 = 0.001;
        double _MinimumNormalExtentCm = 0.001;
        double _PlaneToleranceCm = 0.00001;
        double _AbsoluteVolumeToleranceCm3 = 0.01;
        double _RelativeVolumeTolerance = 0.0001;
        int32 _MaximumVertices = 2048;
        int32 _MaximumTriangles = 4096;
        int32 _MaximumPiecesPerSide = 8;

    public:
        CK_PROPERTY(_MinimumOutputVolumeCm3);
        CK_PROPERTY(_MinimumNormalExtentCm);
        CK_PROPERTY(_PlaneToleranceCm);
        CK_PROPERTY(_AbsoluteVolumeToleranceCm3);
        CK_PROPERTY(_RelativeVolumeTolerance);
        CK_PROPERTY(_MaximumVertices);
        CK_PROPERTY(_MaximumTriangles);
        CK_PROPERTY(_MaximumPiecesPerSide);
    };

    struct FCutOptions
    {
    public:
        CK_GENERATED_BODY(FCutOptions);

    private:
        FPlaneFrameLocal _Plane;
        FCapOptions _Cap;
        FCutLimits _Limits;
        EIslandPolicy _IslandPolicy = EIslandPolicy::RejectMultiple;

    public:
        CK_PROPERTY(_Plane);
        CK_PROPERTY(_Cap);
        CK_PROPERTY(_Limits);
        CK_PROPERTY(_IslandPolicy);
    };

    enum class ECutOutcome : uint8
    {
        Succeeded,
        NoIntersection,
        TouchingOnly,
        RejectedTooSmall,
        RejectedLimit,
        RejectedTopology,
        InvalidRequest,
        InternalFailure
    };

    // One connected, admitted solid on one side of the plane.
    struct FCutPiece
    {
    public:
        CK_GENERATED_BODY(FCutPiece);

    private:
        geometry::FValidatedGeometryPtr _Geometry;
        int32 _CapTriangles = 0;

    public:
        CK_PROPERTY_GET(_Geometry);
        CK_PROPERTY_GET(_CapTriangles);

    public:
        CK_DEFINE_CONSTRUCTORS(FCutPiece, _Geometry, _CapTriangles);
    };

    class FCutResult;

    CKRUNTIMEMESH_API auto ValidateOptions(
        const FCutOptions& InOptions) -> bool;

    CKRUNTIMEMESH_API auto Cut(
        const geometry::FValidatedGeometry& InSource,
        const FCutOptions& InOptions) -> FCutResult;

    // Each side's pieces are ordered by volume, largest first; Get_Positive/Get_Negative and their cap counts name
    // that first piece. Under EIslandPolicy::RejectMultiple a success has exactly one piece per side.
    class CKRUNTIMEMESH_API FCutResult final
    {
    public:
        CK_GENERATED_BODY(FCutResult);

    private:
        friend CKRUNTIMEMESH_API auto Cut(
            const geometry::FValidatedGeometry&, const FCutOptions&) -> FCutResult;

        explicit FCutResult(ECutOutcome InOutcome);
        FCutResult(
            TArray<FCutPiece> InPositivePieces,
            TArray<FCutPiece> InNegativePieces);

        ECutOutcome _Outcome;
        TArray<FCutPiece> _PositivePieces;
        TArray<FCutPiece> _NegativePieces;

    public:
        CK_PROPERTY_GET(_Outcome);
        CK_PROPERTY_GET(_PositivePieces);
        CK_PROPERTY_GET(_NegativePieces);

        auto Get_Positive() const -> geometry::FValidatedGeometryPtr;
        auto Get_Negative() const -> geometry::FValidatedGeometryPtr;
        auto Get_PositiveCapTriangles() const -> int32;
        auto Get_NegativeCapTriangles() const -> int32;
        auto Get_IsSuccess() const -> bool;
    };
}
