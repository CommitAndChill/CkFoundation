#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Slice.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "ConstrainedDelaunay2.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"
#include "FrameTypes.h"
#include "Operations/MeshPlaneCut.h"
#include "Selections/MeshConnectedComponents.h"

#include <Algo/StableSort.h>

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
                && static_cast<uint8>(Cap.Get_UVPolicy())
                    <= static_cast<uint8>(ECapUVPolicy::PlanarOnLayer0_KeepInterpolated)
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
                && Limits.Get_MaximumTriangles() >= 4 && Limits.Get_MaximumTriangles() <= 4096
                && Limits.Get_MaximumPiecesPerSide() >= 1 && Limits.Get_MaximumPiecesPerSide() <= 64
                && static_cast<uint8>(InOptions.Get_IslandPolicy())
                    <= static_cast<uint8>(EIslandPolicy::SplitIslands);
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

        // The exterior's value of a UV layer at a cap rim vertex, read from the lowest-ID non-cap triangle touching
        // it so the choice is deterministic where the layer has a seam at the rim.
        auto
            FindRimValue(const FDynamicMesh3& InMesh, const FDynamicMeshUVOverlay& InUVs,
                const TSet<int32>& InCapTriangles, int32 InVertexID, FVector2f& OutValue) -> bool
        {
            auto ExteriorTriangle = FDynamicMesh3::InvalidID;
            for (const auto TriangleID : InMesh.VtxTrianglesItr(InVertexID))
            {
                if (InCapTriangles.Contains(TriangleID) || NOT InUVs.IsSetTriangle(TriangleID))
                {
                    continue;
                }
                if (ExteriorTriangle == FDynamicMesh3::InvalidID || TriangleID < ExteriorTriangle)
                {
                    ExteriorTriangle = TriangleID;
                }
            }
            if (ExteriorTriangle == FDynamicMesh3::InvalidID)
            {
                return false;
            }
            const auto Triangle = InMesh.GetTriangle(ExteriorTriangle);
            const auto Elements = InUVs.GetTriangle(ExteriorTriangle);
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                if (Triangle[Corner] == InVertexID)
                {
                    OutValue = InUVs.GetElement(Elements[Corner]);
                    return true;
                }
            }
            return false;
        }

        // A layer after UV0 under a non-planar policy: one element per cap vertex holding zero or the rim value.
        auto
            SetCapLayerPerVertex(FDynamicMesh3& InMesh, const TArray<int32>& InCapTriangles,
                int32 InLayer, ECapUVPolicy InPolicy) -> bool
        {
            auto* UVs = InMesh.Attributes()->GetUVLayer(InLayer);
            const auto CapSet = TSet<int32>{InCapTriangles};
            auto ElementOfVertex = TMap<int32, int32>{};
            for (const auto TriangleID : InCapTriangles)
            {
                const auto Triangle = InMesh.GetTriangle(TriangleID);
                auto Elements = FIndex3i{};
                for (int32 Corner = 0; Corner < 3; ++Corner)
                {
                    if (const auto* Existing = ElementOfVertex.Find(Triangle[Corner]))
                    {
                        Elements[Corner] = *Existing;
                        continue;
                    }
                    auto Value = FVector2f::Zero();
                    if (InPolicy == ECapUVPolicy::PlanarOnLayer0_KeepInterpolated
                        && NOT FindRimValue(InMesh, *UVs, CapSet, Triangle[Corner], Value))
                    {
                        return false;
                    }
                    Elements[Corner] = UVs->AppendElement(Value);
                    ElementOfVertex.Add(Triangle[Corner], Elements[Corner]);
                }
                if (UVs->SetTriangle(TriangleID, Elements) != EMeshResult::Ok)
                {
                    return false;
                }
            }
            return true;
        }

        // Leaves the mesh uncompacted, so OutCapTriangles remain valid triangle IDs for the island split.
        auto
            CutOneSide(FDynamicMesh3& InMesh, const FCutOptions& InOptions,
                const FFrame3d& InUVFrame, const FVector3d& InCutNormal,
                TArray<int32>& OutCapTriangles) -> bool
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
            OutCapTriangles = CutOperator.HoleFillTriangles[0];
            if (OutCapTriangles.IsEmpty() || NOT SetCapColors(InMesh, OutCapTriangles, Cap.Get_Color()))
            {
                return false;
            }
            FDynamicMeshEditor Editor(&InMesh);
            const auto UVScale = static_cast<float>(1.0 / Cap.Get_CmPerUVUnit());
            for (int32 Layer = 0; Layer < InMesh.Attributes()->NumUVLayers(); ++Layer)
            {
                if (Layer == 0 || Cap.Get_UVPolicy() == ECapUVPolicy::PlanarOnAllLayers)
                {
                    Editor.SetTriangleUVsFromProjection(OutCapTriangles, InUVFrame,
                        UVScale, Cap.Get_UVOffset(), false, Layer);
                    continue;
                }
                if (NOT SetCapLayerPerVertex(InMesh, OutCapTriangles, Layer, Cap.Get_UVPolicy()))
                {
                    return false;
                }
            }
            return true;
        }

        struct FSidePiece
        {
        public:
            CK_GENERATED_BODY(FSidePiece);

        private:
            FDynamicMesh3 _Mesh;
            int32 _CapTriangles = 0;

        public:
            CK_PROPERTY(_Mesh);
            CK_PROPERTY_GET(_CapTriangles);

        public:
            CK_DEFINE_CONSTRUCTORS(FSidePiece, _Mesh, _CapTriangles);
        };

        // RejectMultiple keeps the side whole, so admission rejects a disconnected side exactly as it always has.
        auto
            SplitIntoPieces(FDynamicMesh3&& InMesh, const TArray<int32>& InCapTriangles,
                EIslandPolicy InPolicy) -> TArray<FSidePiece>
        {
            auto Pieces = TArray<FSidePiece>{};
            const auto KeepWhole = [&]() -> TArray<FSidePiece>
            {
                InMesh.RemoveUnusedVertices();
                InMesh.CompactInPlace();
                Pieces.Emplace(MoveTemp(InMesh), InCapTriangles.Num());
                return MoveTemp(Pieces);
            };
            if (InPolicy == EIslandPolicy::RejectMultiple)
            {
                return KeepWhole();
            }
            FMeshConnectedComponents Components(&InMesh);
            Components.FindConnectedTriangles();
            if (Components.Num() <= 1)
            {
                return KeepWhole();
            }
            auto ComponentOfTriangle = TArray<int32>{};
            ComponentOfTriangle.Init(INDEX_NONE, InMesh.MaxTriangleID());
            for (int32 Component = 0; Component < Components.Num(); ++Component)
            {
                for (const auto TriangleID : Components[Component].Indices)
                {
                    ComponentOfTriangle[TriangleID] = Component;
                }
            }
            auto CapTrianglesOfComponent = TArray<int32>{};
            CapTrianglesOfComponent.Init(0, Components.Num());
            for (const auto TriangleID : InCapTriangles)
            {
                ++CapTrianglesOfComponent[ComponentOfTriangle[TriangleID]];
            }
            constexpr auto KeepEveryTriangle = INDEX_NONE;
            constexpr auto OrderByComponent = true;
            auto Meshes = TArray<FDynamicMesh3>{};
            auto ComponentOfMesh = TArray<int32>{};
            FDynamicMeshEditor::SplitMesh(&InMesh, Meshes,
                [&](int32 InTriangleID) { return ComponentOfTriangle[InTriangleID]; },
                KeepEveryTriangle, &ComponentOfMesh, OrderByComponent);
            for (int32 Index = 0; Index < Meshes.Num(); ++Index)
            {
                Pieces.Emplace(MoveTemp(Meshes[Index]), CapTrianglesOfComponent[ComponentOfMesh[Index]]);
            }
            return Pieces;
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

        auto
            SumVolumeCm3(const TArray<FCutPiece>& InPieces) -> double
        {
            auto Total = 0.0;
            for (const auto& Piece : InPieces)
            {
                Total += Piece.Get_Geometry()->Get_VolumeCm3();
            }
            return Total;
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
        FCutResult(TArray<FCutPiece> InPositivePieces, TArray<FCutPiece> InNegativePieces)
        : _Outcome(ECutOutcome::Succeeded), _PositivePieces(MoveTemp(InPositivePieces)),
          _NegativePieces(MoveTemp(InNegativePieces))
    {
    }

    auto
        FCutResult::
        Get_Positive() const -> geometry::FValidatedGeometryPtr
    {
        return _PositivePieces.IsEmpty() ? geometry::FValidatedGeometryPtr{} : _PositivePieces[0].Get_Geometry();
    }

    auto
        FCutResult::
        Get_Negative() const -> geometry::FValidatedGeometryPtr
    {
        return _NegativePieces.IsEmpty() ? geometry::FValidatedGeometryPtr{} : _NegativePieces[0].Get_Geometry();
    }

    auto
        FCutResult::
        Get_PositiveCapTriangles() const -> int32
    {
        return _PositivePieces.IsEmpty() ? 0 : _PositivePieces[0].Get_CapTriangles();
    }

    auto
        FCutResult::
        Get_NegativeCapTriangles() const -> int32
    {
        return _NegativePieces.IsEmpty() ? 0 : _NegativePieces[0].Get_CapTriangles();
    }

    auto
        FCutResult::
        Get_IsSuccess() const -> bool
    {
        const auto IsValidPiece = [](const FCutPiece& InPiece) { return InPiece.Get_Geometry().IsValid(); };
        return _Outcome == ECutOutcome::Succeeded
            && NOT _PositivePieces.IsEmpty() && NOT _NegativePieces.IsEmpty()
            && ck::algo::AllOf(_PositivePieces, IsValidPiece)
            && ck::algo::AllOf(_NegativePieces, IsValidPiece);
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
        auto NegativeCapTriangles = TArray<int32>{};
        auto PositiveCapTriangles = TArray<int32>{};
        if (NOT CutOneSide(NegativeMesh, InOptions, UVFrame, Normal, NegativeCapTriangles)
            || NOT CutOneSide(PositiveMesh, InOptions, UVFrame, -Normal, PositiveCapTriangles))
        {
            return FCutResult(ECutOutcome::RejectedTopology);
        }

        // Every island is held to the same limits a whole half is; one failing island rejects the cut, so a slice
        // never publishes a subset of the source.
        auto NegativePieces = SplitIntoPieces(MoveTemp(NegativeMesh), NegativeCapTriangles,
            InOptions.Get_IslandPolicy());
        auto PositivePieces = SplitIntoPieces(MoveTemp(PositiveMesh), PositiveCapTriangles,
            InOptions.Get_IslandPolicy());
        if (NegativePieces.Num() > Limits.Get_MaximumPiecesPerSide()
            || PositivePieces.Num() > Limits.Get_MaximumPiecesPerSide())
        {
            return FCutResult(ECutOutcome::RejectedLimit);
        }
        const auto FitsPieceLimits = [&](const FSidePiece& InPiece)
        {
            return FitsLimits(InPiece.Get_Mesh(), Limits);
        };
        if (NOT ck::algo::AllOf(NegativePieces, FitsPieceLimits)
            || NOT ck::algo::AllOf(PositivePieces, FitsPieceLimits))
        {
            return FCutResult(ECutOutcome::RejectedLimit);
        }
        const auto HasCap = [](const FSidePiece& InPiece)
        {
            return InPiece.Get_CapTriangles() > 0;
        };
        if (NOT ck::algo::AllOf(NegativePieces, HasCap) || NOT ck::algo::AllOf(PositivePieces, HasCap))
        {
            return FCutResult(ECutOutcome::RejectedTopology);
        }
        constexpr auto PositiveSide = true;
        const auto IsOnNegativeSide = [&](const FSidePiece& InPiece)
        {
            return HasValidSideAndExtent(InPiece.Get_Mesh(), Plane.Get_PositionCm(), Normal, Limits, NOT PositiveSide);
        };
        const auto IsOnPositiveSide = [&](const FSidePiece& InPiece)
        {
            return HasValidSideAndExtent(InPiece.Get_Mesh(), Plane.Get_PositionCm(), Normal, Limits, PositiveSide);
        };
        if (NOT ck::algo::AllOf(NegativePieces, IsOnNegativeSide)
            || NOT ck::algo::AllOf(PositivePieces, IsOnPositiveSide))
        {
            return FCutResult(ECutOutcome::RejectedTooSmall);
        }

        auto AdmitOptions = geometry::FImportOptions{};
        AdmitOptions.Set_MaximumVertices(Limits.Get_MaximumVertices());
        AdmitOptions.Set_MaximumTriangles(Limits.Get_MaximumTriangles());
        AdmitOptions.Set_MinimumVolumeCm3(Limits.Get_MinimumOutputVolumeCm3());
        auto AnyRejected = false;
        auto AnyNonPositiveVolume = false;
        const auto AdmitSide = [&](TArray<FSidePiece>& InPieces) -> TArray<FCutPiece>
        {
            auto Admitted = TArray<FCutPiece>{};
            for (auto& Piece : InPieces)
            {
                const auto Result = geometry::Admit(MoveTemp(Piece.Get_Mesh()), AdmitOptions);
                if (NOT Result.Get_IsReady())
                {
                    AnyRejected = true;
                    AnyNonPositiveVolume |= Result.Get_Failure() == geometry::EImportFailure::NonPositiveVolume;
                    continue;
                }
                Admitted.Emplace(Result.Get_Geometry(), Piece.Get_CapTriangles());
            }
            return Admitted;
        };
        auto Negative = AdmitSide(NegativePieces);
        auto Positive = AdmitSide(PositivePieces);
        if (AnyRejected)
        {
            return FCutResult(AnyNonPositiveVolume ? ECutOutcome::RejectedTooSmall : ECutOutcome::RejectedTopology);
        }
        const auto IsLargeEnough = [&](const FCutPiece& InPiece)
        {
            return InPiece.Get_Geometry()->Get_VolumeCm3() >= Limits.Get_MinimumOutputVolumeCm3();
        };
        if (NOT ck::algo::AllOf(Negative, IsLargeEnough) || NOT ck::algo::AllOf(Positive, IsLargeEnough))
        {
            return FCutResult(ECutOutcome::RejectedTooSmall);
        }
        const auto TotalVolume = SumVolumeCm3(Positive) + SumVolumeCm3(Negative);
        const auto AllowedError = FMath::Max(Limits.Get_AbsoluteVolumeToleranceCm3(),
            Limits.Get_RelativeVolumeTolerance() * InSource.Get_VolumeCm3());
        if (FMath::Abs(TotalVolume - InSource.Get_VolumeCm3()) > AllowedError)
        {
            return FCutResult(ECutOutcome::RejectedTopology);
        }
        const auto IsLarger = [](const FCutPiece& InA, const FCutPiece& InB)
        {
            return InA.Get_Geometry()->Get_VolumeCm3() > InB.Get_Geometry()->Get_VolumeCm3();
        };
        Algo::StableSort(Positive, IsLarger);
        Algo::StableSort(Negative, IsLarger);
        return FCutResult(MoveTemp(Positive), MoveTemp(Negative));
    }
}
