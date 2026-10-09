#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "Intersection/IntrTriangle3Triangle3.h"
#include "MeshQueries.h"
#include "Selections/MeshConnectedComponents.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"

namespace ck::runtimemesh::geometry
{
    using namespace UE::Geometry;

    namespace ck_runtime_mesh_geometry_private
    {
        auto
            IsFinite(
                const FVector3d& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
        }

        auto
            IsFinite(
                const FVector3f& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
        }

        auto
            IsFinite(
                const FVector2f& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y);
        }

        auto
            IsFinite(
                const FVector4f& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
                && FMath::IsFinite(InValue.Z) && FMath::IsFinite(InValue.W);
        }

        template <typename TOverlay>
        auto
            HasFiniteElements(
                const TOverlay* InOverlay)
            -> bool
        {
            if (InOverlay == nullptr)
            {
                return true;
            }
            for (const auto ElementID : InOverlay->ElementIndicesItr())
            {
                if (NOT IsFinite(InOverlay->GetElement(ElementID)))
                {
                    return false;
                }
            }
            return true;
        }

        auto
            AreOptionsValid(
                const FImportOptions& InOptions)
            -> bool
        {
            return InOptions.Get_LODIndex() >= 0
                && FMath::IsFinite(InOptions.Get_SeamToleranceCm())
                && InOptions.Get_SeamToleranceCm() > 0.0
                && InOptions.Get_SeamToleranceCm() <= 0.01
                && FMath::IsFinite(InOptions.Get_IntersectionToleranceCm())
                && InOptions.Get_IntersectionToleranceCm() > 0.0
                && InOptions.Get_IntersectionToleranceCm() <= 0.0001
                && InOptions.Get_IntersectionToleranceCm() <= InOptions.Get_SeamToleranceCm() * 0.1
                && FMath::IsFinite(InOptions.Get_MinimumTriangleAreaCm2())
                && InOptions.Get_MinimumTriangleAreaCm2() > 0.0
                && InOptions.Get_MinimumTriangleAreaCm2() <= 1.0
                && FMath::IsFinite(InOptions.Get_MinimumVolumeCm3())
                && InOptions.Get_MinimumVolumeCm3() > 0.0
                && InOptions.Get_MinimumVolumeCm3() <= 1000000.0
                && InOptions.Get_MaximumVertices() >= 4 && InOptions.Get_MaximumVertices() <= 2048
                && InOptions.Get_MaximumTriangles() >= 4 && InOptions.Get_MaximumTriangles() <= 4096;
        }

