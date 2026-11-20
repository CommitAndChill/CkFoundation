#include "CkHexGrid_Utils.h"

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Utils.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "Engine/World.h"

#include <cmath>

namespace
{
    using EBoardFailure = ECk_HexBoardFailure;
    using ESpatialFailure = ECk_HexGeometryFailure;
    using Geometry = UCk_Utils_HexGeometry_UE;
    using GridUtils = UCk_Utils_HexGrid_UE;

    auto BoardFailureFor(ESpatialFailure InSpatial) -> EBoardFailure
    {
        return InSpatial == ESpatialFailure::UnsupportedTransform
            ? EBoardFailure::UnsupportedTransform : EBoardFailure::SpatialQueryFailed;
    }

    auto TryGetFrame(const FCk_Handle_HexGrid& InGrid, FTransform& OutFrame,
        FCk_HexLayout& OutLayout, EBoardFailure& OutFailure) -> bool
    {
        OutFailure = EBoardFailure::InvalidHandle;
        if (ck::Is_NOT_Valid(InGrid) || NOT GridUtils::Has(InGrid))
        { return false; }
        const auto& Pivot = InGrid.Get<ck::FFragment_HexGrid>().Get_Pivot();
        if (ck::Is_NOT_Valid(Pivot))
        { return false; }
        const auto Transform = UCk_Utils_Transform_UE::Cast(Pivot);
        if (ck::Is_NOT_Valid(Transform))
        { return false; }

        OutFrame = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Transform);
        OutLayout = InGrid.Get<ck::FFragment_HexGrid_Params>().Get_Layout();
        OutFailure = EBoardFailure::None;
        return true;
    }

    auto TryGetMemberFrame(const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InCoordinate,
        FTransform& OutFrame, FCk_HexLayout& OutLayout, EBoardFailure& OutFailure) -> bool
    {
        auto Cell = FCk_Handle_HexCell{};
        if (NOT GridUtils::TryGet_CellAt(InGrid, InCoordinate, Cell, OutFailure)
            || NOT TryGetFrame(InGrid, OutFrame, OutLayout, OutFailure))
        { return false; }
        auto Spatial = ESpatialFailure::None;
        if (NOT Geometry::TryValidateWorldLayout(OutLayout, OutFrame, Spatial))
        {
            OutFailure = BoardFailureFor(Spatial);
            return false;
        }
        return true;
    }

    auto FindWinner(const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InWinner,
        FCk_Handle_HexCell& OutCell, ESpatialFailure& OutSpatialFailure, EBoardFailure& OutFailure) -> bool
    {
        if (GridUtils::TryGet_CellAt(InGrid, InWinner, OutCell, OutFailure))
        { return true; }
        if (OutFailure == EBoardFailure::MissingCell)
        { OutSpatialFailure = ESpatialFailure::OutsideDomain; }
        return false;
    }

    auto TryProjectPoint(const FVector2D& InPoint, const FTransform& InFrame, FVector& OutPoint) -> bool
    {
        const auto Rotation = InFrame.GetRotation().GetNormalized();
        const auto AxisQ = Rotation.GetAxisX();
        const auto AxisR = Rotation.GetAxisY();
        const auto Scale = InFrame.GetScale3D().X;
        const auto Translation = InFrame.GetTranslation();
        const auto Component = [&](double InX, double InY, double InOffset)
        {
            if (Scale < 1.0)
            { return std::fma(InX, Scale * InPoint.X, std::fma(InY, Scale * InPoint.Y, InOffset)); }
            return std::fma(Scale, std::fma(InX, InPoint.X, InY * InPoint.Y), InOffset);
        };
        const auto X = Component(AxisQ.X, AxisR.X, Translation.X);
        const auto Y = Component(AxisQ.Y, AxisR.Y, Translation.Y);
        const auto Z = Component(AxisQ.Z, AxisR.Z, Translation.Z);
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return false; }
        OutPoint = FVector{X, Y, Z};
        return true;
    }
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellFromWorldPoint(
        const FCk_Handle_HexGrid& InGrid, const FVector& InPoint, FCk_Handle_HexCell& OutCell,
        ECk_HexGeometryFailure& OutSpatialFailure, ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutCell = {};
    OutSpatialFailure = ESpatialFailure::None;
    auto Frame = FTransform::Identity;
    auto Layout = FCk_HexLayout{};
    if (NOT TryGetFrame(InGrid, Frame, Layout, OutFailure))
    { return false; }
    auto Winner = FCk_HexCoord{};
    if (NOT Geometry::TryGet_CoordFromWorldPoint(InPoint, Frame, Layout, Winner, OutSpatialFailure))
    {
        OutFailure = BoardFailureFor(OutSpatialFailure);
        return false;
    }
    return FindWinner(InGrid, Winner, OutCell, OutSpatialFailure, OutFailure);
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellFromWorldRay(
        const FCk_Handle_HexGrid& InGrid, const FVector& InOrigin, const FVector& InDirection,
        FCk_Handle_HexCell& OutCell, FVector& OutWorldHit, ECk_HexGeometryFailure& OutSpatialFailure,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    const auto Origin = InOrigin;
    const auto Direction = InDirection;
    OutCell = {};
    OutWorldHit = FVector::ZeroVector;
    OutSpatialFailure = ESpatialFailure::None;
    auto Frame = FTransform::Identity;
    auto Layout = FCk_HexLayout{};
    if (NOT TryGetFrame(InGrid, Frame, Layout, OutFailure))
    { return false; }

    auto Winner = FCk_HexCoord{};
    auto Hit = FVector::ZeroVector;
    if (NOT Geometry::TryGet_CoordFromWorldRay(Origin, Direction, Frame, Layout, Winner, Hit, OutSpatialFailure))
    {
        OutFailure = BoardFailureFor(OutSpatialFailure);
        return false;
    }
    if (NOT FindWinner(InGrid, Winner, OutCell, OutSpatialFailure, OutFailure))
    { return false; }
    OutWorldHit = Hit;
    return true;
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellWorldCenter(
        const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InCoordinate,
        FVector& OutCenter, ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutCenter = FVector::ZeroVector;
    auto Frame = FTransform::Identity;
    auto Layout = FCk_HexLayout{};
    if (NOT TryGetMemberFrame(InGrid, InCoordinate, Frame, Layout, OutFailure))
    { return false; }
    auto Local = FVector2D{};
    auto Spatial = ESpatialFailure::None;
    if (NOT Geometry::TryGet_LocalCenter(InCoordinate, Layout, Local, Spatial))
    {
        OutFailure = BoardFailureFor(Spatial);
        return false;
    }
    if (NOT TryProjectPoint(Local, Frame, OutCenter))
    {
        OutFailure = EBoardFailure::SpatialQueryFailed;
        return false;
    }
    return true;
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellWorldCorners(
        const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InCoordinate,
        TArray<FVector>& OutCorners, ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutCorners.Reset();
    auto Frame = FTransform::Identity;
    auto Layout = FCk_HexLayout{};
    if (NOT TryGetMemberFrame(InGrid, InCoordinate, Frame, Layout, OutFailure))
    { return false; }
    auto Local = TArray<FVector2D>{};
    auto Spatial = ESpatialFailure::None;
    if (NOT Geometry::TryGet_LocalCorners(InCoordinate, Layout, Local, Spatial))
    {
        OutFailure = BoardFailureFor(Spatial);
        return false;
    }
    for (const auto& Point : Local)
    {
        auto Corner = FVector::ZeroVector;
        if (NOT TryProjectPoint(Point, Frame, Corner) || OutCorners.Contains(Corner))
        {
            OutCorners.Reset();
            OutFailure = EBoardFailure::SpatialQueryFailed;
            return false;
        }
        OutCorners.Add(Corner);
    }
    return true;
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellWorldBounds(
        const FCk_Handle_HexGrid& InGrid, const FCk_HexCoord& InCoordinate,
        FBox& OutBounds, ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutBounds = FBox{ForceInit};
    auto Corners = TArray<FVector>{};
    if (NOT TryGet_CellWorldCorners(InGrid, InCoordinate, Corners, OutFailure))
    { return false; }
    for (const auto& Corner : Corners)
    { OutBounds += Corner; }
    return true;
}

auto
    UCk_Utils_HexGrid_UE::
    DebugDraw_Grid(
        const FCk_Handle_HexGrid& InGrid, const FCk_HexGrid_DebugDraw_Options& InOptions)
    -> void
{
    if (ck::Is_NOT_Valid(InGrid) || NOT Has(InGrid))
    { return; }
    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InGrid);
    if (NOT IsValid(World))
    { return; }
    const auto Seconds = InOptions.Get_Duration().Get_Seconds();
    const auto Width = InOptions.Get_LineThickness();
    if (NOT FMath::IsFinite(Seconds) || Seconds < 0.0 || Seconds > MAX_flt ||
        NOT FMath::IsFinite(Width) || Width <= 0.0f || Width > MAX_flt * 0.5f)
    { return; }
    const auto Duration = static_cast<float>(Seconds);
    for (const auto& Cell : Get_Cells(InGrid))
    {
        auto Coord = FCk_HexCoord{};
        auto Corners = TArray<FVector>{};
        auto Failure = EBoardFailure::None;
        if (NOT UCk_Utils_HexCell_UE::TryGet_Coordinate(Cell, Coord)
            || NOT TryGet_CellWorldCorners(InGrid, Coord, Corners, Failure))
        { continue; }
        const auto Selected = InOptions.Get_HighlightSelection() && Coord == InOptions.Get_SelectedCoordinate();
        const auto Color = Selected ? InOptions.Get_SelectedColor() : InOptions.Get_CellColor();
        const auto Thickness = Width * (Selected ? 2.0f : 1.0f);
        for (auto Index = 0; Index < Corners.Num(); ++Index)
        {
            UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Corners[Index], Corners[(Index + 1) % Corners.Num()],
                Color, Duration, Thickness);
        }
        if (InOptions.Get_DrawCoordinates())
        {
            auto Center = FVector::ZeroVector;
            if (TryGet_CellWorldCenter(InGrid, Coord, Center, Failure))
            {
                UCk_Utils_DebugDraw_UE::DrawDebugString(World, Center,
                    FString::Printf(TEXT("(%d,%d)"), Coord.Get_Q(), Coord.Get_R()), Color, Duration);
            }
        }
    }
}
