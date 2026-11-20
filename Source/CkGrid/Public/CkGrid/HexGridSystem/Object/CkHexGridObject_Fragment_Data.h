#pragma once

#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Data.h"
#include <GameplayTagContainer.h>

#include "CkHexGridObject_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKGRID_API FCk_Handle_HexGridObject : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_HexGridObject);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_HexGridObject);

UENUM(BlueprintType)
enum class ECk_HexGridConnectivity : uint8
{
    Ignore,
    RequireConnected
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridConnectivity);

UENUM(BlueprintType)
enum class ECk_HexGridObject_Failure : uint8
{
    None,
    InvalidOwner,
    AlreadyComposed,
    CompositionFailed,
    InvalidObject,
    EmptyFootprint,
    DuplicateOffset,
    InvalidRotation,
    Overflow,
    TooManyCells,
    InvalidConnectivity,
    Disconnected,
    InvalidInitializationIdentity,
    InitializationIdentityRequired
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridObject_Failure);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridObject_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridObject_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footprint", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _FootprintOffsets = {FCk_HexCoord{}};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement Rules", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _RequiredCellTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement Rules", meta = (AllowPrivateAccess = true))
    FGameplayTagContainer _ForbiddenCellTags;

public:
    CK_PROPERTY(_FootprintOffsets);
    CK_PROPERTY(_RequiredCellTags);
    CK_PROPERTY(_ForbiddenCellTags);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridObject_Spec, _FootprintOffsets, _RequiredCellTags, _ForbiddenCellTags);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridObject_ResolveResult
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridObject_ResolveResult);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = true))
    bool _Succeeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = true))
    ECk_HexGridObject_Failure _Failure = ECk_HexGridObject_Failure::InvalidObject;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _Cells;

public:
    CK_PROPERTY_GET(_Succeeded);
    CK_PROPERTY_GET(_Failure);
    CK_PROPERTY_GET(_Cells);
    FCk_HexGridObject_ResolveResult() = default;
    explicit FCk_HexGridObject_ResolveResult(ECk_HexGridObject_Failure InFailure)
        : _Failure(InFailure)
    {}
    explicit FCk_HexGridObject_ResolveResult(TArray<FCk_HexCoord> InCells)
        : _Succeeded(true), _Failure(ECk_HexGridObject_Failure::None), _Cells(MoveTemp(InCells))
    {}
};