        auto
            PreflightLOD(
                const UStaticMesh& InAsset,
                const FStaticMeshLODResources& InLOD,
                const FImportOptions& InOptions)
            -> EImportFailure
        {
            const auto& Buffers = InLOD.VertexBuffers;
            if (NOT InAsset.bAllowCPUAccess
                || NOT InLOD.IndexBuffer.GetAllowCPUAccess()
                || NOT Buffers.PositionVertexBuffer.GetAllowCPUAccess()
                || NOT Buffers.StaticMeshVertexBuffer.GetAllowCPUAccess()
                || Buffers.PositionVertexBuffer.GetVertexData() == nullptr
                || InLOD.IndexBuffer.GetIndexDataSize() == 0)
            {
                return EImportFailure::CpuDataUnavailable;
            }

            const auto RawVertexCount = Buffers.PositionVertexBuffer.GetNumVertices();
            const auto IndexCount = InLOD.IndexBuffer.GetNumIndices();
            if (RawVertexCount > static_cast<uint32>(InOptions.Get_MaximumVertices())
                || IndexCount / 3 > InOptions.Get_MaximumTriangles())
            {
                return EImportFailure::LimitExceeded;
            }
            const auto VertexCount = static_cast<int32>(RawVertexCount);
            if (VertexCount < 4 || IndexCount < 12 || IndexCount % 3 != 0
                || Buffers.StaticMeshVertexBuffer.GetNumVertices() != static_cast<uint32>(VertexCount))
            {
                return EImportFailure::InvalidIndexOrSection;
            }
            if (Buffers.StaticMeshVertexBuffer.GetNumTexCoords() > 4
                || Buffers.StaticMeshVertexBuffer.GetTangentData() == nullptr
                || (Buffers.StaticMeshVertexBuffer.GetNumTexCoords() > 0
                    && Buffers.StaticMeshVertexBuffer.GetTexCoordData() == nullptr))
            {
                return EImportFailure::InvalidAttribute;
            }

            const auto ColorCount = Buffers.ColorVertexBuffer.GetNumVertices();
            if (ColorCount > 0
                && (NOT Buffers.ColorVertexBuffer.GetAllowCPUAccess()
                    || Buffers.ColorVertexBuffer.GetVertexData() == nullptr
                    || ColorCount != static_cast<uint32>(VertexCount)))
            {
                return EImportFailure::CpuDataUnavailable;
            }

            for (int32 VertexID = 0; VertexID < VertexCount; ++VertexID)
            {
                const auto Position = FVector3d{Buffers.PositionVertexBuffer.VertexPosition(VertexID)};
                const auto Normal = Buffers.StaticMeshVertexBuffer.VertexTangentZ(VertexID);
                if (NOT IsFinite(Position)
                    || FMath::Abs(Position.X) > 1000000.0
                    || FMath::Abs(Position.Y) > 1000000.0
                    || FMath::Abs(Position.Z) > 1000000.0
                    || NOT IsFinite(Buffers.StaticMeshVertexBuffer.VertexTangentX(VertexID))
                    || NOT IsFinite(Buffers.StaticMeshVertexBuffer.VertexTangentY(VertexID))
                    || NOT IsFinite(Normal)
                    || FVector3d{Normal}.SquaredLength() <= 0.00000001)
                {
                    return EImportFailure::InvalidAttribute;
                }
                for (uint32 Layer = 0; Layer < Buffers.StaticMeshVertexBuffer.GetNumTexCoords(); ++Layer)
                {
                    if (NOT IsFinite(Buffers.StaticMeshVertexBuffer.GetVertexUV(VertexID, Layer)))
                    {
                        return EImportFailure::InvalidAttribute;
                    }
                }
            }

            TArray<uint8> CoveredTriangles;
            CoveredTriangles.Init(0, IndexCount / 3);
            if (InLOD.Sections.IsEmpty())
            {
                return EImportFailure::InvalidIndexOrSection;
            }
            TSet<uint64> TriangleKeys;
            TMap<uint64, int8> EdgeOrientations;
            for (const auto& Section : InLOD.Sections)
            {
                if (Section.MaterialIndex < 0
                    || Section.MaterialIndex >= InAsset.GetStaticMaterials().Num())
                {
                    return EImportFailure::InvalidMaterial;
                }
                const auto SectionEnd = static_cast<uint64>(Section.FirstIndex)
                    + static_cast<uint64>(Section.NumTriangles) * 3;
                if (Section.NumTriangles == 0 || Section.FirstIndex % 3 != 0
                    || SectionEnd > static_cast<uint64>(IndexCount)
                    || Section.MinVertexIndex > Section.MaxVertexIndex
                    || Section.MaxVertexIndex >= static_cast<uint32>(VertexCount))
                {
                    return EImportFailure::InvalidIndexOrSection;
                }
                for (uint32 TriangleOffset = 0; TriangleOffset < Section.NumTriangles; ++TriangleOffset)
                {
                    const auto TriangleID = Section.FirstIndex / 3 + TriangleOffset;
                    if (CoveredTriangles[TriangleID] != 0)
                    {
                        return EImportFailure::InvalidIndexOrSection;
                    }
                    CoveredTriangles[TriangleID] = 1;
                    uint32 TriangleVertices[3];
                    for (int32 Corner = 0; Corner < 3; ++Corner)
                    {
                        const auto VertexID = InLOD.IndexBuffer.GetIndex(TriangleID * 3 + Corner);
                        if (VertexID < Section.MinVertexIndex || VertexID > Section.MaxVertexIndex)
                        {
                            return EImportFailure::InvalidIndexOrSection;
                        }
                        TriangleVertices[Corner] = VertexID;
                    }
                    if (TriangleVertices[0] == TriangleVertices[1]
                        || TriangleVertices[1] == TriangleVertices[2]
                        || TriangleVertices[2] == TriangleVertices[0])
                    {
                        return EImportFailure::InvalidTopology;
                    }
                    const auto A = FVector3d{Buffers.PositionVertexBuffer.VertexPosition(TriangleVertices[0])};
                    const auto B = FVector3d{Buffers.PositionVertexBuffer.VertexPosition(TriangleVertices[1])};
                    const auto C = FVector3d{Buffers.PositionVertexBuffer.VertexPosition(TriangleVertices[2])};
                    const auto AreaCm2 = (B - A).Cross(C - A).Length() * 0.5;
                    if (NOT FMath::IsFinite(AreaCm2) || AreaCm2 < InOptions.Get_MinimumTriangleAreaCm2())
                    {
                        return EImportFailure::InvalidTopology;
                    }
                    if (TriangleVertices[0] > TriangleVertices[1])
                    {
                        Swap(TriangleVertices[0], TriangleVertices[1]);
                    }
                    if (TriangleVertices[1] > TriangleVertices[2])
                    {
                        Swap(TriangleVertices[1], TriangleVertices[2]);
                    }
                    if (TriangleVertices[0] > TriangleVertices[1])
                    {
                        Swap(TriangleVertices[0], TriangleVertices[1]);
                    }
                    const auto Key = static_cast<uint64>(TriangleVertices[0])
                        + static_cast<uint64>(TriangleVertices[1]) * 2048
                        + static_cast<uint64>(TriangleVertices[2]) * 2048 * 2048;
                    if (TriangleKeys.Contains(Key))
                    {
                        return EImportFailure::InvalidTopology;
                    }
                    TriangleKeys.Add(Key);
                    for (int32 Corner = 0; Corner < 3; ++Corner)
                    {
                        const auto StartVertex = InLOD.IndexBuffer.GetIndex(TriangleID * 3 + Corner);
                        const auto EndVertex = InLOD.IndexBuffer.GetIndex(TriangleID * 3 + (Corner + 1) % 3);
                        const auto EdgeKey = static_cast<uint64>(FMath::Min(StartVertex, EndVertex))
                            + static_cast<uint64>(FMath::Max(StartVertex, EndVertex)) * 2048;
                        const auto Direction = static_cast<int8>(StartVertex < EndVertex ? 1 : -1);
                        if (auto* PreviousDirection = EdgeOrientations.Find(EdgeKey))
                        {
                            if (*PreviousDirection == 0 || *PreviousDirection == Direction)
                            {
                                return EImportFailure::InvalidTopology;
                            }
                            *PreviousDirection = 0;
                        }
                        else
                        {
                            EdgeOrientations.Add(EdgeKey, Direction);
                        }
                    }
                }
            }
            for (const uint8 IsCovered : CoveredTriangles)
            {
                if (IsCovered == 0)
                {
                    return EImportFailure::InvalidIndexOrSection;
                }
            }
            return EImportFailure::None;
        }

