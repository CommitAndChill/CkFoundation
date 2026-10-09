#pragma once

#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"

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

    struct FCapOptions
    {
    public:
        CK_GENERATED_BODY(FCapOptions);

    private:
        int32 _MaterialID = 0;
        FVector4f _Color = FVector4f{1, 1, 1, 1};
        double _CmPerUVUnit = 10.0;
        FVector2f _UVOffset = FVector2f::Zero();

    public:
        CK_PROPERTY(_MaterialID);
        CK_PROPERTY(_Color);
        CK_PROPERTY(_CmPerUVUnit);
        CK_PROPERTY(_UVOffset);
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

    public:
        CK_PROPERTY(_MinimumOutputVolumeCm3);
        CK_PROPERTY(_MinimumNormalExtentCm);
        CK_PROPERTY(_PlaneToleranceCm);
        CK_PROPERTY(_AbsoluteVolumeToleranceCm3);
        CK_PROPERTY(_RelativeVolumeTolerance);
        CK_PROPERTY(_MaximumVertices);
        CK_PROPERTY(_MaximumTriangles);
    };

    struct FCutOptions
    {
    public:
        CK_GENERATED_BODY(FCutOptions);

    private:
        FPlaneFrameLocal _Plane;
        FCapOptions _Cap;
        FCutLimits _Limits;

    public:
        CK_PROPERTY(_Plane);
        CK_PROPERTY(_Cap);
        CK_PROPERTY(_Limits);
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

    class FCutResult;

    CKRUNTIMEMESH_API auto ValidateOptions(
        const FCutOptions& InOptions) -> bool;

    CKRUNTIMEMESH_API auto Cut(
        const geometry::FValidatedGeometry& InSource,
        const FCutOptions& InOptions) -> FCutResult;

    class CKRUNTIMEMESH_API FCutResult final
    {
    public:
        CK_GENERATED_BODY(FCutResult);

    private:
        friend CKRUNTIMEMESH_API auto Cut(
            const geometry::FValidatedGeometry&, const FCutOptions&) -> FCutResult;

        explicit FCutResult(ECutOutcome InOutcome);
        FCutResult(
            geometry::FValidatedGeometryPtr InPositive,
            geometry::FValidatedGeometryPtr InNegative,
            int32 InPositiveCapTriangles,
            int32 InNegativeCapTriangles);

        ECutOutcome _Outcome;
        geometry::FValidatedGeometryPtr _Positive;
        geometry::FValidatedGeometryPtr _Negative;
        int32 _PositiveCapTriangles = 0;
        int32 _NegativeCapTriangles = 0;

    public:
        CK_PROPERTY_GET(_Outcome);
        CK_PROPERTY_GET(_Positive);
        CK_PROPERTY_GET(_Negative);
        CK_PROPERTY_GET(_PositiveCapTriangles);
        CK_PROPERTY_GET(_NegativeCapTriangles);

        auto Get_IsSuccess() const -> bool;
    };
}
