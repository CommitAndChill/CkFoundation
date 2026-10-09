#pragma once

#include "BoxTypes.h"
#include "CkCore/Macros/CkMacros.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Templates/SharedPointer.h"

class UStaticMesh;

namespace ck::runtimemesh::geometry
{
    // Local geometry uses centimetres. These provisional ceilings bound validation work;
    // shipping limits still require the specified benchmark.
    struct FImportOptions
    {
    public:
        CK_GENERATED_BODY(FImportOptions);

    private:
        int32 _LODIndex = 0;
        double _SeamToleranceCm = 0.0001;
        double _IntersectionToleranceCm = 0.0000001;
        double _MinimumTriangleAreaCm2 = 0.00000001;
        double _MinimumVolumeCm3 = 0.000001;
        int32 _MaximumVertices = 2048;
        int32 _MaximumTriangles = 4096;

    public:
        CK_PROPERTY(_LODIndex);
        CK_PROPERTY(_SeamToleranceCm);
        CK_PROPERTY(_IntersectionToleranceCm);
        CK_PROPERTY(_MinimumTriangleAreaCm2);
        CK_PROPERTY(_MinimumVolumeCm3);
        CK_PROPERTY(_MaximumVertices);
        CK_PROPERTY(_MaximumTriangles);
    };

    enum class EImportFailure : uint8
    {
        None,
        InvalidOptions,
        InvalidExecutionContext,
        MissingRenderData,
        InvalidLOD,
        CpuDataUnavailable,
        LimitExceeded,
        InvalidIndexOrSection,
        InvalidMaterial,
        InvalidAttribute,
        AmbiguousSeam,
        InvalidTopology,
        SelfIntersection,
        NonPositiveVolume,
        ConversionFailed
    };

    class FImportResult;

    CKRUNTIMEMESH_API auto ValidateOptions(
        const FImportOptions& InOptions) -> bool;

    // Admits an already constructed mesh; useful for native fixtures and later derived geometry.
    CKRUNTIMEMESH_API auto Admit(
        UE::Geometry::FDynamicMesh3&& InMesh,
        const FImportOptions& InOptions) -> FImportResult;

    CKRUNTIMEMESH_API auto Import(
        const UStaticMesh& InAsset,
        const FImportOptions& InOptions) -> FImportResult;

    class CKRUNTIMEMESH_API FValidatedGeometry final
    {
    public:
        CK_GENERATED_BODY(FValidatedGeometry);

    private:
        friend class FImportResult;
        friend CKRUNTIMEMESH_API auto Admit(UE::Geometry::FDynamicMesh3&&, const FImportOptions&) -> FImportResult;
        friend CKRUNTIMEMESH_API auto Import(const UStaticMesh&, const FImportOptions&) -> FImportResult;

        FValidatedGeometry(
            UE::Geometry::FDynamicMesh3&& InMesh,
            double InVolumeCm3,
            const FVector3d& InCentroidCm);

        UE::Geometry::FDynamicMesh3 _Mesh;
        double _VolumeCm3;
        FVector3d _CentroidCm;
        UE::Geometry::FAxisAlignedBox3d _BoundsCm;

    public:
        CK_PROPERTY_GET(_Mesh);
        CK_PROPERTY_GET(_VolumeCm3);
        CK_PROPERTY_GET(_CentroidCm);
        CK_PROPERTY_GET(_BoundsCm);
    };

    using FValidatedGeometryPtr = TSharedPtr<const FValidatedGeometry, ESPMode::ThreadSafe>;

    class CKRUNTIMEMESH_API FImportResult final
    {
    public:
        CK_GENERATED_BODY(FImportResult);

    private:
        friend CKRUNTIMEMESH_API auto Admit(UE::Geometry::FDynamicMesh3&&, const FImportOptions&) -> FImportResult;
        friend CKRUNTIMEMESH_API auto Import(const UStaticMesh&, const FImportOptions&) -> FImportResult;

        explicit FImportResult(EImportFailure InFailure);
        explicit FImportResult(FValidatedGeometryPtr InGeometry);

        EImportFailure _Failure;
        FValidatedGeometryPtr _Geometry;

    public:
        CK_PROPERTY_GET(_Failure);
        CK_PROPERTY_GET(_Geometry);

        auto Get_IsReady() const -> bool;
    };

}
