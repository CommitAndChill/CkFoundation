#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Readiness_Data.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment_Data.h"

#include "CkHexGridPlacement_Result_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKGRID_API FCk_Handle_HexGridPlacement : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_HexGridPlacement);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_HexGridPlacement);

UENUM(BlueprintType)
enum class ECk_HexGridOccupantState : uint8
{
    Anonymous,
    AwaitingBinding,
    Bound,
    BindingConflict
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridOccupantState);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridPlacement_View
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridPlacement_View);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGuid _PlacementId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Handle_HexGrid _Grid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Handle _Occupant;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGuid _OccupantBindingId;

    // Remapped handle supplied by the saved reference; independent of an accepted object identity.
    UPROPERTY()
    FCk_Handle _ExpectedOccupant;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Handle_HexGridPlacement _Handle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexGridOccupantState _OccupantState = ECk_HexGridOccupantState::Anonymous;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _FootprintOffsets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _RequiredCellTags;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _ForbiddenCellTags;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexCoord _Anchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexRotation _Rotation = ECk_HexRotation::Turn0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexGridConnectivity _Connectivity = ECk_HexGridConnectivity::Ignore;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _Cells;

public:
    CK_PROPERTY(_PlacementId);
    CK_PROPERTY(_Grid);
    CK_PROPERTY(_Occupant);
    CK_PROPERTY(_OccupantBindingId);
    CK_PROPERTY(_ExpectedOccupant);
    CK_PROPERTY(_Handle);
    CK_PROPERTY(_OccupantState);
    CK_PROPERTY(_FootprintOffsets);
    CK_PROPERTY(_RequiredCellTags);
    CK_PROPERTY(_ForbiddenCellTags);
    CK_PROPERTY(_Anchor);
    CK_PROPERTY(_Rotation);
    CK_PROPERTY(_Connectivity);
    CK_PROPERTY(_Cells);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridPlacement_Result
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridPlacement_Result);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    bool _Succeeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexGridFailure _Failure = ECk_HexGridFailure::InvalidGrid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGuid _PlacementId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _FailedCells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexGridPlacement_View _Placement;

public:
    CK_PROPERTY(_Succeeded);
    CK_PROPERTY(_Failure);
    CK_PROPERTY(_PlacementId);
    CK_PROPERTY(_FailedCells);
    CK_PROPERTY(_Placement);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridPlacements_Result
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridPlacements_Result);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    bool _Succeeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexGridFailure _Failure = ECk_HexGridFailure::InvalidGrid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexGridPlacement_View> _Placements;

public:
    CK_PROPERTY(_Succeeded);
    CK_PROPERTY(_Failure);
    CK_PROPERTY(_Placements);
};
