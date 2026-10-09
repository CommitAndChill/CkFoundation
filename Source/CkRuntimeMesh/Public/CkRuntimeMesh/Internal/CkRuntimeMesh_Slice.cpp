#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Slice.h"

#include "ConstrainedDelaunay2.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"
#include "FrameTypes.h"
#include "Operations/MeshPlaneCut.h"

namespace ck::runtimemesh::slice
{
    namespace ck_runtime_mesh_slice
    {
        using namespace UE::Geometry;

        auto
            IsFinite(const FVector3d& InValue) -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
                && FMath::IsFinite(InValue.Z);
        }

        auto
            IsFinite(const FVector4f& InValue) -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
                && FMath::IsFinite(InValue.Z) && FMath::IsFinite(InValue.W);
        }

        auto
            AreOptionsValid(const FCutOptions& InOptions) -> bool
        {
            const auto& Plane = InOptions.Get_Plane();
            const auto& Cap = InOptions.Get_Cap();
            const auto& Limits = InOptions.Get_Limits();
            const auto& Position = Plane.Get_PositionCm();
            const auto& Normal = Plane.Get_Normal();
            const auto& Tangent = Plane.Get_Tangent();
            const auto& Color = Cap.Get_Color();
            const auto& Offset = Cap.Get_UVOffset();
            return IsFinite(Position) && FMath::Abs(Position.X) <= 1000000.0
                && FMath::Abs(Position.Y) <= 1000000.0 && FMath::Abs(Position.Z) <= 1000000.0
                && IsFinite(Normal) && Normal.SquaredLength() >= 0.000001
                && Normal.SquaredLength() <= 1000000.0
                && IsFinite(Tangent) && Tangent.SquaredLength() >= 0.000001
                && Tangent.SquaredLength() <= 1000000.0
                && IsFinite(Color) && Color.X >= 0 && Color.X <= 1
                && Color.Y >= 0 && Color.Y <= 1 && Color.Z >= 0 && Color.Z <= 1
                && Color.W >= 0 && Color.W <= 1
                && FMath::IsFinite(Offset.X) && FMath::IsFinite(Offset.Y)
                && FMath::Abs(Offset.X) <= 1000000.0 && FMath::Abs(Offset.Y) <= 1000000.0
                && Cap.Get_MaterialID() >= 0
                && FMath::IsFinite(Cap.Get_CmPerUVUnit())
                && Cap.Get_CmPerUVUnit() >= 0.001 && Cap.Get_CmPerUVUnit() <= 1000000.0
                && FMath::IsFinite(Limits.Get_MinimumOutputVolumeCm3())
                && Limits.Get_MinimumOutputVolumeCm3() > 0
                && Limits.Get_MinimumOutputVolumeCm3() <= 1000000.0
                && FMath::IsFinite(Limits.Get_MinimumNormalExtentCm())
                && Limits.Get_MinimumNormalExtentCm() > 0
                && Limits.Get_MinimumNormalExtentCm() <= 10000.0
                && FMath::IsFinite(Limits.Get_PlaneToleranceCm())
                && Limits.Get_PlaneToleranceCm() > 0
                && Limits.Get_PlaneToleranceCm() <= 0.001
                && FMath::IsFinite(Limits.Get_AbsoluteVolumeToleranceCm3())
                && Limits.Get_AbsoluteVolumeToleranceCm3() > 0
                && Limits.Get_AbsoluteVolumeToleranceCm3() <= 1.0
                && FMath::IsFinite(Limits.Get_RelativeVolumeTolerance())
                && Limits.Get_RelativeVolumeTolerance() >= 0
                && Limits.Get_RelativeVolumeTolerance() <= 0.001
                && Limits.Get_MaximumVertices() >= 4 && Limits.Get_MaximumVertices() <= 2048
                && Limits.Get_MaximumTriangles() >= 4 && Limits.Get_MaximumTriangles() <= 4096;
        }

        auto
            AddZeroExteriorUVs(FDynamicMesh3& InMesh) -> void
        {
            auto* Attributes = InMesh.Attributes();
            if (Attributes->NumUVLayers() != 0)
            {
                return;
            }
            Attributes->SetNumUVLayers(1);
            auto* UVs = Attributes->GetUVLayer(0);
            for (const auto TriangleID : InMesh.TriangleIndicesItr())
            {
                const auto A = UVs->AppendElement(FVector2f::Zero());
                const auto B = UVs->AppendElement(FVector2f::Zero());
                const auto C = UVs->AppendElement(FVector2f::Zero());
                UVs->SetTriangle(TriangleID, FIndex3i{A, B, C});
            }
        }

        auto
            SetCapColors(FDynamicMesh3& InMesh, const TArray<int32>& InCapTriangles,
                const FVector4f& InColor) -> bool
        {
            auto* Colors = InMesh.Attributes()->PrimaryColors();
            if (Colors == nullptr)
            {
                return false;
            }
            for (const auto TriangleID : InCapTriangles)
            {
                const auto A = Colors->AppendElement(InColor);
                const auto B = Colors->AppendElement(InColor);
                const auto C = Colors->AppendElement(InColor);
                if (Colors->SetTriangle(TriangleID, FIndex3i{A, B, C}) != EMeshResult::Ok)
                {
                    return false;
                }
            }
            return true;
        }

        auto
            CutOneSide(FDynamicMesh3& InMesh, const FCutOptions& InOptions,
                const FFrame3d& InUVFrame, const FVector3d& InCutNormal,
                int32& OutCapTriangles) -> bool
        {
            const auto& Cap = InOptions.Get_Cap();
            FMeshPlaneCut CutOperator(&InMesh, InOptions.Get_Plane().Get_PositionCm(), InCutNormal);
            CutOperator.PlaneTolerance = InOptions.Get_Limits().Get_PlaneToleranceCm();
            CutOperator.bCollapseDegenerateEdgesOnCut = false;
            if (NOT CutOperator.Cut() || CutOperator.OpenBoundaries.Num() != 1
                || CutOperator.OpenBoundaries[0].CutLoopsFailed
                || CutOperator.OpenBoundaries[0].FoundOpenSpans
                || CutOperator.OpenBoundaries[0].CutLoops.IsEmpty())
            {
                return false;
            }
            if (NOT CutOperator.HoleFill(ConstrainedDelaunayTriangulate<double>, false,
                -1, Cap.Get_MaterialID()))
            {
                return false;
            }
            const auto& CapTriangles = CutOperator.HoleFillTriangles[0];
            OutCapTriangles = CapTriangles.Num();
            if (OutCapTriangles <= 0 || NOT SetCapColors(InMesh, CapTriangles, Cap.Get_Color()))
            {
                return false;
            }
            FDynamicMeshEditor Editor(&InMesh);
            const auto UVScale = static_cast<float>(1.0 / Cap.Get_CmPerUVUnit());
            for (int32 Layer = 0; Layer < InMesh.Attributes()->NumUVLayers(); ++Layer)
            {
                Editor.SetTriangleUVsFromProjection(CapTriangles, InUVFrame,
                    UVScale, Cap.Get_UVOffset(), false, Layer);
            }
            InMesh.RemoveUnusedVertices();
            InMesh.CompactInPlace();
            return true;
        }

        auto
            FitsLimits(const FDynamicMesh3& InMesh, const FCutLimits& InLimits) -> bool
        {
            return InMesh.VertexCount() <= InLimits.Get_MaximumVertices()
                && InMesh.TriangleCount() <= InLimits.Get_MaximumTriangles()
                && InMesh.MaxVertexID() <= InLimits.Get_MaximumVertices()
                && InMesh.MaxTriangleID() <= InLimits.Get_MaximumTriangles();
        }

        auto
            HasValidSideAndExtent(const FDynamicMesh3& InMesh, const FVector3d& InPlanePosition,
                const FVector3d& InPlaneNormal, const FCutLimits& InLimits,
                bool InPositiveSide) -> bool
        {
            double Minimum = TNumericLimits<double>::Max();
            double Maximum = -TNumericLimits<double>::Max();
            for (const auto VertexID : InMesh.VertexIndicesItr())
            {
                const auto Distance = (InMesh.GetVertex(VertexID) - InPlanePosition).Dot(InPlaneNormal);
                Minimum = FMath::Min(Minimum, Distance);
                Maximum = FMath::Max(Maximum, Distance);
            }
            return FMath::IsFinite(Minimum) && FMath::IsFinite(Maximum)
                && Maximum - Minimum >= InLimits.Get_MinimumNormalExtentCm()
                && (InPositiveSide ? Minimum >= -InLimits.Get_PlaneToleranceCm()
                    : Maximum <= InLimits.Get_PlaneToleranceCm());
        }
    }

    auto
        ValidateOptions(
            const FCutOptions& InOptions)
        -> bool
    {
        if (NOT ck_runtime_mesh_slice::AreOptionsValid(InOptions))
        { return false; }

        const auto& Plane = InOptions.Get_Plane();
        const auto Normal = Plane.Get_Normal().GetSafeNormal();
        const auto TangentCandidate = Plane.Get_Tangent()
            - Normal * Plane.Get_Tangent().Dot(Normal);
        return TangentCandidate.SquaredLength() >= 0.000001;
    }

    FCutResult::
        FCutResult(ECutOutcome InOutcome)
        : _Outcome(InOutcome)
    {
    }

    FCutResult::
        FCutResult(geometry::FValidatedGeometryPtr InPositive,
            geometry::FValidatedGeometryPtr InNegative,
            int32 InPositiveCapTriangles, int32 InNegativeCapTriangles)
        : _Outcome(ECutOutcome::Succeeded), _Positive(MoveTemp(InPositive)),
          _Negative(MoveTemp(InNegative)), _PositiveCapTriangles(InPositiveCapTriangles),
          _NegativeCapTriangles(InNegativeCapTriangles)
    {
    }

    auto
        FCutResult::
        Get_IsSuccess() const -> bool
    {
        return _Outcome == ECutOutcome::Succeeded
            && _Positive.IsValid() && _Negative.IsValid();
    }

    auto
        Cut(const geometry::FValidatedGeometry& InSource,
            const FCutOptions& InOptions) -> FCutResult
    {
        using namespace ck_runtime_mesh_slice;
        if (NOT ValidateOptions(InOptions))
        {
            return FCutResult(ECutOutcome::InvalidRequest);
        }
        const auto& Plane = InOptions.Get_Plane();
        const auto& Limits = InOptions.Get_Limits();
        const auto Normal = Plane.Get_Normal().GetSafeNormal();
        const auto TangentCandidate = Plane.Get_Tangent()
            - Normal * Plane.Get_Tangent().Dot(Normal);
        if (TangentCandidate.SquaredLength() < 0.000001)
        {
            return FCutResult(ECutOutcome::InvalidRequest);
        }
        const auto Tangent = TangentCandidate.GetSafeNormal();
        const auto Bitangent = Normal.Cross(Tangent);
        const auto UVFrame = FFrame3d{Plane.Get_PositionCm(), Tangent, Bitangent, Normal};
        double Minimum = TNumericLimits<double>::Max();
        double Maximum = -TNumericLimits<double>::Max();
        for (const auto VertexID : InSource.Get_Mesh().VertexIndicesItr())
        {
            const auto Distance = (InSource.Get_Mesh().GetVertex(VertexID)
                - Plane.Get_PositionCm()).Dot(Normal);
            Minimum = FMath::Min(Minimum, Distance);
            Maximum = FMath::Max(Maximum, Distance);
        }
        const auto Epsilon = Limits.Get_PlaneToleranceCm();
        if (Minimum > Epsilon || Maximum < -Epsilon)
        {
            return FCutResult(ECutOutcome::NoIntersection);
        }
        if (Minimum >= -Epsilon || Maximum <= Epsilon)
        {
            return FCutResult(ECutOutcome::TouchingOnly);
        }
        if (NOT FitsLimits(InSource.Get_Mesh(), Limits))
        {
            return FCutResult(ECutOutcome::RejectedLimit);
        }
        if (-Minimum < Limits.Get_MinimumNormalExtentCm()
            || Maximum < Limits.Get_MinimumNormalExtentCm())
        {
            return FCutResult(ECutOutcome::RejectedTooSmall);
        }
        auto NegativeMesh = UE::Geometry::FDynamicMesh3{InSource.Get_Mesh()};
        auto PositiveMesh = UE::Geometry::FDynamicMesh3{InSource.Get_Mesh()};
        AddZeroExteriorUVs(NegativeMesh);
        AddZeroExteriorUVs(PositiveMesh);
        int32 NegativeCapTriangles = 0;
        int32 PositiveCapTriangles = 0;
        if (NOT CutOneSide(NegativeMesh, InOptions, UVFrame, Normal, NegativeCapTriangles)
            || NOT CutOneSide(PositiveMesh, InOptions, UVFrame, -Normal, PositiveCapTriangles))
        {
            return FCutResult(ECutOutcome::RejectedTopology);
        }
        if (NOT FitsLimits(NegativeMesh, Limits) || NOT FitsLimits(PositiveMesh, Limits))
        {
            return FCutResult(ECutOutcome::RejectedLimit);
        }
        if (NOT HasValidSideAndExtent(NegativeMesh, Plane.Get_PositionCm(), Normal,
                Limits, false)
            || NOT HasValidSideAndExtent(PositiveMesh, Plane.Get_PositionCm(), Normal,
                Limits, true))
        {
            return FCutResult(ECutOutcome::RejectedTooSmall);
        }
        auto AdmitOptions = geometry::FImportOptions{};
        AdmitOptions.Set_MaximumVertices(Limits.Get_MaximumVertices());
        AdmitOptions.Set_MaximumTriangles(Limits.Get_MaximumTriangles());
        AdmitOptions.Set_MinimumVolumeCm3(Limits.Get_MinimumOutputVolumeCm3());
        const auto NegativeResult = geometry::Admit(MoveTemp(NegativeMesh), AdmitOptions);
        const auto PositiveResult = geometry::Admit(MoveTemp(PositiveMesh), AdmitOptions);
        if (NOT NegativeResult.Get_IsReady() || NOT PositiveResult.Get_IsReady())
        {
            if (NegativeResult.Get_Failure() == geometry::EImportFailure::NonPositiveVolume
                || PositiveResult.Get_Failure() == geometry::EImportFailure::NonPositiveVolume)
            {
                return FCutResult(ECutOutcome::RejectedTooSmall);
            }
            return FCutResult(ECutOutcome::RejectedTopology);
        }
        const auto& Negative = NegativeResult.Get_Geometry();
        const auto& Positive = PositiveResult.Get_Geometry();
        if (Negative->Get_VolumeCm3() < Limits.Get_MinimumOutputVolumeCm3()
            || Positive->Get_VolumeCm3() < Limits.Get_MinimumOutputVolumeCm3())
        {
            return FCutResult(ECutOutcome::RejectedTooSmall);
        }
        const auto TotalVolume = Positive->Get_VolumeCm3() + Negative->Get_VolumeCm3();
        const auto AllowedError = FMath::Max(Limits.Get_AbsoluteVolumeToleranceCm3(),
            Limits.Get_RelativeVolumeTolerance() * InSource.Get_VolumeCm3());
        if (FMath::Abs(TotalVolume - InSource.Get_VolumeCm3()) > AllowedError)
        {
            return FCutResult(ECutOutcome::RejectedTopology);
        }
        return FCutResult(Positive, Negative, PositiveCapTriangles, NegativeCapTriangles);
    }
}
