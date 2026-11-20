#pragma once

#include "CkCore/Time/CkTime.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Data.h"
#include <GameplayTagContainer.h>

#include "CkHexGrid_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKGRID_API FCk_Handle_HexGrid : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_HexGrid);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_HexGrid);

UENUM(BlueprintType)
enum class ECk_HexBoardFailure : uint8
{
    None,
    InvalidHandle,
    AlreadyComposed,
    InvalidSpec,
    CapacityExceeded,
    CompositionFailed,
    MissingCell,
    InvalidDirection,
    CoordinateOverflow,
    UnsupportedTransform,
    SpatialQueryFailed,
    InvalidSaveIdentity
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexBoardFailure);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexCellTagOverride
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexCellTagOverride);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Cells", meta = (AllowPrivateAccess = true))
    FCk_HexCoord _Coordinate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Cells", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _Tags;

public:
    CK_PROPERTY(_Coordinate);
    CK_PROPERTY(_Tags);

    CK_DEFINE_CONSTRUCTORS(FCk_HexCellTagOverride, _Coordinate, _Tags);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridAuthoredBlocker
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexGridAuthoredBlocker);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Named Blockers", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _Cells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Named Blockers", meta = (AllowPrivateAccess = true))
    FGameplayTag _Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Named Blockers", meta = (AllowPrivateAccess = true))
    bool _InitiallyActive = true;

public:
    CK_PROPERTY(_Cells);
    CK_PROPERTY(_Name);
    CK_PROPERTY(_InitiallyActive);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridAuthoredBlocker, _Cells, _Name, _InitiallyActive);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGrid_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexGrid_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (AllowPrivateAccess = true))
    FCk_HexLayout _Layout;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board Shape", meta = (AllowPrivateAccess = true))
    FCk_HexDomain _Domain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (AllowPrivateAccess = true))
    FTransform _Pivot = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Cells", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _DefaultCellTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Cells", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _InitialDisabledCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Cells", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCellTagOverride> _PerCellTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Named Blockers", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexGridAuthoredBlocker> _Blockers;

public:
    CK_PROPERTY(_Layout);
    CK_PROPERTY(_Domain);
    CK_PROPERTY(_Pivot);
    CK_PROPERTY(_DefaultCellTags);
    CK_PROPERTY(_InitialDisabledCells);
    CK_PROPERTY(_PerCellTags);
    CK_PROPERTY(_Blockers);

    CK_DEFINE_CONSTRUCTORS(FCk_HexGrid_Spec, _Layout, _Domain, _Pivot, _DefaultCellTags, _InitialDisabledCells, _PerCellTags);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGrid_DebugDraw_Options
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexGrid_DebugDraw_Options);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowPrivateAccess = true))
    FLinearColor _CellColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowPrivateAccess = true))
    FLinearColor _SelectedColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowPrivateAccess = true))
    float _LineThickness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (AllowPrivateAccess = true))
    FCk_Time _Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Labels", meta = (AllowPrivateAccess = true))
    bool _DrawCoordinates = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (AllowPrivateAccess = true))
    bool _HighlightSelection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (AllowPrivateAccess = true))
    FCk_HexCoord _SelectedCoordinate;

public:
    CK_PROPERTY(_CellColor);
    CK_PROPERTY(_SelectedColor);
    CK_PROPERTY(_LineThickness);
    CK_PROPERTY(_Duration);
    CK_PROPERTY(_DrawCoordinates);
    CK_PROPERTY(_HighlightSelection);
    CK_PROPERTY(_SelectedCoordinate);

    CK_DEFINE_CONSTRUCTORS(FCk_HexGrid_DebugDraw_Options, _CellColor, _SelectedColor, _LineThickness, _Duration, _DrawCoordinates, _HighlightSelection, _SelectedCoordinate);
};