        auto
            AreEdgesCoincident(
                const FDynamicMesh3& InMesh,
                int32 InFirstEdge,
                int32 InSecondEdge,
                double InToleranceSquared)
            -> bool
        {
            FVector3d A, B, C, D;
            InMesh.GetEdgeV(InFirstEdge, A, B);
            InMesh.GetEdgeV(InSecondEdge, C, D);
            return (FVector3d::DistSquared(A, C) <= InToleranceSquared
                    && FVector3d::DistSquared(B, D) <= InToleranceSquared)
                || (FVector3d::DistSquared(A, D) <= InToleranceSquared
                    && FVector3d::DistSquared(B, C) <= InToleranceSquared);
        }

        auto
            ReconstructSeams(
                FDynamicMesh3& InMesh,
                const FImportOptions& InOptions)
            -> EImportFailure
        {
            TArray<int32> BoundaryEdges;
            for (const auto EdgeID : InMesh.BoundaryEdgeIndicesItr())
            {
                BoundaryEdges.Add(EdgeID);
            }
            const auto ToleranceSquared = FMath::Square(InOptions.Get_SeamToleranceCm());
            for (int32 First = 0; First < BoundaryEdges.Num(); ++First)
            {
                int32 MatchCount = 0;
                for (int32 Second = 0; Second < BoundaryEdges.Num(); ++Second)
                {
                    if (First != Second && AreEdgesCoincident(
                        InMesh, BoundaryEdges[First], BoundaryEdges[Second], ToleranceSquared))
                    {
                        ++MatchCount;
                    }
                }
                if (MatchCount > 1)
                {
                    return EImportFailure::AmbiguousSeam;
                }
            }

            FMergeCoincidentMeshEdges Merge(&InMesh);
            Merge.MergeVertexTolerance = InOptions.Get_SeamToleranceCm();
            Merge.OnlyUniquePairs = true;
            Merge.bWeldAttrsOnMergedEdges = false;
            if (NOT Merge.Apply())
            {
                return EImportFailure::InvalidTopology;
            }
            return InMesh.IsClosed() ? EImportFailure::None : EImportFailure::InvalidTopology;
        }

