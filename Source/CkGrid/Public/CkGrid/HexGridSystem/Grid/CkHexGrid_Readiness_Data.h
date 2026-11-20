#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"
#include "CkHexGrid_Readiness_Data.generated.h"

UENUM(BlueprintType)
enum class ECk_HexGridReadiness : uint8
{
    AwaitingRestore, AwaitingReplication, Ready, Unavailable, Destroying
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridReadiness);

UENUM(BlueprintType)
enum class ECk_HexGridFailure : uint8
{
    None, NotReady, InvalidGrid, InvalidObject, InvalidPlacementId,
    InvalidFootprint, InvalidRotation, Overflow, AnchorOutsideDomain, OutsideDomain,
    Blocked, Occupied, TagMismatch, Disconnected, NotAuthority, Cancelled,
    CrossGridTransfer, InternalFailure, InvalidSchema, RecipeMismatch, InvalidProjection,
    DuplicatePlacementId, OccupantAlreadyPlaced, LateOccupantConflict, RestoreTimedOut,
    RestoreBaselineMissing, ReplicationTimedOut, Destroyed
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridFailure);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridReadiness_Result
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridReadiness_Result);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_HexGridReadiness _State = ECk_HexGridReadiness::Unavailable;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_HexGridFailure _Failure = ECk_HexGridFailure::InvalidGrid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _PlacementId;

public:
    CK_PROPERTY_GET(_State);
    CK_PROPERTY_GET(_Failure);
    CK_PROPERTY_GET(_PlacementId);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridReadiness_Result, _State, _Failure, _PlacementId);
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGrid_ReadinessChanged, FCk_Handle_HexGrid, InGrid,
    const FCk_HexGridReadiness_Result&, InResult);

// Describes the rejected update. A previous accepted projection can still be Ready.
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGrid_ReplicationBaselineRejected, FCk_Handle_HexGrid, InGrid,
    const FCk_HexGridReadiness_Result&, InResult);
