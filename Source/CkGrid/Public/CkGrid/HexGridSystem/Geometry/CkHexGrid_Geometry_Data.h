#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkHexGrid_Geometry_Data.generated.h"

namespace ck::hex_grid
{
    inline constexpr int32 DomainCellLimit = 65536;
}

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_HexDirection : uint8
{
    PlusQ,
    PlusR,
    MinusQPlusR,
    MinusQ,
    MinusR,
    PlusQMinusR
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexDirection);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_HexOrientation : uint8
{
    Pointy,
    Flat
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexOrientation);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_HexRotation : uint8
{
    Turn0,
    Turn1,
    Turn2,
    Turn3,
    Turn4,
    Turn5
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexRotation);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_HexDomainKind : uint8
{
    Disk,
    Explicit
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexDomainKind);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_HexGeometryFailure : uint8
{
    None,
    InvalidCoordinate,
    InvalidDirection,
    InvalidRotation,
    InvalidOrientation,
    InvalidLayout,
    InvalidDomain,
    DuplicateCoordinate,
    LimitExceeded,
    Overflow,
    PrecisionLoss,
    UnsupportedTransform,
    OffPlane,
    ParallelRay,
    BehindRay,
    OutsideDomain
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGeometryFailure);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexCoord
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexCoord);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _Q = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _R = 0;

public:
    CK_PROPERTY(_Q);
    CK_PROPERTY(_R);

    CK_DEFINE_CONSTRUCTORS(FCk_HexCoord, _Q, _R);

public:
    auto
    Get_S() const -> int64;

    auto
    operator==(
        const FCk_HexCoord& InOther) const -> bool;

    friend CKGRID_API uint32
    GetTypeHash(
        const FCk_HexCoord& InCoordinate);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexLayout
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexLayout);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_HexOrientation _Orientation = ECk_HexOrientation::Pointy;

    /** Center-to-corner distance in local Unreal units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _OuterRadius = 100.0;

public:
    CK_PROPERTY(_Orientation);
    CK_PROPERTY(_OuterRadius);

    CK_DEFINE_CONSTRUCTORS(FCk_HexLayout, _Orientation, _OuterRadius);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexDomain
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_HexDomain);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_HexDomainKind _Kind = ECk_HexDomainKind::Disk;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, EditCondition = "_Kind == ECk_HexDomainKind::Disk", EditConditionHides))
    FCk_HexCoord _Center;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, EditCondition = "_Kind == ECk_HexDomainKind::Disk", EditConditionHides))
    int32 _RingRadius = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, EditCondition = "_Kind == ECk_HexDomainKind::Explicit", EditConditionHides))
    TArray<FCk_HexCoord> _Coordinates;

public:
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_Center);
    CK_PROPERTY(_RingRadius);
    CK_PROPERTY(_Coordinates);

    CK_DEFINE_CONSTRUCTORS(FCk_HexDomain, _Kind, _Center, _RingRadius, _Coordinates);
};

// --------------------------------------------------------------------------------------------------------------------