        auto
            PopulateOpaqueWhiteColors(
                FDynamicMesh3& InMesh)
            -> bool
        {
            auto* Attributes = InMesh.Attributes();
            Attributes->EnablePrimaryColors();
            auto* Colors = Attributes->PrimaryColors();
            TArray<int32> Elements;
            Elements.Init(FDynamicMesh3::InvalidID, InMesh.MaxVertexID());
            for (const auto VertexID : InMesh.VertexIndicesItr())
            {
                Elements[VertexID] = Colors->AppendElement(FVector4f{1.0f, 1.0f, 1.0f, 1.0f});
            }
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                const auto Triangle = InMesh.GetTriangle(TriangleID);
                if (Colors->SetTriangle(TriangleID, FIndex3i{
                    Elements[Triangle.A], Elements[Triangle.B], Elements[Triangle.C]}) != EMeshResult::Ok)
                {
                    return false;
                }
            }
            return true;
        }

        auto
            IsPointOnSegment(
                const FVector3d& InPoint,
                const FVector3d& InStart,
                const FVector3d& InEnd,
                double InToleranceSquared)
            -> bool
        {
            const auto Edge = InEnd - InStart;
            const auto LengthSquared = Edge.SquaredLength();
            const auto T = LengthSquared > 0.0
                ? FMath::Clamp((InPoint - InStart).Dot(Edge) / LengthSquared, 0.0, 1.0)
                : 0.0;
            return (InPoint - (InStart + Edge * T)).SquaredLength() <= InToleranceSquared;
        }

        auto
            IsExpectedSharedContact(
                const FDynamicMesh3& InMesh,
                int32 InFirstTriangle,
                int32 InSecondTriangle,
                const FVector3d& InPoint,
                double InToleranceSquared)
            -> bool
        {
            const auto First = InMesh.GetTriangle(InFirstTriangle);
            const auto Second = InMesh.GetTriangle(InSecondTriangle);
            int32 Shared[3];
            int32 SharedCount = 0;
            for (int32 FirstCorner = 0; FirstCorner < 3; ++FirstCorner)
            {
                for (int32 SecondCorner = 0; SecondCorner < 3; ++SecondCorner)
                {
                    if (First[FirstCorner] == Second[SecondCorner])
                    {
                        Shared[SharedCount++] = First[FirstCorner];
                    }
                }
            }
            if (SharedCount == 1)
            {
                return FVector3d::DistSquared(InPoint, InMesh.GetVertex(Shared[0])) <= InToleranceSquared;
            }
            if (SharedCount == 2)
            {
                return IsPointOnSegment(
                    InPoint, InMesh.GetVertex(Shared[0]), InMesh.GetVertex(Shared[1]), InToleranceSquared);
            }
            return false;
        }

        auto
            HasUnexpectedSelfIntersection(
                const FDynamicMesh3& InMesh,
                const FImportOptions& InOptions)
            -> bool
        {
            TArray<int32> Triangles;
            TArray<FAxisAlignedBox3d> Bounds;
            Triangles.Reserve(InMesh.TriangleCount());
            Bounds.Reserve(InMesh.TriangleCount());
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                Triangles.Add(TriangleID);
                Bounds.Add(InMesh.GetTriBounds(TriangleID));
            }
            const auto ToleranceSquared = FMath::Square(InOptions.Get_IntersectionToleranceCm());
            for (int32 FirstIndex = 0; FirstIndex < Triangles.Num(); ++FirstIndex)
            {
                for (int32 SecondIndex = FirstIndex + 1; SecondIndex < Triangles.Num(); ++SecondIndex)
                {
                    // Intersects excludes touching and zero-thickness coplanar bounds.
                    if (Bounds[FirstIndex].DistanceSquared(Bounds[SecondIndex]) > ToleranceSquared)
                    {
                        continue;
                    }
                    FVector3d A0, A1, A2, B0, B1, B2;
                    InMesh.GetTriVertices(Triangles[FirstIndex], A0, A1, A2);
                    InMesh.GetTriVertices(Triangles[SecondIndex], B0, B1, B2);
                    auto Intersection = FIntrTriangle3Triangle3d{
                        FTriangle3d{A0, A1, A2}, FTriangle3d{B0, B1, B2}};
                    Intersection.SetReportCoplanarIntersection(true);
                    Intersection.SetTolerance(InOptions.Get_IntersectionToleranceCm());
                    if (NOT Intersection.Find())
                    {
                        continue;
                    }
                    if (Intersection.Quantity <= 0)
                    {
                        return true;
                    }
                    for (int32 PointIndex = 0; PointIndex < Intersection.Quantity; ++PointIndex)
                    {
                        if (NOT IsExpectedSharedContact(
                            InMesh, Triangles[FirstIndex], Triangles[SecondIndex],
                            Intersection.Points[PointIndex], ToleranceSquared))
                        {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        auto
            ValidateMesh(
                const FDynamicMesh3& InMesh,
                const FImportOptions& InOptions)
            -> EImportFailure
        {
            if (InMesh.VertexCount() > InOptions.Get_MaximumVertices()
                || InMesh.TriangleCount() > InOptions.Get_MaximumTriangles()
                || InMesh.MaxVertexID() > InOptions.Get_MaximumVertices()
                || InMesh.MaxTriangleID() > InOptions.Get_MaximumTriangles())
            {
                return EImportFailure::LimitExceeded;
            }
            if (InMesh.VertexCount() < 4 || InMesh.TriangleCount() < 4)
            {
                return EImportFailure::InvalidTopology;
            }
            for (const auto VertexID : InMesh.VertexIndicesItr())
            {
                if (NOT InMesh.IsReferencedVertex(VertexID))
                {
                    return EImportFailure::InvalidTopology;
                }
                const auto Position = InMesh.GetVertex(VertexID);
                if (NOT IsFinite(Position)
                    || FMath::Abs(Position.X) > 1000000.0
                    || FMath::Abs(Position.Y) > 1000000.0
                    || FMath::Abs(Position.Z) > 1000000.0)
                {
                    return EImportFailure::InvalidAttribute;
                }
            }
            if (NOT InMesh.HasAttributes())
            {
                return EImportFailure::InvalidAttribute;
            }
            const auto* RequiredAttributes = InMesh.Attributes();
            if (RequiredAttributes->NumUVLayers() > 4
                || RequiredAttributes->PrimaryNormals() == nullptr
                || RequiredAttributes->PrimaryColors() == nullptr
                || NOT RequiredAttributes->HasMaterialID())
            {
                return EImportFailure::InvalidAttribute;
            }
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                if (NOT RequiredAttributes->PrimaryNormals()->IsSetTriangle(TriangleID)
                    || NOT RequiredAttributes->PrimaryColors()->IsSetTriangle(TriangleID))
                {
                    return EImportFailure::InvalidAttribute;
                }
                for (int32 Layer = 0; Layer < RequiredAttributes->NumUVLayers(); ++Layer)
                {
                    if (NOT RequiredAttributes->GetUVLayer(Layer)->IsSetTriangle(TriangleID))
                    {
                        return EImportFailure::InvalidAttribute;
                    }
                }
            }
            if (NOT InMesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly))
            {
                return EImportFailure::InvalidTopology;
            }
            for (const auto VertexID : InMesh.VertexIndicesItr())
            {
                if (InMesh.IsBowtieVertex(VertexID))
                {
                    return EImportFailure::InvalidTopology;
                }
            }
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                if (NOT FMath::IsFinite(InMesh.GetTriArea(TriangleID))
                    || InMesh.GetTriArea(TriangleID) < InOptions.Get_MinimumTriangleAreaCm2())
                {
                    return EImportFailure::InvalidTopology;
                }
            }
            const auto* Attributes = InMesh.Attributes();
            if (NOT HasFiniteElements(Attributes->PrimaryNormals())
                || NOT HasFiniteElements(Attributes->PrimaryColors()))
            {
                return EImportFailure::InvalidAttribute;
            }
            for (const auto NormalID : Attributes->PrimaryNormals()->ElementIndicesItr())
            {
                if (FVector3d{Attributes->PrimaryNormals()->GetElement(NormalID)}.SquaredLength()
                    <= 0.00000001)
                {
                    return EImportFailure::InvalidAttribute;
                }
            }
            for (int32 Layer = 0; Layer < Attributes->NumUVLayers(); ++Layer)
            {
                if (NOT HasFiniteElements(Attributes->GetUVLayer(Layer)))
                {
                    return EImportFailure::InvalidAttribute;
                }
            }
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                if (Attributes->GetMaterialID()->GetValue(TriangleID) < 0)
                {
                    return EImportFailure::InvalidAttribute;
                }
            }
            if (NOT InMesh.IsClosed())
            {
                return EImportFailure::InvalidTopology;
            }
            FMeshConnectedComponents Components(&InMesh);
            Components.FindConnectedTriangles();
            if (Components.Num() != 1)
            {
                return EImportFailure::InvalidTopology;
            }
            if (HasUnexpectedSelfIntersection(InMesh, InOptions))
            {
                return EImportFailure::SelfIntersection;
            }
            FVector3d Centroid;
            const auto VolumeArea = TMeshQueries<FDynamicMesh3>::GetVolumeAreaCenter(InMesh, Centroid);
            if (NOT FMath::IsFinite(VolumeArea.X) || VolumeArea.X < InOptions.Get_MinimumVolumeCm3()
                || NOT IsFinite(Centroid))
            {
                return EImportFailure::NonPositiveVolume;
            }
            return EImportFailure::None;
        }
    }

    using namespace ck_runtime_mesh_geometry_private;

    auto
        ValidateOptions(
            const FImportOptions& InOptions)
        -> bool
    {
        return AreOptionsValid(InOptions);
    }

    FValidatedGeometry::
        FValidatedGeometry(
            FDynamicMesh3&& InMesh,
            double InVolumeCm3,
            const FVector3d& InCentroidCm)
        : _Mesh(MoveTemp(InMesh))
        , _VolumeCm3(InVolumeCm3)
        , _CentroidCm(InCentroidCm)
        , _BoundsCm(_Mesh.GetBounds())
    {
    }

    FImportResult::
        FImportResult(
            EImportFailure InFailure)
        : _Failure(InFailure)
    {
    }

    FImportResult::
        FImportResult(
            FValidatedGeometryPtr InGeometry)
        : _Failure(EImportFailure::None)
        , _Geometry(MoveTemp(InGeometry))
    {
    }

    auto
        FImportResult::
        Get_IsReady() const
        -> bool
    {
        return Get_Failure() == EImportFailure::None && Get_Geometry().IsValid();
    }

    auto
        Admit(
            FDynamicMesh3&& InMesh,
            const FImportOptions& InOptions)
        -> FImportResult
    {
        if (NOT AreOptionsValid(InOptions))
        {
            return FImportResult(EImportFailure::InvalidOptions);
        }
        const auto Failure = ValidateMesh(InMesh, InOptions);
        if (Failure != EImportFailure::None)
        {
            return FImportResult(Failure);
        }
        FVector3d Centroid;
        const auto VolumeCm3 = TMeshQueries<FDynamicMesh3>::GetVolumeAreaCenter(InMesh, Centroid).X;
        return FImportResult(FValidatedGeometryPtr(
            new FValidatedGeometry(MoveTemp(InMesh), VolumeCm3, Centroid)));
    }

    auto
        Import(
            const UStaticMesh& InAsset,
            const FImportOptions& InOptions)
        -> FImportResult
    {
        if (NOT IsInGameThread())
        {
            return FImportResult(EImportFailure::InvalidExecutionContext);
        }
        if (NOT AreOptionsValid(InOptions))
        {
            return FImportResult(EImportFailure::InvalidOptions);
        }
        const auto* RenderData = InAsset.GetRenderData();
        if (RenderData == nullptr)
        {
            return FImportResult(EImportFailure::MissingRenderData);
        }
        if (NOT RenderData->LODResources.IsValidIndex(InOptions.Get_LODIndex()))
        {
            return FImportResult(EImportFailure::InvalidLOD);
        }
        // Stream-out advances the first resident LOD on the game thread before releasing its CPU buffers.
        if (InOptions.Get_LODIndex() < RenderData->CurrentFirstLODIdx)
        {
            return FImportResult(EImportFailure::CpuDataUnavailable);
        }
        const auto LOD = TRefCountPtr<const FStaticMeshLODResources>{
            &RenderData->LODResources[InOptions.Get_LODIndex()]};
        const auto PreflightFailure = PreflightLOD(InAsset, *LOD, InOptions);
        if (PreflightFailure != EImportFailure::None)
        {
            return FImportResult(PreflightFailure);
        }

        FDynamicMesh3 Mesh;
        FStaticMeshLODResourcesToDynamicMesh::ConversionOptions Conversion;
        Conversion.BuildScale = FVector3d::One();
        Conversion.bWantTangents = false;
        const auto HasColors = LOD->VertexBuffers.ColorVertexBuffer.GetNumVertices() != 0;
        if (NOT FStaticMeshLODResourcesToDynamicMesh::Convert(
                LOD.GetReference(), Conversion, Mesh, HasColors,
                [&LOD](int32 InVertexID) -> FColor
                {
                    return LOD->VertexBuffers.ColorVertexBuffer.VertexColor(InVertexID);
                })
            || Mesh.TriangleCount() != LOD->IndexBuffer.GetNumIndices() / 3
            || Mesh.VertexCount() != static_cast<int32>(LOD->VertexBuffers.PositionVertexBuffer.GetNumVertices()))
        {
            return FImportResult(EImportFailure::ConversionFailed);
        }
        if (LOD->VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() == 0)
        {
            Mesh.Attributes()->SetNumUVLayers(0);
        }
        if (NOT HasColors && NOT PopulateOpaqueWhiteColors(Mesh))
        {
            return FImportResult(EImportFailure::ConversionFailed);
        }
        const auto SeamFailure = ReconstructSeams(Mesh, InOptions);
        if (SeamFailure != EImportFailure::None)
        {
            return FImportResult(SeamFailure);
        }
        const auto* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
        const auto SourceVertexCount = LOD->VertexBuffers.PositionVertexBuffer.GetNumVertices();
        for (uint32 SourceVertexID = 0; SourceVertexID < SourceVertexCount; ++SourceVertexID)
        {
            if (NormalOverlay == nullptr || NOT NormalOverlay->IsElement(SourceVertexID))
            {
                return FImportResult(EImportFailure::ConversionFailed);
            }
            const auto ParentVertexID = NormalOverlay->GetParentVertex(SourceVertexID);
            if (NOT Mesh.IsVertex(ParentVertexID)
                || FVector3d::DistSquared(
                    FVector3d{LOD->VertexBuffers.PositionVertexBuffer.VertexPosition(SourceVertexID)},
                    Mesh.GetVertex(ParentVertexID)) > FMath::Square(InOptions.Get_SeamToleranceCm()))
            {
                return FImportResult(EImportFailure::AmbiguousSeam);
            }
        }
        return Admit(MoveTemp(Mesh), InOptions);
    }
}
